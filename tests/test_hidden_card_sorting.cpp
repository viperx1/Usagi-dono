#include <QTest>
#include "../usagi/src/animechain.h"
#include "../usagi/src/animestats.h"

/**
 * Test suite for hidden card sorting behavior
 * 
 * Validates that:
 * - In chain mode, chains where ALL anime are hidden are displayed at the end
 * - In chain mode, chains with at least one non-hidden anime are sorted normally (not moved to end)
 * - Within each chain, anime maintain their prequel→sequel relation order regardless of hidden status
 * - This chain-level hidden behavior is consistent across all sort criteria
 * - In non-chain mode (tested elsewhere), individual hidden cards go to the end
 */
class TestHiddenCardSorting : public QObject
{
    Q_OBJECT

private slots:
    void testHiddenChainByTitle();
    void testHiddenChainByType();
    void testHiddenChainByDate();
    void testHiddenChainByEpisodeCount();
    void testHiddenChainByCompletion();
    void testHiddenChainByLastPlayed();
    void testMixedHiddenChain();
    void testAllHiddenChain();
    void testAllVisibleChains();
    void testMissingDataTreatedAsVisible();
    void testHiddenCardsWithinChainKeepRelationOrder();

private:
    // Helper: Create a simple lookup function with no relations
    static AnimeChain::RelationLookupFunc noRelationsLookup() {
        return [](int) -> QPair<int,int> {
            return QPair<int,int>(0, 0);  // No relations
        };
    }
};

// Mock CardCreationData structure matching the actual one used in sorting
struct MockCardData {
    QString animeTitle;
    QString typeName;
    QString startDate;
    AnimeStats stats;
    qint64 lastPlayed;
    qint64 recentEpisodeAirDate;
    bool isHidden;
    
    MockCardData() : lastPlayed(0), recentEpisodeAirDate(0), isHidden(false) {}
};

void TestHiddenCardSorting::testHiddenChainByTitle()
{
    // Create three chains
    auto lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Visible
    AnimeChain chain2(200, lookup);  // Visible
    AnimeChain chain3(300, lookup);  // Hidden
    
    // Create mock data cache
    QMap<int, MockCardData> dataCache;
    dataCache[100].animeTitle = "Attack on Titan";
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Zetman";
    dataCache[200].isHidden = false;
    
    dataCache[300].animeTitle = "Berserk";  // Alphabetically would be first
    dataCache[300].isHidden = true;          // But it's hidden
    
    // Test ascending: visible chains sorted normally, hidden chain at end
    int result12 = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    QVERIFY2(result12 < 0, "Attack on Titan < Zetman (both visible)");
    
    int result13 = chain1.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    QVERIFY2(result13 < 0, "Attack on Titan (visible) < Berserk (hidden) - hidden goes to end");
    
    int result23 = chain2.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    QVERIFY2(result23 < 0, "Zetman (visible) < Berserk (hidden) - hidden goes to end");
    
    // Test descending: visible chains sorted normally (reversed), hidden chain still at end
    result12 = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, false);
    QVERIFY2(result12 > 0, "In descending: Attack on Titan > Zetman (both visible)");
    
    result13 = chain1.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, false);
    QVERIFY2(result13 < 0, "In descending: Attack on Titan (visible) < Berserk (hidden) - hidden still at end");
    
    result23 = chain2.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, false);
    QVERIFY2(result23 < 0, "In descending: Zetman (visible) < Berserk (hidden) - hidden still at end");
}

void TestHiddenCardSorting::testHiddenChainByType()
{
    auto lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    AnimeChain chain3(300, lookup);
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].typeName = "TV Series";
    dataCache[100].isHidden = false;
    
    dataCache[200].typeName = "OVA";
    dataCache[200].isHidden = false;
    
    dataCache[300].typeName = "Movie";  // Alphabetically would be in middle
    dataCache[300].isHidden = true;
    
    // Test ascending
    int result13 = chain1.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeType, true);
    QVERIFY2(result13 < 0, "TV Series (visible) < Movie (hidden) - hidden goes to end");
    
    // Test descending
    result13 = chain1.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeType, false);
    QVERIFY2(result13 < 0, "In descending: TV Series (visible) < Movie (hidden) - hidden still at end");
}

void TestHiddenCardSorting::testHiddenChainByDate()
{
    auto lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    AnimeChain chain3(300, lookup);
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].startDate = "2020-01-01";
    dataCache[100].isHidden = false;
    
    dataCache[200].startDate = "2022-06-15";
    dataCache[200].isHidden = false;
    
    dataCache[300].startDate = "2010-03-20";  // Oldest, would normally be first
    dataCache[300].isHidden = true;
    
    // Test ascending
    int result13 = chain1.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeDate, true);
    QVERIFY2(result13 < 0, "2020 (visible) < 2010 (hidden) - hidden goes to end");
    
    // Test descending
    result13 = chain1.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRepresentativeDate, false);
    QVERIFY2(result13 < 0, "In descending: 2020 (visible) < 2010 (hidden) - hidden still at end");
}

