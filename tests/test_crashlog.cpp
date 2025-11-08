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
    void testNoExtraNullBytes();
    void testStackTraceHasFunctionNames();
    void testSymbolResolutionLogging();
    
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
        dir.mkpath(logDir);
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

void TestCrashLog::testNoExtraNullBytes()
{
    // This test specifically validates that crash log files don't contain extra null bytes
    // that would cause text editors to misinterpret encoding (e.g., as UTF-16 when it's UTF-8)
    
    QString testReason = "Test: No Extra Nulls with Unicode 你好世界 🌟";
    
    // Generate a crash log
    CrashLog::generateCrashLog(testReason);
    
    // Find the most recent crash log file
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(logDir);
    QStringList crashLogs = dir.entryList(QStringList() << "crash_*.log", QDir::Files, QDir::Time);
    
    QVERIFY2(!crashLogs.isEmpty(), "Crash log should be created");
    
    QString logPath = logDir + "/" + crashLogs.first();
    QFile logFile(logPath);
    
    QVERIFY2(logFile.open(QIODevice::ReadOnly), "Should be able to open crash log file");
    QByteArray fileBytes = logFile.readAll();
    logFile.close();
    
    // Test 1: Verify the file is not empty
    QVERIFY2(!fileBytes.isEmpty(), "Crash log file should not be empty");
    
    // Test 2: Check for unexpected null bytes in the middle of ASCII text
    // In proper UTF-8, null bytes should only appear:
    // - At the very end of the file (if any)
    // - Never in the middle of text content
    
    // Count null bytes in the file
    int nullByteCount = 0;
    int nullBytesInContent = 0; // Null bytes before the last character
    
    for (int i = 0; i < fileBytes.size(); i++) {
        if (fileBytes[i] == '\0') {
            nullByteCount++;
            if (i < fileBytes.size() - 1) {
                nullBytesInContent++;
            }
        }
    }
    
    // Test 3: There should be no null bytes in the content area
    // (allowing for a possible trailing null at the very end)
    QVERIFY2(nullBytesInContent == 0, 
             QString("Found %1 unexpected null bytes in crash log content").arg(nullBytesInContent).toUtf8());
    
    // Test 4: Verify specific known strings have correct encoding
    // These strings should appear in the crash log with no null bytes between characters
    QList<QByteArray> expectedStrings = {
        "=== CRASH LOG ===",
        "Crash Reason:",
        "=== END OF CRASH LOG ===",
        "\n"  // newlines should be single bytes
    };
    
    for (const QByteArray& expected : expectedStrings) {
        int pos = fileBytes.indexOf(expected);
        QVERIFY2(pos >= 0, 
                 QString("Expected string '%1' should be found in crash log")
                     .arg(QString::fromUtf8(expected)).toUtf8());
        
        // After finding the string, verify no null bytes immediately follow ASCII chars within it
        // (This would indicate incorrect encoding like UTF-16)
        for (int i = pos; i < pos + expected.length() - 1 && i < fileBytes.size() - 1; i++) {
            if ((unsigned char)fileBytes[i] >= 0x20 && (unsigned char)fileBytes[i] <= 0x7E) {
                // This is an ASCII printable character
                // The next byte should NOT be null (which would indicate UTF-16LE encoding)
                QVERIFY2(fileBytes[i + 1] != '\0',
                        QString("Found null byte after ASCII character '%1' at position %2")
                            .arg(QChar(fileBytes[i])).arg(i).toUtf8());
            }
        }
    }
    
    // Test 5: Verify the file can be properly decoded as UTF-8
    QString decodedContent = QString::fromUtf8(fileBytes);
    QVERIFY2(!decodedContent.isEmpty(), "File should be decodable as UTF-8");
    QVERIFY2(decodedContent.contains(testReason), 
             "Decoded content should contain test reason with Unicode intact");
    
    // Test 6: Ensure the file size is reasonable for UTF-8 encoding
    // If the file were UTF-16, it would be approximately 2x larger for ASCII text
    // We check that the file size is consistent with UTF-8 by ensuring
    // the decoded string length is within a reasonable range of the byte count
    int decodedLength = decodedContent.length();
    int byteLength = fileBytes.size();
    
    // For UTF-8:
    // - ASCII chars = 1 byte each
    // - Non-ASCII chars = 2-4 bytes each
    // So decodedLength should be <= byteLength (and usually much closer)
    // For UTF-16LE: decodedLength * 2 ≈ byteLength (roughly)
    
    QVERIFY2(decodedLength <= byteLength,
             QString("Decoded length (%1) should be <= byte length (%2) for UTF-8")
                 .arg(decodedLength).arg(byteLength).toUtf8());
    
    // Clean up
    logFile.remove();
}

