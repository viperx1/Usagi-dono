#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryFile>
#include <QSignalSpy>
#include <QElapsedTimer>
#include "../usagi/src/anidbapi.h"
#include "../usagi/src/hash/ed2k.h"

/**
 * Test to verify the UI freeze fix for pre-hashed files.
 * 
 * The issue was that when a file already has a hash in the database,
 * the ed2khash function was emitting notifyPartsDone signals in a tight loop.
 * For large files with thousands of parts, this would queue thousands of signals
 * to the UI thread's event queue, causing the UI to freeze.
 * 
 * The fix ensures only a single completion signal is emitted for pre-hashed files,
 * keeping the UI responsive.
 */
class TestUIFreezeFix : public QObject
{
    Q_OBJECT

private slots:
    void testPreHashedFileEmitsMinimalSignals();
    void testLargePreHashedFileDoesNotFloodEventQueue();

private:
    // Helper method to store a hash in the database for testing
    void storeHashInDatabase(const QString &filePath, const QString &hash)
    {
        QSqlDatabase db = QSqlDatabase::database();
        QSqlQuery query(db);
        query.prepare("INSERT OR REPLACE INTO local_files (path, filename, ed2k_hash, status) VALUES (?, ?, ?, 1)");
        query.addBindValue(filePath);
        query.addBindValue(QFileInfo(filePath).fileName());
        query.addBindValue(hash);
        QVERIFY(query.exec());
    }
};

void TestUIFreezeFix::testPreHashedFileEmitsMinimalSignals()
{
    // Create AniDBApi instance
    AniDBApi api("test", 1);
    
    // Create a temporary file with enough data to have multiple parts
    // 500KB = approximately 5 parts of 102400 bytes each
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray testData(500 * 1024, 'X');
    tempFile.write(testData);
    tempFile.close();
    QString filePath = tempFile.fileName();
    
    // Store a hash in the database (simulating a pre-hashed file)
    storeHashInDatabase(filePath, "abcdef1234567890abcdef1234567890");
    
    // Set up signal spy to count how many times notifyPartsDone is emitted
    QSignalSpy partsSpy(&api, &ed2k::notifyPartsDone);
    
    // Hash the file (should reuse stored hash)
    int result = api.ed2khash(filePath);
    QCOMPARE(result, 1);
    
    // The fix: verify that only ONE signal was emitted instead of multiple signals in a loop
    // Before the fix: Would emit 5 signals (one for each part)
    // After the fix: Emits only 1 signal (the completion signal)
    QCOMPARE(partsSpy.count(), 1);
    
    // Verify the signal indicates completion (parts done = total parts)
    QList<QVariant> arguments = partsSpy.takeFirst();
    int totalParts = arguments.at(0).toInt();
    int partsDone = arguments.at(1).toInt();
    QCOMPARE(partsDone, totalParts); // Should be at 100% completion
}

void TestUIFreezeFix::testLargePreHashedFileDoesNotFloodEventQueue()
{
    // Create AniDBApi instance
    AniDBApi api("test", 1);
    
    // Create a temporary file with data that would result in many parts
    // 10MB = approximately 100 parts of 102400 bytes each
    // Before the fix, this would emit 100 signals in a tight loop
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    
    // Write 10MB of data
    const int chunkSize = 1024 * 1024; // 1MB chunks
    QByteArray chunk(chunkSize, 'Y');
    for (int i = 0; i < 10; i++) {
        tempFile.write(chunk);
    }
    tempFile.close();
    QString filePath = tempFile.fileName();
    
    // Store a hash in the database
    storeHashInDatabase(filePath, "fedcba9876543210fedcba9876543210");
    
    // Set up signal spy
    QSignalSpy partsSpy(&api, &ed2k::notifyPartsDone);
    
    // Measure time to process (should be very fast since hash is pre-computed)
    QElapsedTimer timer;
    timer.start();
    
    int result = api.ed2khash(filePath);
    qint64 elapsed = timer.elapsed();
    
    QCOMPARE(result, 1);
    
    // Verify only ONE signal was emitted, not ~100 signals
    QCOMPARE(partsSpy.count(), 1);
    
    // Processing should be nearly instant (< 100ms) since we're just reading from database
    // Before the fix, emitting 100 signals could take longer due to event queue processing
    QVERIFY2(elapsed < 100, QString("Processing took %1ms, expected < 100ms").arg(elapsed).toLatin1());
    
    qDebug() << "Large pre-hashed file processed in" << elapsed << "ms with" << partsSpy.count() << "signal(s)";
}

QTEST_MAIN(TestUIFreezeFix)
#include "test_ui_freeze_fix.moc"
