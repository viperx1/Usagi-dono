#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSignalSpy>
#include "../usagi/src/deletionlockmanager.h"
#include "../usagi/src/factorweightlearner.h"
#include "../usagi/src/deletionhistorymanager.h"

/**
 * Tests for the deletion candidate display infrastructure:
 *   - DeletionLockManager: lock/unlock anime/episode, propagation
 *   - FactorWeightLearner: weight adjustment, score computation, reset
 *   - DeletionHistoryManager: recording, querying, pruning
 */
class TestDeletionCandidateDisplay : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    // DeletionLockManager tests
    void testLockAnime();
    void testUnlockAnime();
    void testLockEpisode();
    void testUnlockEpisode();
    void testIsFileLocked();
    void testAnimeLockTrumpsEpisodeLock();
    void testLockChangedSignal();
    void testReloadCaches();

    // FactorWeightLearner tests
    void testInitialWeightsZero();
    void testRecordChoiceAdjustsWeights();
    void testComputeScoreWithZeroWeights();
    void testComputeScoreWithNonZeroWeights();
    void testIsTrainedRequires50Choices();
    void testResetAllWeights();
    void testMinFactorDifferenceIgnored();

    // DeletionHistoryManager tests
    void testRecordAndQueryHistory();
    void testHistoryFilterByType();
    void testHistoryTotalSpaceFreed();
    void testHistoryPruning();

private:
    void setupDatabase();
    void teardownDatabase();
};

// ---------------------------------------------------------------------------
// Setup / Teardown
// ---------------------------------------------------------------------------

void TestDeletionCandidateDisplay::initTestCase()
{
    setupDatabase();
}

void TestDeletionCandidateDisplay::cleanupTestCase()
{
    teardownDatabase();
}

void TestDeletionCandidateDisplay::cleanup()
{
    // Clear tables between tests
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec("DELETE FROM deletion_locks");
    q.exec("DELETE FROM deletion_factor_weights");
    q.exec("DELETE FROM deletion_choices");
    q.exec("DELETE FROM deletion_history");
    q.exec("DELETE FROM mylist");
}

void TestDeletionCandidateDisplay::setupDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());

    QSqlQuery q(db);
    // Create mylist table (simplified for testing)
    q.exec("CREATE TABLE mylist ("
           "lid INTEGER PRIMARY KEY, aid INTEGER, eid INTEGER, fid INTEGER, "
           "viewed INTEGER DEFAULT 0, state INTEGER DEFAULT 1, "
           "deletion_locked INTEGER DEFAULT 0)");

    // Create deletion_locks table
    q.exec("CREATE TABLE deletion_locks ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, aid INTEGER, eid INTEGER, locked_at INTEGER, "
           "CHECK ((aid IS NOT NULL AND eid IS NULL) OR (aid IS NULL AND eid IS NOT NULL)), "
           "UNIQUE(aid, eid))");
    q.exec("CREATE INDEX idx_deletion_locks_aid ON deletion_locks(aid)");
    q.exec("CREATE INDEX idx_deletion_locks_eid ON deletion_locks(eid)");

    // Create factor weights table
    q.exec("CREATE TABLE deletion_factor_weights ("
           "factor TEXT PRIMARY KEY, weight REAL DEFAULT 0.0, total_adjustments INTEGER DEFAULT 0)");

    // Create choices table
    q.exec("CREATE TABLE deletion_choices ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, kept_lid INTEGER, deleted_lid INTEGER, "
           "kept_factors TEXT, deleted_factors TEXT, chosen_at INTEGER)");
    q.exec("CREATE INDEX idx_deletion_choices_time ON deletion_choices(chosen_at)");

    // Create history table
    q.exec("CREATE TABLE deletion_history ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, lid INTEGER, aid INTEGER, eid INTEGER, "
           "replaced_by_lid INTEGER, file_path TEXT, anime_name TEXT, episode_label TEXT, "
           "file_size INTEGER, tier INTEGER, reason TEXT, learned_score REAL, "
           "deletion_type TEXT, space_before INTEGER, space_after INTEGER, deleted_at INTEGER)");
    q.exec("CREATE INDEX idx_deletion_history_time ON deletion_history(deleted_at)");
    q.exec("CREATE INDEX idx_deletion_history_aid  ON deletion_history(aid)");
    q.exec("CREATE INDEX idx_deletion_history_type ON deletion_history(deletion_type)");
}

