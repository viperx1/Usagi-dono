#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryFile>
#include "../usagi/src/watchsessionmanager.h"

class TestWatchSessionManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Session tests
    void testStartSession();
    void testStartSessionFromFile();
    void testEndSession();
    void testMarkEpisodeWatched();
    void testSessionPersistence();
    
    // File marking tests
    void testCalculateMarkScore();
    void testSetFileMarkType();
    void testGetFilesForDeletion();
    void testGetFilesForDownload();
    
    // Settings tests
    void testAheadBuffer();
    void testDeletionThreshold();
    void testAutoMarkDeletion();
    void testDelayedAutoMarkDeletion();
    
    // Anime relations tests
    void testGetOriginalPrequel();
    void testGetSeriesChain();
    
    // Auto-start session tests
    void testAutoStartSessionsForExistingAnime();

private:
    QTemporaryFile *tempDbFile;
    WatchSessionManager *manager;
    void setupTestData();
};

void TestWatchSessionManager::initTestCase()
{
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
    
    // Settings table
    q.exec("CREATE TABLE IF NOT EXISTS settings ("
           "name TEXT PRIMARY KEY, "
           "value TEXT"
           ")");
    
    // Mylist table - matches real schema with local_file reference and fid
    q.exec("CREATE TABLE IF NOT EXISTS mylist ("
           "lid INTEGER PRIMARY KEY, "
           "fid INTEGER, "
           "aid INTEGER, "
           "eid INTEGER, "
           "local_watched INTEGER DEFAULT 0, "
           "local_file INTEGER"
           ")");
    
    // Episode table
    q.exec("CREATE TABLE IF NOT EXISTS episode ("
           "eid INTEGER PRIMARY KEY, "
           "epno TEXT"
           ")");
    
    // Anime table with relations
    q.exec("CREATE TABLE IF NOT EXISTS anime ("
           "aid INTEGER PRIMARY KEY, "
           "name_romaji TEXT, "
           "related_aid_list TEXT, "
           "related_aid_type TEXT, "
           "is_hidden INTEGER DEFAULT 0"
           ")");
    
    // Local files table - matches real schema (id, path, filename, status, ed2k_hash, binding_status)
    q.exec("CREATE TABLE IF NOT EXISTS local_files ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "path TEXT UNIQUE, "
           "filename TEXT, "
           "status INTEGER DEFAULT 0, "
           "ed2k_hash TEXT, "
           "binding_status INTEGER DEFAULT 0"
           ")");
    
    // File table - matches real schema for file size lookup
    q.exec("CREATE TABLE IF NOT EXISTS file ("
           "fid INTEGER PRIMARY KEY, "
           "aid INTEGER, "
           "eid INTEGER, "
           "gid INTEGER, "
           "size BIGINT, "
           "ed2k TEXT, "
           "filename TEXT"
           ")");
}

void TestWatchSessionManager::cleanupTestCase()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    delete tempDbFile;
}

void TestWatchSessionManager::init()
{
    setupTestData();
    manager = new WatchSessionManager();
}

void TestWatchSessionManager::cleanup()
{
    delete manager;
    manager = nullptr;
    
    // Clear test data
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec("DELETE FROM mylist");
    q.exec("DELETE FROM episode");
    q.exec("DELETE FROM anime");
    q.exec("DELETE FROM local_files");
    q.exec("DELETE FROM watch_sessions");
    q.exec("DELETE FROM session_watched_episodes");
    q.exec("DELETE FROM file_marks");
}

