#include <QTest>
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
    QSignalSpy threadStartedSpy(&hasherThread, &HasherThread::threadStarted);
    
    // Record the main thread ID
    Qt::HANDLE mainThreadId = QThread::currentThreadId();
    
    // Start the thread
    hasherThread.start();
    
    // Wait for threadStarted signal to capture the worker thread ID
    // Use QTest::qWait() instead of QSignalSpy::wait() for better compatibility with static Qt builds
    for (int i = 0; i < 100 && threadStartedSpy.count() == 0; ++i) {
        QTest::qWait(10);
    }
    QVERIFY(threadStartedSpy.count() == 1);
    
    // Extract the worker thread ID from the signal
    Qt::HANDLE workerThreadId = threadStartedSpy.at(0).at(0).value<Qt::HANDLE>();
    
    // Verify they are different threads
    QVERIFY(mainThreadId != workerThreadId);
    
    // The requestNextFile signal is emitted right after threadStarted,
    // so it should already be in the spy. If not, wait for it.
    if (requestSpy.count() == 0) {
        for (int i = 0; i < 100 && requestSpy.count() == 0; ++i) {
            QTest::qWait(10);
        }
    }
    QVERIFY(requestSpy.count() >= 1);
    
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
    // This test simulates the real-world scenario where hashing can be
    // stopped and resumed. On Windows with static Qt builds, restarting
    // the same QThread object can cause TLS (Thread Local Storage) issues,
    // so we test with separate thread instances to ensure robustness.
    
    // Create temporary files to hash
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
    
    // === First hash cycle ===
    {
        HasherThread hasherThread;
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
    }
    
    // === Second hash cycle (Resume with new thread instance) ===
    {
        HasherThread hasherThread;
        QSignalSpy requestSpy2(&hasherThread, &HasherThread::requestNextFile);
        QSignalSpy hashSpy2(&hasherThread, &HasherThread::sendHash);
        
        // Start the thread (simulating user clicking start button again)
        hasherThread.start();
        
        // Wait for initial requestNextFile signal
        // This verifies that a new thread instance works correctly after stopping
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
}

QTEST_MAIN(TestHasherThread)
#include "test_hasher_thread.moc"
