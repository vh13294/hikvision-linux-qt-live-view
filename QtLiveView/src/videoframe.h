#pragma once
#include <QFrame>
#include <QLabel>

// A plain black frame that the Hikvision SDK renders into.
// Call videoWinId() and pass the result as hPlayWnd in NET_DVR_PREVIEWINFO.
// Call setStatus() to overlay a centered message (e.g. errors); clearStatus() hides it.
class VideoFrame : public QFrame
{
    Q_OBJECT
public:
    explicit VideoFrame(QWidget *parent = nullptr);
    WId     videoWinId() const;
    void    setStatus(const QString &text);
    void    clearStatus();

signals:
    void rightClicked();
    void resized();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QFrame *m_playArea;
    QLabel *m_statusLabel;
};