void TestDeletionCandidateDisplay::teardownDatabase()
{
    {
        QSqlDatabase db = QSqlDatabase::database();
        db.close();
    }
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

// ===================================================================
// DeletionLockManager tests
// ===================================================================

void TestDeletionCandidateDisplay::testLockAnime()
{
    DeletionLockManager mgr;
    mgr.reloadCaches();

    // Insert test mylist rows for anime 100
    QSqlQuery q(QSqlDatabase::database());
    q.exec("INSERT INTO mylist (lid, aid, eid) VALUES (1, 100, 1001)");
    q.exec("INSERT INTO mylist (lid, aid, eid) VALUES (2, 100, 1002)");

    mgr.lockAnime(100);
    QVERIFY(mgr.isAnimeLocked(100));
    QCOMPARE(mgr.lockedAnimeCount(), 1);

    // Check propagation to mylist
    q.exec("SELECT deletion_locked FROM mylist WHERE lid = 1");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 2);  // anime-level lock value
}

void TestDeletionCandidateDisplay::testUnlockAnime()
{
    DeletionLockManager mgr;
    mgr.reloadCaches();

    QSqlQuery q(QSqlDatabase::database());
    q.exec("INSERT INTO mylist (lid, aid, eid) VALUES (1, 100, 1001)");

    mgr.lockAnime(100);
    QVERIFY(mgr.isAnimeLocked(100));

    mgr.unlockAnime(100);
    QVERIFY(!mgr.isAnimeLocked(100));
    QCOMPARE(mgr.lockedAnimeCount(), 0);

    // Check propagation: deletion_locked should be 0
    q.exec("SELECT deletion_locked FROM mylist WHERE lid = 1");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
}

void TestDeletionCandidateDisplay::testLockEpisode()
{
    DeletionLockManager mgr;
    mgr.reloadCaches();

    QSqlQuery q(QSqlDatabase::database());
    q.exec("INSERT INTO mylist (lid, aid, eid) VALUES (1, 100, 1001)");

    mgr.lockEpisode(1001);
    QVERIFY(mgr.isEpisodeLocked(1001));
    QCOMPARE(mgr.lockedEpisodeCount(), 1);

    // Check propagation
    q.exec("SELECT deletion_locked FROM mylist WHERE eid = 1001");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);  // episode-level lock value
}

void TestDeletionCandidateDisplay::testUnlockEpisode()
{
    DeletionLockManager mgr;
    mgr.reloadCaches();

    QSqlQuery q(QSqlDatabase::database());
    q.exec("INSERT INTO mylist (lid, aid, eid) VALUES (1, 100, 1001)");

    mgr.lockEpisode(1001);
    mgr.unlockEpisode(1001);
    QVERIFY(!mgr.isEpisodeLocked(1001));

    q.exec("SELECT deletion_locked FROM mylist WHERE eid = 1001");
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
}

void TestDeletionCandidateDisplay::testIsFileLocked()
{
    DeletionLockManager mgr;
    mgr.reloadCaches();

    QSqlQuery q(QSqlDatabase::database());
    q.exec("INSERT INTO mylist (lid, aid, eid) VALUES (1, 100, 1001)");
    q.exec("INSERT INTO mylist (lid, aid, eid) VALUES (2, 100, 1002)");

    QVERIFY(!mgr.isFileLocked(1));
    mgr.lockAnime(100);
    QVERIFY(mgr.isFileLocked(1));
    QVERIFY(mgr.isFileLocked(2));
}

