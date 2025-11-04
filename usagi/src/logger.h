#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QObject>

/**
 * Unified logging system for Usagi-dono
 * 
 * This class provides a centralized logging mechanism that:
 * - Outputs to console (qDebug) for development
 * - Emits signal to update the Log tab in the UI
 * 
 * Note: CrashLog is intentionally kept separate for emergency crash situations only.
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
     * Logs a message to console and UI log tab
     * 
     * @param msg The message to log
     * @param file MANDATORY source file name (must not be empty) - prefer using LOG macro over __FILE__
     * @param line MANDATORY source line number (must be > 0) - prefer using LOG macro over __LINE__
     * 
     * IMPORTANT: The file and line parameters are MANDATORY and enforced by assertions.
     * If the application hits these assertions at runtime, it means Logger::log is being
     * called INCORRECTLY. Use the LOG(msg) macro instead (preferred), which automatically
     * provides file and line information.
     */
    static void log(const QString &msg, const QString &file, int line);
    
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
};

/**
 * Convenience macro for logging with file and line info
 * Usage: LOG("Your message")
 */
#define LOG(msg) Logger::log(msg, __FILE__, __LINE__)

#endif // LOGGER_H
