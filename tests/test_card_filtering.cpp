#include <QTest>
#include <QString>
#include "../usagi/src/animemetadatacache.h"

/**
 * Test suite for card filtering logic
 * 
 * Tests validate:
 * - Alternative titles cache search functionality
 * - Case-insensitive matching
 * - Partial string matching
 * - Empty search handling
 */
class TestCardFiltering : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    
    // Cache tests
    void testCacheAddAndRetrieve();
    void testCacheCaseInsensitiveMatch();
    void testCachePartialMatch();
    void testCacheMultipleTitles();
    void testCacheEmptySearch();
    void testCacheNoMatch();
    void testCacheMultipleAnime();
    
private:
    AnimeMetadataCache *cache;
};

void TestCardFiltering::initTestCase()
{
    cache = new AnimeMetadataCache();
}

void TestCardFiltering::cleanupTestCase()
{
    delete cache;
}

void TestCardFiltering::cleanup()
{
    cache->clear();
}

void TestCardFiltering::testCacheAddAndRetrieve()
{
    // Test adding and retrieving titles
    QStringList titles;
    titles << "Cowboy Bebop" << "カウボーイビバップ";
    
    cache->addAnime(1, titles);
    
    QVERIFY(cache->contains(1));
    QCOMPARE(cache->size(), 1);
    
    QStringList retrieved = cache->getTitles(1);
    QCOMPARE(retrieved.size(), 2);
    QVERIFY(retrieved.contains("Cowboy Bebop"));
    QVERIFY(retrieved.contains("カウボーイビバップ"));
}

void TestCardFiltering::testCacheCaseInsensitiveMatch()
{
    // Test case-insensitive matching
    QStringList titles;
    titles << "Cowboy Bebop";
    cache->addAnime(1, titles);
    
    // All these should match
    QVERIFY(cache->matchesAnyTitle(1, "cowboy"));
    QVERIFY(cache->matchesAnyTitle(1, "COWBOY"));
    QVERIFY(cache->matchesAnyTitle(1, "CowBoy"));
    QVERIFY(cache->matchesAnyTitle(1, "Cowboy Bebop"));
    QVERIFY(cache->matchesAnyTitle(1, "COWBOY BEBOP"));
    QVERIFY(cache->matchesAnyTitle(1, "bebop"));
    QVERIFY(cache->matchesAnyTitle(1, "BEBOP"));
}

void TestCardFiltering::testCachePartialMatch()
{
    // Test partial string matching
    QStringList titles;
    titles << "Mobile Suit Gundam";
    cache->addAnime(123, titles);
    
    // All these should match (partial strings)
    QVERIFY(cache->matchesAnyTitle(123, "Mobile"));
    QVERIFY(cache->matchesAnyTitle(123, "Suit"));
    QVERIFY(cache->matchesAnyTitle(123, "Gundam"));
    QVERIFY(cache->matchesAnyTitle(123, "Mobile Suit"));
    QVERIFY(cache->matchesAnyTitle(123, "Suit Gundam"));
    QVERIFY(cache->matchesAnyTitle(123, "bile Su"));  // Middle of words
}

void TestCardFiltering::testCacheMultipleTitles()
{
    // Test matching against multiple titles
    QStringList titles;
    titles << "Seikai no Monshou" << "Crest of the Stars" << "星界の紋章";
    cache->addAnime(1, titles);
    
    // Should match any title
    QVERIFY(cache->matchesAnyTitle(1, "Seikai"));
    QVERIFY(cache->matchesAnyTitle(1, "Crest"));
    QVERIFY(cache->matchesAnyTitle(1, "Stars"));
    QVERIFY(cache->matchesAnyTitle(1, "星界"));
}

void TestCardFiltering::testCacheEmptySearch()
{
    // Empty search should match everything
    QStringList titles;
    titles << "Any Title";
    cache->addAnime(1, titles);
    
    QVERIFY(cache->matchesAnyTitle(1, ""));
}

void TestCardFiltering::testCacheNoMatch()
{
    // Test non-matching searches
    QStringList titles;
    titles << "Cowboy Bebop";
    cache->addAnime(1, titles);
    
    // These should NOT match
    QVERIFY(!cache->matchesAnyTitle(1, "Gundam"));
    QVERIFY(!cache->matchesAnyTitle(1, "Evangelion"));
    QVERIFY(!cache->matchesAnyTitle(1, "xyz123"));
    
    // Non-existent anime ID
    QVERIFY(!cache->matchesAnyTitle(999, "Cowboy"));
}

void TestCardFiltering::testCacheMultipleAnime()
{
    // Test with multiple anime in cache
    QStringList titles1;
    titles1 << "Cowboy Bebop" << "カウボーイビバップ";
    cache->addAnime(1, titles1);
    
    QStringList titles2;
    titles2 << "Trigun" << "トライガン";
    cache->addAnime(2, titles2);
    
    QStringList titles3;
    titles3 << "Samurai Champloo" << "サムライチャンプルー";
    cache->addAnime(3, titles3);
    
    QCOMPARE(cache->size(), 3);
    
    // Each anime should match its own titles only
    QVERIFY(cache->matchesAnyTitle(1, "Cowboy"));
    QVERIFY(!cache->matchesAnyTitle(1, "Trigun"));
    QVERIFY(!cache->matchesAnyTitle(1, "Samurai"));
    
    QVERIFY(cache->matchesAnyTitle(2, "Trigun"));
    QVERIFY(!cache->matchesAnyTitle(2, "Cowboy"));
    QVERIFY(!cache->matchesAnyTitle(2, "Samurai"));
    
    QVERIFY(cache->matchesAnyTitle(3, "Samurai"));
    QVERIFY(cache->matchesAnyTitle(3, "Champloo"));
    QVERIFY(!cache->matchesAnyTitle(3, "Cowboy"));
    QVERIFY(!cache->matchesAnyTitle(3, "Trigun"));
}

QTEST_MAIN(TestCardFiltering)
#include "test_card_filtering.moc"