void TestHiddenCardSorting::testHiddenChainByEpisodeCount()
{
    auto lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].stats.setNormalEpisodes(12);
    dataCache[100].stats.setOtherEpisodes(0);
    dataCache[100].isHidden = false;
    
    dataCache[200].stats.setNormalEpisodes(100);  // More episodes
    dataCache[200].stats.setOtherEpisodes(0);
    dataCache[200].isHidden = true;
    
    // Test ascending
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeEpisodeCount, true);
    QVERIFY2(result < 0, "12 episodes (visible) < 100 episodes (hidden) - hidden goes to end");
    
    // Test descending
    result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeEpisodeCount, false);
    QVERIFY2(result < 0, "In descending: 12 episodes (visible) < 100 episodes (hidden) - hidden still at end");
}

void TestHiddenCardSorting::testHiddenChainByCompletion()
{
    auto lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    
    QMap<int, MockCardData> dataCache;
    // Chain 1: 50% completion, visible
    dataCache[100].stats.setNormalEpisodes(12);
    dataCache[100].stats.setNormalViewed(6);
    dataCache[100].stats.setOtherEpisodes(0);
    dataCache[100].stats.setOtherViewed(0);
    dataCache[100].isHidden = false;
    
    // Chain 2: 100% completion, hidden
    dataCache[200].stats.setNormalEpisodes(24);
    dataCache[200].stats.setNormalViewed(24);
    dataCache[200].stats.setOtherEpisodes(0);
    dataCache[200].stats.setOtherViewed(0);
    dataCache[200].isHidden = true;
    
    // Test ascending
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeCompletion, true);
    QVERIFY2(result < 0, "50% (visible) < 100% (hidden) - hidden goes to end");
    
    // Test descending
    result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeCompletion, false);
    QVERIFY2(result < 0, "In descending: 50% (visible) < 100% (hidden) - hidden still at end");
}

void TestHiddenCardSorting::testHiddenChainByLastPlayed()
{
    auto lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].lastPlayed = 1000000;  // Played recently
    dataCache[100].isHidden = false;
    
    dataCache[200].lastPlayed = 2000000;  // Played more recently
    dataCache[200].isHidden = true;
    
    // Test ascending
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeLastPlayed, true);
    QVERIFY2(result < 0, "Older timestamp (visible) < Newer timestamp (hidden) - hidden goes to end");
    
    // Test descending
    result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeLastPlayed, false);
    QVERIFY2(result < 0, "In descending: Older timestamp (visible) < Newer timestamp (hidden) - hidden still at end");
}

void TestHiddenCardSorting::testMixedHiddenChain()
{
    // Test a chain with multiple anime where some are hidden and some are not
    // If at least one anime in the chain is visible, the entire chain should NOT go to end
    
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        if (aid == 100) return QPair<int,int>(0, 101);  // 100 -> 101 (sequel)
        if (aid == 101) return QPair<int,int>(100, 102); // 101 has prequel 100, sequel 102
        if (aid == 102) return QPair<int,int>(101, 0);   // 102 has prequel 101
        return QPair<int,int>(0, 0);
    };
    
    // Create a chain with three anime: 100 (visible) -> 101 (hidden) -> 102 (hidden)
    AnimeChain mixedChain(100, lookup);
    mixedChain.expand(lookup);
    
    // Create a fully hidden chain for comparison
    AnimeChain hiddenChain(200, noRelationsLookup());
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].animeTitle = "Series Part 1";
    dataCache[100].isHidden = false;  // Visible
    
    dataCache[101].animeTitle = "Series Part 2";
    dataCache[101].isHidden = true;   // Hidden
    
    dataCache[102].animeTitle = "Series Part 3";
    dataCache[102].isHidden = true;   // Hidden
    
    dataCache[200].animeTitle = "Completely Hidden Series";
    dataCache[200].isHidden = true;   // Hidden
    
    // Mixed chain (has at least one visible) should come before fully hidden chain
    int result = mixedChain.compareWith(hiddenChain, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    QVERIFY2(result < 0, "Mixed chain (has visible anime) < Fully hidden chain");
    
    // Test descending - same behavior
    result = mixedChain.compareWith(hiddenChain, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, false);
    QVERIFY2(result < 0, "In descending: Mixed chain (has visible anime) < Fully hidden chain");
}

