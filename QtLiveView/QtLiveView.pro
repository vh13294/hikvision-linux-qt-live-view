TEMPLATE = app
TARGET   = QtLiveView

QT += core gui widgets

HEADERS += src/deviceconfig.h \
           src/videoframe.h   \
           src/liveviewwindow.h

SOURCES += src/main.cpp          \
           src/videoframe.cpp    \
           src/liveviewwindow.cpp

INCLUDEPATH += src/ \
               ../incEn

unix {
    LIBS += -lhcnetsdk -lPlayCtrl -lAudioRender -lSuperRender
}
