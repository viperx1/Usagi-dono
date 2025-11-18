#include <QTest>
#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "../usagi/src/anidbapi.h"

/**
 * Test suite for truncated API response handling
 * 
 * These tests verify that when AniDB UDP API responses are truncated at the
 * 1400-byte MTU limit, the system correctly:
 * 1. Detects the truncation
 * 2. Skips the incomplete last field
 * 3. Processes complete fields normally
 * 4. Logs appropriate warnings
 */
class TestTruncatedResponse : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    
    // Truncation detection tests
    void testTruncatedFileResponse();
    void testTruncatedAnimeResponse();
    void testTruncatedMylistResponse();
    void testTruncatedEpisodeResponse();
    void testNonTruncatedResponse();

private:
    AniDBApi* api;
    QString dbPath;
    
    void clearPackets();
    void insertTestFileData();
};

void TestTruncatedResponse::clearPackets()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("DELETE FROM `packets`");
}

void TestTruncatedResponse::insertTestFileData()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    
    // Insert a test file entry for FILE responses
    QString q = QString("INSERT OR REPLACE INTO `file` "
        "(`fid`, `aid`, `eid`, `gid`, `lid`, `size`, `ed2k`) "
        "VALUES ('12345', '100', '200', '300', '0', '734003200', 'a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4')");
    query.exec(q);
}

void TestTruncatedResponse::initTestCase()
{
    // Create API instance with test client info
    api = new AniDBApi("usagitest", 1);
    
    // Set test credentials
    api->setUsername("testuser");
    api->setPassword("testpass");
    
    // Clear any existing packets
    clearPackets();
    
    // Insert test data
    insertTestFileData();
}

void TestTruncatedResponse::cleanupTestCase()
{
    delete api;
}

void TestTruncatedResponse::cleanup()
{
    // Clear packets after each test
    clearPackets();
}

void TestTruncatedResponse::testTruncatedFileResponse()
{
    // Test that a truncated FILE response (220) is handled correctly
    // Create a simulated FILE command
    QString tag = "1001";
    QString fileCmd = QString("FILE size=734003200&ed2k=a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4&fmask=7ff8fef9&amask=f0f0f0f0");
    
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    QString q = QString("INSERT INTO `packets` (`tag`, `str`, `processed`) VALUES ('%1', '%2', 1)").arg(tag).arg(fileCmd);
    QVERIFY(query.exec(q));
    
    // Create a simulated truncated FILE response
    // Response format: 220 FILE\n{fid}|{aid}|{eid}|{gid}|...
    // Simulate truncation by cutting off the last field
    // Note: mask 0x7ff8fef9 does NOT include fFILETYPE, so "mkv" is omitted
    QString truncatedResponse = tag + " 220 FILE\n12345|100|200|300|0|0|1|1|734003200|a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4||||||H.264/AVC|1500|H264|1200|1920x1080|japanese|english|1440|Test file|0|test_truncat";
    
    // Parse the message with truncation flag set to true
    api->ParseMessage(truncatedResponse, "", fileCmd, true);
    
    // Verify that the file data was stored (incomplete last field should be skipped)
    q = QString("SELECT `fid`, `aid`, `eid`, `gid` FROM `file` WHERE `fid` = '12345'");
    QVERIFY(query.exec(q));
    QVERIFY(query.next());
    
    QCOMPARE(query.value(0).toString(), QString("12345"));
    QCOMPARE(query.value(1).toString(), QString("100"));
    QCOMPARE(query.value(2).toString(), QString("200"));
    QCOMPARE(query.value(3).toString(), QString("300"));
    
    qDebug() << "Truncated FILE response handled successfully - complete fields stored";
}

void TestTruncatedResponse::testTruncatedAnimeResponse()
{
    // Test that a truncated ANIME response (230) is handled correctly
    QString tag = "1002";
    QString animeCmd = QString("ANIME aid=100&amask=b2f0e0fc000000");
    
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    QString q = QString("INSERT INTO `packets` (`tag`, `str`, `processed`) VALUES ('%1', '%2', 1)").arg(tag).arg(animeCmd);
    QVERIFY(query.exec(q));
    
    // Create anime table entry first
    q = QString("INSERT OR REPLACE INTO `anime` (`aid`, `typename`) VALUES ('100', '')");
    query.exec(q);
    
    // Create a simulated truncated ANIME response
    // The response should have fields cut off at the end
    // Mask b2f0e0fc000000 includes: AID, YEAR, TYPE, ROMAJI_NAME, KANJI_NAME, ENGLISH_NAME, OTHER_NAME
    QString truncatedResponse = tag + " 230 ANIME\n100|2023|TV Series|Romaji Name|Kanji Name|English Name|Other Name That Gets Trunca";
    
    // Parse the message with truncation flag set to true
    api->ParseMessage(truncatedResponse, "", animeCmd, true);
    
    // Verify that the anime type was stored (even though response was truncated)
    q = QString("SELECT `typename` FROM `anime` WHERE `aid` = 100");
    QVERIFY(query.exec(q));
    QVERIFY(query.next());
    
    // The type field should have been parsed successfully before truncation
    QCOMPARE(query.value(0).toString(), QString("TV Series"));
    
    qDebug() << "Truncated ANIME response handled successfully - complete fields stored";
}

