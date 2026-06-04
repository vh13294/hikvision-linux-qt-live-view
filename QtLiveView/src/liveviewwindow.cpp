#include "liveviewwindow.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMetaObject>
#include <QScreen>

// Global pointer so the static SDK callback can reach the window instance.
static LiveViewWindow *g_window = nullptr;

static QString sdkErrorString(DWORD code, const QString &ip)
{
    QString desc;
    switch (code) {
    case 1:  desc = "Auth Failed";             break;  // NET_DVR_PASSWORD_ERROR
    case 2:  desc = "Insufficient Privilege";  break;  // NET_DVR_NOENOUGHPRI
    case 4:  desc = "Channel Error";           break;  // NET_DVR_CHANNEL_ERROR
    case 5:  desc = "Max Connections Reached"; break;  // NET_DVR_OVER_MAXLINK
    case 7:  desc = "Offline";                 break;  // NET_DVR_NETWORK_FAIL_CONNECT
    case 8:  desc = "Network Send Error";      break;  // NET_DVR_NETWORK_SEND_ERROR
    case 9:  desc = "Network Recv Error";      break;  // NET_DVR_NETWORK_RECV_ERROR
    case 10: desc = "Connection Timeout";      break;  // NET_DVR_NETWORK_RECV_TIMEOUT
    case 17: desc = "Parameter Error";         break;  // NET_DVR_PARAMETER_ERROR
    case 26: desc = "Password Format Error";   break;  // NET_DVR_PASSWORD_FORMAT_ERROR
    case 47: desc = "User Not Found";          break;  // NET_DVR_USERNOTEXIST
    case 152: desc = "Username Not Exist";     break;  // NET_DVR_USERNAME_NOT_EXIST
    case 153: desc = "Account Locked";         break;  // NET_DVR_USER_LOCKED
    default: desc = QString("Error %1").arg(code); break;
    }
    return QString("%1\n%2").arg(ip, desc);
}

// Fires on an SDK-internal thread — must not touch Qt objects directly.
static void CALLBACK exceptionCallback(DWORD dwType, LONG /*lUserID*/, LONG lHandle, void * /*pUser*/)
{
    if (!g_window) return;
    const long handle = static_cast<long>(lHandle);

    switch (dwType) {
    case EXCEPTION_PREVIEW:
    case EXCEPTION_RECONNECT:
        // SDK is handling reconnect internally — just update the status label.
        QMetaObject::invokeMethod(g_window, "onStreamReconnecting",
                                  Qt::QueuedConnection, Q_ARG(long, handle));
        break;
    case PREVIEW_RECONNECTSUCCESS:
        QMetaObject::invokeMethod(g_window, "onStreamReconnected",
                                  Qt::QueuedConnection, Q_ARG(long, handle));
        break;
    case EXCEPTION_RELOGIN_FAILED:
    case EXCEPTION_PREVIEW_RECONNECT_CLOSED:
        // SDK gave up — stop the dead handle and queue a manual retry.
        QMetaObject::invokeMethod(g_window, "onStreamDropped",
                                  Qt::QueuedConnection, Q_ARG(long, handle));
        break;
    default:
        break;
    }
}

// Called on an SDK-internal thread. Feeds raw encoded data directly into
// PlayCtrl, bypassing the HCPreview smart-overlay rendering pipeline.
void CALLBACK LiveViewWindow::rawDataCallback(LONG /*lPlayHandle*/, DWORD dwDataType,
                                              BYTE *pBuffer, DWORD dwBufSize, void *pUser)
{
    auto *ctx = static_cast<RawPlayCtx *>(pUser);
    if (!ctx || ctx->port < 0)
        return;

    if (dwDataType == NET_DVR_SYSHEAD) {
        PlayM4_SetStreamOpenMode(ctx->port, STREAME_REALTIME);
        if (PlayM4_OpenStream(ctx->port, pBuffer, dwBufSize, SOURCE_BUF_MIN)) {
            PlayM4_Play(ctx->port, static_cast<PLAYM4_HWND>(ctx->wid));
            ctx->opened = true;
        }
    } else if (dwDataType == NET_DVR_STREAMDATA && ctx->opened) {
        PlayM4_InputData(ctx->port, pBuffer, dwBufSize);
    }
}