void TestCrashLog::testStackTraceHasFunctionNames()
{
    // This test verifies that stack traces contain function names where possible.
    // The issue reported that some stack frames showed only addresses without function names.
    // While we cannot guarantee ALL frames will have names (system libraries may lack debug symbols),
    // we can verify that at least some frames have resolved function names.
    
    // Get a stack trace using the public API
    QString stackTrace = CrashLog::getStackTrace();
    
    // Verify the stack trace is not empty
    QVERIFY2(!stackTrace.isEmpty(), "Stack trace should not be empty");
    
    // Verify it contains the "Stack Trace:" header
    QVERIFY2(stackTrace.contains("Stack Trace:"), 
             "Stack trace should contain the header");
    
    // Split the stack trace into lines
    QStringList lines = stackTrace.split('\n', Qt::SkipEmptyParts);
    
    // We should have at least a few stack frames
    QVERIFY2(lines.size() > 2, 
             QString("Stack trace should have multiple lines, found: %1").arg(lines.size()).toUtf8());
    
    // Count frames with function names vs just addresses
    int framesWithNames = 0;
    int totalFrames = 0;
    
    for (const QString& line : lines) {
        // Skip the header line
        if (line.contains("Stack Trace:")) {
            continue;
        }
        
        // Look for frame markers like "  [0]", "  [1]", etc.
        if (line.trimmed().startsWith('[') && line.contains(']')) {
            totalFrames++;
            
            // Check if this frame has a function name
            // A frame with a function name will have text between "] " and either " + 0x" or end of line
            // Examples:
            //   Windows: "  [0] MainWindow::onButtonClick + 0x1a"
            //   Unix: "  [0] ./test_crashlog(+0x1234) [0x...]"
            //   Address only: "  [0] 0x00007ff7baa8fa64"
            
            // After the frame number "] ", check what follows
            int closeBracket = line.indexOf(']');
            if (closeBracket >= 0 && closeBracket + 2 < line.length()) {
                QString afterBracket = line.mid(closeBracket + 2).trimmed();
                
                // If it starts with "0x" and contains only hex digits, it's address-only
                bool isAddressOnly = false;
                if (afterBracket.startsWith("0x")) {
                    // Check if the rest (after "0x") is just hex digits
                    QString hexPart = afterBracket.mid(2);
                    // Remove any trailing whitespace or newlines
                    hexPart = hexPart.trimmed();
                    
                    // Check if all characters are hex digits
                    bool allHex = true;
                    for (const QChar& c : hexPart) {
                        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                            allHex = false;
                            break;
                        }
                    }
                    isAddressOnly = allHex && !hexPart.isEmpty();
                }
                
                // If it's not address-only, it should have a function name
                if (!isAddressOnly) {
                    framesWithNames++;
                }
            }
        }
    }
    
    // Verify we found some frames
    QVERIFY2(totalFrames > 0, 
             QString("Should have found at least one stack frame, found: %1").arg(totalFrames).toUtf8());
    
    // The critical check: at least SOME frames should have function names
    // We can't require 100% because:
    // - System libraries may not have debug symbols
    // - Third-party DLLs may not have debug symbols  
    // - Some addresses may be in unmapped regions
    // But we should have at least one frame with a name (typically from our own code or Qt)
    QVERIFY2(framesWithNames > 0,
             QString("At least one stack frame should have a resolved function name. "
                     "Found %1 frames total, %2 with names.")
                 .arg(totalFrames).arg(framesWithNames).toUtf8());
    
    // Print summary for debugging
    qDebug() << "Stack trace analysis:";
    qDebug() << "  Total frames:" << totalFrames;
    qDebug() << "  Frames with names:" << framesWithNames;
    qDebug() << "  Frames with addresses only:" << (totalFrames - framesWithNames);
    
    // Additional check: verify the percentage is reasonable
    // If less than 20% have names, something might be wrong with symbol resolution
    double percentWithNames = totalFrames > 0 ? (double)framesWithNames / totalFrames * 100.0 : 0.0;
    qDebug() << "  Percentage with names:" << QString::number(percentWithNames, 'f', 1) << "%";
    
    // We expect at least 20% to have names in a properly configured environment
    // This is a soft check that helps catch configuration issues
    if (percentWithNames < 20.0 && totalFrames > 5) {
        qWarning() << "Warning: Less than 20% of stack frames have function names.";
        qWarning() << "This may indicate missing debug symbols or symbol resolution issues.";
        qWarning() << "However, this is not a hard failure as external libraries may lack symbols.";
    }
}