void TestHiddenCardSorting::testAllHiddenChain()
{
    // Test a chain where ALL anime are hidden - should go to end
    
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        if (aid == 100) return QPair<int,int>(0, 101);
        if (aid == 101) return QPair<int,int>(100, 0);
        return QPair<int,int>(0, 0);
    };
    
    // Create a chain with two hidden anime
    AnimeChain allHiddenChain(100, lookup);
    allHiddenChain.expand(lookup);
    
    // Create a visible chain for comparison
    AnimeChain visibleChain(200, noRelationsLookup());
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].animeTitle = "Hidden Part 1";
    dataCache[100].isHidden = true;
    
    dataCache[101].animeTitle = "Hidden Part 2";
    dataCache[101].isHidden = true;
    
    dataCache[200].animeTitle = "Visible Series";
    dataCache[200].isHidden = false;
    
    // Visible chain should come before all-hidden chain
    int result = visibleChain.compareWith(allHiddenChain, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    QVERIFY2(result < 0, "Visible chain < All-hidden chain");
    
    // Test descending - same behavior
    result = visibleChain.compareWith(allHiddenChain, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, false);
    QVERIFY2(result < 0, "In descending: Visible chain < All-hidden chain");
}

void TestHiddenCardSorting::testAllVisibleChains()
{
    // Test that when all chains are visible, normal sorting applies
    
    auto lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);
    AnimeChain chain2(200, lookup);
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].animeTitle = "Zetman";
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Attack on Titan";
    dataCache[200].isHidden = false;
    
    // Test ascending - normal alphabetical order
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    QVERIFY2(result > 0, "Zetman > Attack on Titan (both visible, normal sort)");
    
    // Test descending - reversed
    result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, false);
    QVERIFY2(result < 0, "In descending: Zetman < Attack on Titan (both visible, reversed)");
}

void TestHiddenCardSorting::testMissingDataTreatedAsVisible()
{
    // Test that anime with missing data is treated as visible (safe default)
    
    auto lookup = noRelationsLookup();
    AnimeChain chainWithMissingData(100, lookup);
    AnimeChain hiddenChain(200, lookup);
    
    QMap<int, MockCardData> dataCache;
    // Note: No data for anime 100 (missing from cache)
    
    dataCache[200].animeTitle = "Hidden Series";
    dataCache[200].isHidden = true;
    
    // Chain with missing data should be treated as visible and come before hidden chain
    int result = chainWithMissingData.compareWith(hiddenChain, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    QVERIFY2(result < 0, "Chain with missing data (treated as visible) < Hidden chain");
}

void TestHiddenCardSorting::testHiddenCardsWithinChainKeepRelationOrder()
{
    // Test that hidden cards WITHIN a non-fully-hidden chain maintain their relation order
    // This verifies that hidden status doesn't affect INTERNAL chain ordering, only EXTERNAL chain sorting
    
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        if (aid == 100) return QPair<int,int>(0, 101);    // 100 -> 101 (sequel)
        if (aid == 101) return QPair<int,int>(100, 102);  // 100 <- 101 -> 102
        if (aid == 102) return QPair<int,int>(101, 103);  // 101 <- 102 -> 103
        if (aid == 103) return QPair<int,int>(102, 0);    // 102 <- 103
        return QPair<int,int>(0, 0);
    };
    
    // Create a chain: 100 (visible) -> 101 (hidden) -> 102 (hidden) -> 103 (visible)
    // After orderChain: 100 (visible) -> 101 (hidden) -> 102 (hidden) -> 103 (visible)
    AnimeChain mixedChain(100, lookup);
    mixedChain.expand(lookup);
    
    QMap<int, MockCardData> dataCache;
    dataCache[100].isHidden = false;  // Visible
    dataCache[101].isHidden = true;   // Hidden
    dataCache[102].isHidden = true;   // Hidden
    dataCache[103].isHidden = false;  // Visible
    
    // Verify the chain contains all 4 anime
    QList<int> chainAnimeIds = mixedChain.getAnimeIds();
    QCOMPARE(chainAnimeIds.size(), 4);
    
    // Verify the chain is ordered by relation (prequel->sequel), NOT by hidden status
    // Expected order: 100 -> 101 -> 102 -> 103
    QVERIFY2(chainAnimeIds.indexOf(100) < chainAnimeIds.indexOf(101), 
             "100 (visible) comes before 101 (hidden) - relation order preserved");
    QVERIFY2(chainAnimeIds.indexOf(101) < chainAnimeIds.indexOf(102), 
             "101 (hidden) comes before 102 (hidden) - relation order preserved");
    QVERIFY2(chainAnimeIds.indexOf(102) < chainAnimeIds.indexOf(103), 
             "102 (hidden) comes before 103 (visible) - relation order preserved");
    
    // This chain has visible anime (100 and 103), so it should be sorted normally (not moved to end)
    // when compared with a fully hidden chain
    AnimeChain fullyHiddenChain(200, noRelationsLookup());
    dataCache[200].animeTitle = "Fully Hidden";
    dataCache[200].isHidden = true;
    
    int result = mixedChain.compareWith(fullyHiddenChain, dataCache, AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    QVERIFY2(result < 0, "Mixed chain (has visible anime) < Fully hidden chain - chain-level sorting works");
}

QTEST_MAIN(TestHiddenCardSorting)
#include "test_hidden_card_sorting.moc"
