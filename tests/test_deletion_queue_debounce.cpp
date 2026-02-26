#include <QTest>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "../usagi/src/deletionqueue.h"
#include "../usagi/src/hybriddeletionclassifier.h"
#include "../usagi/src/deletionlockmanager.h"
#include "../usagi/src/factorweightlearner.h"
#include "../usagi/src/watchsessionmanager.h"

/**
 * Tests that DeletionQueue::scheduleRebuild() debounces rapid calls
 * into a single rebuild, while rebuild() fires immediately.
 */
class TestDeletionQueueDebounce : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void testScheduleRebuildCoalesces();
    void testRebuildFiresImmediately();
    void testRebuildCancelsPendingSchedule();
    void testDebounceConstant();

private:
    void setupDatabase();
    void teardownDatabase();
};

// ---------------------------------------------------------------------------
// Setup / Teardown
// ---------------------------------------------------------------------------

void TestDeletionQueueDebounce::setupDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());

    QSqlQuery q(db);
    q.exec("CREATE TABLE IF NOT EXISTS mylist ("
           "lid INTEGER PRIMARY KEY, state INTEGER DEFAULT 0, "
           "local_file INTEGER)");
    q.exec("CREATE TABLE IF NOT EXISTS local_files ("
           "id INTEGER PRIMARY KEY, path TEXT)");
    q.exec("CREATE TABLE IF NOT EXISTS deletion_locks ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "lock_type TEXT NOT NULL, target_id INTEGER NOT NULL, "
           "UNIQUE(lock_type, target_id))");
    q.exec("CREATE TABLE IF NOT EXISTS deletion_factor_weights ("
           "factor TEXT PRIMARY KEY, weight REAL DEFAULT 0.0)");
    q.exec("CREATE TABLE IF NOT EXISTS deletion_choices ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "kept_lid INTEGER, deleted_lid INTEGER, timestamp TEXT)");
}

void TestDeletionQueueDebounce::teardownDatabase()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void TestDeletionQueueDebounce::initTestCase()
{
    setupDatabase();
}

void TestDeletionQueueDebounce::cleanupTestCase()
{
    teardownDatabase();
}

void TestDeletionQueueDebounce::cleanup()
{
    QSqlQuery q(QSqlDatabase::database());
    q.exec("DELETE FROM mylist");
    q.exec("DELETE FROM local_files");
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void TestDeletionQueueDebounce::testScheduleRebuildCoalesces()
{
    DeletionLockManager lockManager;
    FactorWeightLearner learner;
    WatchSessionManager sessionManager;
    HybridDeletionClassifier classifier(lockManager, learner, sessionManager);
    DeletionQueue queue(classifier, lockManager, learner);

    QSignalSpy spy(&queue, &DeletionQueue::queueRebuilt);
    QVERIFY(spy.isValid());

    // Call scheduleRebuild many times rapidly
    for (int i = 0; i < 20; ++i) {
        queue.scheduleRebuild();
    }

    // No immediate signal — rebuild is deferred
    QCOMPARE(spy.count(), 0);

    // Wait for the debounce timer to fire
    QVERIFY(spy.wait(DeletionQueue::DEBOUNCE_MS + 500));

    // Only a single rebuild should have occurred
    QCOMPARE(spy.count(), 1);
}

void TestDeletionQueueDebounce::testRebuildFiresImmediately()
{
    DeletionLockManager lockManager;
    FactorWeightLearner learner;
    WatchSessionManager sessionManager;
    HybridDeletionClassifier classifier(lockManager, learner, sessionManager);
    DeletionQueue queue(classifier, lockManager, learner);

    QSignalSpy spy(&queue, &DeletionQueue::queueRebuilt);
    QVERIFY(spy.isValid());

    // Direct rebuild() fires synchronously
    queue.rebuild();

    QCOMPARE(spy.count(), 1);
}

void TestDeletionQueueDebounce::testRebuildCancelsPendingSchedule()
{
    DeletionLockManager lockManager;
    FactorWeightLearner learner;
    WatchSessionManager sessionManager;
    HybridDeletionClassifier classifier(lockManager, learner, sessionManager);
    DeletionQueue queue(classifier, lockManager, learner);

    QSignalSpy spy(&queue, &DeletionQueue::queueRebuilt);
    QVERIFY(spy.isValid());

    // Schedule a deferred rebuild
    queue.scheduleRebuild();
    QCOMPARE(spy.count(), 0);

    // Immediate rebuild should cancel the pending timer
    queue.rebuild();
    QCOMPARE(spy.count(), 1);

    // Wait past the original debounce — no second rebuild should fire
    QTest::qWait(DeletionQueue::DEBOUNCE_MS + 200);
    QCOMPARE(spy.count(), 1);
}

void TestDeletionQueueDebounce::testDebounceConstant()
{
    QVERIFY(DeletionQueue::DEBOUNCE_MS > 0);
    QVERIFY(DeletionQueue::DEBOUNCE_MS <= 2000);
}

QTEST_MAIN(TestDeletionQueueDebounce)
#include "test_deletion_queue_debounce.moc"
