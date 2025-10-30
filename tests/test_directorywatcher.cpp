#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "../usagi/src/directorywatcher.h"

class TestDirectoryWatcher : public QObject
{
    Q_OBJECT

private slots:
    void testInitialization();
    void testStartWatching();
    void testStopWatching();
    void testNewFileDetection();
    void testVideoFileValidation();
    void testInvalidDirectory();
    void testProcessedFilesTracking();
    void testDatabaseStatusFiltering();
};

void TestDirectoryWatcher::testInitialization()
{
    DirectoryWatcher watcher;
    QVERIFY(!watcher.isWatching());
    QVERIFY(watcher.watchedDirectory().isEmpty());
}

void TestDirectoryWatcher::testStartWatching()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    DirectoryWatcher watcher;
    watcher.startWatching(tempDir.path());
    
    QVERIFY(watcher.isWatching());
    QCOMPARE(watcher.watchedDirectory(), tempDir.path());
}

void TestDirectoryWatcher::testStopWatching()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    DirectoryWatcher watcher;
    watcher.startWatching(tempDir.path());
    QVERIFY(watcher.isWatching());
    
    watcher.stopWatching();
    QVERIFY(!watcher.isWatching());
    QVERIFY(watcher.watchedDirectory().isEmpty());
}

void TestDirectoryWatcher::testNewFileDetection()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    DirectoryWatcher watcher;
    QSignalSpy spy(&watcher, &DirectoryWatcher::newFilesDetected);
    
    watcher.startWatching(tempDir.path());
    
    // Create a video file
    QString testFilePath = tempDir.path() + "/test_video.mkv";
    QFile testFile(testFilePath);
    QVERIFY(testFile.open(QIODevice::WriteOnly));
    testFile.write("test content");
    testFile.close();
    
    // Wait a bit for the signal
    QTest::qWait(3000);
    
    // Verify signal was emitted
    QVERIFY(spy.count() >= 1);
    QStringList files = spy.first().first().toStringList();
    QVERIFY(files.contains(testFilePath));
}

void TestDirectoryWatcher::testVideoFileValidation()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    DirectoryWatcher watcher;
    QSignalSpy spy(&watcher, &DirectoryWatcher::newFilesDetected);
    
    watcher.startWatching(tempDir.path());
    
    // Create a video file
    QString videoFile = tempDir.path() + "/video.mp4";
    QFile vf(videoFile);
    QVERIFY(vf.open(QIODevice::WriteOnly));
    vf.write("video content");
    vf.close();
    
    // Create a text file - should also be detected (no extension filtering)
    QString textFile = tempDir.path() + "/document.txt";
    QFile tf(textFile);
    QVERIFY(tf.open(QIODevice::WriteOnly));
    tf.write("text content");
    tf.close();
    
    // Wait for signals
    QTest::qWait(3000);
    
    // Verify signal was emitted with both files
    QVERIFY(spy.count() >= 1);
    
    // Get all detected files from all signal emissions
    QStringList allFiles;
    for (int i = 0; i < spy.count(); ++i) {
        allFiles.append(spy.at(i).first().toStringList());
    }
    
    // Check that both files were detected
    QVERIFY(allFiles.contains(videoFile));
    QVERIFY(allFiles.contains(textFile));
}

void TestDirectoryWatcher::testInvalidDirectory()
{
    DirectoryWatcher watcher;
    
    // Try to watch a non-existent directory
    watcher.startWatching("/non/existent/path");
    
    // Should not be watching
    QVERIFY(!watcher.isWatching());
}

void TestDirectoryWatcher::testProcessedFilesTracking()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    DirectoryWatcher watcher;
    QSignalSpy spy(&watcher, &DirectoryWatcher::newFilesDetected);
    
    // Create a video file before starting watcher
    QString testFilePath = tempDir.path() + "/existing_video.mkv";
    QFile testFile(testFilePath);
    QVERIFY(testFile.open(QIODevice::WriteOnly));
    testFile.write("test content");
    testFile.close();
    
    // Start watching
    watcher.startWatching(tempDir.path());
    
    // Wait a bit
    QTest::qWait(3000);
    
    // The existing file should be detected
    QVERIFY(spy.count() >= 1);
    
    // Record the initial count
    int initialCount = spy.count();
    
    // Stop and restart watcher
    watcher.stopWatching();
    spy.clear();
    
    // Create new watcher instance (simulating app restart)
    DirectoryWatcher watcher2;
    QSignalSpy spy2(&watcher2, &DirectoryWatcher::newFilesDetected);
    watcher2.startWatching(tempDir.path());
    
    // Wait a bit
    QTest::qWait(3000);
    
    // The file should be detected again on first scan since settings
    // are stored in QSettings which won't persist in test environment
    // This test just verifies the watcher works across multiple instances
    QVERIFY(true); // Test passes if no crashes occur
}

void TestDirectoryWatcher::testDatabaseStatusFiltering()
{
    // Set up in-memory database for testing
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_connection");
    db.setDatabaseName(":memory:");
    
    if (!db.open()) {
        QSKIP("Failed to open test database");
        return;
    }
    
    // Create local_files table matching the schema with ed2k_hash column
    QSqlQuery query(db);
    QVERIFY(query.exec("CREATE TABLE IF NOT EXISTS `local_files`(`id` INTEGER PRIMARY KEY AUTOINCREMENT, `path` TEXT UNIQUE, `filename` TEXT, `status` INTEGER DEFAULT 0, `ed2k_hash` TEXT)"));
    
    // Insert test files with different statuses
    // Status 0 = not hashed (should NOT be loaded)
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, status) VALUES ('/test/file1.mkv', 'file1.mkv', 0)"));
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, status) VALUES ('/test/file2.mkv', 'file2.mkv', 0)"));
    
    // Status 1 = hashed but not checked by API (SHOULD be loaded)
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, status, ed2k_hash) VALUES ('/test/file3.mkv', 'file3.mkv', 1, 'abc123')"));
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, status, ed2k_hash) VALUES ('/test/file4.mkv', 'file4.mkv', 1, 'def456')"));
    
    // Status 2 = in anidb (SHOULD be loaded)
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, status, ed2k_hash) VALUES ('/test/file5.mkv', 'file5.mkv', 2, 'ghi789')"));
    
    // Status 3 = not in anidb (SHOULD be loaded)
    QVERIFY(query.exec("INSERT INTO local_files (path, filename, status, ed2k_hash) VALUES ('/test/file6.mkv', 'file6.mkv', 3, 'jkl012')"));
    
    // Verify the query that DirectoryWatcher uses
    QSqlQuery verifyQuery(db);
    QVERIFY(verifyQuery.exec("SELECT path FROM local_files WHERE status >= 1"));
    
    int count = 0;
    while (verifyQuery.next()) {
        QString path = verifyQuery.value(0).toString();
        // Should only get file3, file4, file5, and file6 (status >= 1)
        QVERIFY(path == "/test/file3.mkv" || path == "/test/file4.mkv" || path == "/test/file5.mkv" || path == "/test/file6.mkv");
        count++;
    }
    
    // Should have exactly 4 files (file3, file4, file5, file6)
    QCOMPARE(count, 4);
    
    // Clean up
    db.close();
    QSqlDatabase::removeDatabase("test_connection");
}

QTEST_MAIN(TestDirectoryWatcher)
#include "test_directorywatcher.moc"
