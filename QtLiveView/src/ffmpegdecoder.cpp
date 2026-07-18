#include "ffmpegdecoder.h"
#include <QDebug>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

FfmpegDecoder::FfmpegDecoder(QObject *parent)
    : QThread(parent)
{
    m_buffer.reserve(MAX_BUFFER_BYTES);
}

FfmpegDecoder::~FfmpegDecoder()
{
    shutdown();
}

void FfmpegDecoder::feed(const unsigned char *data, int size)
{
    if (size <= 0 || m_stop.load())
        return;

    QMutexLocker lock(&m_bufMutex);
    // Backlog cap: if the decoder can't keep up, drop everything buffered so
    // far. The codec is flushed and recovers at the next keyframe — a brief
    // glitch instead of ever-growing latency.
    if (m_buffer.size() - m_head + size > MAX_BUFFER_BYTES) {
        m_buffer.clear();
        m_head = 0;
        m_flushNeeded.store(true);
    }
    m_buffer.append(reinterpret_cast<const char *>(data), size);
    m_bufCond.wakeOne();
}

int FfmpegDecoder::readPacket(void *opaque, unsigned char *buf, int bufSize)
{
    auto *self = static_cast<FfmpegDecoder *>(opaque);

    QMutexLocker lock(&self->m_bufMutex);
    while (!self->m_stop.load() && self->m_buffer.size() == self->m_head)
        self->m_bufCond.wait(&self->m_bufMutex, 100);
    if (self->m_stop.load())
        return AVERROR_EOF;

    int n = qMin(self->m_buffer.size() - self->m_head, bufSize);
    memcpy(buf, self->m_buffer.constData() + self->m_head, n);
    self->m_head += n;

    // Reclaim consumed bytes occasionally instead of on every read.
    if (self->m_head > COMPACT_THRESHOLD) {
        self->m_buffer.remove(0, self->m_head);
        self->m_head = 0;
    }
    return n;
}

bool FfmpegDecoder::takeFrame(VideoFrameData &out)
{
    QMutexLocker lock(&m_frameMutex);
    if (!m_hasFrame)
        return false;
    out = m_latest;
    m_hasFrame = false;
    return true;
}

void FfmpegDecoder::shutdown()
{
    {
        QMutexLocker lock(&m_bufMutex);
        m_stop.store(true);
        m_bufCond.wakeAll();
    }
    wait(3000);
}

// FFmpeg asks which pixel format to use; pick VAAPI when offered, otherwise
// the first (software) format so decode still works without a GPU.
static AVPixelFormat getHwFormat(AVCodecContext *, const AVPixelFormat *fmts)
{
    for (const AVPixelFormat *p = fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == AV_PIX_FMT_VAAPI)
            return *p;
    }
    return fmts[0];
}

void FfmpegDecoder::deliverFrame(const AVFrame *f)
{
    VideoFrameData d;
    d.width  = f->width;
    d.height = f->height;
    const int chromaH = (f->height + 1) / 2;

    if (f->format == AV_PIX_FMT_NV12) {
        d.nv12    = true;
        d.strideY = f->linesize[0];
        d.strideU = f->linesize[1];
        d.y = QByteArray(reinterpret_cast<const char *>(f->data[0]), d.strideY * f->height);
        d.u = QByteArray(reinterpret_cast<const char *>(f->data[1]), d.strideU * chromaH);
    } else if (f->format == AV_PIX_FMT_YUV420P || f->format == AV_PIX_FMT_YUVJ420P) {
        d.strideY = f->linesize[0];
        d.strideU = f->linesize[1];
        d.strideV = f->linesize[2];
        d.y = QByteArray(reinterpret_cast<const char *>(f->data[0]), d.strideY * f->height);
        d.u = QByteArray(reinterpret_cast<const char *>(f->data[1]), d.strideU * chromaH);
        d.v = QByteArray(reinterpret_cast<const char *>(f->data[2]), d.strideV * chromaH);
    } else {
        static bool warned = false;
        if (!warned) {
            warned = true;
            qWarning() << "FfmpegDecoder: unsupported pixel format"
                       << av_get_pix_fmt_name(static_cast<AVPixelFormat>(f->format));
        }
        return;
    }

    {
        QMutexLocker lock(&m_frameMutex);
        m_latest   = std::move(d);
        m_hasFrame = true;
    }
    emit frameReady();
}