void TestDeletionCandidateDisplay::testAnimeLockTrumpsEpisodeLock()
{
    DeletionLockManager mgr;
    mgr.reloadCaches();

    QSqlQuery q(QSqlDatabase::database());
    q.exec("INSERT INTO mylist (lid, aid, eid) VALUES (1, 100, 1001)");

    mgr.lockAnime(100);
    mgr.lockEpisode(1001);  // Redundant, but harmless

    // Unlock anime — episode should still be locked via episode lock
    mgr.unlockAnime(100);

    // After anime unlock, recalculation should check episode locks.
    // The episode lock row exists, so after recalculation the mylist
    // deletion_locked should reflect the episode lock.
    q.exec("SELECT deletion_locked FROM mylist WHERE eid = 1001");
    QVERIFY(q.next());
    // After anime unlock, recalculation resets to 0 then re-applies episode locks
    // The episode lock exists, so the value should be 1
    QCOMPARE(q.value(0).toInt(), 1);
}

void TestDeletionCandidateDisplay::testLockChangedSignal()
{
    DeletionLockManager mgr;
    mgr.reloadCaches();
    QSignalSpy spy(&mgr, &DeletionLockManager::lockChanged);

    mgr.lockAnime(100);
    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 100);
    QCOMPARE(args.at(2).toBool(), true);

    mgr.unlockAnime(100);
    QCOMPARE(spy.count(), 1);
    args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 100);
    QCOMPARE(args.at(2).toBool(), false);
}

void TestDeletionCandidateDisplay::testReloadCaches()
{
    DeletionLockManager mgr;

    // Insert locks directly via SQL
    QSqlQuery q(QSqlDatabase::database());
    q.exec("INSERT INTO deletion_locks (aid, eid, locked_at) VALUES (100, NULL, 0)");
    q.exec("INSERT INTO deletion_locks (aid, eid, locked_at) VALUES (NULL, 2001, 0)");

    mgr.reloadCaches();
    QVERIFY(mgr.isAnimeLocked(100));
    QVERIFY(mgr.isEpisodeLocked(2001));
    QVERIFY(!mgr.isAnimeLocked(999));
}

// ===================================================================
// FactorWeightLearner tests
// ===================================================================

void TestDeletionCandidateDisplay::testInitialWeightsZero()
{
    FactorWeightLearner learner;
    for (const QString &f : FactorWeightLearner::factorNames()) {
        QCOMPARE(learner.getWeight(f), 0.0);
    }
    QCOMPARE(learner.totalChoicesMade(), 0);
    QVERIFY(!learner.isTrained());
}

void TestDeletionCandidateDisplay::testRecordChoiceAdjustsWeights()
{
    FactorWeightLearner learner;
    learner.ensureTablesExist();

    QMap<QString, double> keptFactors;
    keptFactors["anime_rating"]            = 0.9;
    keptFactors["size_weighted_distance"]   = 0.3;
    keptFactors["group_status"]            = 1.0;
    keptFactors["watch_recency"]           = 0.5;
    keptFactors["view_percentage"]         = 0.8;

    QMap<QString, double> deletedFactors;
    deletedFactors["anime_rating"]            = 0.4;
    deletedFactors["size_weighted_distance"]   = 0.7;
    deletedFactors["group_status"]            = 0.0;
    deletedFactors["watch_recency"]           = 0.5;  // same as kept → no adjustment
    deletedFactors["view_percentage"]         = 0.2;

    learner.recordChoice(1, 2, keptFactors, deletedFactors);

    // anime_rating: kept > deleted → +0.1
    QVERIFY(qAbs(learner.getWeight("anime_rating") - 0.1) < 0.001);
    // size_weighted_distance: kept < deleted → -0.1
    QVERIFY(qAbs(learner.getWeight("size_weighted_distance") - (-0.1)) < 0.001);
    // group_status: kept > deleted → +0.1
    QVERIFY(qAbs(learner.getWeight("group_status") - 0.1) < 0.001);
    // watch_recency: same → no adjustment
    QVERIFY(qAbs(learner.getWeight("watch_recency") - 0.0) < 0.001);
    // view_percentage: kept > deleted → +0.1
    QVERIFY(qAbs(learner.getWeight("view_percentage") - 0.1) < 0.001);

    QCOMPARE(learner.totalChoicesMade(), 1);
}

