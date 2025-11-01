#include <QtWidgets/QApplication>
#include "window.h"
#include "crashlog.h"
#include "logger.h"
//#include "main.h"

// Note: Qt static plugins are imported via qt_import_plugins() in CMakeLists.txt
// Manual Q_IMPORT_PLUGIN() is not needed and causes conflicts

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
    Logger::log("Usagi-dono application starting [main.cpp]");
    
    app.addLibraryPath(".\\plugins");
    window = new Window;
    window->show();
    return app.exec();
}
