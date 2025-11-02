#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include "../usagi/src/anidbapi.h"

class TestHashDuplicateReuse : public QObject
{
    Q_OBJECT

private slots:
    void testDuplicateFileHashReuse();
    void testDuplicateFileWithDifferentSizeNoReuse();
    void testNoHashAvailableForDuplicate();
};

void TestHashDuplicateReuse::testDuplicateFileHashReuse()
{
    // Create AniDBApi instance
    AniDBApi api("test", 1);
    
    // Clean up stale entries from previous test runs
    {
        QSqlDatabase db = QSqlDatabase::database();
        QVERIFY2(db.isValid(), "Database connection is not valid");
        QVERIFY2(db.isOpen(), "Database is not open");
        QSqlQuery cleanupQuery(db);
        QVERIFY2(cleanupQuery.exec("DELETE FROM local_files WHERE path LIKE '/tmp/%'"), 
                 "Failed to clean up stale database entries");
    }
    
    // Create temporary files with identical content
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    QString filePath1 = tempDir.path() + "/dir1/video.mkv";
    QString filePath2 = tempDir.path() + "/dir2/video.mkv";
    
    // Create directories
    QDir().mkpath(tempDir.path() + "/dir1");
    QDir().mkpath(tempDir.path() + "/dir2");
    
    // Create files with identical content
    QFile file1(filePath1);
    QVERIFY(file1.open(QIODevice::WriteOnly));
    QByteArray testData = "This is test video content for duplicate hash reuse";
    file1.write(testData);
    file1.close();
    
    QFile file2(filePath2);
    QVERIFY(file2.open(QIODevice::WriteOnly));
    file2.write(testData);
    file2.close();
    
    // Use the default database connection that AniDBApi created
    QSqlDatabase db = QSqlDatabase::database();
    QVERIFY2(db.isValid(), "Database connection is not valid");
    QVERIFY2(db.isOpen(), "Database is not open");
    QSqlQuery query(db);
    
    // Insert first file with a hash (using proper 32-char ED2K hash format)
    query.prepare("INSERT INTO local_files (path, filename, ed2k_hash, status) VALUES (?, ?, ?, 1)");
    query.addBindValue(filePath1);
    query.addBindValue("video.mkv");
    query.addBindValue("abcdef1234567890abcdef1234567890");
    QVERIFY(query.exec());
    query.finish(); // Release query resources
    
    // Insert second file WITHOUT a hash (NULL hash)
    query.prepare("INSERT INTO local_files (path, filename, status) VALUES (?, ?, 0)");
    query.addBindValue(filePath2);
    query.addBindValue("video.mkv");
    QVERIFY(query.exec());
    query.finish(); // Release query resources
    
    // Verify second file has no hash initially
    QSqlQuery verifyQuery1(db);
    verifyQuery1.prepare("SELECT ed2k_hash FROM local_files WHERE path = ?");
    verifyQuery1.addBindValue(filePath2);
    QVERIFY(verifyQuery1.exec());
    QVERIFY(verifyQuery1.next());
    QVERIFY(verifyQuery1.value(0).isNull() || verifyQuery1.value(0).toString().isEmpty());
    verifyQuery1.finish(); // Release the query resources to avoid database locks
    
    // Call getLocalFileHash for the second file (should find and copy hash from first file)
    QString retrievedHash = api.getLocalFileHash(filePath2);
    
    // Verify the hash was copied from the first file
    QCOMPARE(retrievedHash, QString("abcdef1234567890abcdef1234567890"));
    
    // Verify the hash was actually stored in the database for the second file
    QSqlQuery verifyQuery2(db);
    verifyQuery2.prepare("SELECT ed2k_hash, status FROM local_files WHERE path = ?");
    verifyQuery2.addBindValue(filePath2);
    QVERIFY(verifyQuery2.exec());
    QVERIFY(verifyQuery2.next());
    
    QString storedHash = verifyQuery2.value(0).toString();
    int status = verifyQuery2.value(1).toInt();
    
    QCOMPARE(storedHash, QString("abcdef1234567890abcdef1234567890"));
    QCOMPARE(status, 1); // Status should be updated to 1 (hashed)
}

