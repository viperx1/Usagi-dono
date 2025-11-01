#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QObject>

/**
 * Unified logging system for Usagi-dono
 * 
 * This class provides a centralized logging mechanism that:
 * - Outputs to console (qDebug) for development
 * - Writes to persistent log file via CrashLog
 * - Emits signal to update the Log tab in the UI
 * 
 * Usage:
 *   Logger::log("Your message here");
 *   Logger::log(QString("Formatted %1 message %2").arg(var1).arg(var2));
 */
class Logger : public QObject
{
    Q_OBJECT
    
public:
    /**
     * Main unified logging function
     * Logs a message to console, persistent file, and UI log tab
     * 
     * @param msg The message to log
     * @param file Optional source file name for context (default: empty)
     * @param line Optional source line number for context (default: 0)
     */
    static void log(const QString &msg, const QString &file = QString(), int line = 0);
    
    /**
     * Get the singleton instance of the Logger
     */
    static Logger* instance();
    
signals:
    /**
     * Signal emitted when a message is logged
     * The Window class should connect to this to update the Log tab
     */
    void logMessage(QString message);
    
private:
    Logger();
    static Logger* s_instance;
};

/**
 * Convenience macro for logging with file and line info
 * Usage: LOG("Your message")
 */
#define LOG(msg) Logger::log(msg, __FILE__, __LINE__)

#endif // LOGGER_H
