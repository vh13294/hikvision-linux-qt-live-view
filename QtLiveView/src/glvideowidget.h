#pragma once
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include "ffmpegdecoder.h"

// Renders decoded NV12/I420 frames with an OpenGL shader (YUV → RGB on the
// GPU). Replaces the PlayCtrl render window when hwDecode is enabled.
class GLVideoWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit GLVideoWidget(QWidget *parent = nullptr);
    ~GLVideoWidget() override;

    // GUI thread: display a new frame on the next repaint.
    void present(const VideoFrameData &frame);

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    void uploadTextures();

    QOpenGLShaderProgram m_prog;
    GLuint         m_tex[3] = {0, 0, 0};
    VideoFrameData m_frame;
    bool           m_dirty = false;
};
