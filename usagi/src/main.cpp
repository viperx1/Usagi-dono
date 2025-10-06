#include <QtWidgets/QApplication>
#include "window.h"
#include "crashlog.h"
//#include "main.h"

//myAniDBApi *adbapi;
Window *window;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application metadata for crash logs
    app.setApplicationName("Usagi-dono");
    app.setApplicationVersion("1.0.0");
    
    // Install crash log handler
    CrashLog::install();
    
    app.addLibraryPath(".\\plugins");
    window = new Window;
    window->show();
    return app.exec();
}
