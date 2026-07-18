#pragma once
#include <QFrame>
#include <QLabel>

class GLVideoWidget;

// A plain black frame hosting the video surface.
// Native mode (default): an empty native child window — pass videoWinId() as
// hPlayWnd and the SDK renders into it.
// GL mode (hwDecode): hosts a GLVideoWidget that our FFmpeg decoder paints.
// Call setStatus() to overlay a centered message (e.g. errors); clearStatus() hides it.
class VideoFrame : public QFrame
{
    Q_OBJECT
public:
    explicit VideoFrame(QWidget *parent = nullptr, bool glVideo = false);
    WId     videoWinId() const;
    GLVideoWidget *glWidget() const { return m_glWidget; }
    void    setStatus(const QString &text);
    void    clearStatus();

signals:
    void rightClicked();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QFrame        *m_playArea = nullptr;
    GLVideoWidget *m_glWidget = nullptr;
    QLabel        *m_statusLabel;
};
