#include "logger.h"
#include <QDebug>
#include <QDateTime>
#include <QMutex>
#include <cassert>

// Static instance pointer and mutex for thread safety
// Note: The instance is intentionally never deleted as it should live for the
// application's lifetime. This is standard practice for application-level singletons.
static Logger* s_instance = nullptr;
static QMutex s_instanceMutex;

Logger::Logger() : QObject(nullptr)
{
}

Logger* Logger::instance()
{
    // Double-checked locking pattern for thread-safe singleton
    // QMutex provides proper memory barriers ensuring correct memory ordering
    // across threads (see Qt documentation for QMutex memory ordering guarantees)
    if (!s_instance)
    {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance)
        {
            s_instance = new Logger();
        }
    }
    return s_instance;
}

void Logger::log(const QString &msg, const QString &file, int line)
{
    // MANDATORY: file and line parameters MUST be valid.
    // These assertions enforce correct usage of Logger::log.
    // If these assertions are hit at runtime, it means Logger::log is being called
    // INCORRECTLY and needs to be fixed. Use the LOG(msg) macro instead, which
    // automatically provides __FILE__ and __LINE__.
    assert(!file.isEmpty() && "Logger::log: file parameter is MANDATORY - use LOG(msg) macro instead");
    assert(line > 0 && "Logger::log: line parameter is MANDATORY - use LOG(msg) macro instead");
    // Build the full message
    QString fullMessage;

    // Extract just the filename from the full path
    QString filename = file;
    int lastSlash = filename.lastIndexOf('/');
    if (lastSlash == -1)
    {
        lastSlash = filename.lastIndexOf('\\');
    }
    if (lastSlash >= 0)
    {
        filename = filename.mid(lastSlash + 1);
    }
    //timestamp
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    fullMessage = QString("[%1] [%2:%3] %4").arg(timestamp).arg(filename).arg(line).arg(msg);
    
    // 1. Output to console (for development and debugging)
    qDebug().noquote() << fullMessage;
    
    // 2. Emit signal for UI log tab
    emit instance()->logMessage(fullMessage);
}
