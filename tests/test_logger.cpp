#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "../usagi/src/logger.h"
#include "../usagi/src/crashlog.h"

class TestLogger : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Initialize crash log system (required for Logger to work)
        CrashLog::install();
    }

    void testLoggerSingleton()
    {
        // Test that Logger::instance() returns the same instance
        Logger* instance1 = Logger::instance();
        Logger* instance2 = Logger::instance();
        QCOMPARE(instance1, instance2);
        QVERIFY(instance1 != nullptr);
    }

    void testLoggerSignalEmission()
    {
        // Test that Logger emits signal when logging
        Logger* logger = Logger::instance();
        QSignalSpy spy(logger, &Logger::logMessage);
        
        QString testMessage = "Test log message";
        Logger::log(testMessage);
        
        // Verify signal was emitted
        QCOMPARE(spy.count(), 1);
        
        // Verify signal argument
        QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.at(0).toString(), testMessage);
    }

    void testLoggerWithFileAndLine()
    {
        // Test that Logger includes file and line info when provided
        Logger* logger = Logger::instance();
        QSignalSpy spy(logger, &Logger::logMessage);
        
        QString testMessage = "Test message with context";
        Logger::log(testMessage, "test.cpp", 42);
        
        // Verify signal was emitted
        QCOMPARE(spy.count(), 1);
        
        // Verify signal includes file and line info
        QList<QVariant> arguments = spy.takeFirst();
        QString loggedMessage = arguments.at(0).toString();
        QVERIFY(loggedMessage.contains("test.cpp"));
        QVERIFY(loggedMessage.contains("42"));
        QVERIFY(loggedMessage.contains(testMessage));
    }

    void testLoggerMacro()
    {
        // Test the LOG macro
        Logger* logger = Logger::instance();
        QSignalSpy spy(logger, &Logger::logMessage);
        
        LOG("Test using LOG macro");
        
        // Verify signal was emitted
        QCOMPARE(spy.count(), 1);
        
        // Verify message includes file info from macro
        QList<QVariant> arguments = spy.takeFirst();
        QString loggedMessage = arguments.at(0).toString();
        QVERIFY(loggedMessage.contains("test_logger.cpp"));
        QVERIFY(loggedMessage.contains("Test using LOG macro"));
    }

    void testMultipleLogCalls()
    {
        // Test multiple log calls in sequence
        Logger* logger = Logger::instance();
        QSignalSpy spy(logger, &Logger::logMessage);
        
        Logger::log("Message 1");
        Logger::log("Message 2");
        Logger::log("Message 3");
        
        // Verify all signals were emitted
        QCOMPARE(spy.count(), 3);
    }
};

QTEST_MAIN(TestLogger)
#include "test_logger.moc"
