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
    
    // File scoring tests
    void testCalculateDeletionScore();
    
    // Settings tests
    void testAheadBuffer();
    void testDeletionThreshold();
    void testAutoMarkDeletion();
    
    // Anime relations tests
    void testGetOriginalPrequel();
    void testGetSeriesChain();
    
    // Auto-start session tests
    void testAutoStartSessionsForExistingAnime();
    
    // File version tests
    void testFileVersionScoring();

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
    
    // Anime table with relations - using actual db column names
    q.exec("CREATE TABLE IF NOT EXISTS anime ("
           "aid INTEGER PRIMARY KEY, "
           "name_romaji TEXT, "
           "relaidlist TEXT, "
           "relaidtype TEXT, "
           "is_hidden INTEGER DEFAULT 0"
           ")");
    
    // Local files table - matches real schema (id, path, filename, status, ed2k_hash, binding_status)
    q.exec("CREATE TABLE IF NOT EXISTS local_files ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "path TEXT UNIQUE, "
           "filename TEXT, "
           "status INTEGER DEFAULT 0, "
           "ed2k TEXT, "
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
           "filename TEXT, "
           "state INTEGER DEFAULT 0"
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
    q.exec("INSERT INTO anime (aid, name_romaji, relaidlist, relaidtype, is_hidden) "
           "VALUES (1, 'Test Anime 1', '2', 'sequel', 0)");
    
    // Anime 2: Sequel of Anime 1
    q.exec("INSERT INTO anime (aid, name_romaji, relaidlist, relaidtype, is_hidden) "
           "VALUES (2, 'Test Anime 2', '1''3', 'prequel''sequel', 0)");
    
    // Anime 3: Sequel of Anime 2 (hidden)
    q.exec("INSERT INTO anime (aid, name_romaji, relaidlist, relaidtype, is_hidden) "
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

// ========== File Scoring Tests ==========

void TestWatchSessionManager::testCalculateDeletionScore()
{
    // Start session and calculate scores
    manager->startSession(1, 3);
    
    // Current episode should have high score
    int score3 = manager->calculateDeletionScore(1003); // Episode 3
    int score4 = manager->calculateDeletionScore(1004); // Episode 4 (ahead)
    int score1 = manager->calculateDeletionScore(1001); // Episode 1 (behind)
    
    // Episodes ahead should have higher scores than those behind
    QVERIFY(score4 > score1);
    
    // Active session should add points
    QVERIFY(score3 > 0);
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

void TestWatchSessionManager::testFileVersionScoring()
{
    // Test that files with older revisions get lower scores
    // This test verifies the file version scoring feature
    // When multiple local files exist for same episode, older versions get penalties
    // based on count of local files with higher versions
    
    // State field encoding per AniDB API:
    //   Bit 0 (1): FILE_CRCOK
    //   Bit 1 (2): FILE_CRCERR
    //   Bit 2 (4): FILE_ISV2 - version 2
    //   Bit 3 (8): FILE_ISV3 - version 3
    //   Bit 4 (16): FILE_ISV4 - version 4
    //   Bit 5 (32): FILE_ISV5 - version 5
    // No version bits set = version 1
    
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    
    // Create a second file for the same episode (ep 1) with a different version
    // First, add local_file entry
    q.prepare("INSERT INTO local_files (path, filename) VALUES (?, ?)");
    q.addBindValue("/test/anime1/episode1_v2.mkv");
    q.addBindValue("episode1_v2.mkv");
    q.exec();
    int localFileId2 = q.lastInsertId().toInt();
    
    // Create file entry with state = 4 (FILE_ISV2 bit set = v2) for newer file
    int fid2 = 5100;
    q.prepare("INSERT INTO file (fid, aid, eid, size, filename, state) VALUES (?, 1, 101, ?, ?, 4)");
    q.addBindValue(fid2);
    q.addBindValue(qint64(500) * 1024 * 1024);
    q.addBindValue("episode1_v2.mkv");
    q.exec();
    
    // Update original file to have state = 1 (FILE_CRCOK, no version bits = v1)
    q.exec("UPDATE file SET state = 1 WHERE fid = 5001");
    
    // Create mylist entry for the new file (lid 1100)
    q.prepare("INSERT INTO mylist (lid, fid, aid, eid, local_watched, local_file) VALUES (1100, ?, 1, 101, 0, ?)");
    q.addBindValue(fid2);
    q.addBindValue(localFileId2);
    q.exec();
    
    // Reload manager to pick up new data
    delete manager;
    manager = new WatchSessionManager();
    manager->startSession(1, 1);
    
    // Get scores for both files (same episode, different versions)
    // v1 has 1 local file with higher version (v2), so gets -1000 penalty
    // v2 has 0 local files with higher version, so no penalty
    int scoreV1 = manager->calculateDeletionScore(1001);  // v1 (older)
    int scoreV2 = manager->calculateDeletionScore(1100);  // v2 (newer)
    
    // The older version (v1) should have a lower score (more eligible for deletion)
    // than the newer version (v2)
    QVERIFY2(scoreV1 < scoreV2, 
             QString("Older version (score=%1) should have lower score than newer version (score=%2)")
             .arg(scoreV1).arg(scoreV2).toLatin1().constData());
}

QTEST_MAIN(TestWatchSessionManager)
#include "test_watchsessionmanager.moc"
