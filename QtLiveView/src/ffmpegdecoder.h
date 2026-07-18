#pragma once
#include <QByteArray>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <atomic>

// One decoded frame in CPU memory. NV12 (VAAPI output): y + interleaved uv in
// 'u'. I420 (software-decode output): separate y/u/v planes.
struct VideoFrameData {
    int width  = 0;
    int height = 0;
    bool nv12  = false;
    int strideY = 0, strideU = 0, strideV = 0;
    QByteArray y, u, v;
    bool isValid() const { return width > 0 && height > 0; }
};

// Decodes the Hikvision MPEG-PS stream delivered by the SDK raw-data callback
// using FFmpeg, with VAAPI hardware acceleration when available and automatic
// software fallback.
//
// feed() is called from the SDK's internal thread. Decoded frames are picked
// up on the GUI thread via takeFrame() after a frameReady() signal; only the
// latest frame is kept. If decode falls behind the stream, the input backlog
// is dropped wholesale, so live-view latency stays bounded instead of
// accumulating.
class FfmpegDecoder : public QThread
{
    Q_OBJECT
public:
    explicit FfmpegDecoder(QObject *parent = nullptr);
    ~FfmpegDecoder() override;

    // SDK thread: append stream data (SYSHEAD + STREAMDATA payloads).
    void feed(const unsigned char *data, int size);

    // GUI thread: fetch the newest decoded frame. Returns false if none pending.
    bool takeFrame(VideoFrameData &out);

    // GUI thread: stop the decode thread and wait for it to finish.
    void shutdown();

signals:
    void frameReady();
    void decodeError(const QString &message);

protected:
    void run() override;

private:
    static int readPacket(void *opaque, unsigned char *buf, int bufSize);
    void deliverFrame(const struct AVFrame *frame);

    // Input FIFO fed by the SDK callback, drained by the demuxer.
    static constexpr int MAX_BUFFER_BYTES  = 4 * 1024 * 1024;
    static constexpr int COMPACT_THRESHOLD = 1 * 1024 * 1024;
    QMutex            m_bufMutex;
    QWaitCondition    m_bufCond;
    QByteArray        m_buffer;
    int               m_head = 0;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_flushNeeded{false};

    // Latest-frame mailbox (decode thread writes, GUI thread reads).
    QMutex         m_frameMutex;
    VideoFrameData m_latest;
    bool           m_hasFrame = false;
};