void FfmpegDecoder::run()
{
    av_log_set_level(AV_LOG_ERROR);

    constexpr int AVIO_BUF_SIZE = 32 * 1024;
    unsigned char *avioBuf = static_cast<unsigned char *>(av_malloc(AVIO_BUF_SIZE));
    AVIOContext *avio = avio_alloc_context(avioBuf, AVIO_BUF_SIZE, 0, this,
                                           &FfmpegDecoder::readPacket, nullptr, nullptr);

    AVFormatContext *fmt = avformat_alloc_context();
    fmt->pb = avio;
    fmt->flags |= AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_NOBUFFER;
    // Live stream: keep probing short so the first frame appears quickly.
    fmt->probesize            = 512 * 1024;
    fmt->max_analyze_duration = AV_TIME_BASE / 2;

    AVCodecContext *codecCtx = nullptr;
    AVPacket *pkt     = nullptr;
    AVFrame  *frame   = nullptr;
    AVFrame  *swFrame = nullptr;
    int videoIdx = -1;

    // Hikvision delivers MPEG-PS over the SDK tunnel.
    // const_cast: FFmpeg < 5.0 (e.g. Ubuntu 22.04's 4.4) declares the fmt
    // parameter of avformat_open_input() as non-const.
    const AVInputFormat *psFormat = av_find_input_format("mpeg");
    if (avformat_open_input(&fmt, nullptr,
                            const_cast<AVInputFormat *>(psFormat), nullptr) < 0) {
        if (!m_stop.load()) {
            qWarning() << "FfmpegDecoder: failed to open stream";
            emit decodeError("Decode Error: bad stream");
        }
        goto cleanup;
    }
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        if (!m_stop.load())
            emit decodeError("Decode Error: no stream info");
        goto cleanup;
    }

    videoIdx = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoIdx < 0) {
        emit decodeError("Decode Error: no video stream");
        goto cleanup;
    }

    {
        const AVCodecParameters *par = fmt->streams[videoIdx]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(par->codec_id);
        if (!codec) {
            emit decodeError("Decode Error: unsupported codec");
            goto cleanup;
        }

        codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecCtx, par);
        codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

        AVBufferRef *hwDevice = nullptr;
        if (av_hwdevice_ctx_create(&hwDevice, AV_HWDEVICE_TYPE_VAAPI,
                                   nullptr, nullptr, 0) == 0) {
            for (int i = 0;; i++) {
                const AVCodecHWConfig *cfg = avcodec_get_hw_config(codec, i);
                if (!cfg)
                    break;
                if ((cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                        && cfg->device_type == AV_HWDEVICE_TYPE_VAAPI) {
                    codecCtx->hw_device_ctx = av_buffer_ref(hwDevice);
                    codecCtx->get_format    = getHwFormat;
                    qInfo() << "FfmpegDecoder: VAAPI hardware decode enabled for"
                            << codec->name;
                    break;
                }
            }
            av_buffer_unref(&hwDevice);
        }
        if (!codecCtx->hw_device_ctx)
            qInfo() << "FfmpegDecoder: VAAPI unavailable, using software decode for"
                    << codec->name;

        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            emit decodeError("Decode Error: codec open failed");
            goto cleanup;
        }
    }

    pkt     = av_packet_alloc();
    frame   = av_frame_alloc();
    swFrame = av_frame_alloc();

    while (!m_stop.load()) {
        if (m_flushNeeded.exchange(false))
            avcodec_flush_buffers(codecCtx);

        int ret = av_read_frame(fmt, pkt);
        if (ret < 0) {
            if (m_stop.load() || ret == AVERROR_EOF)
                break;
            continue;  // transient demux error — resync on next pack
        }
        if (pkt->stream_index != videoIdx) {
            av_packet_unref(pkt);
            continue;
        }

        if (avcodec_send_packet(codecCtx, pkt) == 0) {
            while (avcodec_receive_frame(codecCtx, frame) == 0) {
                if (frame->format == AV_PIX_FMT_VAAPI) {
                    av_frame_unref(swFrame);
                    if (av_hwframe_transfer_data(swFrame, frame, 0) == 0)
                        deliverFrame(swFrame);
                } else {
                    deliverFrame(frame);
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(pkt);
    }

cleanup:
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&swFrame);
    avcodec_free_context(&codecCtx);
    if (fmt)
        avformat_close_input(&fmt);
    if (avio) {
        av_freep(&avio->buffer);
        avio_context_free(&avio);
    }
}
