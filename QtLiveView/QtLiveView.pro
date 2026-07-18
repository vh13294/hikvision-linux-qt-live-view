TEMPLATE = app
TARGET   = QtLiveView

QT += core gui widgets

HEADERS += src/deviceconfig.h \
           src/videoframe.h   \
           src/liveviewwindow.h \
           src/ffmpegdecoder.h \
           src/glvideowidget.h

SOURCES += src/main.cpp          \
           src/videoframe.cpp    \
           src/liveviewwindow.cpp \
           src/ffmpegdecoder.cpp \
           src/glvideowidget.cpp

INCLUDEPATH += src/ \
               ../incEn \
               ../QtDemo/includeCn

unix {
    LIBS += -lhcnetsdk
    # FFmpeg for the hwDecode path (VAAPI hardware decode with sw fallback)
    LIBS += -lavformat -lavcodec -lavutil
}
