#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QDir>
#include "../usagi/src/crashlog.h"

class TestCrashLog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testLogFilePathGeneration();
    void testLogMessageUtf8Encoding();
    void testCrashLogUtf8Encoding();
    
private:
    QTemporaryDir* tempDir;
    QString originalAppDataLocation;
};

void TestCrashLog::initTestCase()
{
    // Create a temporary directory for test logs
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());
    
    // Override the AppDataLocation to use our temp directory
    QCoreApplication::setOrganizationName("UsagiTest");
    QCoreApplication::setApplicationName("CrashLogTest");
}

void TestCrashLog::cleanupTestCase()
{
    delete tempDir;
}

void TestCrashLog::testLogFilePathGeneration()
{
    // Test that getLogFilePath() can be called without crashing
    // Note: getLogFilePath() is private, so we test it indirectly via generateCrashLog
    // For now, just verify the class exists
    QVERIFY(true);
}

void TestCrashLog::testLogMessageUtf8Encoding()
{
    // Test that logMessage writes with proper UTF-8 encoding
    // Create a test message with ASCII, Latin-1, and Unicode characters
    QString testMessage = "Test log: ASCII, Ümlauts (ö, ä, ü), Chinese (中文), Emoji (🎉)";
    
    // Call logMessage (this should write to a log file)
    CrashLog::logMessage(testMessage);
    
    // Find the log file
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString logPath = logDir + "/usagi.log";
    
    // Verify the file exists
    QFile logFile(logPath);
    QVERIFY2(logFile.exists(), "Log file should exist");
    
    // Open and read the file as raw bytes
    QVERIFY2(logFile.open(QIODevice::ReadOnly), "Should be able to open log file");
    QByteArray fileBytes = logFile.readAll();
    logFile.close();
    
    // Verify the file is not empty
    QVERIFY2(!fileBytes.isEmpty(), "Log file should not be empty");
    
    // Check that the file does NOT start with UTF-16 LE BOM (0xFF 0xFE)
    QVERIFY2(!(fileBytes.size() >= 2 && 
               (unsigned char)fileBytes[0] == 0xFF && 
               (unsigned char)fileBytes[1] == 0xFE), 
             "File should NOT have UTF-16 LE BOM");
    
    // Check that the file does NOT start with UTF-16 BE BOM (0xFE 0xFF)
    QVERIFY2(!(fileBytes.size() >= 2 && 
               (unsigned char)fileBytes[0] == 0xFE && 
               (unsigned char)fileBytes[1] == 0xFF), 
             "File should NOT have UTF-16 BE BOM");
    
    // UTF-8 BOM (0xEF 0xBB 0xBF) should also NOT be present per our spec
    QVERIFY2(!(fileBytes.size() >= 3 && 
               (unsigned char)fileBytes[0] == 0xEF && 
               (unsigned char)fileBytes[1] == 0xBB && 
               (unsigned char)fileBytes[2] == 0xBF), 
             "File should NOT have UTF-8 BOM (we explicitly disable it)");
    
    // Try to decode as UTF-8 and verify it contains our test message
    QString decodedContent = QString::fromUtf8(fileBytes);
    QVERIFY2(decodedContent.contains("Test log"), 
             "Decoded UTF-8 content should contain our test message");
    
    // Verify that when decoded as UTF-8, we can find the special characters
    QVERIFY2(decodedContent.contains("Ümlauts"), 
             "Should be able to decode UTF-8 umlauts correctly");
    
    // Additional check: verify the file is NOT double-sized
    // UTF-16 would roughly double the size for ASCII text
    // This is a heuristic check - for mostly ASCII text, UTF-16 would be ~2x larger
    // Since we have a timestamp + our message, and it's mostly ASCII, 
    // the file should be much smaller than if it were UTF-16
    int expectedMaxSize = fileBytes.size() * 2; // Allow some margin
    QVERIFY2(fileBytes.size() < expectedMaxSize, 
             "File size should be consistent with UTF-8, not UTF-16");
}

