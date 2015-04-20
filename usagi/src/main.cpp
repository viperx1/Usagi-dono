#include <QtWidgets/QApplication>
#include "window.h"
//#include "main.h"

//myAniDBApi *adbapi;
Window *window;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.addLibraryPath(".\\plugins");
    window = new Window;
    window->show();
    return app.exec();
}