void TestWatchSessionManager::setupTestData()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    
    // Create test anime with relations
    // Anime 1: Original (no prequel)
    q.exec("INSERT INTO anime (aid, name_romaji, related_aid_list, related_aid_type, is_hidden) "
           "VALUES (1, 'Test Anime 1', '2', 'sequel', 0)");
    
    // Anime 2: Sequel of Anime 1
    q.exec("INSERT INTO anime (aid, name_romaji, related_aid_list, related_aid_type, is_hidden) "
           "VALUES (2, 'Test Anime 2', '1''3', 'prequel''sequel', 0)");
    
    // Anime 3: Sequel of Anime 2 (hidden)
    q.exec("INSERT INTO anime (aid, name_romaji, related_aid_list, related_aid_type, is_hidden) "
           "VALUES (3, 'Test Anime 3', '2', 'prequel', 1)");
    
    // Anime 4: Standalone (no relations)
    q.exec("INSERT INTO anime (aid, name_romaji, is_hidden) "
           "VALUES (4, 'Standalone Anime', 0)");
    
    // Create test episodes
    for (int ep = 1; ep <= 12; ep++) {
        q.prepare("INSERT INTO episode (eid, epno) VALUES (?, ?)");
        q.addBindValue(100 + ep);
        q.addBindValue(QString::number(ep));
        q.exec();
    }
    
    // Create test mylist entries with local_file references and file size entries
    for (int ep = 1; ep <= 6; ep++) {
        // First insert into local_files to get the id
        q.prepare("INSERT INTO local_files (path, filename) VALUES (?, ?)");
        q.addBindValue(QString("/test/anime1/episode%1.mkv").arg(ep));
        q.addBindValue(QString("episode%1.mkv").arg(ep));
        q.exec();
        int localFileId = q.lastInsertId().toInt();
        
        // Create file entry with size (500 MB each = 500*1024*1024 bytes)
        int fid = 5000 + ep;
        q.prepare("INSERT INTO file (fid, aid, eid, size, filename) VALUES (?, 1, ?, ?, ?)");
        q.addBindValue(fid);
        q.addBindValue(100 + ep);
        q.addBindValue(qint64(500) * 1024 * 1024); // 500 MB
        q.addBindValue(QString("episode%1.mkv").arg(ep));
        q.exec();
        
        // Then insert into mylist with local_file reference and fid
        q.prepare("INSERT INTO mylist (lid, fid, aid, eid, local_watched, local_file) VALUES (?, ?, 1, ?, 0, ?)");
        q.addBindValue(1000 + ep);
        q.addBindValue(fid);
        q.addBindValue(100 + ep);
        q.addBindValue(localFileId);
        q.exec();
    }
    
    // Create entries for anime 2 (no local files)
    for (int ep = 1; ep <= 6; ep++) {
        q.prepare("INSERT INTO mylist (lid, aid, eid, local_watched) VALUES (?, 2, ?, 0)");
        q.addBindValue(2000 + ep);
        q.addBindValue(100 + ep);
        q.exec();
    }
}

// ========== Session Tests ==========

void TestWatchSessionManager::testStartSession()
{
    // Start session for anime 1
    bool result = manager->startSession(1, 1);
    QVERIFY(result);
    QVERIFY(manager->hasActiveSession(1));
    QCOMPARE(manager->getCurrentSessionEpisode(1), 1);
}

void TestWatchSessionManager::testStartSessionFromFile()
{
    // Start session from file lid=1003 (episode 3 of anime 1)
    bool result = manager->startSessionFromFile(1003);
    QVERIFY(result);
    QVERIFY(manager->hasActiveSession(1));
    QCOMPARE(manager->getCurrentSessionEpisode(1), 3);
}

void TestWatchSessionManager::testEndSession()
{
    // Start then end a session
    manager->startSession(1, 1);
    QVERIFY(manager->hasActiveSession(1));
    
    manager->endSession(1);
    QVERIFY(!manager->hasActiveSession(1));
}

void TestWatchSessionManager::testMarkEpisodeWatched()
{
    // Start session and mark episodes
    manager->startSession(1, 1);
    
    manager->markEpisodeWatched(1, 1);
    QCOMPARE(manager->getCurrentSessionEpisode(1), 2);
    
    manager->markEpisodeWatched(1, 2);
    QCOMPARE(manager->getCurrentSessionEpisode(1), 3);
    
    // Marking a later episode should advance position
    manager->markEpisodeWatched(1, 5);
    QCOMPARE(manager->getCurrentSessionEpisode(1), 6);
}

void TestWatchSessionManager::testSessionPersistence()
{
    // Start session and mark episodes
    manager->startSession(1, 1);
    manager->markEpisodeWatched(1, 1);
    manager->markEpisodeWatched(1, 2);
    
    // Save to database
    manager->saveToDatabase();
    
    // Create new manager instance to test loading
    delete manager;
    manager = new WatchSessionManager();
    
    // Session should still be active
    QVERIFY(manager->hasActiveSession(1));
    QCOMPARE(manager->getCurrentSessionEpisode(1), 3);
}

