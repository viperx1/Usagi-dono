/**
 * Test to verify the fix for anime titles containing pipe characters
 * 
 * Issue: Anime (aid 8895) "Shin Evangelion Gekijouban:||" breaks API response parsing
 * because the pipe symbol in the title causes misalignment when parsing pipe-delimited data.
 * 
 * This test verifies that:
 * 1. Anime titles with pipe characters are correctly parsed and stored
 * 2. The full title is preserved, including the pipe characters
 */

#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "../usagi/src/anidbapi.h"

class TestPipeInTitle : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    
    // Test anime titles with pipes
    void testAnimeTitleWithPipes();
    void testAnimeTitleWithMultiplePipes();
    void testAnimeTitleWithTrailingPipes();
    void testNormalTitleStillWorks();

private:
    AniDBApi* api;
    
    QString getAnimeTitleFromDb(int aid);
    int getAnimeTitlesCount();
};

void TestPipeInTitle::initTestCase()
{
    // Create API instance
    api = new AniDBApi("usagitest", 1);
}

void TestPipeInTitle::cleanupTestCase()
{
    delete api;
}

void TestPipeInTitle::cleanup()
{
    // Clean up anime titles table after each test
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("DELETE FROM `anime_titles`");
}

// Helper function to get anime title from database
QString TestPipeInTitle::getAnimeTitleFromDb(int aid)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.prepare("SELECT `title` FROM `anime_titles` WHERE `aid` = ?");
    query.addBindValue(aid);
    
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

// Helper function to count anime titles in database
int TestPipeInTitle::getAnimeTitlesCount()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("SELECT COUNT(*) FROM `anime_titles`");
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

void TestPipeInTitle::testAnimeTitleWithPipes()
{
    // Test the specific case mentioned in the issue: aid 8895 "Shin Evangelion Gekijouban:||"
    QByteArray testData = 
        "8895|1|x-jat|Shin Evangelion Gekijouban:||\n";
    
    api->parseAndStoreAnimeTitles(testData);
    
    int count = getAnimeTitlesCount();
    QCOMPARE(count, 1);
    
    QString title = getAnimeTitleFromDb(8895);
    QCOMPARE(title, QString("Shin Evangelion Gekijouban:||"));
}

void TestPipeInTitle::testAnimeTitleWithMultiplePipes()
{
    // Test a title with multiple pipe characters in different positions
    QByteArray testData = 
        "1234|1|en|Title | With | Multiple | Pipes\n";
    
    api->parseAndStoreAnimeTitles(testData);
    
    int count = getAnimeTitlesCount();
    QCOMPARE(count, 1);
    
    QString title = getAnimeTitleFromDb(1234);
    QCOMPARE(title, QString("Title | With | Multiple | Pipes"));
}

void TestPipeInTitle::testAnimeTitleWithTrailingPipes()
{
    // Test a title ending with pipe characters
    QByteArray testData = 
        "5678|2|ja|Title With Trailing Pipes|||\n";
    
    api->parseAndStoreAnimeTitles(testData);
    
    int count = getAnimeTitlesCount();
    QCOMPARE(count, 1);
    
    QString title = getAnimeTitleFromDb(5678);
    QCOMPARE(title, QString("Title With Trailing Pipes|||"));
}

void TestPipeInTitle::testNormalTitleStillWorks()
{
    // Ensure normal titles without pipes still work correctly
    QByteArray testData = 
        "9999|1|en|Normal Title Without Pipes\n"
        "9998|1|ja|Another Normal Title\n";
    
    api->parseAndStoreAnimeTitles(testData);
    
    int count = getAnimeTitlesCount();
    QCOMPARE(count, 2);
    
    QString title1 = getAnimeTitleFromDb(9999);
    QCOMPARE(title1, QString("Normal Title Without Pipes"));
    
    QString title2 = getAnimeTitleFromDb(9998);
    QCOMPARE(title2, QString("Another Normal Title"));
}

QTEST_MAIN(TestPipeInTitle)
#include "test_pipe_in_title.moc"
