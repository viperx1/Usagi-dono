#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "../usagi/src/logger.h"

class TestLogger : public QObject
{
    Q_OBJECT

private slots:
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
        LOG(testMessage);
        
        // Verify signal was emitted
        QCOMPARE(spy.count(), 1);
        
        // Verify signal argument contains the message and file info
        QList<QVariant> arguments = spy.takeFirst();
        QString loggedMessage = arguments.at(0).toString();
        QVERIFY(loggedMessage.contains(testMessage));
        QVERIFY(loggedMessage.contains("test_logger.cpp"));
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
        
        LOG("Message 1");
        LOG("Message 2");
        LOG("Message 3");
        
        // Verify all signals were emitted
        QCOMPARE(spy.count(), 3);
    }
};

QTEST_MAIN(TestLogger)
#include "test_logger.moc"