void LiveViewWindow::stopRawPlay(RawPlayCtx *ctx)
{
    if (!ctx) return;
    if (ctx->opened) {
        PlayM4_Stop(ctx->port);
        PlayM4_CloseStream(ctx->port);
    }
    PlayM4_FreePort(ctx->port);
    delete ctx;
}

LiveViewWindow::LiveViewWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_central(new QWidget(this))
    , m_grid(new QGridLayout(m_central))
    , m_retryTimer(new QTimer(this))
{
    g_window = this;

    m_config = loadConfig("./config/DeviceConfig.json");
    if (!m_config.valid) {
        QMessageBox::critical(this, "Config Error",
            "Failed to load ./config/DeviceConfig.json.\n"
            "Make sure the file exists next to the binary.");
        QApplication::quit();
        return;
    }

    setCentralWidget(m_central);
    m_grid->setSpacing(0);
    m_grid->setContentsMargins(0, 0, 0, 0);

    int cols = qBound(1, m_config.gridSize, 4);
    for (int i = 0; i < cols * cols; i++) {
        VideoFrame *frame = new VideoFrame(m_central);
        connect(frame, &VideoFrame::rightClicked, this, &LiveViewWindow::onRightClick);
        m_frames.append(frame);
        m_grid->addWidget(frame, i / cols, i % cols);
    }

    m_retryTimer->setSingleShot(false);
    m_retryTimer->setInterval(RETRY_STAGGER_MS);
    connect(m_retryTimer, &QTimer::timeout, this, &LiveViewWindow::retryNext);

    initSdk();

    QList<QScreen*> screens = QApplication::screens();
    int screenIdx = qBound(0, m_config.monitorIndex, screens.size() - 1);
    setGeometry(screens[screenIdx]->geometry());

    showFullScreen();
    startAllStreams();
}

LiveViewWindow::~LiveViewWindow()
{
    g_window = nullptr;
    m_retryTimer->stop();
    stopAllStreams();
    NET_DVR_Cleanup();
}

void LiveViewWindow::initSdk()
{
    QString exePath = QFile::symLinkTarget("/proc/self/exe");
    QString libDir  = exePath.left(exePath.lastIndexOf('/')) + "/lib";

    NET_DVR_LOCAL_SDK_PATH sdkPath{};
    QByteArray comPath = (libDir + "/HCNetSDKCom").toLatin1();
    strncpy(sdkPath.sPath, comPath.constData(), sizeof(sdkPath.sPath) - 1);
    NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_SDK_PATH, &sdkPath);

    NET_DVR_LOCAL_SDK_PATH cryptoPath{};
    QByteArray cryptoLib = (libDir + "/libcrypto.so.1.1").toLatin1();
    strncpy(cryptoPath.sPath, cryptoLib.constData(), sizeof(cryptoPath.sPath) - 1);
    NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_LIBEAY_PATH, &cryptoPath);

    NET_DVR_LOCAL_SDK_PATH sslPath{};
    QByteArray sslLib = (libDir + "/libssl.so.1.1").toLatin1();
    strncpy(sslPath.sPath, sslLib.constData(), sizeof(sslPath.sPath) - 1);
    NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_SSLEAY_PATH, &sslPath);

    if (!NET_DVR_Init()) {
        QMessageBox::critical(this, "SDK Error",
            QString("NET_DVR_Init failed: error %1").arg(NET_DVR_GetLastError()));
        return;
    }

    NET_DVR_SetLogToFile(3, const_cast<char*>("./sdkLog"), false);
    NET_DVR_SetConnectTime(10000, 1);
    NET_DVR_SetExceptionCallBack_V30(0, nullptr, exceptionCallback, nullptr);
}

