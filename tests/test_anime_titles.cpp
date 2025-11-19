#include <QTest>
#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include "../usagi/src/anidbapi.h"

/**
 * Test suite for anime titles import functionality
 * 
 * Tests validate:
 * - Database table creation
 * - Anime titles parsing and storage
 * - Update timestamp tracking
 * - 24-hour update interval enforcement
 */
class TestAnimeTitles : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    
    // Database tests
    void testAnimeTitlesTableExists();
    void testLastUpdateTimestampStorage();
    
    // Update logic tests
    void testShouldUpdateWhenNeverDownloaded();
    void testShouldNotUpdateWithin24Hours();
    void testShouldUpdateAfter24Hours();
    
    // Parsing tests
    void testParseAnimeTitlesFormat();
    void testParseAnimeTitlesWithSpecialCharacters();
    void testParseAnimeTitlesSkipsComments();

private:
    AniDBApi* api;
    
    void setLastUpdateTimestamp(qint64 secondsAgo);
    qint64 getLastUpdateTimestamp();
    int getAnimeTitlesCount();
};

void TestAnimeTitles::initTestCase()
{
    // Create API instance
    api = new AniDBApi("usagitest", 1);
}

void TestAnimeTitles::cleanupTestCase()
{
    delete api;
}

void TestAnimeTitles::cleanup()
{
    // Clean up anime titles table after each test
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("DELETE FROM `anime_titles`");
    query.exec("DELETE FROM `settings` WHERE `name` = 'last_anime_titles_update'");
}

// Helper function to set last update timestamp
void TestAnimeTitles::setLastUpdateTimestamp(qint64 secondsAgo)
{
    QDateTime timestamp = QDateTime::currentDateTime().addSecs(-secondsAgo);
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'last_anime_titles_update', '%1');")
                .arg(timestamp.toSecsSinceEpoch());
    query.exec(q);
}

// Helper function to get last update timestamp
qint64 TestAnimeTitles::getLastUpdateTimestamp()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("SELECT `value` FROM `settings` WHERE `name` = 'last_anime_titles_update'");
    if (query.next()) {
        return query.value(0).toLongLong();
    }
    return 0;
}

// Helper function to count anime titles in database
int TestAnimeTitles::getAnimeTitlesCount()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("SELECT COUNT(*) FROM `anime_titles`");
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

// ===== Database Tests =====

void TestAnimeTitles::testAnimeTitlesTableExists()
{
    // Test that anime_titles table was created
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    
    // Try to select from the table (will fail if table doesn't exist)
    bool result = query.exec("SELECT * FROM `anime_titles` LIMIT 1");
    QVERIFY2(result, "anime_titles table should exist");
}

void TestAnimeTitles::testLastUpdateTimestampStorage()
{
    // Test that we can store and retrieve the last update timestamp
    qint64 testTimestamp = QDateTime::currentSecsSinceEpoch();
    
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'last_anime_titles_update', '%1');")
                .arg(testTimestamp);
    bool result = query.exec(q);
    QVERIFY2(result, "Should be able to store last update timestamp");
    
    qint64 retrieved = getLastUpdateTimestamp();
    QCOMPARE(retrieved, testTimestamp);
}

// ===== Update Logic Tests =====

void TestAnimeTitles::testShouldUpdateWhenNeverDownloaded()
{
    // When there's no last update timestamp, should return true
    bool shouldUpdate = api->shouldUpdateAnimeTitles();
    QVERIFY2(shouldUpdate, "Should update when never downloaded before");
}

void TestAnimeTitles::testShouldNotUpdateWithin24Hours()
{
    // Set last update to 12 hours ago (43200 seconds)
    setLastUpdateTimestamp(43200);
    
    // Should not update within 24 hours
    bool shouldUpdate = api->shouldUpdateAnimeTitles();
    QVERIFY2(!shouldUpdate, "Should not update within 24 hours of last update");
}

void TestAnimeTitles::testShouldUpdateAfter24Hours()
{
    // Set last update to 25 hours ago (90000 seconds)
    setLastUpdateTimestamp(90000);
    
    // Should update after 24 hours
    bool shouldUpdate = api->shouldUpdateAnimeTitles();
    QVERIFY2(shouldUpdate, "Should update after 24 hours since last update");
}

// ===== Parsing Tests =====

void TestAnimeTitles::testParseAnimeTitlesFormat()
{
    // Test parsing of anime titles data
    QByteArray testData = 
        "1|1|x-jat|Seikai no Monshou\n"
        "1|2|en|Crest of the Stars\n"
        "1|3|ja|星界の紋章\n"
        "2|1|x-jat|Kidou Senshi Gundam\n"
        "2|2|en|Mobile Suit Gundam\n";
    
    api->parseAndStoreAnimeTitles(testData);
    
    int count = getAnimeTitlesCount();
    QCOMPARE(count, 5);
}

void TestAnimeTitles::testParseAnimeTitlesWithSpecialCharacters()
{
    // Test parsing with special characters that need escaping
    QByteArray testData = 
        "1|1|en|Title with 'single quotes'\n"
        "2|1|en|Title with \"double quotes\"\n"
        "3|1|en|Normal Title\n";
    
    api->parseAndStoreAnimeTitles(testData);
    
    int count = getAnimeTitlesCount();
    QVERIFY2(count >= 3, "Should parse titles with special characters");
    
    // Verify the title with single quotes was stored correctly
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("SELECT `title` FROM `anime_titles` WHERE `aid` = 1");
    if (query.next()) {
        QString title = query.value(0).toString();
        QCOMPARE(title, QString("Title with 'single quotes'"));
    } else {
        QFAIL("Should find title with single quotes");
    }
}

void TestAnimeTitles::testParseAnimeTitlesSkipsComments()
{
    // Test that comment lines are skipped
    QByteArray testData = 
        "# This is a comment\n"
        "1|1|en|Valid Title\n"
        "# Another comment\n"
        "2|1|en|Another Valid Title\n";
    
    api->parseAndStoreAnimeTitles(testData);
    
    int count = getAnimeTitlesCount();
    QCOMPARE(count, 2);
}

QTEST_MAIN(TestAnimeTitles)
#include "test_anime_titles.moc"
