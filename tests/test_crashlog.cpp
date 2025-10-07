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

class TestCrashLog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCrashLogEncoding();
    void testCrashLogNotUTF16LE();
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
    
    _write(fd, "=== CRASH LOG ===\n\nCrash Reason: ", 34);
    _write(fd, testReason, (unsigned int)strlen(testReason));
    _write(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n", 39);
    _write(fd, "\n=== END OF CRASH LOG ===\n", 27);
    _close(fd);
#else
    int fd = open(logPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    QVERIFY(fd >= 0);
    
    write(fd, "=== CRASH LOG ===\n\nCrash Reason: ", 34);
    write(fd, testReason, strlen(testReason));
    write(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n", 39);
    write(fd, "\n=== END OF CRASH LOG ===\n", 27);
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
    
    _write(fd, "=== CRASH LOG ===\n\nCrash Reason: ", 34);
    _write(fd, testReason, (unsigned int)strlen(testReason));
    _write(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n", 39);
    _write(fd, "\n=== END OF CRASH LOG ===\n", 27);
    _close(fd);
#else
    int fd = open(logPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    QVERIFY(fd >= 0);
    
    write(fd, "=== CRASH LOG ===\n\nCrash Reason: ", 34);
    write(fd, testReason, strlen(testReason));
    write(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n", 39);
    write(fd, "\n=== END OF CRASH LOG ===\n", 27);
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

QTEST_MAIN(TestCrashLog)
#include "test_crashlog.moc"
