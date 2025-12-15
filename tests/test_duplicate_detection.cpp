#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "../usagi/src/anidbapi.h"

class TestDuplicateDetection : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    void testGetDuplicateLocalFileIds();
    void testGetAllDuplicateHashes();
    void testNoDuplicatesFound();
    void testMultipleDuplicateGroups();

private:
    AniDBApi *api;
    QSqlDatabase db;
};

void TestDuplicateDetection::initTestCase()
{
    // Ensure clean slate: remove any existing default connection
    {
        QString defaultConn = QSqlDatabase::defaultConnection();
        if (QSqlDatabase::contains(defaultConn)) {
            QSqlDatabase existingDb = QSqlDatabase::database(defaultConn, false);
            if (existingDb.isOpen()) {
                existingDb.close();
            }
            QSqlDatabase::removeDatabase(defaultConn);
        }
    }
    
    // Set up test database
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());
}

void TestDuplicateDetection::cleanupTestCase()
{
    if (db.isOpen()) {
        db.close();
    }
    
    // Clear the QSqlDatabase object to release the connection reference
    db = QSqlDatabase();
    
    // Now safely remove the database connection
    QString defaultConn = QSqlDatabase::defaultConnection();
    if (QSqlDatabase::contains(defaultConn)) {
        QSqlDatabase::removeDatabase(defaultConn);
    }
}

void TestDuplicateDetection::init()
{
    // Create API instance (will create tables)
    api = new AniDBApi("test", 1);
    
    // Clean up any existing test data
    QSqlQuery query(db);
    query.exec("DELETE FROM local_files");
}

void TestDuplicateDetection::cleanup()
{
    delete api;
    api = nullptr;
}

void TestDuplicateDetection::testGetDuplicateLocalFileIds()
{
    // Create test files with same hash (simulating duplicates)
    QString hash = "abcdef1234567890abcdef1234567890";
    
    // Insert both files with same hash
    QSqlQuery query(db);
    query.prepare("INSERT INTO local_files (path, filename, ed2k_hash, file_size, status) VALUES (?, ?, ?, ?, 1)");
    
    query.addBindValue("/test/video_720p.mkv");
    query.addBindValue("video_720p.mkv");
    query.addBindValue(hash);
    query.addBindValue(1000000);
    QVERIFY(query.exec());
    
    query.addBindValue("/test/video_1080p.mkv");
    query.addBindValue("video_1080p.mkv");
    query.addBindValue(hash);
    query.addBindValue(2000000);
    QVERIFY(query.exec());
    
    // Get duplicate IDs
    QList<int> duplicates = api->getDuplicateLocalFileIds(hash);
    
    // Should return 2 IDs
    QCOMPARE(duplicates.size(), 2);
}

void TestDuplicateDetection::testGetAllDuplicateHashes()
{
    // Insert multiple groups of duplicates
    QString hash1 = "hash1111111111111111111111111111";
    QString hash2 = "hash2222222222222222222222222222";
    QString uniqueHash = "unique11111111111111111111111111";
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO local_files (path, filename, ed2k_hash, file_size, status) VALUES (?, ?, ?, ?, 1)");
    
    // Group 1: Two files with hash1
    query.addBindValue("/test/anime1_720p_v1.mkv");
    query.addBindValue("anime1_720p_v1.mkv");
    query.addBindValue(hash1);
    query.addBindValue(100000000);
    QVERIFY(query.exec());
    
    query.addBindValue("/test/anime1_1080p.mkv");
    query.addBindValue("anime1_1080p.mkv");
    query.addBindValue(hash1);
    query.addBindValue(200000000);
    QVERIFY(query.exec());
    
    // Group 2: Two files with hash2
    query.addBindValue("/test/anime2_dvd.mkv");
    query.addBindValue("anime2_dvd.mkv");
    query.addBindValue(hash2);
    query.addBindValue(150000000);
    QVERIFY(query.exec());
    
    query.addBindValue("/test/anime2_BluRay.mkv");
    query.addBindValue("anime2_BluRay.mkv");
    query.addBindValue(hash2);
    query.addBindValue(250000000);
    QVERIFY(query.exec());
    
    // Unique file (no duplicate)
    query.addBindValue("/test/unique_file.mkv");
    query.addBindValue("unique_file.mkv");
    query.addBindValue(uniqueHash);
    query.addBindValue(300000000);
    QVERIFY(query.exec());
    
    // Get all duplicate hashes
    QStringList duplicateHashes = api->getAllDuplicateHashes();
    
    // Should return 2 hashes (hash1 and hash2, but not uniqueHash)
    QCOMPARE(duplicateHashes.size(), 2);
    QVERIFY(duplicateHashes.contains(hash1));
    QVERIFY(duplicateHashes.contains(hash2));
    QVERIFY(!duplicateHashes.contains(uniqueHash));
}

