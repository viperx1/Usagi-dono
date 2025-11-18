#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "../usagi/src/anidbapi.h"

class TestHashStorage : public QObject
{
    Q_OBJECT

private slots:
    void testUpdateLocalFileHash();
    void testStatusProgression();
};

void TestHashStorage::testUpdateLocalFileHash()
{
    // Create AniDBApi instance (it will set up its own database)
    AniDBApi api("test", 1);
    
    // Use the default database connection that AniDBApi created
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    
    // Insert a test file with status 0 (not hashed)
    QVERIFY(query.exec("INSERT OR REPLACE INTO local_files (path, filename, status) VALUES ('/test/video.mkv', 'video.mkv', 0)"));
    
    // Update the file with hash and status 1 (hashed but not checked by API)
    api.updateLocalFileHash("/test/video.mkv", "abc123def456", 1);
    
    // Verify the update
    QSqlQuery verifyQuery(db);
    QVERIFY(verifyQuery.exec("SELECT ed2k_hash, status FROM local_files WHERE path = '/test/video.mkv'"));
    QVERIFY(verifyQuery.next());
    
    QString hash = verifyQuery.value(0).toString();
    int status = verifyQuery.value(1).toInt();
    
    QCOMPARE(hash, QString("abc123def456"));
    QCOMPARE(status, 1);
}

void TestHashStorage::testStatusProgression()
{
    // Create AniDBApi instance (it will set up its own database)
    AniDBApi api("test", 1);
    
    // Use the default database connection that AniDBApi created
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    
    // Insert a test file
    QVERIFY(query.exec("INSERT OR REPLACE INTO local_files (path, filename, status) VALUES ('/test/video2.mkv', 'video2.mkv', 0)"));
    
    // Progress 1: File is hashed (status 0 -> 1)
    api.updateLocalFileHash("/test/video2.mkv", "test_hash_123", 1);
    
    QSqlQuery verifyQuery1(db);
    QVERIFY(verifyQuery1.exec("SELECT status FROM local_files WHERE path = '/test/video2.mkv'"));
    QVERIFY(verifyQuery1.next());
    QCOMPARE(verifyQuery1.value(0).toInt(), 1);
    
    // Progress 2: File is checked by API and found in anidb (status 1 -> 2)
    api.UpdateLocalFileStatus("/test/video2.mkv", 2);
    
    QSqlQuery verifyQuery2(db);
    QVERIFY(verifyQuery2.exec("SELECT status FROM local_files WHERE path = '/test/video2.mkv'"));
    QVERIFY(verifyQuery2.next());
    QCOMPARE(verifyQuery2.value(0).toInt(), 2);
    
    // Progress 3: Verify hash is preserved
    QSqlQuery verifyQuery3(db);
    QVERIFY(verifyQuery3.exec("SELECT ed2k_hash FROM local_files WHERE path = '/test/video2.mkv'"));
    QVERIFY(verifyQuery3.next());
    QCOMPARE(verifyQuery3.value(0).toString(), QString("test_hash_123"));
}

QTEST_MAIN(TestHashStorage)
#include "test_hash_storage.moc"
