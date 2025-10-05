#include <QtTest/QtTest>
#include "../usagi/src/hash/ed2k.h"
#include <string>
#include <cstring>
#include <QFile>
#include <QTemporaryFile>

class TestHashFunctions : public QObject
{
    Q_OBJECT

private slots:
    // MD4 tests
    void testMD4EmptyFile();
    void testMD4FileHashing();
    
    // ED2K tests
    void testED2KInitialization();
    void testED2KBasicHashing();
    void testED2KFileHashing();
};

// ===== MD4 Tests =====

void TestHashFunctions::testMD4EmptyFile()
{
    // Create a temporary file
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.close();
    
    // Test that we can hash an empty file without crashing
    MD4 md4;
    QByteArray filename = tempFile.fileName().toLatin1();
    md4.File(filename.data());
    
    // If we get here, the test passed (no crash)
    QVERIFY(true);
}

void TestHashFunctions::testMD4FileHashing()
{
    // Create a temporary file with known content
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("test content");
    tempFile.close();
    
    // Test that we can hash a file without crashing
    MD4 md4;
    QByteArray filename = tempFile.fileName().toLatin1();
    md4.File(filename.data());
    
    // If we get here, the test passed (no crash)
    QVERIFY(true);
}

// ===== ED2K Tests =====

void TestHashFunctions::testED2KInitialization()
{
    ed2k hasher;
    hasher.Init();
    
    // After initialization, the hash should be ready
    // Just verify no crash occurs
    QVERIFY(true);
}

void TestHashFunctions::testED2KBasicHashing()
{
    ed2k hasher;
    hasher.Init();
    
    // Test with simple data
    unsigned char testData[] = "test data";
    hasher.Update(testData, strlen((char*)testData));
    hasher.Final();
    
    std::string result = hasher.HexDigest();
    
    // Verify we got a valid hex string (32 characters for MD4)
    QCOMPARE(result.length(), (size_t)32);
    
    // Verify it contains only hex characters
    for (char c : result) {
        QVERIFY((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

void TestHashFunctions::testED2KFileHashing()
{
    // Create a temporary file with test content
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray testData = "This is a test file for ED2K hashing";
    tempFile.write(testData);
    tempFile.close();
    
    // Test ED2K file hashing
    ed2k hasher;
    QString filename = tempFile.fileName();
    int result = hasher.ed2khash(filename);
    
    // Verify the hash was computed (returns 1 on success)
    QCOMPARE(result, 1);
    
    // Verify the hash string is valid (ed2k link format)
    QVERIFY(!hasher.ed2khashstr.isEmpty());
    QVERIFY(hasher.ed2khashstr.startsWith("ed2k://|file|"));
}

QTEST_MAIN(TestHashFunctions)
#include "test_hash.moc"

