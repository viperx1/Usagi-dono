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
    
    // Series chain expansion scenario tests
    void testSeriesChainExpansionScenario();
    void testMissingAnimeDetection();
    
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

void TestCardFiltering::testSeriesChainExpansionScenario()
{
    // Simulate the bug scenario:
    // 1. User searches for "evangelion" - finds anime 22
    // 2. User enables series chain - expands to include anime 202 (sequel)
    // 3. BUT anime 202 wasn't in the cache, so search should still find it
    //    by ensuring all anime in a chain get their titles cached
    
    // Anime 22: Evangelion (original)
    QStringList titles22;
    titles22 << "Shin Seiki Evangelion" << "Neon Genesis Evangelion" << "Evangelion";
    cache->addAnime(22, titles22);
    
    // Anime 202: End of Evangelion (sequel) - initially NOT in cache
    // (This simulates the bug where series chain adds anime without cached titles)
    
    // Search for "evangelion" - should find anime 22
    QVERIFY2(cache->matchesAnyTitle(22, "evangelion"), "Should find anime 22 by searching 'evangelion'");
    
    // Anime 202 not in cache - search should NOT find it
    QVERIFY2(!cache->matchesAnyTitle(202, "evangelion"), "Should NOT find anime 202 (not in cache)");
    
    // Now simulate adding anime 202 to cache (what the fix does)
    QStringList titles202;
    titles202 << "Shin Seiki Evangelion Movie: Air/Magokoro wo, Kimi ni" << "End of Evangelion" << "Evangelion Movie";
    cache->addAnime(202, titles202);
    
    // After adding to cache, search should find it
    QVERIFY2(cache->matchesAnyTitle(202, "evangelion"), "Should find anime 202 after adding to cache");
}

void TestCardFiltering::testMissingAnimeDetection()
{
    // Test detecting which anime in a list are missing from cache
    // This is what the fix needs to do before creating cards
    
    // Add some anime to cache
    cache->addAnime(100, QStringList() << "Anime 100");
    cache->addAnime(101, QStringList() << "Anime 101");
    
    // Check which anime from a list are in cache
    QList<int> animeToCheck;
    animeToCheck << 100 << 101 << 102 << 103;
    
    QList<int> missingFromCache;
    for (int aid : animeToCheck) {
        if (!cache->contains(aid)) {
            missingFromCache.append(aid);
        }
    }
    
    // Should find that 102 and 103 are missing
    QCOMPARE(missingFromCache.size(), 2);
    QVERIFY(missingFromCache.contains(102));
    QVERIFY(missingFromCache.contains(103));
    
    // Add the missing anime
    cache->addAnime(102, QStringList() << "Anime 102");
    cache->addAnime(103, QStringList() << "Anime 103");
    
    // Verify all are now in cache
    QVERIFY(cache->contains(100));
    QVERIFY(cache->contains(101));
    QVERIFY(cache->contains(102));
    QVERIFY(cache->contains(103));
}

QTEST_MAIN(TestCardFiltering)
#include "test_card_filtering.moc"
