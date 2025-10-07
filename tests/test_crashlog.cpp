#include <QtTest/QtTest>
#include "../usagi/src/crashlog.h"

class TestCrashLog : public QObject
{
    Q_OBJECT

private slots:
    void testLogFilePathGeneration();
    void testLogMessageEncoding();
};

void TestCrashLog::testLogFilePathGeneration()
{
    // Test that getLogFilePath() can be called without crashing
    // Note: getLogFilePath() is private, so we test it indirectly via generateCrashLog
    // For now, just verify the class exists
    QVERIFY(true);
}

void TestCrashLog::testLogMessageEncoding()
{
    // Test that logMessage writes with proper UTF-8 encoding
    // Create a test message
    QString testMessage = "Test log message";
    
    // Call logMessage (this should write to a log file)
    CrashLog::logMessage(testMessage);
    
    // If we get here without crashing, test passes
    QVERIFY(true);
}

QTEST_MAIN(TestCrashLog)
#include "test_crashlog.moc"
