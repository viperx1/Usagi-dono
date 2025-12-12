#include <QTest>
#include "../usagi/src/animechain.h"
#include "../usagi/src/animestats.h"

/**
 * Test suite for AnimeChain sorting with various criteria
 * 
 * Validates that all sorting criteria work correctly when series chain is enabled
 */
class TestChainSorting : public QObject
{
    Q_OBJECT

private slots:
    void testSortByTitle();
    void testSortByType();
    void testSortByDate();
    void testSortByEpisodeCount();
    void testSortByCompletion();
    void testSortByLastPlayed();
};

// Mock CardCreationData structure for testing
// Matches the actual CardCreationData structure fields used in sorting
struct MockCardData {
    QString animeTitle;
    QString typeName;
    QString startDate;
    AnimeStats stats;
    qint64 lastPlayed;
};

void TestChainSorting::testSortByTitle()
{
    // Create relation lookup (chains with single anime for simplicity)
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        return QPair<int,int>(0, 0);  // No relations
    };
    
    // Create two simple chains
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    
    // Create mock data cache
    QMap<int, MockCardData> dataCache;
    dataCache[100].animeTitle = "Zetman";
    dataCache[200].animeTitle = "Attack on Titan";
    
    // Test ascending
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    QVERIFY(result > 0);  // "Zetman" > "Attack on Titan" (alphabetically)
    
    // Test descending
    result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, false);
    QVERIFY(result < 0);  // In descending, "Zetman" < "Attack on Titan" (reversed)
}

void TestChainSorting::testSortByType()
{
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        return QPair<int,int>(0, 0);
    };
    
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].typeName = "TV Series";
    dataCache[200].typeName = "Movie";
    
    // Test ascending
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeType, true);
    QVERIFY(result > 0);  // "TV Series" > "Movie" (alphabetically)
    
    // Test descending
    result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeType, false);
    QVERIFY(result < 0);
}

void TestChainSorting::testSortByDate()
{
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        return QPair<int,int>(0, 0);
    };
    
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].startDate = "2020-01-01";
    dataCache[200].startDate = "2021-06-15";
    
    // Test ascending
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeDate, true);
    QVERIFY(result < 0);  // 2020 < 2021
    
    // Test descending
    result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeDate, false);
    QVERIFY(result > 0);  // In descending, 2020 > 2021
}

void TestChainSorting::testSortByEpisodeCount()
{
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        return QPair<int,int>(0, 0);
    };
    
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    
    QMap<int, MockCardData> dataCache;
    // Chain 1: 12 normal + 1 special = 13 total episodes
    dataCache[100].stats.setNormalEpisodes(12);
    dataCache[100].stats.setOtherEpisodes(1);
    
    // Chain 2: 24 normal + 0 special = 24 total episodes
    dataCache[200].stats.setNormalEpisodes(24);
    dataCache[200].stats.setOtherEpisodes(0);
    
    // Test ascending
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeEpisodeCount, true);
    QVERIFY(result < 0);  // 13 < 24
    
    // Test descending
    result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeEpisodeCount, false);
    QVERIFY(result > 0);  // In descending, 13 > 24
}

void TestChainSorting::testSortByCompletion()
{
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        return QPair<int,int>(0, 0);
    };
    
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    
    QMap<int, MockCardData> dataCache;
    // Chain 1: 6 of 12 watched = 50% completion
    dataCache[100].stats.setNormalEpisodes(12);
    dataCache[100].stats.setNormalViewed(6);
    dataCache[100].stats.setOtherEpisodes(0);
    dataCache[100].stats.setOtherViewed(0);
    
    // Chain 2: 20 of 24 watched = ~83% completion
    dataCache[200].stats.setNormalEpisodes(24);
    dataCache[200].stats.setNormalViewed(20);
    dataCache[200].stats.setOtherEpisodes(0);
    dataCache[200].stats.setOtherViewed(0);
    
    // Test ascending
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeCompletion, true);
    QVERIFY(result < 0);  // 50% < 83%
    
    // Test descending
    result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeCompletion, false);
    QVERIFY(result > 0);  // In descending, 50% > 83%
}

void TestChainSorting::testSortByLastPlayed()
{
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        return QPair<int,int>(0, 0);
    };
    
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    AnimeChain chain3(300, lookup);
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].lastPlayed = 1000000;  // Played recently
    dataCache[200].lastPlayed = 500000;   // Played earlier
    dataCache[300].lastPlayed = 0;        // Never played
    
    // Test ascending: older first
    int result12 = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeLastPlayed, true);
    QVERIFY(result12 > 0);  // 1000000 > 500000
    
    // Test descending: newer first
    result12 = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeLastPlayed, false);
    QVERIFY(result12 < 0);  // In descending, 1000000 < 500000
    
    // Test that never-played always goes to end (ascending)
    int result13_asc = chain1.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeLastPlayed, true);
    QVERIFY(result13_asc < 0);  // Played < Never played (unplayed at end)
    
    // Test that never-played always goes to end (descending)
    int result13_desc = chain1.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeLastPlayed, false);
    QVERIFY(result13_desc < 0);  // Played < Never played (unplayed always at end regardless of order)
    
    // Test reverse direction
    int result31_asc = chain3.compareWith(chain1, dataCache, AnimeChain::SortCriteria::ByRepresentativeLastPlayed, true);
    QVERIFY(result31_asc > 0);  // Never played > Played (unplayed at end)
    
    int result31_desc = chain3.compareWith(chain1, dataCache, AnimeChain::SortCriteria::ByRepresentativeLastPlayed, false);
    QVERIFY(result31_desc > 0);  // Never played > Played (unplayed always at end)
}

QTEST_MAIN(TestChainSorting)
#include "test_chain_sorting.moc"
