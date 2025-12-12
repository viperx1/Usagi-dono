#include <QTest>
#include <QString>
#include "../usagi/src/animefilter.h"
#include "../usagi/src/animemetadatacache.h"
#include "../usagi/src/animestats.h"
#include "../usagi/src/cachedanimedata.h"

/**
 * Test suite for the refactored filter classes
 * 
 * Tests the SOLID-based filter architecture:
 * - AnimeDataAccessor
 * - Individual filter classes
 * - CompositeFilter
 */
class TestFilterClasses : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    
    // AnimeDataAccessor tests
    void testDataAccessor_withCachedData();
    void testDataAccessor_hasData();
    
    // Individual filter tests
    void testSearchFilter_matches();
    void testSearchFilter_noMatch();
    void testSearchFilter_empty();
    void testSearchFilter_description();
    
    void testTypeFilter_matches();
    void testTypeFilter_noFilter();
    void testTypeFilter_description();
    
    void testCompletionFilter_completed();
    void testCompletionFilter_watching();
    void testCompletionFilter_notStarted();
    void testCompletionFilter_description();
    
    void testUnwatchedFilter_enabled();
    void testUnwatchedFilter_disabled();
    void testUnwatchedFilter_description();
    
    void testAdultContentFilter_hide();
    void testAdultContentFilter_showOnly();
    void testAdultContentFilter_ignore();
    void testAdultContentFilter_description();
    
    // CompositeFilter tests
    void testCompositeFilter_empty();
    void testCompositeFilter_singleFilter();
    void testCompositeFilter_multipleFilters();
    void testCompositeFilter_allPass();
    void testCompositeFilter_oneFails();
    void testCompositeFilter_description();
    void testCompositeFilter_clear();
    
private:
    AnimeMetadataCache* m_cache;
    
    // Helper to create cached data for testing
    MyListCardManager::CachedAnimeData createCachedData(
        const QString& title, const QString& type, int eptotal, bool is18,
        int normalEp, int normalViewed, int otherEp, int otherViewed);
};

void TestFilterClasses::initTestCase()
{
    m_cache = new AnimeMetadataCache();
}

void TestFilterClasses::cleanupTestCase()
{
    delete m_cache;
}

void TestFilterClasses::cleanup()
{
    m_cache->clear();
}

MyListCardManager::CachedAnimeData TestFilterClasses::createCachedData(
    const QString& title, const QString& type, int eptotal, bool is18,
    int normalEp, int normalViewed, int otherEp, int otherViewed)
{
    AnimeStats stats;
    stats.setNormalEpisodes(normalEp);
    stats.setNormalViewed(normalViewed);
    stats.setOtherEpisodes(otherEp);
    stats.setOtherViewed(otherViewed);
    stats.setTotalNormalEpisodes(eptotal);
    
    return MyListCardManager::CachedAnimeData(
        title,      // animeName
        type,       // typeName
        eptotal,    // eptotal
        is18,       // is18Restricted
        0,          // dateStarted
        0,          // dateFinished
        0,          // lastPlayed
        stats       // stats
    );
}

// ============================================================================
// AnimeDataAccessor Tests
// ============================================================================

