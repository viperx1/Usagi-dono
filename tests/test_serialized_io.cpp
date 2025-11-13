#include <QtTest/QtTest>
#include "../usagi/src/hash/ed2k.h"
#include <QTemporaryFile>
#include <QThread>
#include <QThreadPool>
#include <QElapsedTimer>
#include <QVector>

/**
 * Test to verify that serialized I/O can be enabled/disabled
 * and that it affects file hashing behavior appropriately.
 */
class TestSerializedIO : public QObject
{
    Q_OBJECT

private slots:
    void testSerializedIOFlag();
    void testSerializedIODisabled();
    void testSerializedIOEnabled();
    void testMultipleFilesSerializedIO();
};

void TestSerializedIO::testSerializedIOFlag()
{
    // Test that we can get and set the serialized I/O flag
    
    // Default should be false (disabled)
    QCOMPARE(ed2k::getSerializedIO(), false);
    
    // Enable it
    ed2k::setSerializedIO(true);
    QCOMPARE(ed2k::getSerializedIO(), true);
    
    // Disable it
    ed2k::setSerializedIO(false);
    QCOMPARE(ed2k::getSerializedIO(), false);
}

void TestSerializedIO::testSerializedIODisabled()
{
    // Create a temporary file with test content
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray testData(1024 * 100, 'A'); // 100KB
    tempFile.write(testData);
    tempFile.close();
    
    // Ensure serialized I/O is disabled
    ed2k::setSerializedIO(false);
    
    // Test hashing with serialized I/O disabled
    ed2k hasher;
    QString filename = tempFile.fileName();
    int result = hasher.ed2khash(filename);
    
    // Verify the hash was computed successfully
    QCOMPARE(result, 1);
    
    // Verify we got a valid hash
    QVERIFY(!hasher.ed2khashstr.isEmpty());
    QVERIFY(hasher.ed2khashstr.startsWith("ed2k://|file|"));
}

void TestSerializedIO::testSerializedIOEnabled()
{
    // Create a temporary file with test content
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray testData(1024 * 100, 'B'); // 100KB
    tempFile.write(testData);
    tempFile.close();
    
    // Enable serialized I/O
    ed2k::setSerializedIO(true);
    
    // Test hashing with serialized I/O enabled
    ed2k hasher;
    QString filename = tempFile.fileName();
    int result = hasher.ed2khash(filename);
    
    // Verify the hash was computed successfully
    QCOMPARE(result, 1);
    
    // Verify we got a valid hash
    QVERIFY(!hasher.ed2khashstr.isEmpty());
    QVERIFY(hasher.ed2khashstr.startsWith("ed2k://|file|"));
    
    // Clean up: disable serialized I/O for other tests
    ed2k::setSerializedIO(false);
}

void TestSerializedIO::testMultipleFilesSerializedIO()
{
    // Create multiple temporary files
    QVector<QTemporaryFile*> tempFiles;
    QVector<QString> filePaths;
    QVector<QString> expectedHashes;
    
    for (int i = 0; i < 3; ++i)
    {
        QTemporaryFile *tempFile = new QTemporaryFile();
        QVERIFY(tempFile->open());
        QByteArray data(50 * 1024, 'C' + i); // 50KB, different content
        tempFile->write(data);
        tempFile->close();
        filePaths.append(tempFile->fileName());
        tempFiles.append(tempFile);
    }
    
    // Test with serialized I/O disabled (parallel I/O)
    ed2k::setSerializedIO(false);
    
    for (const QString &filePath : filePaths)
    {
        ed2k hasher;
        int result = hasher.ed2khash(filePath);
        QCOMPARE(result, 1);
        expectedHashes.append(hasher.ed2khashstr);
    }
    
    // Test with serialized I/O enabled (sequential I/O)
    ed2k::setSerializedIO(true);
    
    for (int i = 0; i < filePaths.size(); ++i)
    {
        ed2k hasher;
        int result = hasher.ed2khash(filePaths[i]);
        QCOMPARE(result, 1);
        
        // Verify that hash is the same regardless of serialized I/O setting
        // (the algorithm should produce identical results)
        QCOMPARE(hasher.ed2khashstr, expectedHashes[i]);
    }
    
    // Clean up
    ed2k::setSerializedIO(false);
    for (QTemporaryFile *tempFile : tempFiles)
    {
        delete tempFile;
    }
}

QTEST_MAIN(TestSerializedIO)
#include "test_serialized_io.moc"