// ========== File Marking Tests ==========

void TestWatchSessionManager::testCalculateMarkScore()
{
    // Start session and calculate scores
    manager->startSession(1, 3);
    
    // Current episode should have high score
    int score3 = manager->calculateMarkScore(1003); // Episode 3
    int score4 = manager->calculateMarkScore(1004); // Episode 4 (ahead)
    int score1 = manager->calculateMarkScore(1001); // Episode 1 (behind)
    
    // Episodes ahead should have higher scores than those behind
    QVERIFY(score4 > score1);
    
    // Active session should add points
    QVERIFY(score3 > 0);
}

void TestWatchSessionManager::testSetFileMarkType()
{
    // Set mark type for a file
    manager->setFileMarkType(1001, FileMarkType::ForDownload);
    QCOMPARE(manager->getFileMarkType(1001), FileMarkType::ForDownload);
    
    manager->setFileMarkType(1002, FileMarkType::ForDeletion);
    QCOMPARE(manager->getFileMarkType(1002), FileMarkType::ForDeletion);
    
    // Getting info should include mark type
    FileMarkInfo info = manager->getFileMarkInfo(1001);
    QCOMPARE(info.markType, FileMarkType::ForDownload);
}

void TestWatchSessionManager::testGetFilesForDeletion()
{
    // Mark some files for deletion
    manager->setFileMarkType(1001, FileMarkType::ForDeletion);
    manager->setFileMarkType(1002, FileMarkType::ForDeletion);
    manager->setFileMarkType(1003, FileMarkType::ForDownload); // Not deletion
    
    QList<int> deletionFiles = manager->getFilesForDeletion();
    QCOMPARE(deletionFiles.size(), 2);
    QVERIFY(deletionFiles.contains(1001));
    QVERIFY(deletionFiles.contains(1002));
    QVERIFY(!deletionFiles.contains(1003));
}

void TestWatchSessionManager::testGetFilesForDownload()
{
    // Mark some files for download
    manager->setFileMarkType(1001, FileMarkType::ForDownload);
    manager->setFileMarkType(1002, FileMarkType::ForDownload);
    manager->setFileMarkType(1003, FileMarkType::ForDeletion); // Not download
    
    QList<int> downloadFiles = manager->getFilesForDownload();
    QCOMPARE(downloadFiles.size(), 2);
    QVERIFY(downloadFiles.contains(1001));
    QVERIFY(downloadFiles.contains(1002));
    QVERIFY(!downloadFiles.contains(1003));
}

// ========== Settings Tests ==========

void TestWatchSessionManager::testAheadBuffer()
{
    // Default ahead buffer
    int defaultBuffer = manager->getAheadBuffer();
    QCOMPARE(defaultBuffer, 3); // Default is 3
    
    // Change ahead buffer
    manager->setAheadBuffer(5);
    QCOMPARE(manager->getAheadBuffer(), 5);
    
    // Test persistence
    delete manager;
    manager = new WatchSessionManager();
    QCOMPARE(manager->getAheadBuffer(), 5);
}

void TestWatchSessionManager::testDeletionThreshold()
{
    // Default threshold type
    QCOMPARE(manager->getDeletionThresholdType(), DeletionThresholdType::FixedGB);
    
    // Change threshold settings
    manager->setDeletionThresholdType(DeletionThresholdType::Percentage);
    manager->setDeletionThresholdValue(25.0);
    
    QCOMPARE(manager->getDeletionThresholdType(), DeletionThresholdType::Percentage);
    QCOMPARE(manager->getDeletionThresholdValue(), 25.0);
    
    // Test persistence
    delete manager;
    manager = new WatchSessionManager();
    QCOMPARE(manager->getDeletionThresholdType(), DeletionThresholdType::Percentage);
    QCOMPARE(manager->getDeletionThresholdValue(), 25.0);
}