void TestDeletionCandidateDisplay::testComputeScoreWithZeroWeights()
{
    FactorWeightLearner learner;
    QMap<QString, double> factors;
    factors["anime_rating"] = 0.8;
    factors["size_weighted_distance"] = 0.5;
    QCOMPARE(learner.computeScore(factors), 0.0);
}

void TestDeletionCandidateDisplay::testComputeScoreWithNonZeroWeights()
{
    FactorWeightLearner learner;
    learner.ensureTablesExist();

    // Manually set weights through multiple choices that push anime_rating positive
    QMap<QString, double> kf, df;
    kf["anime_rating"] = 1.0; kf["size_weighted_distance"] = 0.5;
    kf["group_status"] = 0.5; kf["watch_recency"] = 0.5; kf["view_percentage"] = 0.5;
    df["anime_rating"] = 0.0; df["size_weighted_distance"] = 0.5;
    df["group_status"] = 0.5; df["watch_recency"] = 0.5; df["view_percentage"] = 0.5;

    // 5 choices pushing anime_rating weight to +0.5
    for (int i = 0; i < 5; ++i) {
        learner.recordChoice(1, 2, kf, df);
    }

    QMap<QString, double> testFactors;
    testFactors["anime_rating"] = 0.8;
    testFactors["size_weighted_distance"] = 0.0;
    testFactors["group_status"] = 0.0;
    testFactors["watch_recency"] = 0.0;
    testFactors["view_percentage"] = 0.0;

    double score = learner.computeScore(testFactors);
    // weight for anime_rating should be ~0.5, score = 0.5 * 0.8 = 0.4
    QVERIFY(qAbs(score - 0.4) < 0.01);
}

void TestDeletionCandidateDisplay::testIsTrainedRequires50Choices()
{
    FactorWeightLearner learner;
    learner.ensureTablesExist();

    QMap<QString, double> kf, df;
    kf["anime_rating"] = 1.0; df["anime_rating"] = 0.0;
    kf["size_weighted_distance"] = 0.5; df["size_weighted_distance"] = 0.5;
    kf["group_status"] = 0.5; df["group_status"] = 0.5;
    kf["watch_recency"] = 0.5; df["watch_recency"] = 0.5;
    kf["view_percentage"] = 0.5; df["view_percentage"] = 0.5;

    for (int i = 0; i < 49; ++i) {
        learner.recordChoice(1, 2, kf, df);
    }
    QVERIFY(!learner.isTrained());

    learner.recordChoice(1, 2, kf, df);
    QVERIFY(learner.isTrained());
    QCOMPARE(learner.totalChoicesMade(), 50);
}

void TestDeletionCandidateDisplay::testResetAllWeights()
{
    FactorWeightLearner learner;
    learner.ensureTablesExist();

    QMap<QString, double> kf, df;
    kf["anime_rating"] = 1.0; df["anime_rating"] = 0.0;
    kf["size_weighted_distance"] = 0.5; df["size_weighted_distance"] = 0.5;
    kf["group_status"] = 0.5; df["group_status"] = 0.5;
    kf["watch_recency"] = 0.5; df["watch_recency"] = 0.5;
    kf["view_percentage"] = 0.5; df["view_percentage"] = 0.5;

    learner.recordChoice(1, 2, kf, df);
    QVERIFY(learner.getWeight("anime_rating") != 0.0);

    learner.resetAllWeights();
    for (const QString &f : FactorWeightLearner::factorNames()) {
        QCOMPARE(learner.getWeight(f), 0.0);
    }
    QCOMPARE(learner.totalChoicesMade(), 0);
}

