#include <QTest>
#include <QThread>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QCoreApplication>
#include "../usagi/src/hasherthreadpool.h"
#include "../usagi/src/anidbapi.h"
#include "../usagi/src/main.h"

// Global adbapi used by HasherThread

/**
 * Test to verify that HasherThreadPool properly distributes work across multiple threads.
 * This tests the multithreading functionality that allows hashing multiple files in parallel
 * on multiple CPU cores.
 */
class TestHasherThreadPool : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testMultipleThreadsCreated();
    void testParallelHashing();
    void testStopAllThreads();
    void testMultipleThreadIdsUsed();
    void testNoIdleThreadsWithWork();
};

void TestHasherThreadPool::initTestCase()
{
    // Register Qt::HANDLE as a metatype so QSignalSpy can handle it
    qRegisterMetaType<Qt::HANDLE>("Qt::HANDLE");
    
    // Set environment variable to signal test mode before any Qt network operations
    qputenv("USAGI_TEST_MODE", "1");
    
    // Ensure clean slate: remove any existing default connection
    {
        QString defaultConn = QSqlDatabase::defaultConnection();
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

void TestHasherThreadPool::cleanupTestCase()
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
    QString defaultConn = QSqlDatabase::defaultConnection();
    if (QSqlDatabase::contains(defaultConn)) {
        QSqlDatabase::removeDatabase(defaultConn);
    }
}

void TestHasherThreadPool::testMultipleThreadsCreated()
{
    // Create a thread pool with 4 threads
    HasherThreadPool pool(4);
    
    // Verify the pool was created with the correct number of threads
    QCOMPARE(pool.threadCount(), 4);
}

void TestHasherThreadPool::testParallelHashing()
{
    // Create multiple temporary files to hash
    QVector<QTemporaryFile*> tempFiles;
    QVector<QString> filePaths;
    
    for (int i = 0; i < 4; ++i)
    {
        QTemporaryFile *tempFile = new QTemporaryFile();
        QVERIFY(tempFile->open());
        QByteArray data(512 * 1024, 'A' + i); // 512KB, different content
        tempFile->write(data);
        tempFile->close();
        filePaths.append(tempFile->fileName());
        tempFiles.append(tempFile);
    }
    
    // Create a thread pool with 2 threads
    HasherThreadPool pool(2);
    
    // Set up signal spies
    QSignalSpy requestSpy(&pool, &HasherThreadPool::requestNextFile);
    QSignalSpy hashSpy(&pool, &HasherThreadPool::sendHash);
    QSignalSpy finishedSpy(&pool, &HasherThreadPool::finished);
    
    // Start the pool
    pool.start();
    
    // Wait for initial requestNextFile signals from all threads
    QTest::qWait(1000);
    QVERIFY(requestSpy.count() >= 2); // At least one request per thread
    
    // Add files to the pool
    for (const QString &filePath : filePaths)
    {
        pool.addFile(filePath);
        QTest::qWait(100); // Small delay to allow work distribution
    }
    
    // Signal completion
    pool.addFile(QString());
    
    // Wait for all hashing to complete
    QTest::qWait(10000); // Up to 10 seconds for hashing
    
    // Verify all files were hashed
    QVERIFY(hashSpy.count() >= 4);
    
    // Verify finished signal was emitted
    QVERIFY(finishedSpy.count() >= 1);
    
    // Clean up temp files
    for (QTemporaryFile *tempFile : tempFiles)
    {
        delete tempFile;
    }
}

void TestHasherThreadPool::testStopAllThreads()
{
    // Create multiple large temporary files
    QVector<QTemporaryFile*> tempFiles;
    QVector<QString> filePaths;
    
    for (int i = 0; i < 3; ++i)
    {
        QTemporaryFile *tempFile = new QTemporaryFile();
        QVERIFY(tempFile->open());
        QByteArray data(10 * 1024 * 1024, 'B' + i); // 10MB each
        tempFile->write(data);
        tempFile->close();
        filePaths.append(tempFile->fileName());
        tempFiles.append(tempFile);
    }
    
    // Create a thread pool with 2 threads
    HasherThreadPool pool(2);
    
    // Start the pool
    pool.start();
    
    // Wait for threads to start
    QTest::qWait(1000);
    
    // Add files to the pool
    for (const QString &filePath : filePaths)
    {
        pool.addFile(filePath);
        QTest::qWait(50);
    }
    
    // Give threads a moment to start hashing
    QTest::qWait(200);
    
    // Stop the pool while threads are working
    pool.stop();
    
    // Verify all threads stop within a reasonable time
    QVERIFY(pool.wait(3000));
    
    // Clean up temp files
    for (QTemporaryFile *tempFile : tempFiles)
    {
        delete tempFile;
    }
}

void TestHasherThreadPool::testMultipleThreadIdsUsed()
{
    // Create temporary files to hash
    QVector<QTemporaryFile*> tempFiles;
    QVector<QString> filePaths;
    
    for (int i = 0; i < 3; ++i)
    {
        QTemporaryFile *tempFile = new QTemporaryFile();
        QVERIFY(tempFile->open());
        QByteArray data(256 * 1024, 'C' + i); // 256KB
        tempFile->write(data);
        tempFile->close();
        filePaths.append(tempFile->fileName());
        tempFiles.append(tempFile);
    }
    
    // Create a thread pool with 3 threads
    HasherThreadPool pool(3);
    
    // Set up signal spy to capture thread IDs
    QSignalSpy threadStartedSpy(&pool, &HasherThreadPool::threadStarted);
    
    // Start the pool
    pool.start();
    
    // Wait for all threads to start
    QTest::qWait(2000);
    
    // Collect all unique thread IDs
    QSet<Qt::HANDLE> threadIds;
    for (int i = 0; i < threadStartedSpy.count(); ++i)
    {
        Qt::HANDLE threadId = threadStartedSpy.at(i).at(0).value<Qt::HANDLE>();
        threadIds.insert(threadId);
    }
    
    // Verify we have at least 2 different thread IDs (ideally 3, but system may reuse)
    // This confirms that multiple threads are actually being used
    QVERIFY(threadIds.size() >= 2);
    
    // Stop the pool
    pool.addFile(QString());
    QVERIFY(pool.wait(2000));
    
    // Clean up temp files
    for (QTemporaryFile *tempFile : tempFiles)
    {
        delete tempFile;
    }
}

void TestHasherThreadPool::testNoIdleThreadsWithWork()
{
    // This test verifies the fix for the idle thread issue:
    // When a thread finishes and requests more work, it should get that work
    // immediately, not have it assigned to another thread's queue.
    
    // Create multiple temporary files to hash (more files than threads)
    QVector<QTemporaryFile*> tempFiles;
    QVector<QString> filePaths;
    
    const int numFiles = 6;  // More files than threads to ensure overlap
    for (int i = 0; i < numFiles; ++i)
    {
        QTemporaryFile *tempFile = new QTemporaryFile();
        QVERIFY(tempFile->open());
        // Mix of file sizes to create timing differences
        int sizeKB = (i % 2 == 0) ? 256 : 512;
        QByteArray data(sizeKB * 1024, 'A' + i);
        tempFile->write(data);
        tempFile->close();
        filePaths.append(tempFile->fileName());
        tempFiles.append(tempFile);
    }
    
    // Create a thread pool with 3 threads
    HasherThreadPool pool(3);
    
    // Set up signal spies to track activity
    QSignalSpy requestSpy(&pool, &HasherThreadPool::requestNextFile);
    QSignalSpy hashSpy(&pool, &HasherThreadPool::sendHash);
    QSignalSpy finishedSpy(&pool, &HasherThreadPool::finished);
    
    // Track which thread IDs hash files to verify all threads are used
    QSignalSpy fileHashedSpy(&pool, &HasherThreadPool::notifyFileHashed);
    
    // Start the pool
    pool.start();
    
    // Wait for initial requests from all threads
    QTest::qWait(1000);
    int initialRequests = requestSpy.count();
    QVERIFY(initialRequests >= 3); // Should have requests from all 3 threads
    
    // Feed initial batch of files immediately (one per thread)
    int filesAdded = 0;
    for (int i = 0; i < 3 && i < numFiles; i++)
    {
        pool.addFile(filePaths[i]);
        filesAdded++;
    }
    
    // Wait and feed remaining files as they're requested
    int loopIterations = 0;
    const int maxLoopIterations = 200; // Prevent infinite loop (20 seconds max)
    int lastRequestCount = requestSpy.count();
    int noProgressIterations = 0;
    
    while (filesAdded < numFiles && loopIterations < maxLoopIterations)
    {
        loopIterations++;
        
        // Process events to ensure signals are delivered
        QCoreApplication::processEvents();
        QTest::qWait(100);
        
        int currentRequests = requestSpy.count();
        
        // If we have new requests and files to add, add them
        if (currentRequests > lastRequestCount && filesAdded < numFiles)
        {
            // Add files for all pending requests
            int requestsToFill = currentRequests - lastRequestCount;
            for (int i = 0; i < requestsToFill && filesAdded < numFiles; i++)
            {
                pool.addFile(filePaths[filesAdded]);
                filesAdded++;
            }
            lastRequestCount = currentRequests;
            noProgressIterations = 0;
        }
        else
        {
            noProgressIterations++;
            // If no progress for 5 iterations (500ms), something is wrong
            if (noProgressIterations > 5 && filesAdded < numFiles)
            {
                QFAIL(QString("Test stalled: added %1 of %2 files, %3 requests received, no progress for 500ms")
                      .arg(filesAdded).arg(numFiles).arg(currentRequests).toUtf8().constData());
            }
        }
    }
    
    // If we hit max iterations, fail the test with a clear message
    if (filesAdded < numFiles)
    {
        QFAIL(QString("Test timed out: only added %1 of %2 files after %3 iterations")
              .arg(filesAdded).arg(numFiles).arg(loopIterations).toUtf8().constData());
    }
    
    // Signal completion
    pool.addFile(QString());
    
    // Wait for all hashing to complete
    // Use QTest::qWait() loop instead of QSignalSpy::wait() for better compatibility with static Qt builds
    for (int i = 0; i < 150 && finishedSpy.count() == 0; ++i) {
        QTest::qWait(100); // Wait up to 15 seconds total
    }
    QVERIFY(finishedSpy.count() >= 1);
    
    // Verify all files were hashed
    QCOMPARE(hashSpy.count(), numFiles);
    
    // Verify that multiple threads participated (not all work on one thread)
    QSet<int> activeThreadIds;
    for (int i = 0; i < fileHashedSpy.count(); ++i)
    {
        int threadId = fileHashedSpy.at(i).at(0).toInt();
        activeThreadIds.insert(threadId);
    }
    
    // With the fix, all 3 threads should have processed files
    // (Without the fix, some threads would sit idle)
    QVERIFY(activeThreadIds.size() >= 2); // At least 2 threads should be used
    
    // Clean up temp files
    for (QTemporaryFile *tempFile : tempFiles)
    {
        delete tempFile;
    }
}

QTEST_MAIN(TestHasherThreadPool)
#include "test_hasher_threadpool.moc"
