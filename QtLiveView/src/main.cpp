#include <QApplication>
#include "liveviewwindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QApplication app(argc, argv);
    LiveViewWindow window;
    window.show();
    return app.exec();
}