void TestDuplicateDetection::testNoDuplicatesFound()
{
    // Insert files with different hashes
    QSqlQuery query(db);
    query.prepare("INSERT INTO local_files (path, filename, ed2k_hash, file_size, status) VALUES (?, ?, ?, ?, 1)");
    
    query.addBindValue("/test/file1.mkv");
    query.addBindValue("file1.mkv");
    query.addBindValue("hash1111111111111111111111111111");
    query.addBindValue(100000000);
    QVERIFY(query.exec());
    
    query.addBindValue("/test/file2.mkv");
    query.addBindValue("file2.mkv");
    query.addBindValue("hash2222222222222222222222222222");
    query.addBindValue(200000000);
    QVERIFY(query.exec());
    
    // Get all duplicate hashes (should be empty)
    QStringList duplicateHashes = api->getAllDuplicateHashes();
    QVERIFY(duplicateHashes.isEmpty());
}

void TestDuplicateDetection::testMultipleDuplicateGroups()
{
    // Insert multiple groups of duplicates with varying sizes
    QString hash1 = "hash1111111111111111111111111111";
    QString hash2 = "hash2222222222222222222222222222";
    QString hash3 = "hash3333333333333333333333333333";
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO local_files (path, filename, ed2k_hash, file_size, status) VALUES (?, ?, ?, ?, 1)");
    
    // Group 1: 2 duplicates
    query.addBindValue("/test/group1_file1.mkv");
    query.addBindValue("group1_file1.mkv");
    query.addBindValue(hash1);
    query.addBindValue(100000000);
    QVERIFY(query.exec());
    
    query.addBindValue("/test/group1_file2.mkv");
    query.addBindValue("group1_file2.mkv");
    query.addBindValue(hash1);
    query.addBindValue(200000000);
    QVERIFY(query.exec());
    
    // Group 2: 3 duplicates
    query.addBindValue("/test/group2_file1.mkv");
    query.addBindValue("group2_file1.mkv");
    query.addBindValue(hash2);
    query.addBindValue(150000000);
    QVERIFY(query.exec());
    
    query.addBindValue("/test/group2_file2.mkv");
    query.addBindValue("group2_file2.mkv");
    query.addBindValue(hash2);
    query.addBindValue(250000000);
    QVERIFY(query.exec());
    
    query.addBindValue("/test/group2_file3.mkv");
    query.addBindValue("group2_file3.mkv");
    query.addBindValue(hash2);
    query.addBindValue(350000000);
    QVERIFY(query.exec());
    
    // Group 3: 2 duplicates
    query.addBindValue("/test/group3_file1.mkv");
    query.addBindValue("group3_file1.mkv");
    query.addBindValue(hash3);
    query.addBindValue(400000000);
    QVERIFY(query.exec());
    
    query.addBindValue("/test/group3_file2.mkv");
    query.addBindValue("group3_file2.mkv");
    query.addBindValue(hash3);
    query.addBindValue(500000000);
    QVERIFY(query.exec());
    
    // Get all duplicate hashes
    QStringList duplicateHashes = api->getAllDuplicateHashes();
    QCOMPARE(duplicateHashes.size(), 3);
    
    // Verify each group
    QList<int> group1 = api->getDuplicateLocalFileIds(hash1);
    QCOMPARE(group1.size(), 2);
    
    QList<int> group2 = api->getDuplicateLocalFileIds(hash2);
    QCOMPARE(group2.size(), 3);
    
    QList<int> group3 = api->getDuplicateLocalFileIds(hash3);
    QCOMPARE(group3.size(), 2);
}

QTEST_MAIN(TestDuplicateDetection)
#include "test_duplicate_detection.moc"
