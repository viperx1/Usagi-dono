#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryFile>
#include <QSignalSpy>
#include "../usagi/src/anidbapi.h"
#include "../usagi/src/hash/ed2k.h"

class TestHashReuse : public QObject
{
    Q_OBJECT

private slots:
    void testHashReuse();
    void testProgressSignalsEmitted();
};

void TestHashReuse::testHashReuse()
{
    // Create AniDBApi instance
    AniDBApi api("test", 1);
    
    // Create a temporary file with test content
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QByteArray testData = "This is test data for hash reuse";
    tempFile.write(testData);
    tempFile.close();
    QString filePath = tempFile.fileName();
    
    // First hash - should compute the hash
    int result1 = api.ed2khash(filePath);
    QCOMPARE(result1, 1);
    QString firstHash = api.ed2khashstr;
    QVERIFY(!firstHash.isEmpty());
    
    // Store the hash in local_files table
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO local_files (path, filename, ed2k_hash, status) VALUES (?, ?, ?, 1)");
    query.addBindValue(filePath);
    query.addBindValue(QFileInfo(filePath).fileName());
    
    // Extract just the hash from the ed2k link
    QString hashOnly = firstHash.section('|', 4, 4);
    query.addBindValue(hashOnly);
    QVERIFY(query.exec());
    
    // Second hash - should reuse the stored hash
    int result2 = api.ed2khash(filePath);
    QCOMPARE(result2, 1);
    QString secondHash = api.ed2khashstr;
    
    // Verify both hashes are the same
    QCOMPARE(firstHash, secondHash);
}

void TestHashReuse::testProgressSignalsEmitted()
{
    // Create AniDBApi instance
    AniDBApi api("test", 1);
    
    // Create a temporary file with enough data to have multiple parts
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    
    // Write 200KB of data (should result in 2 parts: 102400 + 102400)
    QByteArray testData(200 * 1024, 'X');
    tempFile.write(testData);
    tempFile.close();
    QString filePath = tempFile.fileName();
    
    // Store a hash in the database
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO local_files (path, filename, ed2k_hash, status) VALUES (?, ?, ?, 1)");
    query.addBindValue(filePath);
    query.addBindValue(QFileInfo(filePath).fileName());
    query.addBindValue("abcdef1234567890abcdef1234567890"); // Dummy hash
    QVERIFY(query.exec());
    
    // Set up signal spies
    QSignalSpy partsSpy(&api, &ed2k::notifyPartsDone);
    QSignalSpy hashedSpy(&api, &ed2k::notifyFileHashed);
    
    // Hash the file (should reuse stored hash)
    int result = api.ed2khash(filePath);
    QCOMPARE(result, 1);
    
    // Verify notifyPartsDone was emitted (at least once for progress)
    QVERIFY(partsSpy.count() > 0);
    
    // Verify notifyFileHashed was emitted exactly once
    QCOMPARE(hashedSpy.count(), 1);
}

QTEST_MAIN(TestHashReuse)
#include "test_hash_reuse.moc"
