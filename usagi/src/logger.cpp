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
    // Empty file and line parameters are NOT allowed.
    assert(!file.isEmpty() && "Logger::log: file parameter is empty");
    assert(line > 0 && "Logger::log: line parameter invalid");
    // Build the full message with optional file/line info
    QString fullMessage;
    if (!file.isEmpty() && line > 0)
    {
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
        
        fullMessage = QString("[%1:%2] %3").arg(filename).arg(line).arg(msg);
    }
    else
    {
        fullMessage = msg;
    }
    
    // 1. Output to console (for development and debugging)
    qDebug().noquote() << fullMessage;
    
    // 2. Emit signal for UI log tab (with timestamp)
    // The timestamp will be added by the receiving slot to maintain consistency
    emit instance()->logMessage(fullMessage);
}
