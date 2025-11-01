#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryFile>
#include <QThread>
#include <QScopedPointer>
#include "../usagi/src/anidbapi.h"

class TestThreadSafety : public QObject
{
    Q_OBJECT

private slots:
    void testGetLocalFileHashFromWorkerThread();
};

// Worker thread class that mimics HasherThread behavior
class HashWorkerThread : public QThread
{
    Q_OBJECT
public:
    AniDBApi *api;
    QString filePath;
    QString retrievedHash;
    bool success;

    HashWorkerThread(AniDBApi *apiInstance, const QString &path) 
        : api(apiInstance), filePath(path), success(false) {}

    void run() override
    {
        // This mimics what HasherThread does - calling ed2khash from a worker thread
        // The ed2khash function calls getLocalFileHash, which was causing the crash
        retrievedHash = api->getLocalFileHash(filePath);
        success = !retrievedHash.isEmpty();
    }
};

void TestThreadSafety::testGetLocalFileHashFromWorkerThread()
{
    // Create AniDBApi instance (creates main thread database connection)
    AniDBApi api("test", 1);
    
    // Use the default database connection to set up test data
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    
    // Insert a test file with a known hash
    QString testPath = "/test/video_threadsafe.mkv";
    QString testHash = "abc123def456789";
    
    query.prepare("INSERT OR REPLACE INTO local_files (path, filename, ed2k_hash, status) VALUES (?, ?, ?, 1)");
    query.addBindValue(testPath);
    query.addBindValue("video_threadsafe.mkv");
    query.addBindValue(testHash);
    QVERIFY(query.exec());
    
    // Create a worker thread and access the database from it
    // This should not crash if thread-safety is implemented correctly
    // Using QScopedPointer for automatic cleanup
    QScopedPointer<HashWorkerThread> worker(new HashWorkerThread(&api, testPath));
    worker->start();
    
    // Wait for the worker thread to complete
    QVERIFY(worker->wait(5000)); // 5 second timeout
    
    // Verify the hash was retrieved successfully from the worker thread
    QVERIFY(worker->success);
    QCOMPARE(worker->retrievedHash, testHash);
    
    // worker is automatically deleted by QScopedPointer
}

QTEST_MAIN(TestThreadSafety)
#include "test_thread_safety.moc"
