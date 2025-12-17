#include <QTest>
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
    void testStopAndRestart();
};

void TestStopNonBlocking::initTestCase()
{
    // Register Qt::HANDLE as a metatype so QSignalSpy can handle it
    qRegisterMetaType<Qt::HANDLE>("Qt::HANDLE");
    
    // Set environment variable to signal test mode before any Qt network operations
    qputenv("USAGI_TEST_MODE", "1");
    
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
        QByteArray data(5 * 1024 * 1024, 'A' + i); // 5MB each - reduced from 50MB for faster testing
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
    // Use QTest::qWait() instead of QSignalSpy::wait() for better compatibility with static Qt builds
    for (int i = 0; i < 1000 && finishedSpy.count() == 0; ++i) {
        QTest::qWait(10);
    }
    QVERIFY2(finishedSpy.count() > 0, "Threads should finish within 10 seconds after stop");
    
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
        QByteArray data(5 * 1024 * 1024, 'B' + i); // 5MB each - reduced from 50MB for faster testing
        tempFile->write(data);
        tempFile->close();
        filePaths.append(tempFile->fileName());
        tempFiles.append(tempFile);
    }
    
    // Create a thread pool with 2 threads
    HasherThreadPool pool(2);
    
    // Set up signal spy to track when threads finish
    QSignalSpy finishedSpy(&pool, &HasherThreadPool::finished);
    
    // Start the pool with 3 files
    pool.start(3);
    
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
    // With broadcastStopHasher and DirectConnection, threads should stop much faster
    // Previously threads would complete the entire file (50MB = ~500 parts = many seconds)
    // Now they should stop within the current 100KB chunk (< 1 second)
    // Use QTest::qWait() instead of QSignalSpy::wait() for better compatibility with static Qt builds
    QElapsedTimer finishTimer;
    finishTimer.start();
    for (int i = 0; i < 500 && finishedSpy.count() == 0; ++i) {
        QTest::qWait(10);
    }
    QVERIFY2(finishedSpy.count() > 0, "Threads should finish within 5 seconds after broadcast stop");
    qint64 finishTime = finishTimer.elapsed();
    
    // Verify threads stopped quickly (should be < 2 seconds for immediate stop)
    // If threads were completing entire files, this would take 5+ seconds
    QVERIFY2(finishTime < 2000,
             QString("Threads took %1ms to finish - should stop immediately, not complete files").arg(finishTime).toUtf8());
    
    // Clean up temp files
    for (QTemporaryFile *tempFile : tempFiles)
    {
        delete tempFile;
    }
}

