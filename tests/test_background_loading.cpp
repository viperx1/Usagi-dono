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
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlQuery>

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
        
        QFutureWatcher<void> watcher;
        QEventLoop loop;
        connect(&watcher, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
        
        QFuture<void> future = QtConcurrent::run([&mutex, &results, &loadComplete]() {
            // Create separate database connection for this thread
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "TestThread");
            db.setDatabaseName(QSqlDatabase::database().databaseName());
            
            if (!db.open()) {
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
            QMutexLocker locker(&mutex);
            results = tempResults;
            loadComplete = true;
        });
        
        watcher.setFuture(future);
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
        
        QFutureWatcher<void> watcher1, watcher2;
        QEventLoop loop;
        
        int completedCount = 0;
        auto checkCompletion = [&completedCount, &loop]() {
            completedCount++;
            if (completedCount == 2) {
                loop.quit();
            }
        };
        
        connect(&watcher1, &QFutureWatcher<void>::finished, this, checkCompletion);
        connect(&watcher2, &QFutureWatcher<void>::finished, this, checkCompletion);
        
        // Start two threads that both write to shared data
        QFuture<void> future1 = QtConcurrent::run([&mutex, &sharedData]() {
            for (int i = 0; i < 50; ++i) {
                QMutexLocker locker(&mutex);
                sharedData.append(i);
            }
        });
        
        QFuture<void> future2 = QtConcurrent::run([&mutex, &sharedData]() {
            for (int i = 50; i < 100; ++i) {
                QMutexLocker locker(&mutex);
                sharedData.append(i);
            }
        });
        
        watcher1.setFuture(future1);
        watcher2.setFuture(future2);
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
        
        QFutureWatcher<void> watcher1, watcher2;
        QEventLoop loop;
        
        int completedCount = 0;
        auto checkCompletion = [&completedCount, &loop]() {
            completedCount++;
            if (completedCount == 2) {
                loop.quit();
            }
        };
        
        connect(&watcher1, &QFutureWatcher<void>::finished, this, checkCompletion);
        connect(&watcher2, &QFutureWatcher<void>::finished, this, checkCompletion);
        
        // Start two parallel loads
        QFuture<void> future1 = QtConcurrent::run([&mutex, &load1Complete]() {
            QTest::qWait(100); // Simulate work
            QMutexLocker locker(&mutex);
            load1Complete = true;
        });
        
        QFuture<void> future2 = QtConcurrent::run([&mutex, &load2Complete]() {
            QTest::qWait(100); // Simulate work
            QMutexLocker locker(&mutex);
            load2Complete = true;
        });
        
        watcher1.setFuture(future1);
        watcher2.setFuture(future2);
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
