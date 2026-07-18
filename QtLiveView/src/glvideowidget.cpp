#include "glvideowidget.h"

// GL_LUMINANCE / GL_LUMINANCE_ALPHA work on both desktop compatibility
// profiles and GLES2, which is what QOpenGLWidget gives us by default.
static const char *kVertexShader = R"(
    attribute vec2 aPos;
    attribute vec2 aTc;
    varying vec2 vTc;
    void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
        vTc = aTc;
    }
)";

// BT.601 limited-range YUV → RGB.
static const char *kFragmentShader = R"(
    varying vec2 vTc;
    uniform sampler2D texY;
    uniform sampler2D texU;
    uniform sampler2D texV;
    uniform int uNv12;
    void main() {
        float y = texture2D(texY, vTc).r;
        float u, v;
        if (uNv12 == 1) {
            vec4 uv = texture2D(texU, vTc);
            u = uv.r;
            v = uv.a;
        } else {
            u = texture2D(texU, vTc).r;
            v = texture2D(texV, vTc).r;
        }
        y = 1.1643 * (y - 0.0625);
        u -= 0.5;
        v -= 0.5;
        gl_FragColor = vec4(y + 1.5958 * v,
                            y - 0.39173 * u - 0.81290 * v,
                            y + 2.017 * u,
                            1.0);
    }
)";

GLVideoWidget::GLVideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
}

GLVideoWidget::~GLVideoWidget()
{
    makeCurrent();
    if (m_tex[0])
        glDeleteTextures(3, m_tex);
    doneCurrent();
}

void GLVideoWidget::present(const VideoFrameData &frame)
{
    m_frame = frame;  // QByteArray planes are implicitly shared — cheap copy
    m_dirty = true;
    update();
}

void GLVideoWidget::initializeGL()
{
    initializeOpenGLFunctions();

    m_prog.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    m_prog.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    m_prog.link();

    glGenTextures(3, m_tex);
    for (GLuint tex : m_tex) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void GLVideoWidget::uploadTextures()
{
    const int chromaH = (m_frame.height + 1) / 2;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, m_tex[0]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, m_frame.strideY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frame.width, m_frame.height,
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame.y.constData());

    if (m_frame.nv12) {
        glBindTexture(GL_TEXTURE_2D, m_tex[1]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_frame.strideU / 2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
                     m_frame.width / 2, chromaH,
                     0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, m_frame.u.constData());
    } else {
        glBindTexture(GL_TEXTURE_2D, m_tex[1]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_frame.strideU);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frame.width / 2, chromaH,
                     0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame.u.constData());

        glBindTexture(GL_TEXTURE_2D, m_tex[2]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_frame.strideV);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frame.width / 2, chromaH,
                     0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frame.v.constData());
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void GLVideoWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    if (!m_frame.isValid())
        return;

    if (m_dirty) {
        uploadTextures();
        m_dirty = false;
    }

    // Fullscreen quad, texture flipped vertically (image rows are top-down).
    static const GLfloat pos[] = { -1, -1,  1, -1,  -1, 1,  1, 1 };
    static const GLfloat tc[]  = {  0,  1,  1,  1,   0, 0,  1, 0 };

    m_prog.bind();
    for (int i = 0; i < 3; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_tex[i]);
    }
    m_prog.setUniformValue("texY", 0);
    m_prog.setUniformValue("texU", 1);
    m_prog.setUniformValue("texV", 2);
    m_prog.setUniformValue("uNv12", m_frame.nv12 ? 1 : 0);

    m_prog.enableAttributeArray("aPos");
    m_prog.enableAttributeArray("aTc");
    m_prog.setAttributeArray("aPos", pos, 2);
    m_prog.setAttributeArray("aTc", tc, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_prog.disableAttributeArray("aPos");
    m_prog.disableAttributeArray("aTc");
    m_prog.release();
}
