#include <QTest>
#include "../usagi/src/animechain.h"
#include "../usagi/src/animestats.h"

/**
 * Test suite for recent episode air date sorting
 * 
 * Validates that:
 * - Anime with more recent episode air dates sort higher (when descending)
 * - Anime with no air date (0) are placed at the end
 * - Hidden chains are still placed at the end regardless of air date
 */
class TestRecentEpisodeAirDateSort : public QObject
{
    Q_OBJECT

private slots:
    void testBasicAirDateSortAscending();
    void testBasicAirDateSortDescending();
    void testZeroAirDateGoesToEnd();
    void testHiddenChainWithRecentAirDate();

private:
    // Helper: Create a simple lookup function with no relations
    static AnimeChain::RelationLookupFunc noRelationsLookup() {
        return [](int) -> QPair<int,int> {
            return QPair<int,int>(0, 0);  // No relations
        };
    }
};

// Mock CardCreationData structure matching the actual one used in sorting
struct MockCardDataAirDate {
    QString animeTitle;
    QString typeName;
    QString startDate;
    AnimeStats stats;
    qint64 lastPlayed;
    qint64 recentEpisodeAirDate;
    bool isHidden;
    
    MockCardDataAirDate() : lastPlayed(0), recentEpisodeAirDate(0), isHidden(false) {}
};

void TestRecentEpisodeAirDateSort::testBasicAirDateSortAscending()
{
    // Create three chains with different air dates
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Oldest
    AnimeChain chain2(200, lookup);  // Middle
    AnimeChain chain3(300, lookup);  // Newest
    
    // Create mock data cache
    QMap<int, MockCardDataAirDate> dataCache;
    dataCache[100].animeTitle = "Old Anime";
    dataCache[100].recentEpisodeAirDate = 1609459200;  // 2021-01-01
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Middle Anime";
    dataCache[200].recentEpisodeAirDate = 1640995200;  // 2022-01-01
    dataCache[200].isHidden = false;
    
    dataCache[300].animeTitle = "Recent Anime";
    dataCache[300].recentEpisodeAirDate = 1672531200;  // 2023-01-01
    dataCache[300].isHidden = false;
    
    // Test ascending: oldest should come first
    int result12 = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, true);
    QVERIFY2(result12 < 0, "Old Anime (2021) < Middle Anime (2022) in ascending");
    
    int result23 = chain2.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, true);
    QVERIFY2(result23 < 0, "Middle Anime (2022) < Recent Anime (2023) in ascending");
}

void TestRecentEpisodeAirDateSort::testBasicAirDateSortDescending()
{
    // Create three chains with different air dates
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Oldest
    AnimeChain chain2(200, lookup);  // Middle
    AnimeChain chain3(300, lookup);  // Newest
    
    // Create mock data cache
    QMap<int, MockCardDataAirDate> dataCache;
    dataCache[100].animeTitle = "Old Anime";
    dataCache[100].recentEpisodeAirDate = 1609459200;  // 2021-01-01
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Middle Anime";
    dataCache[200].recentEpisodeAirDate = 1640995200;  // 2022-01-01
    dataCache[200].isHidden = false;
    
    dataCache[300].animeTitle = "Recent Anime";
    dataCache[300].recentEpisodeAirDate = 1672531200;  // 2023-01-01
    dataCache[300].isHidden = false;
    
    // Test descending: newest should come first
    int result32 = chain3.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result32 < 0, "Recent Anime (2023) < Middle Anime (2022) in descending");
    
    int result21 = chain2.compareWith(chain1, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result21 < 0, "Middle Anime (2022) < Old Anime (2021) in descending");
}

void TestRecentEpisodeAirDateSort::testZeroAirDateGoesToEnd()
{
    // Create two chains: one with air date, one without
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Has air date
    AnimeChain chain2(200, lookup);  // No air date
    
    // Create mock data cache
    QMap<int, MockCardDataAirDate> dataCache;
    dataCache[100].animeTitle = "Aired Anime";
    dataCache[100].recentEpisodeAirDate = 1672531200;  // 2023-01-01
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Not Aired Anime";
    dataCache[200].recentEpisodeAirDate = 0;  // No air date
    dataCache[200].isHidden = false;
    
    // Test ascending: anime with air date should come before anime without
    int result12 = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, true);
    QVERIFY2(result12 < 0, "Aired Anime < Not Aired Anime in ascending (no air date goes to end)");
    
    // Test descending: anime with air date should still come before anime without
    int result12Desc = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result12Desc < 0, "Aired Anime < Not Aired Anime in descending (no air date goes to end)");
}

void TestRecentEpisodeAirDateSort::testHiddenChainWithRecentAirDate()
{
    // Create two chains: one visible with old date, one hidden with recent date
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Visible, old
    AnimeChain chain2(200, lookup);  // Hidden, recent
    
    // Create mock data cache
    QMap<int, MockCardDataAirDate> dataCache;
    dataCache[100].animeTitle = "Visible Old Anime";
    dataCache[100].recentEpisodeAirDate = 1609459200;  // 2021-01-01
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Hidden Recent Anime";
    dataCache[200].recentEpisodeAirDate = 1672531200;  // 2023-01-01 (more recent)
    dataCache[200].isHidden = true;
    
    // Test: visible should come before hidden, regardless of air date
    int result12 = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, true);
    QVERIFY2(result12 < 0, "Visible Old Anime < Hidden Recent Anime (hidden goes to end)");
    
    int result12Desc = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result12Desc < 0, "Visible Old Anime < Hidden Recent Anime in descending (hidden still goes to end)");
}

QTEST_MAIN(TestRecentEpisodeAirDateSort)
#include "test_recent_episode_air_date_sort.moc"
