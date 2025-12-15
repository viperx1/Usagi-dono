#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryFile>
#include "../usagi/src/watchchunkmanager.h"

class TestWatchChunkManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testRecordChunk();
    void testGetWatchedChunks();
    void testClearChunks();
    void testCalculateWatchPercentage();
    void testShouldMarkAsWatched_ShortFile();
    void testShouldMarkAsWatched_LongFile();
    void testShouldMarkAsWatched_InsufficientWatching();
    void testLocalWatchedStatus();
    void testChunkCaching();

private:
    QTemporaryFile *tempDbFile;
    WatchChunkManager *manager;
};

void TestWatchChunkManager::initTestCase()
{
    // Ensure clean slate: remove any existing default connection
    {
        QString defaultConn = QSqlDatabase::defaultConnection;
        if (QSqlDatabase::contains(defaultConn)) {
            QSqlDatabase existingDb = QSqlDatabase::database(defaultConn, false);
            if (existingDb.isOpen()) {
                existingDb.close();
            }
            QSqlDatabase::removeDatabase(defaultConn);
        }
    }
    
    // Create a temporary database for testing
    tempDbFile = new QTemporaryFile();
    QVERIFY(tempDbFile->open());
    tempDbFile->close();
    
    // Setup database connection
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(tempDbFile->fileName());
    QVERIFY(db.open());
    
    // Create required tables
    QSqlQuery q(db);
    q.exec("CREATE TABLE IF NOT EXISTS `mylist`("
           "`lid` INTEGER PRIMARY KEY, "
           "`local_watched` INTEGER DEFAULT 0"
           ")");
    
    q.exec("CREATE TABLE IF NOT EXISTS `watch_chunks`("
           "`id` INTEGER PRIMARY KEY AUTOINCREMENT, "
           "`lid` INTEGER NOT NULL, "
           "`chunk_index` INTEGER NOT NULL, "
           "`watched_at` INTEGER NOT NULL, "
           "UNIQUE(`lid`, `chunk_index`)"
           ")");
    
    q.exec("CREATE INDEX IF NOT EXISTS `idx_watch_chunks_lid` ON `watch_chunks`(`lid`)");
    
    // Insert test mylist entries
    q.exec("INSERT INTO mylist (lid, local_watched) VALUES (1, 0)");
    q.exec("INSERT INTO mylist (lid, local_watched) VALUES (2, 0)");
    q.exec("INSERT INTO mylist (lid, local_watched) VALUES (3, 0)");
    
    // Create manager instance
    manager = new WatchChunkManager();
}

void TestWatchChunkManager::cleanupTestCase()
{
    delete manager;
    
    // Close the database first
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        db.close();
    }
    
    // Clear the QSqlDatabase object to release the connection reference
    db = QSqlDatabase();
    
    // Now safely remove the database connection
    QString defaultConn = QSqlDatabase::defaultConnection;
    if (QSqlDatabase::contains(defaultConn)) {
        QSqlDatabase::removeDatabase(defaultConn);
    }
    
    delete tempDbFile;
}

void TestWatchChunkManager::testRecordChunk()
{
    // Test recording a chunk
    manager->recordChunk(1, 0);
    manager->recordChunk(1, 1);
    manager->recordChunk(1, 2);
    
    QSet<int> chunks = manager->getWatchedChunks(1);
    QCOMPARE(chunks.size(), 3);
    QVERIFY(chunks.contains(0));
    QVERIFY(chunks.contains(1));
    QVERIFY(chunks.contains(2));
}

void TestWatchChunkManager::testGetWatchedChunks()
{
    // Clear previous chunks
    manager->clearChunks(2);
    
    // Record some chunks
    manager->recordChunk(2, 5);
    manager->recordChunk(2, 10);
    manager->recordChunk(2, 15);
    
    QSet<int> chunks = manager->getWatchedChunks(2);
    QCOMPARE(chunks.size(), 3);
    QVERIFY(chunks.contains(5));
    QVERIFY(chunks.contains(10));
    QVERIFY(chunks.contains(15));
}