void TestWatchSessionManager::testAutoMarkDeletion()
{
    // Default state
    QVERIFY(!manager->isAutoMarkDeletionEnabled());
    
    // Enable auto-mark
    manager->setAutoMarkDeletionEnabled(true);
    QVERIFY(manager->isAutoMarkDeletionEnabled());
    
    // Test persistence
    delete manager;
    manager = new WatchSessionManager();
    QVERIFY(manager->isAutoMarkDeletionEnabled());
}

void TestWatchSessionManager::testDelayedAutoMarkDeletion()
{
    // Test that setWatchedPath() and setAutoMarkDeletionEnabled() don't trigger 
    // auto-mark before performInitialScan(). This prevents marking files for 
    // deletion before mylist data is loaded.
    
    // Initially, no files should be marked for deletion
    QList<int> deletionFiles = manager->getFilesForDeletion();
    QCOMPARE(deletionFiles.size(), 0);
    
    // Enable auto-mark deletion - should NOT trigger marking before initial scan
    manager->setAutoMarkDeletionEnabled(true);
    QVERIFY(manager->isAutoMarkDeletionEnabled());
    
    // Files should still not be marked (because initial scan hasn't happened)
    deletionFiles = manager->getFilesForDeletion();
    QCOMPARE(deletionFiles.size(), 0);
    
    // Setting watched path should also NOT trigger auto-mark before initial scan
    manager->setWatchedPath("/some/test/path");
    
    // Files should still not be marked
    deletionFiles = manager->getFilesForDeletion();
    QCOMPARE(deletionFiles.size(), 0);
    
    // Now call performInitialScan - this should trigger the scan
    // (Note: actual marking depends on disk space threshold, which may not trigger
    // in test environment with invalid path, but the scan logic is executed)
    manager->performInitialScan();
    
    // The test verifies that the delay mechanism works correctly:
    // - Before performInitialScan(): 0 files marked (verified above)
    // - After performInitialScan(): scan runs (threshold may or may not trigger marking)
    // The key assertion is that NO files were marked before performInitialScan()
}

// ========== Anime Relations Tests ==========

void TestWatchSessionManager::testGetOriginalPrequel()
{
    // Anime 3 should trace back to Anime 1
    int original = manager->getOriginalPrequel(3);
    QCOMPARE(original, 1);
    
    // Anime 2 should trace back to Anime 1
    original = manager->getOriginalPrequel(2);
    QCOMPARE(original, 1);
    
    // Anime 1 is the original
    original = manager->getOriginalPrequel(1);
    QCOMPARE(original, 1);
    
    // Standalone anime has no prequel
    original = manager->getOriginalPrequel(4);
    QCOMPARE(original, 4);
}

void TestWatchSessionManager::testGetSeriesChain()
{
    // Starting from any anime in the chain should give full series
    QList<int> chain = manager->getSeriesChain(3);
    
    // Chain should be [1, 2, 3]
    QCOMPARE(chain.size(), 3);
    QCOMPARE(chain[0], 1);
    QCOMPARE(chain[1], 2);
    QCOMPARE(chain[2], 3);
    
    // Standalone anime should only include itself
    chain = manager->getSeriesChain(4);
    QCOMPARE(chain.size(), 1);
    QCOMPARE(chain[0], 4);
}

void TestWatchSessionManager::testAutoStartSessionsForExistingAnime()
{
    // Initially, no sessions should be active
    QVERIFY(!manager->hasActiveSession(1));
    QVERIFY(!manager->hasActiveSession(2));
    
    // Perform initial scan - this should auto-start sessions for anime with local files
    manager->performInitialScan();
    
    // Anime 1 has local files (set up in setupTestData), so it should now have an active session
    QVERIFY(manager->hasActiveSession(1));
    
    // Anime 2 does NOT have local files (no local_file reference), so it should not have a session
    QVERIFY(!manager->hasActiveSession(2));
    
    // Session should start at episode 1
    QCOMPARE(manager->getCurrentSessionEpisode(1), 1);
    
    // Calling performInitialScan again should not create duplicate sessions
    manager->performInitialScan();
    QVERIFY(manager->hasActiveSession(1));
    QCOMPARE(manager->getCurrentSessionEpisode(1), 1);
}

QTEST_MAIN(TestWatchSessionManager)
#include "test_watchsessionmanager.moc"
