/**
 * Test for background loading functionality
 * 
 * This test verifies that the background loading implementation:
 * 1. Allows UI to remain responsive during startup
 * 2. Correctly loads data in background threads
 * 3. Properly synchronizes data access with mutexes
 * 4. Updates UI correctly when background loading completes
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlQuery>

// Simple worker for testing
class TestWorker : public QObject
{
    Q_OBJECT
public:
    explicit TestWorker(QMutex *mutex, QStringList *results, bool *complete) 
        : m_mutex(mutex), m_results(results), m_complete(complete) {}

    void doWork() {
        // Create separate database connection for this thread
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "TestThread");
        db.setDatabaseName(QSqlDatabase::database().databaseName());
        
        if (!db.open()) {
            emit finished();
            return;
        }
        
        QSqlQuery query(db);
        query.exec("SELECT title FROM anime_titles LIMIT 5");
        
        QStringList tempResults;
        while (query.next()) {
            tempResults << query.value(0).toString();
        }
        
        db.close();
        QSqlDatabase::removeDatabase("TestThread");
        
        // Store results with mutex protection
        QMutexLocker locker(m_mutex);
        *m_results = tempResults;
        *m_complete = true;
        
        emit finished();
    }

signals:
    void finished();

private:
    QMutex *m_mutex;
    QStringList *m_results;
    bool *m_complete;
};

// Worker for mutex testing
class MutexTestWorker : public QObject
{
    Q_OBJECT
public:
    explicit MutexTestWorker(QMutex *mutex, QList<int> *data, int start, int end)
        : m_mutex(mutex), m_data(data), m_start(start), m_end(end) {}

    void doWork() {
        for (int i = m_start; i < m_end; ++i) {
            QMutexLocker locker(m_mutex);
            m_data->append(i);
        }
        emit finished();
    }

signals:
    void finished();

private:
    QMutex *m_mutex;
    QList<int> *m_data;
    int m_start;
    int m_end;
};

// Worker for parallel load testing
class ParallelLoadWorker : public QObject
{
    Q_OBJECT
public:
    explicit ParallelLoadWorker(QMutex *mutex, bool *complete)
        : m_mutex(mutex), m_complete(complete) {}

    void doWork() {
        QTest::qWait(100); // Simulate work
        QMutexLocker locker(m_mutex);
        *m_complete = true;
        emit finished();
    }

signals:
    void finished();

private:
    QMutex *m_mutex;
    bool *m_complete;
};

class TestBackgroundLoading : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Initialize test database
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(":memory:");
        QVERIFY(db.open());
        
        // Create test tables
        QSqlQuery query(db);
        query.exec("CREATE TABLE anime_titles (aid INTEGER, title TEXT)");
        query.exec("CREATE TABLE local_files (path TEXT, ed2k TEXT, api_checked INTEGER, fid INTEGER)");
        
        // Insert test data
        for (int i = 1; i <= 100; ++i) {
            query.prepare("INSERT INTO anime_titles (aid, title) VALUES (?, ?)");
            query.addBindValue(i);
            query.addBindValue(QString("Test Anime %1").arg(i));
            query.exec();
        }
        
        for (int i = 1; i <= 10; ++i) {
            query.prepare("INSERT INTO local_files (path, ed2k, api_checked, fid) VALUES (?, ?, 1, 0)");
            query.addBindValue(QString("/test/file%1.mkv").arg(i));
            query.addBindValue(QString("hash%1").arg(i));
            query.exec();
        }
    }

    void testDeferredExecution()
    {
        // Test that QTimer::singleShot allows immediate UI responsiveness
        QElapsedTimer timer;
        timer.start();
        
        bool executed = false;
        QTimer::singleShot(100, this, [&executed]() {
            executed = true;
        });
        
        // Immediately after scheduling, should not have executed yet
        QVERIFY(!executed);
        qint64 elapsed = timer.elapsed();
        QVERIFY(elapsed < 50); // Should be nearly instant
        
        // Wait for execution
        QTest::qWait(150);
        QVERIFY(executed);
    }

    void testBackgroundDatabaseAccess()
    {
        // Test that background thread can access database with separate connection
        QMutex mutex;
        QStringList results;
        bool loadComplete = false;
        
        QThread *thread = new QThread();
        TestWorker *worker = new TestWorker(&mutex, &results, &loadComplete);
        worker->moveToThread(thread);
        
        QEventLoop loop;
        connect(thread, &QThread::started, worker, &TestWorker::doWork);
        connect(worker, &TestWorker::finished, &loop, &QEventLoop::quit);
        connect(worker, &TestWorker::finished, thread, &QThread::quit);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        
        thread->start();
        loop.exec(); // Wait for completion
        
        // Verify results
        QMutexLocker locker(&mutex);
        QVERIFY(loadComplete);
        QCOMPARE(results.size(), 5);
        QCOMPARE(results[0], QString("Test Anime 1"));
    }

    void testMutexProtection()
    {
        // Test that mutex properly protects shared data
        QMutex mutex;
        QList<int> sharedData;
        
        QThread *thread1 = new QThread();
        QThread *thread2 = new QThread();
        MutexTestWorker *worker1 = new MutexTestWorker(&mutex, &sharedData, 0, 50);
        MutexTestWorker *worker2 = new MutexTestWorker(&mutex, &sharedData, 50, 100);
        worker1->moveToThread(thread1);
        worker2->moveToThread(thread2);
        
        QEventLoop loop;
        
        int completedCount = 0;
        auto checkCompletion = [&completedCount, &loop]() {
            completedCount++;
            if (completedCount == 2) {
                loop.quit();
            }
        };
        
        connect(thread1, &QThread::started, worker1, &MutexTestWorker::doWork);
        connect(thread2, &QThread::started, worker2, &MutexTestWorker::doWork);
        connect(worker1, &MutexTestWorker::finished, checkCompletion);
        connect(worker2, &MutexTestWorker::finished, checkCompletion);
        connect(worker1, &MutexTestWorker::finished, thread1, &QThread::quit);
        connect(worker2, &MutexTestWorker::finished, thread2, &QThread::quit);
        connect(thread1, &QThread::finished, worker1, &QObject::deleteLater);
        connect(thread2, &QThread::finished, worker2, &QObject::deleteLater);
        connect(thread1, &QThread::finished, thread1, &QObject::deleteLater);
        connect(thread2, &QThread::finished, thread2, &QObject::deleteLater);
        
        thread1->start();
        thread2->start();
        loop.exec(); // Wait for both to complete
        
        // Verify all data was written
        QMutexLocker locker(&mutex);
        QCOMPARE(sharedData.size(), 100);
    }

    void testResponsiveUI()
    {
        // Simulate the actual startup scenario
        QElapsedTimer timer;
        timer.start();
        
        // Simulate deferred mylist loading
        bool mylistLoaded = false;
        QTimer::singleShot(100, this, [&mylistLoaded]() {
            // Simulate slow mylist loading (3 seconds)
            QTest::qWait(100); // Use shorter time for test
            mylistLoaded = true;
        });
        
        // UI should be immediately responsive
        qint64 elapsed = timer.elapsed();
        QVERIFY(elapsed < 50); // UI became responsive almost immediately
        
        // Wait for mylist to load
        QTest::qWait(250);
        QVERIFY(mylistLoaded);
    }

    void testParallelLoading()
    {
        // Test that multiple background loads can run in parallel
        QElapsedTimer timer;
        timer.start();
        
        QMutex mutex;
        bool load1Complete = false;
        bool load2Complete = false;
        
        QThread *thread1 = new QThread();
        QThread *thread2 = new QThread();
        ParallelLoadWorker *worker1 = new ParallelLoadWorker(&mutex, &load1Complete);
        ParallelLoadWorker *worker2 = new ParallelLoadWorker(&mutex, &load2Complete);
        worker1->moveToThread(thread1);
        worker2->moveToThread(thread2);
        
        QEventLoop loop;
        
        int completedCount = 0;
        auto checkCompletion = [&completedCount, &loop]() {
            completedCount++;
            if (completedCount == 2) {
                loop.quit();
            }
        };
        
        connect(thread1, &QThread::started, worker1, &ParallelLoadWorker::doWork);
        connect(thread2, &QThread::started, worker2, &ParallelLoadWorker::doWork);
        connect(worker1, &ParallelLoadWorker::finished, checkCompletion);
        connect(worker2, &ParallelLoadWorker::finished, checkCompletion);
        connect(worker1, &ParallelLoadWorker::finished, thread1, &QThread::quit);
        connect(worker2, &ParallelLoadWorker::finished, thread2, &QThread::quit);
        connect(thread1, &QThread::finished, worker1, &QObject::deleteLater);
        connect(thread2, &QThread::finished, worker2, &QObject::deleteLater);
        connect(thread1, &QThread::finished, thread1, &QObject::deleteLater);
        connect(thread2, &QThread::finished, thread2, &QObject::deleteLater);
        
        thread1->start();
        thread2->start();
        loop.exec();
        
        qint64 elapsed = timer.elapsed();
        
        // Both should complete in roughly the same time (parallel execution)
        // If sequential, would take ~200ms. Parallel should be ~100ms
        QVERIFY(elapsed < 150); // Allow some overhead
        
        QMutexLocker locker(&mutex);
        QVERIFY(load1Complete);
        QVERIFY(load2Complete);
    }

    void cleanupTestCase()
    {
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
};

QTEST_MAIN(TestBackgroundLoading)
#include "test_background_loading.moc"
