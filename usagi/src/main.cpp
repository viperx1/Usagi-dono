#include <QtWidgets/QApplication>
#include "window.h"
#include "crashlog.h"
#include "logger.h"
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
    
    // Test unified logging system
    LOG("Usagi-dono application starting [main.cpp]");
    
    app.addLibraryPath(".\\plugins");
    window = new Window;
    window->show();
    return app.exec();
}