void TestWatchChunkManager::testClearChunks()
{
    // Record some chunks
    manager->recordChunk(3, 0);
    manager->recordChunk(3, 1);
    
    // Verify they exist
    QSet<int> chunks = manager->getWatchedChunks(3);
    QVERIFY(chunks.size() > 0);
    
    // Clear chunks
    manager->clearChunks(3);
    
    // Verify they're cleared
    chunks = manager->getWatchedChunks(3);
    QCOMPARE(chunks.size(), 0);
}

void TestWatchChunkManager::testCalculateWatchPercentage()
{
    // Clear previous data
    manager->clearChunks(1);
    
    // For a 10-minute file (600 seconds), we have 10 chunks (60 seconds each)
    int duration = 600; // 10 minutes
    
    // Record 8 chunks (80%)
    for (int i = 0; i < 8; i++) {
        manager->recordChunk(1, i);
    }
    
    double percentage = manager->calculateWatchPercentage(1, duration);
    QCOMPARE(percentage, 80.0);
}

void TestWatchChunkManager::testShouldMarkAsWatched_ShortFile()
{
    // Clear previous data
    manager->clearChunks(1);
    
    // For a 2-minute file (120 seconds) - short file
    int duration = 120;
    
    // Record 1 chunk
    manager->recordChunk(1, 0);
    
    // Should be marked as watched (short files have lower threshold)
    bool shouldMark = manager->shouldMarkAsWatched(1, duration);
    QVERIFY(shouldMark);
}

void TestWatchChunkManager::testShouldMarkAsWatched_LongFile()
{
    // Clear previous data
    manager->clearChunks(2);
    
    // For a 20-minute file (1200 seconds), we have 20 chunks
    int duration = 1200;
    
    // Record 16 chunks (80%)
    for (int i = 0; i < 16; i++) {
        manager->recordChunk(2, i);
    }
    
    // Should be marked as watched (80% threshold met)
    bool shouldMark = manager->shouldMarkAsWatched(2, duration);
    QVERIFY(shouldMark);
}

void TestWatchChunkManager::testShouldMarkAsWatched_InsufficientWatching()
{
    // Clear previous data
    manager->clearChunks(3);
    
    // For a 20-minute file (1200 seconds), we have 20 chunks
    int duration = 1200;
    
    // Record only 5 chunks (25%)
    for (int i = 0; i < 5; i++) {
        manager->recordChunk(3, i);
    }
    
    // Should NOT be marked as watched (below 80% threshold)
    bool shouldMark = manager->shouldMarkAsWatched(3, duration);
    QVERIFY(!shouldMark);
}

void TestWatchChunkManager::testLocalWatchedStatus()
{
    // Test setting and getting local watched status
    manager->updateLocalWatchedStatus(1, true);
    bool status = manager->getLocalWatchedStatus(1);
    QVERIFY(status);
    
    manager->updateLocalWatchedStatus(1, false);
    status = manager->getLocalWatchedStatus(1);
    QVERIFY(!status);
}

void TestWatchChunkManager::testChunkCaching()
{
    // Clear previous data
    manager->clearChunks(1);
    
    // Record chunks
    manager->recordChunk(1, 0);
    manager->recordChunk(1, 1);
    
    // Get chunks (should load from database and cache)
    QSet<int> chunks1 = manager->getWatchedChunks(1);
    
    // Record another chunk
    manager->recordChunk(1, 2);
    
    // Get chunks again (should use cache and include new chunk)
    QSet<int> chunks2 = manager->getWatchedChunks(1);
    
    QCOMPARE(chunks2.size(), 3);
    QVERIFY(chunks2.contains(0));
    QVERIFY(chunks2.contains(1));
    QVERIFY(chunks2.contains(2));
}

QTEST_MAIN(TestWatchChunkManager)
#include "test_watchchunkmanager.moc"
