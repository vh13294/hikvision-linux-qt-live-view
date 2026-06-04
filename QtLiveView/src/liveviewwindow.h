#pragma once
#include <QMainWindow>
#include <QGridLayout>
#include <QVector>
#include "deviceconfig.h"
#include "videoframe.h"
#include "HCNetSDK.h"

struct StreamHandle {
    LONG realHandle;
    LONG userId;
};

class LiveViewWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit LiveViewWindow(QWidget *parent = nullptr);
    ~LiveViewWindow();

private:
    void initSdk();
    void startAllStreams();
    void stopAllStreams();

    AppConfig            m_config;
    QWidget             *m_central;
    QGridLayout         *m_grid;
    QVector<VideoFrame*> m_frames;
    QVector<StreamHandle> m_streams;
    QVector<LONG>        m_userIds;
};
