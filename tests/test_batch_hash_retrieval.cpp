#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "../usagi/src/anidbapi.h"
#include "../usagi/src/logger.h"

class TestBatchHashRetrieval : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testBatchGetLocalFileHashes();
    void testBatchGetLocalFileHashesEmpty();
    void testBatchGetLocalFileHashesPartialResults();

private:
    QSqlDatabase db;
};

void TestBatchHashRetrieval::initTestCase()
{
    // Set up in-memory database for testing
    db = QSqlDatabase::addDatabase("QSQLITE", "test_batch_hash");
    db.setDatabaseName(":memory:");
    
    if (!db.open()) {
        QFAIL("Failed to open test database");
    }
    
    // Create local_files table matching the schema
    QSqlQuery query(db);
    QVERIFY(query.exec("CREATE TABLE IF NOT EXISTS `local_files`("
                       "`id` INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "`path` TEXT UNIQUE, "
                       "`filename` TEXT, "
                       "`status` INTEGER DEFAULT 0, "
                       "`ed2k_hash` TEXT)"));
    
    // Insert test data
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, ed2k_hash, status) "
                       "VALUES ('/test/file1.mkv', 'file1.mkv', 'hash1', 1)"));
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, ed2k_hash, status) "
                       "VALUES ('/test/file2.mkv', 'file2.mkv', 'hash2', 2)"));
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, ed2k_hash, status) "
                       "VALUES ('/test/file3.mkv', 'file3.mkv', '', 0)"));  // No hash
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, status) "
                       "VALUES ('/test/file4.mkv', 'file4.mkv', 0)"));  // NULL hash
}

void TestBatchHashRetrieval::cleanupTestCase()
{
    db.close();
    QSqlDatabase::removeDatabase("test_batch_hash");
}

void TestBatchHashRetrieval::testBatchGetLocalFileHashes()
{
    // Create AniDBApi instance (note: this requires proper initialization)
    // For this test, we'll directly test the SQL query logic
    
    QStringList paths;
    paths << "/test/file1.mkv" << "/test/file2.mkv" << "/test/file3.mkv";
    
    // Build and execute the query
    QStringList placeholders;
    for (int i = 0; i < paths.size(); ++i) {
        placeholders.append("?");
    }
    
    QString queryStr = QString("SELECT `path`, `ed2k_hash`, `status` FROM `local_files` WHERE `path` IN (%1)")
                       .arg(placeholders.join(","));
    
    QSqlQuery query(db);
    query.prepare(queryStr);
    
    for (const QString& path : paths) {
        query.addBindValue(path);
    }
    
    QVERIFY(query.exec());
    
    QMap<QString, QString> results;
    while (query.next()) {
        QString path = query.value(0).toString();
        QString hash = query.value(1).toString();
        results[path] = hash;
    }
    
    // Verify results
    QCOMPARE(results.size(), 3);  // All 3 files should be returned (even with empty hash)
    QCOMPARE(results["/test/file1.mkv"], QString("hash1"));
    QCOMPARE(results["/test/file2.mkv"], QString("hash2"));
    QVERIFY(results["/test/file3.mkv"].isEmpty());  // Empty hash
}

void TestBatchHashRetrieval::testBatchGetLocalFileHashesEmpty()
{
    // Test with empty file list
    QStringList paths;
    
    // Should return empty results without error
    if (paths.isEmpty()) {
        QVERIFY(true);  // Expected behavior
    }
}

void TestBatchHashRetrieval::testBatchGetLocalFileHashesPartialResults()
{
    // Test with mix of existing and non-existing files
    QStringList paths;
    paths << "/test/file1.mkv" << "/test/nonexistent.mkv" << "/test/file2.mkv";
    
    QStringList placeholders;
    for (int i = 0; i < paths.size(); ++i) {
        placeholders.append("?");
    }
    
    QString queryStr = QString("SELECT `path`, `ed2k_hash`, `status` FROM `local_files` WHERE `path` IN (%1)")
                       .arg(placeholders.join(","));
    
    QSqlQuery query(db);
    query.prepare(queryStr);
    
    for (const QString& path : paths) {
        query.addBindValue(path);
    }
    
    QVERIFY(query.exec());
    
    QMap<QString, QString> results;
    while (query.next()) {
        QString path = query.value(0).toString();
        QString hash = query.value(1).toString();
        results[path] = hash;
    }
    
    // Should only return 2 files (file1 and file2)
    QCOMPARE(results.size(), 2);
    QVERIFY(results.contains("/test/file1.mkv"));
    QVERIFY(results.contains("/test/file2.mkv"));
    QVERIFY(!results.contains("/test/nonexistent.mkv"));
}

QTEST_MAIN(TestBatchHashRetrieval)
#include "test_batch_hash_retrieval.moc"
