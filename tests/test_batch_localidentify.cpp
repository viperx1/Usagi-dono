#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "../usagi/src/anidbapi.h"

class TestBatchLocalIdentify : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testBatchLocalIdentify();
    void testBatchLocalIdentifyEmpty();
    void testBatchLocalIdentifyMixed();
    
private:
    AniDBApi *api;
};

void TestBatchLocalIdentify::initTestCase()
{
    api = new AniDBApi("test", 1);
    
    // Set up test data in database
    QSqlDatabase db = QSqlDatabase::database();
    QVERIFY(db.isValid());
    QVERIFY(db.isOpen());
    
    // Create test data in file and mylist tables
    QSqlQuery query(db);
    
    // Insert test files
    query.exec("INSERT OR REPLACE INTO file (fid, size, ed2k) VALUES (1, 100000, 'hash1')");
    query.exec("INSERT OR REPLACE INTO file (fid, size, ed2k) VALUES (2, 200000, 'hash2')");
    query.exec("INSERT OR REPLACE INTO file (fid, size, ed2k) VALUES (3, 300000, 'hash3')");
    
    // Insert test mylist entries (only for fid 1 and 3)
    query.exec("INSERT OR REPLACE INTO mylist (lid, fid) VALUES (1, 1)");
    query.exec("INSERT OR REPLACE INTO mylist (lid, fid) VALUES (2, 3)");
}

void TestBatchLocalIdentify::cleanupTestCase()
{
    delete api;
}

void TestBatchLocalIdentify::testBatchLocalIdentify()
{
    // Test batch LocalIdentify with multiple files
    QList<QPair<qint64, QString>> sizeHashPairs;
    sizeHashPairs.append(qMakePair(100000LL, QString("hash1")));
    sizeHashPairs.append(qMakePair(200000LL, QString("hash2")));
    sizeHashPairs.append(qMakePair(300000LL, QString("hash3")));
    
    QMap<QString, std::bitset<2>> results = api->batchLocalIdentify(sizeHashPairs);
    
    // Verify we got results for all files
    QCOMPARE(results.size(), 3);
    
    // Check file 1 (in both file and mylist)
    QString key1 = "100000:hash1";
    QVERIFY(results.contains(key1));
    QVERIFY(results[key1][AniDBApi::LI_FILE_IN_DB] == 1);
    QVERIFY(results[key1][AniDBApi::LI_FILE_IN_MYLIST] == 1);
    
    // Check file 2 (in file only, not in mylist)
    QString key2 = "200000:hash2";
    QVERIFY(results.contains(key2));
    QVERIFY(results[key2][AniDBApi::LI_FILE_IN_DB] == 1);
    QVERIFY(results[key2][AniDBApi::LI_FILE_IN_MYLIST] == 0);
    
    // Check file 3 (in both file and mylist)
    QString key3 = "300000:hash3";
    QVERIFY(results.contains(key3));
    QVERIFY(results[key3][AniDBApi::LI_FILE_IN_DB] == 1);
    QVERIFY(results[key3][AniDBApi::LI_FILE_IN_MYLIST] == 1);
}

void TestBatchLocalIdentify::testBatchLocalIdentifyEmpty()
{
    // Test with empty list
    QList<QPair<qint64, QString>> sizeHashPairs;
    
    QMap<QString, std::bitset<2>> results = api->batchLocalIdentify(sizeHashPairs);
    
    // Should return empty map
    QVERIFY(results.isEmpty());
}

void TestBatchLocalIdentify::testBatchLocalIdentifyMixed()
{
    // Test with mix of existing and non-existing files
    QList<QPair<qint64, QString>> sizeHashPairs;
    sizeHashPairs.append(qMakePair(100000LL, QString("hash1")));       // exists
    sizeHashPairs.append(qMakePair(999999LL, QString("nonexistent"))); // doesn't exist
    sizeHashPairs.append(qMakePair(200000LL, QString("hash2")));       // exists
    
    QMap<QString, std::bitset<2>> results = api->batchLocalIdentify(sizeHashPairs);
    
    // Verify we got results for all files (including non-existent)
    QCOMPARE(results.size(), 3);
    
    // Check existing file
    QString key1 = "100000:hash1";
    QVERIFY(results.contains(key1));
    QVERIFY(results[key1][AniDBApi::LI_FILE_IN_DB] == 1);
    
    // Check non-existent file (should have both bits = 0)
    QString keyNonExist = "999999:nonexistent";
    QVERIFY(results.contains(keyNonExist));
    QVERIFY(results[keyNonExist][AniDBApi::LI_FILE_IN_DB] == 0);
    QVERIFY(results[keyNonExist][AniDBApi::LI_FILE_IN_MYLIST] == 0);
    
    // Check another existing file
    QString key2 = "200000:hash2";
    QVERIFY(results.contains(key2));
    QVERIFY(results[key2][AniDBApi::LI_FILE_IN_DB] == 1);
    QVERIFY(results[key2][AniDBApi::LI_FILE_IN_MYLIST] == 0);
}

QTEST_MAIN(TestBatchLocalIdentify)
#include "test_batch_localidentify.moc"
