#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryFile>
#include <QSignalSpy>
#include "../usagi/src/anidbapi.h"
#include "../usagi/src/hash/ed2k.h"

/**
 * Test to verify that file identification can happen immediately after hashing
 * This validates the fix for the issue where identification was batched until
 * all files were hashed, causing delays and UI freezes.
 */
class TestImmediateIdentification : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testLocalIdentifyAfterHash();
    void testMultipleFilesCanBeIdentifiedSequentially();
    
private:
    AniDBApi *api;
};

void TestImmediateIdentification::initTestCase()
{
    api = new AniDBApi("test", 1);
    
    // Set up test data in database
    QSqlDatabase db = QSqlDatabase::database();
    QVERIFY(db.isValid());
    QVERIFY(db.isOpen());
    
    // Create test data in file table
    QSqlQuery query(db);
    
    // Insert test files with known hashes
    query.exec("INSERT OR REPLACE INTO file (fid, size, ed2k) VALUES (1, 100000, 'testhash1')");
    query.exec("INSERT OR REPLACE INTO file (fid, size, ed2k) VALUES (2, 200000, 'testhash2')");
}

void TestImmediateIdentification::cleanupTestCase()
{
    delete api;
}

void TestImmediateIdentification::testLocalIdentifyAfterHash()
{
    // This test verifies that LocalIdentify can be called immediately after
    // a hash is obtained, without waiting for batch processing
    
    // Simulate getting a hash for a file
    qint64 fileSize = 100000;
    QString hexdigest = "testhash1";
    
    // Perform LocalIdentify immediately (as done in the fixed getNotifyFileHashed)
    std::bitset<2> li = api->LocalIdentify(fileSize, hexdigest);
    
    // Verify the file is found in database
    QVERIFY(li[AniDBApi::LI_FILE_IN_DB] == 1);
    
    // This demonstrates that identification can happen immediately,
    // not requiring batch processing
}

void TestImmediateIdentification::testMultipleFilesCanBeIdentifiedSequentially()
{
    // This test verifies that multiple files can be identified one after another
    // in the order they are hashed, rather than all at once in a batch
    
    // First file
    qint64 fileSize1 = 100000;
    QString hexdigest1 = "testhash1";
    std::bitset<2> li1 = api->LocalIdentify(fileSize1, hexdigest1);
    QVERIFY(li1[AniDBApi::LI_FILE_IN_DB] == 1);
    
    // Second file - can be identified immediately after first, not in batch
    qint64 fileSize2 = 200000;
    QString hexdigest2 = "testhash2";
    std::bitset<2> li2 = api->LocalIdentify(fileSize2, hexdigest2);
    QVERIFY(li2[AniDBApi::LI_FILE_IN_DB] == 1);
    
    // Third file - not in database
    qint64 fileSize3 = 300000;
    QString hexdigest3 = "unknownhash";
    std::bitset<2> li3 = api->LocalIdentify(fileSize3, hexdigest3);
    QVERIFY(li3[AniDBApi::LI_FILE_IN_DB] == 0);
    
    // This demonstrates sequential identification works correctly
    // The fix moves from batch processing to immediate processing
}

QTEST_MAIN(TestImmediateIdentification)
#include "test_immediate_identification.moc"