void TestHashDuplicateReuse::testDuplicateFileWithDifferentSizeNoReuse()
{
    // Create AniDBApi instance
    AniDBApi api("test", 1);
    
    // Clean up stale entries from previous test runs
    {
        QSqlDatabase db = QSqlDatabase::database();
        QVERIFY2(db.isValid(), "Database connection is not valid");
        QVERIFY2(db.isOpen(), "Database is not open");
        QSqlQuery cleanupQuery(db);
        QVERIFY2(cleanupQuery.exec("DELETE FROM local_files WHERE path LIKE '/tmp/%'"), 
                 "Failed to clean up stale database entries");
    }
    
    // Create temporary files with same name but different sizes
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    QString filePath1 = tempDir.path() + "/dir1/video.mkv";
    QString filePath2 = tempDir.path() + "/dir2/video.mkv";
    
    // Create directories
    QDir().mkpath(tempDir.path() + "/dir1");
    QDir().mkpath(tempDir.path() + "/dir2");
    
    // Create files with different content (different sizes)
    QFile file1(filePath1);
    QVERIFY(file1.open(QIODevice::WriteOnly));
    file1.write("Small content");
    file1.close();
    
    QFile file2(filePath2);
    QVERIFY(file2.open(QIODevice::WriteOnly));
    file2.write("Much larger content with different data and size");
    file2.close();
    
    // Use the default database connection that AniDBApi created
    QSqlDatabase db = QSqlDatabase::database();
    QVERIFY2(db.isValid(), "Database connection is not valid");
    QVERIFY2(db.isOpen(), "Database is not open");
    QSqlQuery query(db);
    
    // Insert first file with a hash (using proper 32-char ED2K hash format)
    query.prepare("INSERT INTO local_files (path, filename, ed2k_hash, status) VALUES (?, ?, ?, 1)");
    query.addBindValue(filePath1);
    query.addBindValue("video.mkv");
    query.addBindValue("1234567890abcdef1234567890abcdef");
    QVERIFY(query.exec());
    query.finish(); // Release query resources
    
    // Insert second file WITHOUT a hash
    query.prepare("INSERT INTO local_files (path, filename, status) VALUES (?, ?, 0)");
    query.addBindValue(filePath2);
    query.addBindValue("video.mkv");
    QVERIFY(query.exec());
    query.finish(); // Release query resources
    
    // Call getLocalFileHash for the second file (should NOT copy hash due to size mismatch)
    QString retrievedHash = api.getLocalFileHash(filePath2);
    
    // Verify no hash was returned (size mismatch prevents reuse)
    QVERIFY(retrievedHash.isEmpty());
    
    // Verify the hash was NOT stored in the database for the second file
    QSqlQuery verifyQuery(db);
    verifyQuery.prepare("SELECT ed2k_hash FROM local_files WHERE path = ?");
    verifyQuery.addBindValue(filePath2);
    QVERIFY(verifyQuery.exec());
    QVERIFY(verifyQuery.next());
    QVERIFY(verifyQuery.value(0).isNull() || verifyQuery.value(0).toString().isEmpty());
}

void TestHashDuplicateReuse::testNoHashAvailableForDuplicate()
{
    // Create AniDBApi instance
    AniDBApi api("test", 1);
    
    // Clean up stale entries from previous test runs
    {
        QSqlDatabase db = QSqlDatabase::database();
        QVERIFY2(db.isValid(), "Database connection is not valid");
        QVERIFY2(db.isOpen(), "Database is not open");
        QSqlQuery cleanupQuery(db);
        QVERIFY2(cleanupQuery.exec("DELETE FROM local_files WHERE path LIKE '/tmp/%'"), 
                 "Failed to clean up stale database entries");
    }
    
    // Create temporary file
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    QString filePath = tempDir.path() + "/video.mkv";
    
    // Create file
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Test content");
    file.close();
    
    // Use the default database connection that AniDBApi created
    QSqlDatabase db = QSqlDatabase::database();
    QVERIFY2(db.isValid(), "Database connection is not valid");
    QVERIFY2(db.isOpen(), "Database is not open");
    QSqlQuery query(db);
    
    // Insert file WITHOUT a hash and no duplicate exists
    query.prepare("INSERT INTO local_files (path, filename, status) VALUES (?, ?, 0)");
    query.addBindValue(filePath);
    query.addBindValue("video.mkv");
    QVERIFY(query.exec());
    query.finish(); // Release query resources
    
    // Call getLocalFileHash (should return empty since no duplicate with hash exists)
    QString retrievedHash = api.getLocalFileHash(filePath);
    
    // Verify no hash was returned
    QVERIFY(retrievedHash.isEmpty());
}

QTEST_MAIN(TestHashDuplicateReuse)
#include "test_hash_duplicate_reuse.moc"