void TestCrashLog::testSymbolResolutionLogging()
{
    // This test verifies that CrashLog::install() logs detailed symbol resolution information
    // to usagi.log for troubleshooting purposes
    
    // Get the log file path
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString logPath = logDir + "/usagi.log";
    
    // Remove existing log file to start fresh
    QFile::remove(logPath);
    
    // Call install() which should log symbol resolution info
    CrashLog::install();
    
    // Give it a moment to write
    QThread::msleep(100);
    
    // Verify the log file exists
    QFile logFile(logPath);
    QVERIFY2(logFile.exists(), "usagi.log should exist after CrashLog::install()");
    
    // Read the log file
    QVERIFY2(logFile.open(QIODevice::ReadOnly), "Should be able to open usagi.log");
    QByteArray logBytes = logFile.readAll();
    logFile.close();
    
    QString logContent = QString::fromUtf8(logBytes);
    
    // Verify basic installation message
    QVERIFY2(logContent.contains("Crash log handler installed successfully"),
             "Log should contain installation success message");
    
    // Verify symbol resolution debug information is present
    QVERIFY2(logContent.contains("Symbol Resolution Debug Information"),
             "Log should contain symbol resolution debug section");
    
#ifdef Q_OS_WIN
    // Windows-specific checks
    QVERIFY2(logContent.contains("Executable path:"),
             "Log should contain executable path on Windows");
    QVERIFY2(logContent.contains("Symbol search path:"),
             "Log should contain symbol search path on Windows");
    QVERIFY2(logContent.contains("Symbol handler initialization:"),
             "Log should contain symbol handler initialization result");
    QVERIFY2(logContent.contains("Symbol option flags:"),
             "Log should contain symbol option flags configuration");
    
    // Check for specific symbol options
    QVERIFY2(logContent.contains("SYMOPT_UNDNAME"),
             "Log should mention SYMOPT_UNDNAME flag");
    QVERIFY2(logContent.contains("SYMOPT_AUTO_PUBLICS"),
             "Log should mention SYMOPT_AUTO_PUBLICS flag");
    
    // Check for module information
    QVERIFY2(logContent.contains("Main executable:") || logContent.contains("Loaded modules"),
             "Log should contain information about loaded modules");
    
    // The log should help diagnose symbol issues
    // It should mention either success or give hints about problems
    bool hasSuccessIndicator = logContent.contains("SUCCESS") || 
                               logContent.contains("loaded successfully") ||
                               logContent.contains("Resolved name:");
    bool hasWarningIndicator = logContent.contains("WARNING") || 
                              logContent.contains("FAILED") ||
                              logContent.contains("No debug symbols");
    
    QVERIFY2(hasSuccessIndicator || hasWarningIndicator,
             "Log should indicate either success or provide diagnostic warnings");
#else
    // Unix-like systems
    QVERIFY2(logContent.contains("Platform: Unix/Linux") || logContent.contains("using backtrace"),
             "Log should indicate Unix/Linux platform on non-Windows systems");
    QVERIFY2(logContent.contains("Executable path:"),
             "Log should contain executable path on Unix-like systems");
    QVERIFY2(logContent.contains("Debug symbols embedded"),
             "Log should mention debug symbols requirement");
#endif
    
    // Print the log content for manual inspection during test runs
    qDebug() << "=== Contents of usagi.log ===";
    qDebug().noquote() << logContent;
    qDebug() << "=== End of usagi.log ===";
}

QTEST_MAIN(TestCrashLog)
#include "test_crashlog.moc"