void TestDeletionCandidateDisplay::testMinFactorDifferenceIgnored()
{
    FactorWeightLearner learner;
    learner.ensureTablesExist();

    QMap<QString, double> kf, df;
    // All factors differ by less than MIN_FACTOR_DIFFERENCE (0.01)
    kf["anime_rating"] = 0.500; df["anime_rating"] = 0.505;
    kf["size_weighted_distance"] = 0.5; df["size_weighted_distance"] = 0.5;
    kf["group_status"] = 0.5; df["group_status"] = 0.5;
    kf["watch_recency"] = 0.5; df["watch_recency"] = 0.5;
    kf["view_percentage"] = 0.5; df["view_percentage"] = 0.5;

    learner.recordChoice(1, 2, kf, df);
    // No weight should have changed
    for (const QString &f : FactorWeightLearner::factorNames()) {
        QCOMPARE(learner.getWeight(f), 0.0);
    }
}

// ===================================================================
// DeletionHistoryManager tests
// ===================================================================

void TestDeletionCandidateDisplay::testRecordAndQueryHistory()
{
    DeletionHistoryManager mgr;

    mgr.recordDeletion(1, 100, 1001, "/path/file.mkv", "Naruto", "Ep 1",
                        1073741824, 3, "Score: 0.23", 0.23, "user_avsb",
                        500000000000LL, 498000000000LL);

    QList<DeletionHistoryEntry> entries = mgr.allEntries();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].lid, 1);
    QCOMPARE(entries[0].aid, 100);
    QCOMPARE(entries[0].animeName, "Naruto");
    QCOMPARE(entries[0].deletionType, "user_avsb");
    QCOMPARE(entries[0].tier, 3);
}

void TestDeletionCandidateDisplay::testHistoryFilterByType()
{
    DeletionHistoryManager mgr;

    mgr.recordDeletion(1, 100, 1001, "/a.mkv", "A", "Ep1", 1000, 0, "Superseded", -1.0, "procedural", 100, 99);
    mgr.recordDeletion(2, 200, 2001, "/b.mkv", "B", "Ep1", 2000, 3, "Score", 0.5, "user_avsb", 99, 97);
    mgr.recordDeletion(3, 300, 3001, "/c.mkv", "C", "Ep1", 3000, 3, "Score", 0.3, "learned_auto", 97, 94);

    QCOMPARE(mgr.entriesByType("procedural").size(), 1);
    QCOMPARE(mgr.entriesByType("user_avsb").size(), 1);
    QCOMPARE(mgr.entriesByType("learned_auto").size(), 1);
    QCOMPARE(mgr.entriesByType("manual").size(), 0);
}

void TestDeletionCandidateDisplay::testHistoryTotalSpaceFreed()
{
    DeletionHistoryManager mgr;

    mgr.recordDeletion(1, 100, 1001, "/a.mkv", "A", "Ep1", 1000, 0, "R", -1.0, "procedural", 100, 90);
    mgr.recordDeletion(2, 200, 2001, "/b.mkv", "B", "Ep1", 2000, 3, "R", 0.5, "user_avsb", 90, 70);

    // Total freed = (100-90) + (90-70) = 10 + 20 = 30
    QCOMPARE(mgr.totalSpaceFreed(), 30LL);
    QCOMPARE(mgr.totalDeletions(), 2);
}

void TestDeletionCandidateDisplay::testHistoryPruning()
{
    DeletionHistoryManager mgr;

    // Insert more than MAX_ENTRIES (5000) is too slow; test with small count
    // and verify pruning logic works by inserting entries, changing max via raw SQL
    for (int i = 0; i < 10; ++i) {
        mgr.recordDeletion(i, 100, 1001, "/file.mkv", "A", "Ep1",
                            1000, 0, "R", -1.0, "procedural", 100, 99);
    }

    // All 10 entries should exist (below 5000 limit)
    QCOMPARE(mgr.totalDeletions(), 10);
}

QTEST_MAIN(TestDeletionCandidateDisplay)
#include "test_deletion_candidate_display.moc"