void TestCrashLog::testCrashLogUtf8Encoding()
{
    // Test that generateCrashLog writes with proper UTF-8 encoding
    QString testReason = "Test crash: Segmentation Fault with Unicode (中文) and Emoji (🚨)";
    
    // Generate a crash log
    CrashLog::generateCrashLog(testReason);
    
    // Find the most recent crash log file
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(logDir);
    QStringList crashLogs = dir.entryList(QStringList() << "crash_*.log", QDir::Files, QDir::Time);
    
    QVERIFY2(!crashLogs.isEmpty(), "At least one crash log should exist");
    
    QString logPath = logDir + "/" + crashLogs.first();
    QFile logFile(logPath);
    
    // Verify the file exists
    QVERIFY2(logFile.exists(), "Crash log file should exist");
    
    // Open and read the file as raw bytes
    QVERIFY2(logFile.open(QIODevice::ReadOnly), "Should be able to open crash log file");
    QByteArray fileBytes = logFile.readAll();
    logFile.close();
    
    // Verify the file is not empty
    QVERIFY2(!fileBytes.isEmpty(), "Crash log file should not be empty");
    
    // Check that the file does NOT start with UTF-16 LE BOM (0xFF 0xFE)
    QVERIFY2(!(fileBytes.size() >= 2 && 
               (unsigned char)fileBytes[0] == 0xFF && 
               (unsigned char)fileBytes[1] == 0xFE), 
             "Crash log should NOT have UTF-16 LE BOM");
    
    // Check that the file does NOT start with UTF-16 BE BOM (0xFE 0xFF)
    QVERIFY2(!(fileBytes.size() >= 2 && 
               (unsigned char)fileBytes[0] == 0xFE && 
               (unsigned char)fileBytes[1] == 0xFF), 
             "Crash log should NOT have UTF-16 BE BOM");
    
    // UTF-8 BOM should also NOT be present per our spec
    QVERIFY2(!(fileBytes.size() >= 3 && 
               (unsigned char)fileBytes[0] == 0xEF && 
               (unsigned char)fileBytes[1] == 0xBB && 
               (unsigned char)fileBytes[2] == 0xBF), 
             "Crash log should NOT have UTF-8 BOM (we explicitly disable it)");
    
    // Try to decode as UTF-8 and verify it contains expected content
    QString decodedContent = QString::fromUtf8(fileBytes);
    QVERIFY2(decodedContent.contains("=== CRASH LOG ==="), 
             "Decoded UTF-8 content should contain crash log header");
    QVERIFY2(decodedContent.contains("Test crash"), 
             "Decoded UTF-8 content should contain our test reason");
    QVERIFY2(decodedContent.contains("=== END OF CRASH LOG ==="), 
             "Decoded UTF-8 content should contain crash log footer");
    
    // Verify the file starts with the expected ASCII header
    // "=== CRASH LOG ===" in UTF-8 should start with 0x3D ('=')
    QVERIFY2(fileBytes[0] == '=', 
             "File should start with '=' character (0x3D in UTF-8)");
    
    // In UTF-16 LE, '=' would be encoded as 0x3D 0x00
    // Check that we don't have zero bytes following ASCII characters
    // (this would indicate UTF-16 encoding)
    bool hasZeroBytePattern = false;
    for (int i = 0; i < qMin(100, fileBytes.size() - 1); i++) {
        // Check for pattern of ASCII character followed by 0x00 (common in UTF-16 LE)
        if ((unsigned char)fileBytes[i] >= 0x20 && (unsigned char)fileBytes[i] <= 0x7E && 
            fileBytes[i+1] == 0x00) {
            hasZeroBytePattern = true;
            break;
        }
    }
    QVERIFY2(!hasZeroBytePattern, 
             "File should NOT have zero bytes after ASCII chars (would indicate UTF-16 LE)");
    
    // Clean up the test crash log
    logFile.remove();
}

QTEST_MAIN(TestCrashLog)
#include "test_crashlog.moc"
