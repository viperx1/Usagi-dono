#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QSignalSpy>
#include <QElapsedTimer>
#include "../usagi/src/hash/ed2k.h"

/**
 * Test to verify signal throttling during actual file hashing.
 * 
 * The issue is that when hashing a large file, the ed2khash function
 * emits notifyPartsDone signal for every 102KB chunk read.
 * For a 1GB file, this would emit ~10,000 signals to the UI thread's
 * event queue, causing the UI to freeze even with UI-side throttling.
 * 
 * The fix throttles signal emission at the source (in the hashing thread)
 * to emit signals at most every 100ms, drastically reducing the number
 * of signals queued to the event loop.
 */
class TestHashingThrottle : public QObject
{
    Q_OBJECT

private slots:
    void testLargeFileHashingEmitsThrottledSignals();
    void testSmallFileStillEmitsCompletionSignal();
    void testThrottlingDoesNotAffectHashAccuracy();
};

void TestHashingThrottle::testLargeFileHashingEmitsThrottledSignals()
{
    // Create ed2k hasher
    ed2k hasher;
    
    // Create a temporary file large enough to have many parts
    // 5MB = approximately 52 parts of 102400 bytes each
    // Without throttling, this would emit 52 signals
    // With throttling (100ms), and assuming hashing takes ~500ms,
    // we should see about 5-6 signals instead
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    
    // Write 5MB of data in chunks
    const int chunkSize = 1024 * 1024; // 1MB chunks
    QByteArray chunk(chunkSize, 'A');
    for (int i = 0; i < 5; i++) {
        tempFile.write(chunk);
    }
    tempFile.close();
    QString filePath = tempFile.fileName();
    
    // Set up signal spy to count signals
    QSignalSpy partsSpy(&hasher, &ed2k::notifyPartsDone);
    
    // Measure time to hash
    QElapsedTimer timer;
    timer.start();
    
    int result = hasher.ed2khash(filePath);
    qint64 elapsed = timer.elapsed();
    
    QCOMPARE(result, 1);
    
    // With throttling at 100ms, we should see significantly fewer signals than parts
    // Expected: ~50 parts, but only ~(elapsed/100) + 1 signals
    int expectedMaxSignals = (elapsed / 100) + 2; // +2 for safety margin
    int actualSignals = partsSpy.count();
    
    qDebug() << "File size: 5MB (~52 parts)";
    qDebug() << "Hashing took:" << elapsed << "ms";
    qDebug() << "Signals emitted:" << actualSignals;
    qDebug() << "Expected max signals:" << expectedMaxSignals;
    
    // Verify we emitted far fewer signals than parts
    QVERIFY2(actualSignals < 52, 
             QString("Expected < 52 signals with throttling, got %1").arg(actualSignals).toLatin1());
    
    // Verify we did emit at least some signals (not zero)
    QVERIFY2(actualSignals > 0,
             "Should emit at least one signal");
    
    // Verify the last signal indicates completion
    if (actualSignals > 0) {
        QList<QVariant> lastSignal = partsSpy.last();
        int totalParts = lastSignal.at(0).toInt();
        int partsDone = lastSignal.at(1).toInt();
        QCOMPARE(partsDone, totalParts);
    }
}

void TestHashingThrottle::testSmallFileStillEmitsCompletionSignal()
{
    // Create ed2k hasher
    ed2k hasher;
    
    // Create a small file (< 102KB, only 1 part)
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray data(50 * 1024, 'B'); // 50KB
    tempFile.write(data);
    tempFile.close();
    QString filePath = tempFile.fileName();
    
    // Set up signal spy
    QSignalSpy partsSpy(&hasher, &ed2k::notifyPartsDone);
    
    int result = hasher.ed2khash(filePath);
    QCOMPARE(result, 1);
    
    // Should emit at least one signal (the completion signal)
    QVERIFY(partsSpy.count() > 0);
    
    // Verify it's a completion signal
    QList<QVariant> signal = partsSpy.first();
    int totalParts = signal.at(0).toInt();
    int partsDone = signal.at(1).toInt();
    QCOMPARE(partsDone, totalParts);
}

void TestHashingThrottle::testThrottlingDoesNotAffectHashAccuracy()
{
    // Create ed2k hasher
    ed2k hasher;
    
    // Create a file with known content
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray data(500 * 1024, 'C'); // 500KB (5 parts)
    tempFile.write(data);
    tempFile.close();
    QString filePath = tempFile.fileName();
    
    // Hash it once with throttling
    int result1 = hasher.ed2khash(filePath);
    QCOMPARE(result1, 1);
    QString hash1 = hasher.HexDigest().c_str();
    
    // Hash it again
    int result2 = hasher.ed2khash(filePath);
    QCOMPARE(result2, 1);
    QString hash2 = hasher.HexDigest().c_str();
    
    // Hashes should be identical - throttling should not affect accuracy
    QCOMPARE(hash1, hash2);
    
    // Hash should not be empty
    QVERIFY(!hash1.isEmpty());
    QCOMPARE(hash1.length(), 32); // MD4 hash is 32 hex characters
}

QTEST_MAIN(TestHashingThrottle)
#include "test_hashing_throttle.moc"
