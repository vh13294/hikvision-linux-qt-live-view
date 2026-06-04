#pragma once
#include <QMainWindow>
#include <QGridLayout>
#include <QTimer>
#include <QVector>
#include "deviceconfig.h"
#include "videoframe.h"
#include "HCNetSDK.h"

class LiveViewWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit LiveViewWindow(QWidget *parent = nullptr);
    ~LiveViewWindow();

    // Called from the SDK exception callback (queued onto the main thread)
    Q_INVOKABLE void onStreamReconnecting(long realHandle);
    Q_INVOKABLE void onStreamReconnected(long realHandle);
    Q_INVOKABLE void onStreamDropped(long realHandle);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onRightClick();
    void retryNext();
    void scheduleRetry();

private:
    struct RetryEntry {
        DeviceEntry device;
        int         channel;
        int         frameIdx;
    };

    struct StreamHandle {
        LONG       realHandle;
        LONG       userId;
        RetryEntry entry;
    };

    void initSdk();
    void startAllStreams();
    void stopAllStreams();
    bool attemptStream(const RetryEntry &e);
    void applyVcaDrawMode(LONG userId, int channel);

    static constexpr int RETRY_STAGGER_MS  = 3000;
    static constexpr int RETRY_COOLDOWN_MS = 60000;

    AppConfig             m_config;
    QWidget              *m_central;
    QGridLayout          *m_grid;
    QVector<VideoFrame*>  m_frames;
    QVector<StreamHandle> m_streams;
    QVector<LONG>         m_userIds;
    int                   m_rightClickCount = 0;

    QVector<RetryEntry>   m_retryQueue;
    int                   m_retryPos = 0;
    QTimer               *m_retryTimer;
};