bool LiveViewWindow::attemptStream(const RetryEntry &e)
{
    NET_DVR_USER_LOGIN_INFO loginInfo{};
    NET_DVR_DEVICEINFO_V40  deviceInfoV40{};
    loginInfo.bUseAsynLogin = false;
    loginInfo.wPort         = e.device.port;
    strncpy(loginInfo.sDeviceAddress, e.device.ip.toLatin1().constData(),
            sizeof(loginInfo.sDeviceAddress) - 1);
    strncpy(loginInfo.sUserName, e.device.username.toLatin1().constData(),
            sizeof(loginInfo.sUserName) - 1);
    strncpy(loginInfo.sPassword, e.device.password.toLatin1().constData(),
            sizeof(loginInfo.sPassword) - 1);

    LONG userId = NET_DVR_Login_V40(&loginInfo, &deviceInfoV40);
    if (userId < 0) {
        m_frames[e.frameIdx]->setStatus(sdkErrorString(NET_DVR_GetLastError(), e.device.ip));
        return false;
    }

    NET_DVR_PREVIEWINFO previewInfo{};
    previewInfo.lChannel        = e.channel;
    previewInfo.dwLinkMode      = e.device.streamType;
    previewInfo.bBlocked        = 1;
    previewInfo.dwDisplayBufNum = 1;

    RawPlayCtx *ctx = nullptr;
    if (m_config.renderRaw) {
        int port = -1;
        if (PlayM4_GetPort(&port)) {
            ctx = new RawPlayCtx{port, m_frames[e.frameIdx]->videoWinId(), false};
        }
        // hPlayWnd stays NULL — PlayCtrl takes over display via the callback
    } else {
        previewInfo.hPlayWnd = (HWND)m_frames[e.frameIdx]->videoWinId();
    }

    LONG realHandle = NET_DVR_RealPlay_V40(userId, &previewInfo,
                                           m_config.renderRaw ? rawDataCallback : nullptr,
                                           ctx);
    if (realHandle < 0) {
        m_frames[e.frameIdx]->setStatus(
            QString("%1  ch%2\n%3").arg(e.device.ip).arg(e.channel)
                .arg(sdkErrorString(NET_DVR_GetLastError(), "").trimmed()));
        NET_DVR_Logout_V30(userId);
        stopRawPlay(ctx);
        return false;
    }

    m_frames[e.frameIdx]->clearStatus();
    m_streams.append({realHandle, userId, e, ctx});
    m_userIds.append(userId);
    return true;
}

void LiveViewWindow::startAllStreams()
{
    int frameIdx = 0;

    for (const DeviceEntry &dev : m_config.devices) {
        NET_DVR_USER_LOGIN_INFO loginInfo{};
        NET_DVR_DEVICEINFO_V40  deviceInfoV40{};
        loginInfo.bUseAsynLogin = false;
        loginInfo.wPort         = dev.port;
        strncpy(loginInfo.sDeviceAddress, dev.ip.toLatin1().constData(),
                sizeof(loginInfo.sDeviceAddress) - 1);
        strncpy(loginInfo.sUserName, dev.username.toLatin1().constData(),
                sizeof(loginInfo.sUserName) - 1);
        strncpy(loginInfo.sPassword, dev.password.toLatin1().constData(),
                sizeof(loginInfo.sPassword) - 1);

        LONG userId = NET_DVR_Login_V40(&loginInfo, &deviceInfoV40);
        if (userId < 0) {
            DWORD err = NET_DVR_GetLastError();
            for (int i = 0; i < dev.channels.size() && frameIdx < m_frames.size(); i++, frameIdx++) {
                m_frames[frameIdx]->setStatus(sdkErrorString(err, dev.ip));
                m_retryQueue.append({dev, dev.channels[i], frameIdx});
            }
            continue;
        }
        m_userIds.append(userId);

        for (int channel : dev.channels) {
            if (frameIdx >= m_frames.size())
                break;

            NET_DVR_PREVIEWINFO previewInfo{};
            previewInfo.lChannel        = channel;
            previewInfo.dwLinkMode      = dev.streamType;
            previewInfo.bBlocked        = 1;
            previewInfo.dwDisplayBufNum = 1;

            RawPlayCtx *ctx = nullptr;
            if (m_config.renderRaw) {
                int port = -1;
                if (PlayM4_GetPort(&port)) {
                    ctx = new RawPlayCtx{port, m_frames[frameIdx]->videoWinId(), false};
                }
            } else {
                previewInfo.hPlayWnd = (HWND)m_frames[frameIdx]->videoWinId();
            }

            LONG realHandle = NET_DVR_RealPlay_V40(userId, &previewInfo,
                                                   m_config.renderRaw ? rawDataCallback : nullptr,
                                                   ctx);
            if (realHandle < 0) {
                DWORD err = NET_DVR_GetLastError();
                m_frames[frameIdx]->setStatus(
                    QString("%1  ch%2\n%3").arg(dev.ip).arg(channel)
                        .arg(sdkErrorString(err, "").trimmed()));
                stopRawPlay(ctx);
                m_retryQueue.append({dev, channel, frameIdx});
            } else {
                m_frames[frameIdx]->clearStatus();
                m_streams.append({realHandle, userId, {dev, channel, frameIdx}, ctx});
            }
            frameIdx++;
        }
    }

    for (; frameIdx < m_frames.size(); frameIdx++)
        m_frames[frameIdx]->setStatus("No Source");

    if (!m_retryQueue.isEmpty())
        scheduleRetry();
}

