#include "liveviewwindow.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QScreen>

LiveViewWindow::LiveViewWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_central(new QWidget(this))
    , m_grid(new QGridLayout(m_central))
{
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

    int cols = qBound(1, m_config.numberOfScreen, 4);
    int rows = cols;
    for (int i = 0; i < rows * cols; i++) {
        VideoFrame *frame = new VideoFrame(m_central);
        m_frames.append(frame);
        m_grid->addWidget(frame, i / cols, i % cols);
    }

    initSdk();

    // Place window on the configured screen
    QList<QScreen*> screens = QApplication::screens();
    int screenIdx = qBound(0, m_config.displayScreen, screens.size() - 1);
    setGeometry(screens[screenIdx]->geometry());

    showFullScreen();
    startAllStreams();
}

LiveViewWindow::~LiveViewWindow()
{
    stopAllStreams();
    NET_DVR_Cleanup();
}

void LiveViewWindow::initSdk()
{
    // Tell the SDK where its component and crypto libraries live.
    // Without this, HPR_LoadDso fails to locate libPlayCtrl.so siblings at runtime.
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
}

void LiveViewWindow::startAllStreams()
{
    int frameIdx = 0;

    for (const DeviceEntry &dev : m_config.devices) {
        NET_DVR_USER_LOGIN_INFO  loginInfo{};
        NET_DVR_DEVICEINFO_V40   deviceInfoV40{};
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
            qWarning("Login failed for %s: error %d",
                     dev.ip.toLatin1().constData(), NET_DVR_GetLastError());
            continue;
        }
        m_userIds.append(userId);

        for (int channel : dev.channels) {
            if (frameIdx >= m_frames.size())
                break;

            NET_DVR_PREVIEWINFO previewInfo{};
            previewInfo.lChannel        = channel;
            previewInfo.dwLinkMode      = dev.streamType;
            previewInfo.hPlayWnd        = (HWND)m_frames[frameIdx]->videoWinId();
            previewInfo.bBlocked        = 1;
            previewInfo.dwDisplayBufNum = 1;

            LONG realHandle = NET_DVR_RealPlay_V40(userId, &previewInfo, nullptr, nullptr);
            if (realHandle < 0) {
                qWarning("RealPlay failed for %s ch%d: error %d",
                         dev.ip.toLatin1().constData(), channel, NET_DVR_GetLastError());
            } else {
                m_streams.append({realHandle, userId});
                frameIdx++;
            }
        }
    }
}

void LiveViewWindow::stopAllStreams()
{
    for (const StreamHandle &sh : m_streams)
        NET_DVR_StopRealPlay(sh.realHandle);
    m_streams.clear();

    for (LONG userId : m_userIds)
        NET_DVR_Logout_V30(userId);
    m_userIds.clear();
}
