#include <QApplication>
#include "liveviewwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    LiveViewWindow window;
    window.show();
    return app.exec();
}
