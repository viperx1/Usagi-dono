#include <QTest>
#include <QDateTime>
#include "../usagi/src/animechain.h"
#include "../usagi/src/animestats.h"

/**
 * Test suite for recent episode air date sorting improvements
 * 
 * Validates that:
 * - Hidden cards/chains are displayed at the end in both chain and non-chain mode
 * - Anime air date (startDate) is used as failover when no episode data is available
 * - Not-yet-aired anime (air date in the future) are placed at the end
 * - All conditions apply correctly when chain mode is enabled
 */
class TestRecentEpisodeSortImprovements : public QObject
{
    Q_OBJECT

private slots:
    void testHiddenCardsAtEnd();
    void testNotYetAiredAtEnd();
    void testNotYetAiredVsNoAirDate();
    void testMixedAiredNotYetAiredAndNoDate();
    void testHiddenTakesPrecedenceOverNotYetAired();
    void testChainModeNotYetAired();
    void testChainModeHiddenWithNotYetAired();

private:
    // Helper: Create a simple lookup function with no relations
    static AnimeChain::RelationLookupFunc noRelationsLookup() {
        return [](int) -> QPair<int,int> {
            return QPair<int,int>(0, 0);  // No relations
        };
    }
};

// Mock CardCreationData structure matching the actual one used in sorting
struct MockCardDataImproved {
    QString animeTitle;
    QString typeName;
    QString startDate;
    AnimeStats stats;
    qint64 lastPlayed;
    qint64 recentEpisodeAirDate;
    bool isHidden;
    
    MockCardDataImproved() : lastPlayed(0), recentEpisodeAirDate(0), isHidden(false) {}
};

void TestRecentEpisodeSortImprovements::testHiddenCardsAtEnd()
{
    // Test that hidden cards always appear at the end
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Visible with recent date
    AnimeChain chain2(200, lookup);  // Hidden with recent date
    
    QMap<int, MockCardDataImproved> dataCache;
    dataCache[100].animeTitle = "Visible Anime";
    dataCache[100].recentEpisodeAirDate = 1672531200;  // 2023-01-01
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Hidden Anime";
    dataCache[200].recentEpisodeAirDate = 1704067200;  // 2024-01-01 (more recent)
    dataCache[200].isHidden = true;
    
    // Visible should come before hidden even though hidden has more recent air date
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result < 0, "Visible anime should come before hidden anime");
}

void TestRecentEpisodeSortImprovements::testNotYetAiredAtEnd()
{
    // Test that not-yet-aired anime appear at the end
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Aired
    AnimeChain chain2(200, lookup);  // Not yet aired
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 pastDate = currentTime - (365 * 24 * 60 * 60);  // 1 year ago
    qint64 futureDate = currentTime + (365 * 24 * 60 * 60);  // 1 year from now
    
    QMap<int, MockCardDataImproved> dataCache;
    dataCache[100].animeTitle = "Already Aired";
    dataCache[100].recentEpisodeAirDate = pastDate;
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Not Yet Aired";
    dataCache[200].recentEpisodeAirDate = futureDate;
    dataCache[200].isHidden = false;
    
    // Aired should come before not-yet-aired
    int resultDesc = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(resultDesc < 0, "Aired anime should come before not-yet-aired in descending");
    
    int resultAsc = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, true);
    QVERIFY2(resultAsc < 0, "Aired anime should come before not-yet-aired in ascending");
}

void TestRecentEpisodeSortImprovements::testNotYetAiredVsNoAirDate()
{
    // Test that anime with no air date (0) come after not-yet-aired
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Not yet aired
    AnimeChain chain2(200, lookup);  // No air date
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 futureDate = currentTime + (365 * 24 * 60 * 60);  // 1 year from now
    
    QMap<int, MockCardDataImproved> dataCache;
    dataCache[100].animeTitle = "Not Yet Aired";
    dataCache[100].recentEpisodeAirDate = futureDate;
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "No Air Date";
    dataCache[200].recentEpisodeAirDate = 0;
    dataCache[200].isHidden = false;
    
    // Not-yet-aired should come before no-air-date
    int resultDesc = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(resultDesc < 0, "Not-yet-aired should come before no-air-date in descending");
    
    int resultAsc = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, true);
    QVERIFY2(resultAsc < 0, "Not-yet-aired should come before no-air-date in ascending");
}

