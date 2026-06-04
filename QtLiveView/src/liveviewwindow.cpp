#include "liveviewwindow.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QScreen>

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
        connect(frame, &VideoFrame::rightClicked, this, &LiveViewWindow::onRightClick);
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
            DWORD err = NET_DVR_GetLastError();
            qWarning("Login failed for %s: error %d", dev.ip.toLatin1().constData(), err);
            // Mark all frames this device would have occupied
            for (int i = 0; i < dev.channels.size() && frameIdx < m_frames.size(); i++, frameIdx++)
                m_frames[frameIdx]->setStatus(sdkErrorString(err, dev.ip));
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
                DWORD err = NET_DVR_GetLastError();
                qWarning("RealPlay failed for %s ch%d: error %d",
                         dev.ip.toLatin1().constData(), channel, err);
                m_frames[frameIdx]->setStatus(
                    QString("%1  ch%2\n%3").arg(dev.ip).arg(channel)
                        .arg(sdkErrorString(err, "").trimmed()));
            } else {
                m_frames[frameIdx]->clearStatus();
                m_streams.append({realHandle, userId});
            }
            frameIdx++;
        }
    }

    // Mark any leftover grid slots that have no configured source
    for (; frameIdx < m_frames.size(); frameIdx++)
        m_frames[frameIdx]->setStatus("No Source");
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

void LiveViewWindow::onRightClick()
{
    m_rightClickCount++;
    if (m_rightClickCount >= 2)
        QApplication::quit();
}