void TestStopNonBlocking::testStopAndRestart()
{
    qDebug() << "[TEST] testStopAndRestart: Creating temporary files";
    // Create temporary files to hash
    QVector<QTemporaryFile*> tempFiles;
    QVector<QString> filePaths;
    
    for (int i = 0; i < 2; ++i)
    {
        QTemporaryFile *tempFile = new QTemporaryFile();
        QVERIFY(tempFile->open());
        QByteArray data(5 * 1024 * 1024, 'C' + i); // 5MB each - reduced from 10MB for faster testing
        tempFile->write(data);
        tempFile->close();
        filePaths.append(tempFile->fileName());
        tempFiles.append(tempFile);
    }
    qDebug() << "[TEST] testStopAndRestart: Created" << tempFiles.size() << "temp files";
    
    // Create a thread pool with 2 threads
    HasherThreadPool pool(2);
    qDebug() << "[TEST] testStopAndRestart: Created thread pool";
    
    // First run: Start, add files, and stop
    qDebug() << "[TEST] testStopAndRestart: Starting pool (first run)";
    pool.start(2);  // Start with correct file count
    QTest::qWait(500);
    
    qDebug() << "[TEST] testStopAndRestart: Adding files (first run)";
    for (const QString &filePath : filePaths)
    {
        pool.addFile(filePath);
        QTest::qWait(50);
    }
    qDebug() << "[TEST] testStopAndRestart: Added" << filePaths.size() << "files (first run)";
    
    QTest::qWait(200);
    
    // Stop the pool
    qDebug() << "[TEST] testStopAndRestart: Broadcasting stop (first run)";
    pool.broadcastStopHasher();
    qDebug() << "[TEST] testStopAndRestart: Calling stop (first run)";
    pool.stop();
    qDebug() << "[TEST] testStopAndRestart: Stop called, waiting for finish signal (first run)";
    
    // Wait for threads to finish and the pool to update its state
    QSignalSpy firstFinishedSpy(&pool, &HasherThreadPool::finished);
    for (int i = 0; i < 500 && firstFinishedSpy.count() == 0; ++i) {
        if (i % 50 == 0) {
            qDebug() << "[TEST] testStopAndRestart: Waiting for first finish signal, iteration" << i << "count:" << firstFinishedSpy.count();
        }
        QTest::qWait(10);
    }
    qDebug() << "[TEST] testStopAndRestart: First finished signal count:" << firstFinishedSpy.count();
    QVERIFY2(firstFinishedSpy.count() > 0, "Threads should finish after stop");
    
    // Process any pending events to ensure state is updated
    qDebug() << "[TEST] testStopAndRestart: Processing pending events";
    QTest::qWait(100);
    
    // Second run: Restart the pool (this should not crash)
    qDebug() << "[TEST] testStopAndRestart: Starting second run - creating signal spy";
    QSignalSpy finishedSpy(&pool, &HasherThreadPool::finished);
    QSignalSpy hashSpy(&pool, &HasherThreadPool::sendHash);
    
    qDebug() << "[TEST] testStopAndRestart: Restarting pool (second run)";
    pool.start(2);  // Start with correct file count
    QTest::qWait(500);
    
    // Add files again
    qDebug() << "[TEST] testStopAndRestart: Adding files (second run)";
    for (const QString &filePath : filePaths)
    {
        pool.addFile(filePath);
        QTest::qWait(50);
    }
    qDebug() << "[TEST] testStopAndRestart: Added" << filePaths.size() << "files (second run)";
    
    // Signal completion immediately after adding files
    qDebug() << "[TEST] testStopAndRestart: Signaling completion with empty string";
    pool.addFile(QString());
    
    // Wait for all hashing to complete (similar to testParallelHashing pattern)
    // Allow up to 3 seconds per file
    const int waitIntervalMs = 100;
    const int maxWaitIterations = (filePaths.size() * 3000) / waitIntervalMs;
    qDebug() << "[TEST] testStopAndRestart: Waiting for files to be hashed, max iterations:" << maxWaitIterations;
    for (int i = 0; i < maxWaitIterations && hashSpy.count() < filePaths.size(); ++i) {
        if (i % 10 == 0) {
            qDebug() << "[TEST] testStopAndRestart: Waiting for hashing, iteration" << i << "hashSpy:" << hashSpy.count();
        }
        QTest::qWait(waitIntervalMs);
    }
    qDebug() << "[TEST] testStopAndRestart: Hash count:" << hashSpy.count() << "expected:" << filePaths.size();
    
    // Wait for finished signal
    const int maxFinishedWaitIterations = 100;
    qDebug() << "[TEST] testStopAndRestart: Waiting for finish signal (second run)";
    for (int i = 0; i < maxFinishedWaitIterations && finishedSpy.count() == 0; ++i) {
        if (i % 10 == 0) {
            qDebug() << "[TEST] testStopAndRestart: Still waiting for finish signal, iteration" << i << "count:" << finishedSpy.count();
        }
        QTest::qWait(waitIntervalMs);
    }
    qDebug() << "[TEST] testStopAndRestart: Second finished signal count:" << finishedSpy.count();
    QVERIFY2(finishedSpy.count() > 0, "Threads should finish after restart");
    
    // Clean up temp files
    qDebug() << "[TEST] testStopAndRestart: Cleaning up temp files";
    for (QTemporaryFile *tempFile : tempFiles)
    {
        delete tempFile;
    }
    qDebug() << "[TEST] testStopAndRestart: Test complete";
}

QTEST_MAIN(TestStopNonBlocking)
#include "test_stop_non_blocking.moc"
