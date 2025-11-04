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
        // Test that Logger includes file and line info when provided with valid parameters
        // Note: This test uses valid parameters. The assertions for empty file/line
        // are intentionally not tested as they would cause test failure - the assertions
        // are meant to catch incorrect usage at runtime during development.
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

    void testLoggerWithoutFileAndLine()
    {
        // Test that Logger works when file and line are not provided
        Logger* logger = Logger::instance();
        QSignalSpy spy(logger, &Logger::logMessage);
        
        QString testMessage = "Test message without context";
        Logger::log(testMessage, "", 0);
        
        // Verify signal was emitted
        QCOMPARE(spy.count(), 1);
        
        // Verify signal contains the message
        QList<QVariant> arguments = spy.takeFirst();
        QString loggedMessage = arguments.at(0).toString();
        QVERIFY(loggedMessage.contains(testMessage));
        
        // Verify format is [timestamp] message (without file:line)
        // The message should start with '[' for the timestamp
        QVERIFY(loggedMessage.startsWith("["));
        
        // Find the closing bracket of the timestamp
        int closingBracket = loggedMessage.indexOf(']');
        QVERIFY(closingBracket > 0);
        
        // Count colons in the timestamp section (should be 2 for HH:mm:ss)
        int colonCount = 0;
        for (int i = 0; i < closingBracket; ++i) {
            if (loggedMessage[i] == ':') {
                colonCount++;
            }
        }
        QCOMPARE(colonCount, 2); // Two colons in HH:mm:ss.zzz format
        
        // Verify there's no second '[' which would indicate [file:line] format
        int secondBracket = loggedMessage.indexOf('[', closingBracket + 1);
        int messageIndex = loggedMessage.indexOf(testMessage);
        if (secondBracket >= 0) {
            // If there's a second '[', it should be after the message (not between timestamp and message)
            QVERIFY(secondBracket >= messageIndex);
        }
    }
};

QTEST_MAIN(TestLogger)
#include "test_logger.moc"
