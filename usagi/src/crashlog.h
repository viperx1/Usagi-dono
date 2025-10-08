#ifndef CRASHLOG_H
#define CRASHLOG_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QDateTime>
#include <QCoreApplication>

class CrashLog
{
public:
    static void install();
    static void logMessage(const QString &message);
    static void generateCrashLog(const QString &reason);
    static QString getStackTrace();
    
private:
    static QString getLogFilePath();
    static QString getSystemInfo();
};

#endif // CRASHLOG_H
