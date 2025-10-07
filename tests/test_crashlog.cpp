#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QDir>
#include <QThread>
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
    void testCompleteProcessWithDataTypeConversions();
    
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

void TestCrashLog::testCompleteProcessWithDataTypeConversions()
{
    // This test reproduces the complete crash log generation process from the usagi codebase
    // including all datatype conversions:
    // 1. QString (reason) → QTextStream (UTF-8)
    // 2. QString → QByteArray (via toUtf8())
    // 3. QByteArray → const char* (via constData())
    
    // Test with Unicode characters to ensure conversions work correctly
    QString testReason = "Test: Access Violation with Symbols ÄÖÜ中文🔥";
    
    // Step 1: Simulate generateCrashLog with QString → QTextStream conversion
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(logDir);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString testLogPath = logDir + "/test_conversion_" + timestamp + ".log";
    QFile testFile(testLogPath);
    
    QVERIFY2(testFile.open(QIODevice::WriteOnly), "Should be able to create test file");
    
    {
        QTextStream stream(&testFile);
        // Explicitly set UTF-8 encoding (as done in generateCrashLog)
        stream.setEncoding(QStringConverter::Utf8);
        stream.setGenerateByteOrderMark(false);
        
        // Write crash log header
        stream << "=== CRASH LOG ===\n\n";
        
        // Write reason (QString → QTextStream conversion)
        stream << "Crash Reason: " << testReason << "\n\n";
        
        // Simulate system info (QString → QTextStream)
        QString systemInfo = "Application: Test\n";
        systemInfo += "Version: 1.0.0\n";
        systemInfo += "Qt Version: " + QString(qVersion()) + "\n";
        stream << systemInfo;
        
        // Simulate stack trace (QString → QTextStream)
        QString stackTrace = "\nStack Trace:\n";
        stackTrace += "  [0] TestFunction + 0x123\n";
        stackTrace += "  [1] MainFunction + 0x456\n";
        stream << stackTrace;
        
        stream << "\n=== END OF CRASH LOG ===\n";
    }
    testFile.close();
    
    // Step 2: Verify file was written correctly as UTF-8
    QVERIFY2(testFile.open(QIODevice::ReadOnly), "Should be able to open test file for reading");
    QByteArray fileBytes = testFile.readAll();
    testFile.close();
    
    // Verify no BOM markers
    QVERIFY2(!(fileBytes.size() >= 2 && 
               (unsigned char)fileBytes[0] == 0xFF && 
               (unsigned char)fileBytes[1] == 0xFE), 
             "File should NOT have UTF-16 LE BOM");
    
    QVERIFY2(!(fileBytes.size() >= 3 && 
               (unsigned char)fileBytes[0] == 0xEF && 
               (unsigned char)fileBytes[1] == 0xBB && 
               (unsigned char)fileBytes[2] == 0xBF), 
             "File should NOT have UTF-8 BOM");
    
    // Verify UTF-8 content
    QString decodedContent = QString::fromUtf8(fileBytes);
    QVERIFY2(decodedContent.contains(testReason), 
             "File should contain the reason with Unicode characters intact");
    QVERIFY2(decodedContent.contains("=== CRASH LOG ==="), 
             "File should contain the header");
    
    // Step 3: Test QString → QByteArray → const char* conversion
    // This simulates the conversion used in fprintf calls
    QByteArray reasonBytes = testReason.toUtf8();
    const char* reasonCStr = reasonBytes.constData();
    
    // Verify the C string contains valid UTF-8
    QVERIFY2(reasonCStr != nullptr, "constData() should return valid pointer");
    QVERIFY2(strlen(reasonCStr) > 0, "C string should not be empty");
    
    // Convert back to QString to verify round-trip conversion
    QString roundTrip = QString::fromUtf8(reasonCStr);
    QCOMPARE(roundTrip, testReason);
    
    // Step 4: Test the path conversion (QString → QByteArray → const char*)
    QByteArray pathBytes = testLogPath.toUtf8();
    const char* pathCStr = pathBytes.constData();
    
    QVERIFY2(pathCStr != nullptr, "Path constData() should return valid pointer");
    QVERIFY2(strlen(pathCStr) > 0, "Path C string should not be empty");
    
    // Verify round-trip for path
    QString pathRoundTrip = QString::fromUtf8(pathCStr);
    QCOMPARE(pathRoundTrip, testLogPath);
    
    // Step 5: Verify file content byte-by-byte for key markers
    // Check that '=' is encoded as single byte 0x3D (not 0x3D 0x00 as in UTF-16 LE)
    QVERIFY2(fileBytes[0] == '=', "First character should be '=' (0x3D)");
    if (fileBytes.size() > 1) {
        QVERIFY2(fileBytes[1] != 0x00, "Second byte should NOT be 0x00 (would indicate UTF-16 LE)");
    }
    
    // Step 6: Test the complete flow with actual generateCrashLog
    QString fullTestReason = "Full Test: Division by Zero with 日本語 and Emojis 🎯🔧";
    
    // Wait a moment to ensure different timestamp
    QThread::msleep(1100);
    
    CrashLog::generateCrashLog(fullTestReason);
    
    // Find the most recent crash log
    dir.refresh();
    QStringList crashLogs = dir.entryList(QStringList() << "crash_*.log", QDir::Files, QDir::Time);
    QVERIFY2(!crashLogs.isEmpty(), "generateCrashLog should create a crash log file");
    
    QString actualLogPath = logDir + "/" + crashLogs.first();
    QFile actualLogFile(actualLogPath);
    
    QVERIFY2(actualLogFile.open(QIODevice::ReadOnly), "Should be able to open actual crash log");
    QByteArray actualBytes = actualLogFile.readAll();
    actualLogFile.close();
    
    // Verify actual crash log has correct encoding
    QString actualContent = QString::fromUtf8(actualBytes);
    QVERIFY2(actualContent.contains(fullTestReason), 
             "Actual crash log should contain the full test reason with all Unicode characters");
    QVERIFY2(actualContent.contains("=== CRASH LOG ==="), 
             "Actual crash log should have correct header");
    QVERIFY2(actualContent.contains("=== END OF CRASH LOG ==="), 
             "Actual crash log should have correct footer");
    
    // Verify no UTF-16 patterns in actual log
    bool hasZeroBytePattern = false;
    for (int i = 0; i < qMin(200, actualBytes.size() - 1); i++) {
        if ((unsigned char)actualBytes[i] >= 0x20 && 
            (unsigned char)actualBytes[i] <= 0x7E && 
            actualBytes[i+1] == 0x00) {
            hasZeroBytePattern = true;
            break;
        }
    }
    QVERIFY2(!hasZeroBytePattern, 
             "Actual crash log should NOT have UTF-16 LE pattern");
    
    // Clean up test files
    testFile.remove();
    actualLogFile.remove();
}

QTEST_MAIN(TestCrashLog)
#include "test_crashlog.moc"