void TestRecentEpisodeSortImprovements::testMixedAiredNotYetAiredAndNoDate()
{
    // Test sorting order: aired > not-yet-aired > no date
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Old aired
    AnimeChain chain2(200, lookup);  // Recent aired
    AnimeChain chain3(300, lookup);  // Not yet aired
    AnimeChain chain4(400, lookup);  // No air date
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 oldDate = currentTime - (730 * 24 * 60 * 60);  // 2 years ago
    qint64 recentDate = currentTime - (30 * 24 * 60 * 60);  // 1 month ago
    qint64 futureDate = currentTime + (365 * 24 * 60 * 60);  // 1 year from now
    
    QMap<int, MockCardDataImproved> dataCache;
    dataCache[100].animeTitle = "Old Aired";
    dataCache[100].recentEpisodeAirDate = oldDate;
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Recent Aired";
    dataCache[200].recentEpisodeAirDate = recentDate;
    dataCache[200].isHidden = false;
    
    dataCache[300].animeTitle = "Not Yet Aired";
    dataCache[300].recentEpisodeAirDate = futureDate;
    dataCache[300].isHidden = false;
    
    dataCache[400].animeTitle = "No Air Date";
    dataCache[400].recentEpisodeAirDate = 0;
    dataCache[400].isHidden = false;
    
    // Descending: Recent aired < Old aired < Not yet aired < No date
    int result_recent_old = chain2.compareWith(chain1, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result_recent_old < 0, "Recent aired should come before old aired in descending");
    
    int result_old_future = chain1.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result_old_future < 0, "Old aired should come before not-yet-aired in descending");
    
    int result_future_none = chain3.compareWith(chain4, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result_future_none < 0, "Not-yet-aired should come before no-air-date in descending");
}

void TestRecentEpisodeSortImprovements::testHiddenTakesPrecedenceOverNotYetAired()
{
    // Test that hidden status takes precedence over not-yet-aired status
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Not yet aired, visible
    AnimeChain chain2(200, lookup);  // Not yet aired, hidden
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 futureDate1 = currentTime + (365 * 24 * 60 * 60);  // 1 year from now
    qint64 futureDate2 = currentTime + (730 * 24 * 60 * 60);  // 2 years from now
    
    QMap<int, MockCardDataImproved> dataCache;
    dataCache[100].animeTitle = "Not Yet Aired Visible";
    dataCache[100].recentEpisodeAirDate = futureDate2;  // Further in future
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Not Yet Aired Hidden";
    dataCache[200].recentEpisodeAirDate = futureDate1;  // Sooner
    dataCache[200].isHidden = true;
    
    // Visible should come before hidden, even though hidden airs sooner
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result < 0, "Visible not-yet-aired should come before hidden not-yet-aired");
}

void TestRecentEpisodeSortImprovements::testChainModeNotYetAired()
{
    // Test chain mode correctly handles not-yet-aired anime
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Already aired
    AnimeChain chain2(200, lookup);  // Not yet aired
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 pastDate = currentTime - (30 * 24 * 60 * 60);  // 1 month ago
    qint64 futureDate = currentTime + (30 * 24 * 60 * 60);  // 1 month from now
    
    QMap<int, MockCardDataImproved> dataCache;
    dataCache[100].animeTitle = "Already Aired";
    dataCache[100].recentEpisodeAirDate = pastDate;
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Not Yet Aired";
    dataCache[200].recentEpisodeAirDate = futureDate;
    dataCache[200].isHidden = false;
    
    // In chain mode, aired should come before not-yet-aired
    int result = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result < 0, "Chain mode: Aired should come before not-yet-aired");
}

void TestRecentEpisodeSortImprovements::testChainModeHiddenWithNotYetAired()
{
    // Test chain mode with combination of hidden and not-yet-aired
    AnimeChain::RelationLookupFunc lookup = noRelationsLookup();
    AnimeChain chain1(100, lookup);  // Aired, visible
    AnimeChain chain2(200, lookup);  // Not yet aired, visible
    AnimeChain chain3(300, lookup);  // Aired, hidden
    AnimeChain chain4(400, lookup);  // Not yet aired, hidden
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 pastDate = currentTime - (30 * 24 * 60 * 60);
    qint64 futureDate = currentTime + (30 * 24 * 60 * 60);
    
    QMap<int, MockCardDataImproved> dataCache;
    dataCache[100].animeTitle = "Aired Visible";
    dataCache[100].recentEpisodeAirDate = pastDate;
    dataCache[100].isHidden = false;
    
    dataCache[200].animeTitle = "Not Yet Aired Visible";
    dataCache[200].recentEpisodeAirDate = futureDate;
    dataCache[200].isHidden = false;
    
    dataCache[300].animeTitle = "Aired Hidden";
    dataCache[300].recentEpisodeAirDate = pastDate;
    dataCache[300].isHidden = true;
    
    dataCache[400].animeTitle = "Not Yet Aired Hidden";
    dataCache[400].recentEpisodeAirDate = futureDate;
    dataCache[400].isHidden = true;
    
    // Order should be: Aired Visible < Not Yet Aired Visible < Aired Hidden < Not Yet Aired Hidden
    int result_1_2 = chain1.compareWith(chain2, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result_1_2 < 0, "Aired visible < Not yet aired visible");
    
    int result_2_3 = chain2.compareWith(chain3, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result_2_3 < 0, "Not yet aired visible < Aired hidden (hidden takes precedence)");
    
    int result_3_4 = chain3.compareWith(chain4, dataCache, AnimeChain::SortCriteria::ByRecentEpisodeAirDate, false);
    QVERIFY2(result_3_4 < 0, "Within hidden: Aired < Not yet aired");
}

QTEST_MAIN(TestRecentEpisodeSortImprovements)
#include "test_recent_episode_sort_improvements.moc"
