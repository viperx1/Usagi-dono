#include <QtWidgets/QApplication>
#include "window.h"
#include "crashlog.h"
//#include "main.h"

// Import static plugins for Qt static builds
#ifdef QT_STATIC
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
#endif

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
