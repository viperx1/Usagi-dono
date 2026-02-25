#include <QTest>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QDir>
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
    
    // File scoring tests — removed (calculateDeletionScore was removed in Phase 5)
    
    // Settings tests
    void testAheadBuffer();
    void testDeletionThreshold();
    void testAutoMarkDeletion();
    
    // Anime relations tests
    void testGetOriginalPrequel();
    void testGetSeriesChain();
    
    // Auto-start session tests
    void testAutoStartSessionsForExistingAnime();
    
    // File version tests — removed (calculateDeletionScore was removed in Phase 5)
    
    // Startup deletion tests
    void testPerformInitialScanWithDeletionEnabled();
    
    // Continuous deletion tests
    void testContinuousDeletionUntilThresholdMet();
    
    // Sequential deletion with API confirmation test
    void testSequentialDeletionWithApiConfirmation();
    void testMissingDuplicateFileDoesNotBypassGapProtection();
    
    // Rating tests
    void testFileRatingWithoutRating();
    void testFileRatingWithZeroRating();
    void testFileRatingWithNormalRating();

private:
    QTemporaryFile *tempDbFile;
    WatchSessionManager *manager;
    void setupTestData();
};

void TestWatchSessionManager::initTestCase()
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
           "viewed INTEGER DEFAULT 0, "
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
           "nameromaji TEXT, "
           "nameenglish TEXT, "
           "namekanji TEXT, "
           "relaidlist TEXT, "
           "relaidtype TEXT, "
           "is_hidden INTEGER DEFAULT 0, "
           "rating TEXT"
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

// ========== File Scoring Tests — removed (calculateDeletionScore removed in Phase 5) ==========

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

// testFileVersionScoring — removed (calculateDeletionScore removed in Phase 5)

void TestWatchSessionManager::testPerformInitialScanWithDeletionEnabled()
{
    // Test that performInitialScan emits deletionCycleRequested when both settings are enabled
    constexpr double HIGH_THRESHOLD_GB = 999999.0;
    
    QSignalSpy cycleSpy(manager, &WatchSessionManager::deletionCycleRequested);
    
    manager->setAutoMarkDeletionEnabled(true);
    manager->setActualDeletionEnabled(true);
    manager->setDeletionThresholdType(DeletionThresholdType::FixedGB);
    manager->setDeletionThresholdValue(HIGH_THRESHOLD_GB);
    
    QVERIFY(manager->isAutoMarkDeletionEnabled());
    QVERIFY(manager->isActualDeletionEnabled());
    
    // Now call performInitialScan - this should emit deletionCycleRequested
    manager->performInitialScan();
    
    // Since space is always below 999999 GB, deletionCycleRequested should be emitted
    QVERIFY(cycleSpy.count() >= 1);
}

void TestWatchSessionManager::testContinuousDeletionUntilThresholdMet()
{
    // Test that onFileDeletionResult emits deletionCycleRequested when space is still low
    
    manager->setAutoMarkDeletionEnabled(true);
    manager->setActualDeletionEnabled(true);
    
    // Verify settings are enabled
    QVERIFY(manager->isAutoMarkDeletionEnabled());
    QVERIFY(manager->isActualDeletionEnabled());
    
    QSignalSpy cycleSpy(manager, &WatchSessionManager::deletionCycleRequested);
    
    // Set high threshold so isDeletionNeeded() returns true
    manager->setDeletionThresholdType(DeletionThresholdType::FixedGB);
    manager->setDeletionThresholdValue(999999.0);
    
    // Simulate successful deletion — should emit deletionCycleRequested
    manager->onFileDeletionResult(1001, 1, true);
    
    // Since space is always below 999999 GB, cycle should be requested again
    QVERIFY(cycleSpy.count() >= 1);
}

void TestWatchSessionManager::testSequentialDeletionWithApiConfirmation()
{
    // Test that onFileDeletionResult emits deletionCycleRequested (not deleteFileRequested)
    // Window handles the cycle by using DeletionQueue
    
    manager->setActualDeletionEnabled(true);
    manager->setDeletionThresholdType(DeletionThresholdType::FixedGB);
    manager->setDeletionThresholdValue(999999.0);
    
    QSignalSpy cycleSpy(manager, &WatchSessionManager::deletionCycleRequested);
    QSignalSpy deletionCompleteSpy(manager, &WatchSessionManager::fileDeleted);
    
    // Simulate successful file deletion
    manager->onFileDeletionResult(1001, 1, true);
    
    // fileDeleted should be emitted
    QCOMPARE(deletionCompleteSpy.count(), 1);
    
    // deletionCycleRequested should be emitted (space is below 999999 GB)
    QVERIFY(cycleSpy.count() >= 1);
}

