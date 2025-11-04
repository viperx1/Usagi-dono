#include <QtTest/QtTest>
#include <QThread>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "../usagi/src/hasherthread.h"
#include "../usagi/src/anidbapi.h"
#include "../usagi/src/main.h"

// Global adbapi used by HasherThread
myAniDBApi *adbapi = nullptr;

/**
 * Test to verify that HasherThread actually runs hashing in a separate thread.
 * This addresses the issue where the previous implementation used exec() and slots,
 * which caused the hashing to run in the main thread, freezing the UI.
 */
class TestHasherThread : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testHashingRunsInSeparateThread();
    void testStopInterruptsHashing();
    void testResumeAfterStop();
};

void TestHasherThread::initTestCase()
{
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

void TestHasherThread::cleanupTestCase()
{
    delete adbapi;
    adbapi = nullptr;
    QSqlDatabase::database().close();
}

void TestHasherThread::testHashingRunsInSeparateThread()
{
    // Create a temporary file to hash
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray data(1024 * 1024, 'A'); // 1MB
    tempFile.write(data);
    tempFile.close();
    QString filePath = tempFile.fileName();
    
    // Create HasherThread
    HasherThread hasherThread;
    
    // Set up signal spy to capture when hashing completes
    QSignalSpy requestSpy(&hasherThread, &HasherThread::requestNextFile);
    QSignalSpy hashSpy(&hasherThread, &HasherThread::sendHash);
    
    // Start the thread
    hasherThread.start();
    
    // Wait for initial requestNextFile signal
    QTest::qWait(1000);
    QVERIFY(requestSpy.count() >= 1);
    
    // Record the main thread ID
    Qt::HANDLE mainThreadId = QThread::currentThreadId();
    
    // Record the worker thread ID
    Qt::HANDLE workerThreadId = hasherThread.currentThreadId();
    
    // Verify they are different threads
    QVERIFY(mainThreadId != workerThreadId);
    
    // Add a file to hash
    hasherThread.addFile(filePath);
    
    // Wait for hashing to complete (requestNextFile will be emitted after hashing)
    QTest::qWait(5000);
    QVERIFY(requestSpy.count() >= 2);
    
    // Verify the hash was computed
    QVERIFY(hashSpy.count() > 0);
    
    // Stop the thread
    hasherThread.addFile(QString()); // Empty path signals completion
    QVERIFY(hasherThread.wait(1000));
}

void TestHasherThread::testStopInterruptsHashing()
{
    // Create a large temporary file that takes time to hash
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray data(10 * 1024 * 1024, 'B'); // 10MB
    tempFile.write(data);
    tempFile.close();
    QString filePath = tempFile.fileName();
    
    // Create HasherThread
    HasherThread hasherThread;
    
    // Start the thread
    hasherThread.start();
    
    // Wait for initial requestNextFile signal
    QSignalSpy requestSpy(&hasherThread, &HasherThread::requestNextFile);
    QTest::qWait(1000);
    QVERIFY(requestSpy.count() >= 1);
    
    // Add a file to hash
    hasherThread.addFile(filePath);
    
    // Give it a moment to start hashing
    QTest::qWait(100);
    
    // Stop the thread while hashing
    hasherThread.stop();
    
    // Verify the thread stops within a reasonable time
    // If it doesn't stop, it means stop() didn't work correctly
    QVERIFY(hasherThread.wait(2000));
}

void TestHasherThread::testResumeAfterStop()
{
    // This test simulates the real-world scenario where the same HasherThread
    // object is reused across multiple start/stop cycles (like in window.cpp)
    
    // Create a temporary file to hash
    QTemporaryFile tempFile1;
    QVERIFY(tempFile1.open());
    QByteArray data1(512 * 1024, 'C'); // 512KB
    tempFile1.write(data1);
    tempFile1.close();
    QString filePath1 = tempFile1.fileName();
    
    QTemporaryFile tempFile2;
    QVERIFY(tempFile2.open());
    QByteArray data2(512 * 1024, 'D'); // 512KB
    tempFile2.write(data2);
    tempFile2.close();
    QString filePath2 = tempFile2.fileName();
    
    // Create HasherThread (simulating global hasherThread in window.cpp)
    HasherThread hasherThread;
    
    // === First hash cycle ===
    QSignalSpy requestSpy1(&hasherThread, &HasherThread::requestNextFile);
    QSignalSpy hashSpy1(&hasherThread, &HasherThread::sendHash);
    
    // Start the thread
    hasherThread.start();
    
    // Wait for initial requestNextFile signal
    QTest::qWait(1000);
    QVERIFY(requestSpy1.count() >= 1);
    
    // Add a file to hash
    hasherThread.addFile(filePath1);
    
    // Wait for hashing to complete
    QTest::qWait(5000);
    QVERIFY(requestSpy1.count() >= 2);
    QVERIFY(hashSpy1.count() > 0);
    
    // Stop the thread (simulating user clicking stop button)
    hasherThread.stop();
    QVERIFY(hasherThread.wait(2000));
    
    // === Second hash cycle (Resume) ===
    QSignalSpy requestSpy2(&hasherThread, &HasherThread::requestNextFile);
    QSignalSpy hashSpy2(&hasherThread, &HasherThread::sendHash);
    
    // Start the thread again (simulating user clicking start button)
    hasherThread.start();
    
    // Wait for initial requestNextFile signal - THIS IS THE KEY TEST
    // If shouldStop flag is not reset, the thread will exit immediately
    // and this wait will timeout
    QTest::qWait(1000);
    QVERIFY(requestSpy2.count() >= 1);
    
    // Add another file to hash
    hasherThread.addFile(filePath2);
    
    // Wait for hashing to complete
    QTest::qWait(5000);
    QVERIFY(requestSpy2.count() >= 2);
    QVERIFY(hashSpy2.count() > 0);
    
    // Clean stop
    hasherThread.addFile(QString()); // Empty path signals completion
    QVERIFY(hasherThread.wait(2000));
}

QTEST_MAIN(TestHasherThread)
#include "test_hasher_thread.moc"