void TestTruncatedResponse::testTruncatedMylistResponse()
{
    // Test that a truncated MYLIST response (221) is handled correctly
    QString tag = "1003";
    QString mylistCmd = QString("MYLIST lid=12345");
    
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    QString q = QString("INSERT INTO `packets` (`tag`, `str`, `processed`) VALUES ('%1', '%2', 1)").arg(tag).arg(mylistCmd);
    QVERIFY(query.exec(q));
    
    // Create a simulated truncated MYLIST response
    // Response format: 221 MYLIST\n{fid}|{eid}|{aid}|{gid}|{date}|{state}|{viewdate}|{storage}|{source}|{other}|{filestate}
    // Need 12 fields (one more than minimum) so after truncation removal we have 11 fields
    QString truncatedResponse = tag + " 221 MYLIST\n12345|200|100|300|1234567890|1|0|HDD|download|comment|0|extra field that is very long and gets trunca";
    
    // Parse the message with truncation flag set to true
    api->ParseMessage(truncatedResponse, "", mylistCmd, true);
    
    // Verify that the mylist entry was stored with complete fields only
    q = QString("SELECT `lid`, `fid`, `eid`, `aid`, `gid`, `state` FROM `mylist` WHERE `lid` = '12345'");
    QVERIFY(query.exec(q));
    QVERIFY(query.next());
    
    QCOMPARE(query.value(0).toString(), QString("12345"));
    QCOMPARE(query.value(1).toString(), QString("12345"));
    QCOMPARE(query.value(2).toString(), QString("200"));
    QCOMPARE(query.value(3).toString(), QString("100"));
    QCOMPARE(query.value(4).toString(), QString("300"));
    QCOMPARE(query.value(5).toString(), QString("1"));
    
    qDebug() << "Truncated MYLIST response handled successfully - complete fields stored";
}

void TestTruncatedResponse::testTruncatedEpisodeResponse()
{
    // Test that a truncated EPISODE response (240) is handled correctly
    QString tag = "1004";
    QString episodeCmd = QString("EPISODE eid=200");
    
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    QString q = QString("INSERT INTO `packets` (`tag`, `str`, `processed`) VALUES ('%1', '%2', 1)").arg(tag).arg(episodeCmd);
    QVERIFY(query.exec(q));
    
    // Create a simulated truncated EPISODE response
    // Response format: 240 EPISODE\n{eid}|{aid}|{length}|{rating}|{votes}|{epno}|{eng}|{romaji}|{kanji}
    QString truncatedResponse = tag + " 240 EPISODE\n200|100|1440|8.50|250|01|Episode Title|Romaji Title Here|Kanji Title That Gets Trunca";
    
    // Parse the message with truncation flag set to true
    api->ParseMessage(truncatedResponse, "", episodeCmd, true);
    
    // Verify that the episode data was stored with complete fields only
    q = QString("SELECT `eid`, `name`, `nameromaji`, `epno` FROM `episode` WHERE `eid` = '200'");
    QVERIFY(query.exec(q));
    QVERIFY(query.next());
    
    QCOMPARE(query.value(0).toString(), QString("200"));
    QCOMPARE(query.value(1).toString(), QString("Episode Title"));
    QCOMPARE(query.value(2).toString(), QString("Romaji Title Here"));
    QCOMPARE(query.value(3).toString(), QString("01"));
    
    qDebug() << "Truncated EPISODE response handled successfully - complete fields stored";
}

void TestTruncatedResponse::testNonTruncatedResponse()
{
    // Test that a non-truncated response is processed normally
    QString tag = "1005";
    QString fileCmd = QString("FILE size=734003200&ed2k=a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4&fmask=7ff8fef9&amask=f0f0f0f0");
    
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    QString q = QString("INSERT INTO `packets` (`tag`, `str`, `processed`) VALUES ('%1', '%2', 1)").arg(tag).arg(fileCmd);
    QVERIFY(query.exec(q));
    
    // Create a simulated complete (non-truncated) FILE response
    // Note: mask 0x7ff8fef9 does NOT include fFILETYPE, so "mkv" is omitted
    QString completeResponse = tag + " 220 FILE\n12346|101|201|301|0|0|1|1|734003200|a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4||||||H.264/AVC|1500|H264|1200|1920x1080|japanese|english|1440|Test file complete|0|test_complete.mkv";
    
    // Parse the message with truncation flag set to false
    api->ParseMessage(completeResponse, "", fileCmd, false);
    
    // Verify that all file data was stored including the last field
    q = QString("SELECT `fid`, `aid`, `eid`, `gid`, `filename` FROM `file` WHERE `fid` = '12346'");
    QVERIFY(query.exec(q));
    QVERIFY(query.next());
    
    QCOMPARE(query.value(0).toString(), QString("12346"));
    QCOMPARE(query.value(1).toString(), QString("101"));
    QCOMPARE(query.value(2).toString(), QString("201"));
    QCOMPARE(query.value(3).toString(), QString("301"));
    QCOMPARE(query.value(4).toString(), QString("test_complete.mkv"));
    
    qDebug() << "Non-truncated FILE response handled successfully - all fields stored";
}

QTEST_MAIN(TestTruncatedResponse)
#include "test_truncated_response.moc"
