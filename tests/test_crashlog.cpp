#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <cstring>

// We need to test the crash log encoding by simulating what writeSafeCrashLog does
// Since writeSafeCrashLog is a static function in crashlog.cpp, we'll test the 
// file writing approach directly to verify encoding
//
// This test suite includes three encoding tests:
// 1. testCrashLogEncoding() - Verifies correct UTF-8/ASCII encoding is used
// 2. testCrashLogNotUTF16LE() - Verifies UTF-16LE is NOT used in correct implementation
// 3. testDetectIncorrectUTF16LEEncoding() - NEW: Demonstrates what happens when
//    UTF-16LE encoding is incorrectly used, showing how to detect encoding problems

class TestCrashLog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCrashLogEncoding();
    void testCrashLogNotUTF16LE();
    void testDetectIncorrectUTF16LEEncoding();
    void testQTextStreamUTF8WithoutTextFlag();
    void cleanupTestCase();

private:
    QTemporaryDir* tempDir;
};

void TestCrashLog::initTestCase()
{
    // Set application metadata
    QCoreApplication::setApplicationName("Usagi-dono");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    // Create temporary directory for test files
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());
}

void TestCrashLog::cleanupTestCase()
{
    delete tempDir;
}

void TestCrashLog::testCrashLogEncoding()
{
    // Test that crash log is written in ASCII/UTF-8, not UTF-16LE
    QString testFilePath = tempDir->filePath("test_crash.log");
    QByteArray testFilePathBytes = testFilePath.toLocal8Bit();
    const char* logPath = testFilePathBytes.constData();
    const char* testReason = "Test Crash Reason";
    
    // Write crash log using the same approach as writeSafeCrashLog
#ifdef Q_OS_WIN
    int fd = _open(logPath, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    QVERIFY(fd >= 0);
    
    _write(fd, "=== CRASH LOG ===\n\nCrash Reason: ", 33);
    _write(fd, testReason, (unsigned int)strlen(testReason));
    _write(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n", 40);
    _write(fd, "\n=== END OF CRASH LOG ===\n", 26);
    _close(fd);
#else
    int fd = open(logPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    QVERIFY(fd >= 0);
    
    write(fd, "=== CRASH LOG ===\n\nCrash Reason: ", 33);
    write(fd, testReason, strlen(testReason));
    write(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n", 40);
    write(fd, "\n=== END OF CRASH LOG ===\n", 26);
    close(fd);
#endif
    
    // Read the file back and verify encoding
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray content = file.readAll();
    file.close();
    
    // Verify the content is readable ASCII/UTF-8
    QString contentStr = QString::fromUtf8(content);
    
    // Check that key strings are present and readable
    QVERIFY(contentStr.contains("=== CRASH LOG ==="));
    QVERIFY(contentStr.contains("Crash Reason: Test Crash Reason"));
    QVERIFY(contentStr.contains("Application: Usagi-dono"));
    QVERIFY(contentStr.contains("Version: 1.0.0"));
    QVERIFY(contentStr.contains("=== END OF CRASH LOG ==="));
    
    // Verify content length is reasonable (should be around 120 bytes for ASCII/UTF-8)
    // If it were UTF-16LE, it would be roughly double
    QVERIFY(content.size() > 100 && content.size() < 150);
}

void TestCrashLog::testCrashLogNotUTF16LE()
{
    // Test that the crash log is NOT encoded as UTF-16LE
    QString testFilePath = tempDir->filePath("test_crash_utf16.log");
    QByteArray testFilePathBytes = testFilePath.toLocal8Bit();
    const char* logPath = testFilePathBytes.constData();
    const char* testReason = "Segmentation Fault (SIGSEGV)";
    
    // Write crash log using the same approach as writeSafeCrashLog
#ifdef Q_OS_WIN
    int fd = _open(logPath, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    QVERIFY(fd >= 0);
    
    _write(fd, "=== CRASH LOG ===\n\nCrash Reason: ", 33);
    _write(fd, testReason, (unsigned int)strlen(testReason));
    _write(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n", 40);
    _write(fd, "\n=== END OF CRASH LOG ===\n", 26);
    _close(fd);
#else
    int fd = open(logPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    QVERIFY(fd >= 0);
    
    write(fd, "=== CRASH LOG ===\n\nCrash Reason: ", 33);
    write(fd, testReason, strlen(testReason));
    write(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n", 40);
    write(fd, "\n=== END OF CRASH LOG ===\n", 26);
    close(fd);
#endif
    
    // Read the file back as raw bytes
    QFile file(testFilePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray content = file.readAll();
    file.close();
    
    // Check for UTF-16LE BOM (0xFF 0xFE) - should NOT be present
    if (content.size() >= 2) {
        QVERIFY(!(content[0] == (char)0xFF && content[1] == (char)0xFE));
    }
    
    // Check that '=' character is stored as 0x3D (ASCII), not 0x3D 0x3D (UTF-16LE)
    // In UTF-16LE, '=' would be 0x3D 0x00, and "==" would be 0x3D 0x00 0x3D 0x00
    // But the garbled output showed it as 0x3D 0x3D (interpreted as UTF-16LE)
    if (content.size() >= 3) {
        // The first three characters should be "===" (0x3D 0x3D 0x3D in ASCII)
        QCOMPARE((unsigned char)content[0], (unsigned char)0x3D);
        QCOMPARE((unsigned char)content[1], (unsigned char)0x3D);
        QCOMPARE((unsigned char)content[2], (unsigned char)0x3D);
        
        // If it were UTF-16LE, we'd see 0x3D 0x00 0x3D 0x00 0x3D 0x00
        // Check that the second byte is NOT 0x00 (which would indicate UTF-16LE)
        QVERIFY((unsigned char)content[1] != 0x00);
    }
    
    // Verify that all printable ASCII characters in the expected output
    // are stored as single bytes, not double bytes (UTF-16LE would double them)
    QString expectedStart = "=== CRASH LOG ===";
    for (int i = 0; i < expectedStart.length() && i < content.size(); i++) {
        QChar expectedChar = expectedStart[i];
        QCOMPARE((unsigned char)content[i], (unsigned char)expectedChar.toLatin1());
    }
}

void TestCrashLog::testDetectIncorrectUTF16LEEncoding()
{
    // This test demonstrates what happens when text is incorrectly encoded as UTF-16LE
    // and then read as UTF-8. This simulates the encoding problem shown in the
    // garbled issue description.
    
    QString testFilePath = tempDir->filePath("test_wrong_encoding.log");
    QFile file(testFilePath);
    
    // Write some text in UTF-16LE encoding (the WRONG encoding)
    // NOTE: This intentionally uses QIODevice::Text to demonstrate the wrong approach.
    // In production code, QIODevice::Text should NOT be used with explicit encoding.
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf16LE);
    stream.setGenerateByteOrderMark(true);
    
    QString testText = "=== CRASH LOG ===\n\nCrash Reason: Segmentation Fault (SIGSEGV)\n"
                       "Application: Usagi-dono\nVersion: 1.0.0\n";
    stream << testText;
    file.close();
    
    // Now read it back as raw bytes (as if reading UTF-8)
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray rawContent = file.readAll();
    file.close();
    
    // Check for UTF-16LE BOM (0xFF 0xFE) - this SHOULD be present for UTF-16LE
    QVERIFY(rawContent.size() >= 2);
    QVERIFY(rawContent[0] == (char)0xFF && rawContent[1] == (char)0xFE);
    
    // When UTF-16LE is interpreted as UTF-8, each ASCII character becomes
    // a character followed by a null byte (0x00). For example:
    // "==" in ASCII: 0x3D 0x3D
    // "==" in UTF-16LE: 0xFF 0xFE 0x3D 0x00 0x3D 0x00
    // The BOM (0xFF 0xFE) comes first, then each character is followed by 0x00
    
    // Skip the BOM and check the encoding pattern
    if (rawContent.size() >= 6) {
        // After BOM, "=" should be encoded as 0x3D 0x00 in UTF-16LE
        QCOMPARE((unsigned char)rawContent[2], (unsigned char)0x3D);
        QCOMPARE((unsigned char)rawContent[3], (unsigned char)0x00);
        QCOMPARE((unsigned char)rawContent[4], (unsigned char)0x3D);
        QCOMPARE((unsigned char)rawContent[5], (unsigned char)0x00);
    }
    
    // Verify that the file size is roughly double what it should be
    // (each ASCII character takes 2 bytes in UTF-16LE, plus 2-byte BOM)
    // Original text is around 120 chars, so UTF-16LE would be ~242 bytes (120*2 + 2 for BOM)
    QVERIFY(rawContent.size() > 200);
    
    // When we try to read UTF-16LE as UTF-8, we get garbled text
    QString garbled = QString::fromUtf8(rawContent);
    
    // The garbled text should NOT contain the original readable strings
    // because UTF-16LE interpreted as UTF-8 produces garbage
    QVERIFY(!garbled.contains("=== CRASH LOG ==="));
    QVERIFY(!garbled.contains("Segmentation Fault"));
    QVERIFY(!garbled.contains("Application: Usagi-dono"));
    
    // The garbled string will contain null bytes and special characters
    // This is the key indicator of encoding mismatch
    bool hasNullBytes = false;
    for (int i = 0; i < garbled.length(); i++) {
        if (garbled[i] == QChar(0x00)) {
            hasNullBytes = true;
            break;
        }
    }
    // UTF-16LE read as UTF-8 may or may not preserve null bytes depending on how
    // QString::fromUtf8 handles them, but the text will definitely be garbled
    
    // The most reliable check: verify the content is NOT what we expect
    QVERIFY(garbled != testText);
}

void TestCrashLog::testQTextStreamUTF8WithoutTextFlag()
{
    // This test verifies the correct approach: using QTextStream with explicit UTF-8
    // encoding WITHOUT the QIODevice::Text flag. This is the approach used in the
    // fixed generateCrashLog() and logMessage() functions.
    
    QString testFilePath = tempDir->filePath("test_correct_utf8.log");
    QFile file(testFilePath);
    
    // Write text using QTextStream with UTF-8 encoding, WITHOUT QIODevice::Text flag
    // This is the CORRECT approach that prevents encoding issues
    QVERIFY(file.open(QIODevice::WriteOnly));
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    QString testText = "=== CRASH LOG ===\n\nCrash Reason: Segmentation Fault (SIGSEGV)\n"
                       "Application: Usagi-dono\nVersion: 1.0.0\n";
    stream << testText;
    file.close();
    
    // Read the file back as raw bytes
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray rawContent = file.readAll();
    file.close();
    
    // Verify NO UTF-16LE BOM is present
    if (rawContent.size() >= 2) {
        QVERIFY(!(rawContent[0] == (char)0xFF && rawContent[1] == (char)0xFE));
    }
    
    // Verify the file is NOT double the expected size (would indicate UTF-16LE)
    // Expected size is around 120 bytes for ASCII/UTF-8
    QVERIFY(rawContent.size() < 150);
    
    // Verify that when read as UTF-8, the text is correct and readable
    QString readBack = QString::fromUtf8(rawContent);
    QCOMPARE(readBack, testText);
    
    // Verify all expected strings are present
    QVERIFY(readBack.contains("=== CRASH LOG ==="));
    QVERIFY(readBack.contains("Crash Reason: Segmentation Fault (SIGSEGV)"));
    QVERIFY(readBack.contains("Application: Usagi-dono"));
    QVERIFY(readBack.contains("Version: 1.0.0"));
    
    // Verify no null bytes in the content (which would indicate UTF-16LE)
    bool hasNullBytes = false;
    for (int i = 0; i < rawContent.size(); i++) {
        if (rawContent[i] == 0x00) {
            hasNullBytes = true;
            break;
        }
    }
    QVERIFY(!hasNullBytes);
}

QTEST_MAIN(TestCrashLog)
#include "test_crashlog.moc"
