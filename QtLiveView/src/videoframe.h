#pragma once
#include <QFrame>

// A plain black frame that the Hikvision SDK renders into.
// Call videoWinId() and pass the result as hPlayWnd in NET_DVR_PREVIEWINFO.
class VideoFrame : public QFrame
{
    Q_OBJECT
public:
    explicit VideoFrame(QWidget *parent = nullptr);
    WId videoWinId() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QFrame *m_playArea;
};