void LiveViewWindow::onStreamReconnecting(long realHandle)
{
    for (const StreamHandle &sh : m_streams) {
        if (sh.realHandle == static_cast<LONG>(realHandle)) {
            m_frames[sh.entry.frameIdx]->setStatus(
                QString("%1\nReconnecting...").arg(sh.entry.device.ip));
            return;
        }
    }
}

void LiveViewWindow::onStreamReconnected(long realHandle)
{
    for (const StreamHandle &sh : m_streams) {
        if (sh.realHandle == static_cast<LONG>(realHandle)) {
            m_frames[sh.entry.frameIdx]->clearStatus();
            return;
        }
    }
}

// Called only when the SDK has given up reconnecting.
void LiveViewWindow::onStreamDropped(long realHandle)
{
    for (int i = 0; i < m_streams.size(); i++) {
        if (m_streams[i].realHandle == static_cast<LONG>(realHandle)) {
            StreamHandle sh = m_streams.takeAt(i);

            NET_DVR_StopRealPlay(sh.realHandle);  // guaranteed to stop the callback before returning
            stopRawPlay(sh.rawCtx);
            NET_DVR_Logout_V30(sh.userId);
            m_userIds.removeOne(sh.userId);

            m_frames[sh.entry.frameIdx]->setStatus(
                sdkErrorString(7 /*NET_DVR_NETWORK_FAIL_CONNECT*/, sh.entry.device.ip));

            // Only queue if not already waiting for retry
            bool alreadyQueued = false;
            for (const RetryEntry &re : m_retryQueue) {
                if (re.frameIdx == sh.entry.frameIdx) { alreadyQueued = true; break; }
            }
            if (!alreadyQueued)
                m_retryQueue.append(sh.entry);

            if (!m_retryTimer->isActive())
                scheduleRetry();
            return;
        }
    }
}

void LiveViewWindow::scheduleRetry()
{
    if (m_retryQueue.isEmpty())
        return;
    m_retryPos = 0;
    m_retryTimer->start();
}

void LiveViewWindow::retryNext()
{
    if (m_retryPos >= m_retryQueue.size()) {
        m_retryTimer->stop();
        QTimer::singleShot(RETRY_COOLDOWN_MS, this, &LiveViewWindow::scheduleRetry);
        return;
    }

    const RetryEntry entry = m_retryQueue[m_retryPos];
    if (attemptStream(entry))
        m_retryQueue.removeAt(m_retryPos);
    else
        m_retryPos++;
}

void LiveViewWindow::stopAllStreams()
{
    m_retryTimer->stop();
    m_retryQueue.clear();

    for (const StreamHandle &sh : m_streams) {
        NET_DVR_StopRealPlay(sh.realHandle);  // stops callback before returning
        stopRawPlay(sh.rawCtx);
    }
    m_streams.clear();

    for (LONG userId : m_userIds)
        NET_DVR_Logout_V30(userId);
    m_userIds.clear();
}

void LiveViewWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        QApplication::quit();
    QMainWindow::keyPressEvent(event);
}

void LiveViewWindow::onRightClick()
{
    m_rightClickCount++;
    if (m_rightClickCount >= 2)
        QApplication::quit();
}
