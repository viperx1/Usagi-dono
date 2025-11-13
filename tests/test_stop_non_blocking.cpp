#include <QtTest/QtTest>
#include <QThread>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QElapsedTimer>
#include "../usagi/src/hasherthreadpool.h"
#include "../usagi/src/anidbapi.h"
#include "../usagi/src/main.h"

// Global adbapi used by HasherThread
myAniDBApi *adbapi = nullptr;

/**
 * Test to verify that stopping the hasher thread pool does not block indefinitely.
 * This simulates the fix for the UI freeze issue when clicking the stop button.
 */
class TestStopNonBlocking : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testStopReturnsQuickly();
    void testStopWithBroadcastReturnsQuickly();
};

void TestStopNonBlocking::initTestCase()
{
    // Register Qt::HANDLE as a metatype so QSignalSpy can handle it
    qRegisterMetaType<Qt::HANDLE>("Qt::HANDLE");
    
    // Set environment variable to signal test mode before any Qt network operations
    qputenv("USAGI_TEST_MODE", "1");
    
    // Initialize the database for testing
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());
    
    // Create necessary tables
    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS local_files ("
               "path TEXT PRIMARY KEY, "
               "filename TEXT, "
               "ed2k_hash TEXT, "
               "status INTEGER)");
    
    // Initialize the global adbapi object
    adbapi = new myAniDBApi("test", 1);
}

void TestStopNonBlocking::cleanupTestCase()
{
    delete adbapi;
    adbapi = nullptr;
    QSqlDatabase::database().close();
}

void TestStopNonBlocking::testStopReturnsQuickly()
{
    // Create large temporary files that would take a while to hash
    QVector<QTemporaryFile*> tempFiles;
    QVector<QString> filePaths;
    
    for (int i = 0; i < 3; ++i)
    {
        QTemporaryFile *tempFile = new QTemporaryFile();
        QVERIFY(tempFile->open());
        QByteArray data(50 * 1024 * 1024, 'A' + i); // 50MB each - large files
        tempFile->write(data);
        tempFile->close();
        filePaths.append(tempFile->fileName());
        tempFiles.append(tempFile);
    }
    
    // Create a thread pool with 2 threads
    HasherThreadPool pool(2);
    
    // Set up signal spy to track when threads finish
    QSignalSpy finishedSpy(&pool, &HasherThreadPool::finished);
    
    // Start the pool
    pool.start();
    
    // Wait for threads to start
    QTest::qWait(500);
    
    // Add files to the pool
    for (const QString &filePath : filePaths)
    {
        pool.addFile(filePath);
        QTest::qWait(50);
    }
    
    // Give threads a moment to start hashing
    QTest::qWait(200);
    
    // Measure how long stop() takes WITHOUT wait()
    // This simulates the fixed button click handler behavior
    QElapsedTimer timer;
    timer.start();
    
    // Call stop but DON'T call wait() - this is the fix
    pool.stop();
    
    qint64 stopTime = timer.elapsed();
    
    // Verify that stop() returns quickly (should be nearly instantaneous)
    // We allow up to 100ms for the call to complete (very generous)
    QVERIFY2(stopTime < 100, 
             QString("stop() took %1ms - should be nearly instantaneous").arg(stopTime).toUtf8());
    
    // Now wait for threads to actually finish (this can take time, but we're not blocking the UI)
    // Wait up to 10 seconds for the finished signal
    QVERIFY2(finishedSpy.wait(10000), "Threads should finish within 10 seconds after stop");
    
    // Clean up temp files
    for (QTemporaryFile *tempFile : tempFiles)
    {
        delete tempFile;
    }
}

void TestStopNonBlocking::testStopWithBroadcastReturnsQuickly()
{
    // Create large temporary files that would take a while to hash
    QVector<QTemporaryFile*> tempFiles;
    QVector<QString> filePaths;
    
    for (int i = 0; i < 3; ++i)
    {
        QTemporaryFile *tempFile = new QTemporaryFile();
        QVERIFY(tempFile->open());
        QByteArray data(50 * 1024 * 1024, 'B' + i); // 50MB each
        tempFile->write(data);
        tempFile->close();
        filePaths.append(tempFile->fileName());
        tempFiles.append(tempFile);
    }
    
    // Create a thread pool with 2 threads
    HasherThreadPool pool(2);
    
    // Set up signal spy to track when threads finish
    QSignalSpy finishedSpy(&pool, &HasherThreadPool::finished);
    
    // Start the pool
    pool.start();
    
    // Wait for threads to start
    QTest::qWait(500);
    
    // Add files to the pool
    for (const QString &filePath : filePaths)
    {
        pool.addFile(filePath);
        QTest::qWait(50);
    }
    
    // Give threads a moment to start hashing
    QTest::qWait(200);
    
    // Measure how long the stop sequence takes WITHOUT wait()
    // This simulates the exact fixed button click handler behavior
    QElapsedTimer timer;
    timer.start();
    
    // 1. Broadcast stop to interrupt hashing operations
    pool.broadcastStopHasher();
    
    // 2. Signal threads to stop
    pool.stop();
    
    qint64 stopTime = timer.elapsed();
    
    // Verify that the stop sequence returns quickly
    // Even with broadcastStopHasher, this should be nearly instantaneous
    QVERIFY2(stopTime < 100,
             QString("stop sequence took %1ms - should be nearly instantaneous").arg(stopTime).toUtf8());
    
    // Now wait for threads to actually finish
    // With broadcastStopHasher, threads should finish faster (but still may take some time)
    QVERIFY2(finishedSpy.wait(5000), "Threads should finish within 5 seconds after broadcast stop");
    
    // Clean up temp files
    for (QTemporaryFile *tempFile : tempFiles)
    {
        delete tempFile;
    }
}

QTEST_MAIN(TestStopNonBlocking)
#include "test_stop_non_blocking.moc"