void TestWatchSessionManager::testMissingDuplicateFileDoesNotBypassGapProtection()
{
    // Gap protection is now handled by DeletionQueue + HybridDeletionClassifier.
    // This test verifies that autoMarkFilesForDeletion emits deletionCycleRequested
    // rather than directly deleting files, so DeletionQueue can apply gap protection.
    
    manager->setAutoMarkDeletionEnabled(true);
    manager->setActualDeletionEnabled(true);
    manager->setDeletionThresholdType(DeletionThresholdType::FixedGB);
    manager->setDeletionThresholdValue(999999.0);
    
    QSignalSpy cycleSpy(manager, &WatchSessionManager::deletionCycleRequested);
    
    manager->autoMarkFilesForDeletion();
    
    // Should emit deletionCycleRequested (not directly delete files)
    QVERIFY(cycleSpy.count() >= 1);
}

void TestWatchSessionManager::testFileRatingWithoutRating()
{
    // Test that anime without rating is treated as RATING_HIGH_THRESHOLD (800)
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    
    // Create anime without rating (NULL rating)
    QVERIFY(q.exec("INSERT INTO anime (aid, name_romaji) VALUES (100, 'Anime Without Rating')"));
    
    // Create mylist entry for this anime
    QVERIFY(q.exec("INSERT INTO mylist (lid, fid, aid, eid) VALUES (9001, 9001, 100, 101)"));
    
    // Get the rating for this file
    int rating = manager->getFileRating(9001);
    
    // Should return RATING_HIGH_THRESHOLD (800)
    QCOMPARE(rating, WatchSessionManager::RATING_HIGH_THRESHOLD);
}

void TestWatchSessionManager::testFileRatingWithZeroRating()
{
    // Test that anime with "0" or "0.00" rating is treated as RATING_HIGH_THRESHOLD (800)
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    
    // Create anime with zero rating
    QVERIFY(q.exec("INSERT INTO anime (aid, name_romaji, rating) VALUES (101, 'Anime With Zero Rating', '0.00')"));
    
    // Create mylist entry for this anime
    QVERIFY(q.exec("INSERT INTO mylist (lid, fid, aid, eid) VALUES (9002, 9002, 101, 101)"));
    
    // Get the rating for this file
    int rating = manager->getFileRating(9002);
    
    // Should return RATING_HIGH_THRESHOLD (800)
    QCOMPARE(rating, WatchSessionManager::RATING_HIGH_THRESHOLD);
}

void TestWatchSessionManager::testFileRatingWithNormalRating()
{
    // Test that anime with normal rating works correctly
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    
    // Create anime with rating 8.75 (should become 875)
    QVERIFY(q.exec("INSERT INTO anime (aid, name_romaji, rating) VALUES (102, 'Highly Rated Anime', '8.75')"));
    
    // Create mylist entry for this anime
    QVERIFY(q.exec("INSERT INTO mylist (lid, fid, aid, eid) VALUES (9003, 9003, 102, 101)"));
    
    // Get the rating for this file
    int rating = manager->getFileRating(9003);
    
    // Should return 875 (8.75 * 100)
    QCOMPARE(rating, 875);
    
    // Create anime with low rating 5.50 (should become 550)
    QVERIFY(q.exec("INSERT INTO anime (aid, name_romaji, rating) VALUES (103, 'Low Rated Anime', '5.50')"));
    
    // Create mylist entry for this anime
    QVERIFY(q.exec("INSERT INTO mylist (lid, fid, aid, eid) VALUES (9004, 9004, 103, 101)"));
    
    // Get the rating for this file
    rating = manager->getFileRating(9004);
    
    // Should return 550 (5.50 * 100)
    QCOMPARE(rating, 550);
}

QTEST_MAIN(TestWatchSessionManager)
#include "test_watchsessionmanager.moc"
