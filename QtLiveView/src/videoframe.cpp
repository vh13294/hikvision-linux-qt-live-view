#include "videoframe.h"
#include <QResizeEvent>

VideoFrame::VideoFrame(QWidget *parent)
    : QFrame(parent)
    , m_playArea(new QFrame(this))
{
    setStyleSheet("background-color: black; border: 1px solid #333;");
    m_playArea->setStyleSheet("background-color: black; border: none;");
    m_playArea->move(1, 1);
}

WId VideoFrame::videoWinId() const
{
    return m_playArea->winId();
}

void VideoFrame::resizeEvent(QResizeEvent *event)
{
    m_playArea->resize(event->size().width() - 2, event->size().height() - 2);
}