void TestFilterClasses::testDataAccessor_withCachedData()
{
    auto cachedData = createCachedData("Test Anime", "TV Series", 26, false, 26, 10, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    QCOMPARE(accessor.getAnimeId(), 1);
    QCOMPARE(accessor.getTitle(), QString("Test Anime"));
    QCOMPARE(accessor.getType(), QString("TV Series"));
    QCOMPARE(accessor.getTotalEpisodes(), 26);
    QCOMPARE(accessor.getNormalEpisodes(), 26);
    QCOMPARE(accessor.getNormalViewed(), 10);
    QCOMPARE(accessor.getOtherEpisodes(), 0);
    QCOMPARE(accessor.getOtherViewed(), 0);
    QVERIFY(!accessor.is18Restricted());
}

void TestFilterClasses::testDataAccessor_hasData()
{
    auto cachedData = createCachedData("Test", "TV Series", 12, false, 12, 0, 0, 0);
    AnimeDataAccessor accessor1(1, nullptr, cachedData);
    QVERIFY(accessor1.hasData());
    
    // Empty cached data
    MyListCardManager::CachedAnimeData emptyData;
    AnimeDataAccessor accessor2(2, nullptr, emptyData);
    QVERIFY(!accessor2.hasData());
}

// ============================================================================
// SearchFilter Tests
// ============================================================================

void TestFilterClasses::testSearchFilter_matches()
{
    auto cachedData = createCachedData("Cowboy Bebop", "TV Series", 26, false, 26, 0, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    SearchFilter filter1("Cowboy", m_cache);
    QVERIFY(filter1.matches(accessor));
    
    SearchFilter filter2("Bebop", m_cache);
    QVERIFY(filter2.matches(accessor));
    
    SearchFilter filter3("cowboy bebop", m_cache);
    QVERIFY(filter3.matches(accessor));
}

void TestFilterClasses::testSearchFilter_noMatch()
{
    auto cachedData = createCachedData("Cowboy Bebop", "TV Series", 26, false, 26, 0, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    SearchFilter filter("Gundam", m_cache);
    QVERIFY(!filter.matches(accessor));
}

void TestFilterClasses::testSearchFilter_empty()
{
    auto cachedData = createCachedData("Any Title", "TV Series", 12, false, 12, 0, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    SearchFilter filter("", m_cache);
    QVERIFY(filter.matches(accessor));  // Empty search matches all
}

void TestFilterClasses::testSearchFilter_description()
{
    SearchFilter filter1("Test", m_cache);
    QVERIFY(filter1.description().contains("Test"));
    
    SearchFilter filter2("", m_cache);
    QVERIFY(filter2.description().contains("No search"));
}

// ============================================================================
// TypeFilter Tests
// ============================================================================

void TestFilterClasses::testTypeFilter_matches()
{
    auto cachedData = createCachedData("Test", "TV Series", 12, false, 12, 0, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    TypeFilter filter1("TV Series");
    QVERIFY(filter1.matches(accessor));
    
    TypeFilter filter2("Movie");
    QVERIFY(!filter2.matches(accessor));
}

void TestFilterClasses::testTypeFilter_noFilter()
{
    auto cachedData = createCachedData("Test", "TV Series", 12, false, 12, 0, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    TypeFilter filter("");  // Empty = no filter
    QVERIFY(filter.matches(accessor));
}

void TestFilterClasses::testTypeFilter_description()
{
    TypeFilter filter1("TV Series");
    QVERIFY(filter1.description().contains("TV Series"));
    
    TypeFilter filter2("");
    QVERIFY(filter2.description().contains("All types"));
}

// ============================================================================
// CompletionFilter Tests
// ============================================================================

void TestFilterClasses::testCompletionFilter_completed()
{
    auto cachedData = createCachedData("Test", "TV Series", 26, false, 26, 26, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    CompletionFilter filter("completed");
    QVERIFY(filter.matches(accessor));
    
    CompletionFilter filter2("watching");
    QVERIFY(!filter2.matches(accessor));
}

void TestFilterClasses::testCompletionFilter_watching()
{
    auto cachedData = createCachedData("Test", "TV Series", 26, false, 26, 10, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    CompletionFilter filter("watching");
    QVERIFY(filter.matches(accessor));
    
    CompletionFilter filter2("completed");
    QVERIFY(!filter2.matches(accessor));
    
    CompletionFilter filter3("notstarted");
    QVERIFY(!filter3.matches(accessor));
}

void TestFilterClasses::testCompletionFilter_notStarted()
{
    auto cachedData = createCachedData("Test", "TV Series", 26, false, 26, 0, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    CompletionFilter filter("notstarted");
    QVERIFY(filter.matches(accessor));
    
    CompletionFilter filter2("watching");
    QVERIFY(!filter2.matches(accessor));
}

void TestFilterClasses::testCompletionFilter_description()
{
    CompletionFilter filter1("completed");
    QCOMPARE(filter1.description(), QString("Completed"));
    
    CompletionFilter filter2("watching");
    QCOMPARE(filter2.description(), QString("Watching"));
    
    CompletionFilter filter3("notstarted");
    QCOMPARE(filter3.description(), QString("Not started"));
    
    CompletionFilter filter4("");
    QVERIFY(filter4.description().contains("All completion"));
}

// ============================================================================
// UnwatchedFilter Tests
// ============================================================================

void TestFilterClasses::testUnwatchedFilter_enabled()
{
    // Has unwatched episodes
    auto cachedData1 = createCachedData("Test", "TV Series", 26, false, 26, 10, 0, 0);
    AnimeDataAccessor accessor1(1, nullptr, cachedData1);
    
    UnwatchedFilter filter(true);
    QVERIFY(filter.matches(accessor1));
    
    // All watched
    auto cachedData2 = createCachedData("Test", "TV Series", 26, false, 26, 26, 0, 0);
    AnimeDataAccessor accessor2(2, nullptr, cachedData2);
    QVERIFY(!filter.matches(accessor2));
}

void TestFilterClasses::testUnwatchedFilter_disabled()
{
    auto cachedData = createCachedData("Test", "TV Series", 26, false, 26, 26, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    UnwatchedFilter filter(false);
    QVERIFY(filter.matches(accessor));  // Disabled filter matches all
}

void TestFilterClasses::testUnwatchedFilter_description()
{
    UnwatchedFilter filter1(true);
    QVERIFY(filter1.description().contains("unwatched"));
    
    UnwatchedFilter filter2(false);
    QVERIFY(filter2.description().contains("all"));
}

// ============================================================================
// AdultContentFilter Tests
// ============================================================================

void TestFilterClasses::testAdultContentFilter_hide()
{
    auto cachedData1 = createCachedData("Normal", "TV Series", 12, false, 12, 0, 0, 0);
    AnimeDataAccessor accessor1(1, nullptr, cachedData1);
    
    auto cachedData2 = createCachedData("Adult", "OVA", 6, true, 6, 0, 0, 0);
    AnimeDataAccessor accessor2(2, nullptr, cachedData2);
    
    AdultContentFilter filter("hide");
    QVERIFY(filter.matches(accessor1));   // Show normal content
    QVERIFY(!filter.matches(accessor2));  // Hide 18+ content
}

void TestFilterClasses::testAdultContentFilter_showOnly()
{
    auto cachedData1 = createCachedData("Normal", "TV Series", 12, false, 12, 0, 0, 0);
    AnimeDataAccessor accessor1(1, nullptr, cachedData1);
    
    auto cachedData2 = createCachedData("Adult", "OVA", 6, true, 6, 0, 0, 0);
    AnimeDataAccessor accessor2(2, nullptr, cachedData2);
    
    AdultContentFilter filter("showonly");
    QVERIFY(!filter.matches(accessor1));  // Hide normal content
    QVERIFY(filter.matches(accessor2));   // Show 18+ content
}

void TestFilterClasses::testAdultContentFilter_ignore()
{
    auto cachedData1 = createCachedData("Normal", "TV Series", 12, false, 12, 0, 0, 0);
    AnimeDataAccessor accessor1(1, nullptr, cachedData1);
    
    auto cachedData2 = createCachedData("Adult", "OVA", 6, true, 6, 0, 0, 0);
    AnimeDataAccessor accessor2(2, nullptr, cachedData2);
    
    AdultContentFilter filter("ignore");
    QVERIFY(filter.matches(accessor1));  // Show normal content
    QVERIFY(filter.matches(accessor2));  // Show 18+ content
}

void TestFilterClasses::testAdultContentFilter_description()
{
    AdultContentFilter filter1("hide");
    QVERIFY(filter1.description().contains("Hide 18+"));
    
    AdultContentFilter filter2("showonly");
    QVERIFY(filter2.description().contains("only 18+"));
    
    AdultContentFilter filter3("ignore");
    QVERIFY(filter3.description().contains("Ignore"));
}

// ============================================================================
// CompositeFilter Tests
// ============================================================================

void TestFilterClasses::testCompositeFilter_empty()
{
    auto cachedData = createCachedData("Test", "TV Series", 12, false, 12, 0, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    CompositeFilter composite;
    QVERIFY(composite.matches(accessor));  // Empty composite matches all
    QCOMPARE(composite.count(), 0);
}

void TestFilterClasses::testCompositeFilter_singleFilter()
{
    auto cachedData = createCachedData("Test", "TV Series", 12, false, 12, 0, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    CompositeFilter composite;
    composite.addFilter(new TypeFilter("TV Series"));
    
    QVERIFY(composite.matches(accessor));
    QCOMPARE(composite.count(), 1);
}

void TestFilterClasses::testCompositeFilter_multipleFilters()
{
    auto cachedData = createCachedData("Cowboy Bebop", "TV Series", 26, false, 26, 10, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    m_cache->addAnime(1, QStringList{"Cowboy Bebop"});
    
    CompositeFilter composite;
    composite.addFilter(new SearchFilter("Cowboy", m_cache));
    composite.addFilter(new TypeFilter("TV Series"));
    composite.addFilter(new CompletionFilter("watching"));
    
    QCOMPARE(composite.count(), 3);
    QVERIFY(composite.matches(accessor));
}

void TestFilterClasses::testCompositeFilter_allPass()
{
    auto cachedData = createCachedData("Test Anime", "TV Series", 26, false, 26, 10, 5, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    m_cache->addAnime(1, QStringList{"Test Anime"});
    
    CompositeFilter composite;
    composite.addFilter(new SearchFilter("Test", m_cache));
    composite.addFilter(new TypeFilter("TV Series"));
    composite.addFilter(new CompletionFilter("watching"));
    composite.addFilter(new UnwatchedFilter(true));
    composite.addFilter(new AdultContentFilter("hide"));
    
    QVERIFY(composite.matches(accessor));
}

void TestFilterClasses::testCompositeFilter_oneFails()
{
    auto cachedData = createCachedData("Test Anime", "Movie", 1, false, 1, 0, 0, 0);
    AnimeDataAccessor accessor(1, nullptr, cachedData);
    
    m_cache->addAnime(1, QStringList{"Test Anime"});
    
    CompositeFilter composite;
    composite.addFilter(new SearchFilter("Test", m_cache));  // Passes
    composite.addFilter(new TypeFilter("TV Series"));        // FAILS (is Movie)
    composite.addFilter(new CompletionFilter("notstarted")); // Passes
    
    QVERIFY(!composite.matches(accessor));  // Should fail because one filter fails
}

void TestFilterClasses::testCompositeFilter_description()
{
    CompositeFilter composite;
    composite.addFilter(new TypeFilter("TV Series"));
    composite.addFilter(new CompletionFilter("watching"));
    
    QString desc = composite.description();
    QVERIFY(desc.contains("TV Series"));
    QVERIFY(desc.contains("Watching"));
    QVERIFY(desc.contains("AND"));
}

void TestFilterClasses::testCompositeFilter_clear()
{
    CompositeFilter composite;
    composite.addFilter(new TypeFilter("TV Series"));
    composite.addFilter(new CompletionFilter("watching"));
    
    QCOMPARE(composite.count(), 2);
    
    composite.clear();
    QCOMPARE(composite.count(), 0);
}

QTEST_MAIN(TestFilterClasses)
#include "test_filter_classes.moc"
