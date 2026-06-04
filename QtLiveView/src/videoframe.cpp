#include "videoframe.h"
#include <QMouseEvent>
#include <QResizeEvent>

VideoFrame::VideoFrame(QWidget *parent)
    : QFrame(parent)
    , m_playArea(new QFrame(this))
    , m_statusLabel(new QLabel(this))
{
    setStyleSheet("background-color: black; border: 1px solid #333;");
    m_playArea->setStyleSheet("background-color: black; border: none;");
    m_playArea->move(1, 1);

    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(
        "color: #ff4444;"
        "background-color: transparent;"
        "font-size: 14px;"
        "border: none;");
    m_statusLabel->hide();
    m_statusLabel->raise();
}

WId VideoFrame::videoWinId() const
{
    return m_playArea->winId();
}

void VideoFrame::setStatus(const QString &text)
{
    m_statusLabel->setText(text);
    m_statusLabel->setGeometry(0, 0, width(), height());
    m_statusLabel->show();
    m_statusLabel->raise();
}

void VideoFrame::clearStatus()
{
    m_statusLabel->hide();
    m_statusLabel->clear();
}

void VideoFrame::resizeEvent(QResizeEvent *event)
{
    m_playArea->resize(event->size().width() - 2, event->size().height() - 2);
    m_statusLabel->setGeometry(0, 0, event->size().width(), event->size().height());
}

void VideoFrame::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        emit rightClicked();
    QFrame::mousePressEvent(event);
}
