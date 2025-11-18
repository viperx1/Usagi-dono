/**
 * Test to verify episode column formatting in mylist anime rows
 * 
 * Issue: "mylist" - Episode column pattern should be:
 * "owned_normal_episodes/total_normal_episodes+owned_non_normal_episodes"
 * 
 * This test verifies that the episode column displays the correct format
 * based on episode counts from mylist.
 * 
 * Note: totalNormalEpisodes is from the anime's Eps attribute (normal episodes only),
 * not EpsTotal (which includes all episode types including specials).
 */

#include <QTest>
#include <QString>

class TestEpisodeColumnFormat : public QObject
{
    Q_OBJECT

private slots:
    void testFormatWithAllData();
    void testFormatWithOnlyNormalEpisodes();
    void testFormatWithoutEps();
    void testFormatWithOnlyOtherEpisodes();
    void testFormatWithNoEpisodes();
    void testMovieWithSpecials();
    
private:
    QString formatEpisodeColumn(int normalEpisodes, int totalNormalEpisodes, int otherEpisodes);
};

QString TestEpisodeColumnFormat::formatEpisodeColumn(int normalEpisodes, int totalNormalEpisodes, int otherEpisodes)
{
    // This replicates the logic from Window::loadMylistFromDatabase() episode column formatting
    // When totalNormalEpisodes is not available (0), show "?" to indicate unknown total
    QString episodeText;
    if(totalNormalEpisodes > 0)
    {
        if(otherEpisodes > 0)
        {
            episodeText = QString("%1/%2+%3").arg(normalEpisodes).arg(totalNormalEpisodes).arg(otherEpisodes);
        }
        else
        {
            episodeText = QString("%1/%2").arg(normalEpisodes).arg(totalNormalEpisodes);
        }
    }
    else
    {
        // If eps is not available, show "?" to indicate unknown total instead of using same value
        if(otherEpisodes > 0)
        {
            episodeText = QString("%1/?+%2").arg(normalEpisodes).arg(otherEpisodes);
        }
        else
        {
            episodeText = QString("%1/?").arg(normalEpisodes);
        }
    }
    return episodeText;
}

void TestEpisodeColumnFormat::testFormatWithAllData()
{
    // Scenario: Anime with normal and special episodes
    // 10 normal episodes in mylist out of 12 total, plus 2 specials
    QString result = formatEpisodeColumn(10, 12, 2);
    QCOMPARE(result, QString("10/12+2"));
    
    // Scenario: Complete anime with extras
    // All 12 normal episodes plus 2 other types
    result = formatEpisodeColumn(12, 12, 2);
    QCOMPARE(result, QString("12/12+2"));
    
    // Scenario: Partial collection with many extras
    // 100 of 200 normal episodes plus 15 other types
    result = formatEpisodeColumn(100, 200, 15);
    QCOMPARE(result, QString("100/200+15"));
}

void TestEpisodeColumnFormat::testFormatWithOnlyNormalEpisodes()
{
    // Scenario: Anime with only normal episodes (no specials/OVAs)
    // 5 normal episodes in mylist out of 12 total
    QString result = formatEpisodeColumn(5, 12, 0);
    QCOMPARE(result, QString("5/12"));
    
    // Scenario: Complete anime with no extras
    // All 26 normal episodes, no other types
    result = formatEpisodeColumn(26, 26, 0);
    QCOMPARE(result, QString("26/26"));
    
    // Scenario: Single episode
    result = formatEpisodeColumn(1, 1, 0);
    QCOMPARE(result, QString("1/1"));
}

void TestEpisodeColumnFormat::testFormatWithoutEps()
{
    // Scenario: Ongoing series where eps is not available (0)
    // 50 normal episodes plus 5 other types in mylist
    // Should display "50/?+5" to indicate unknown total
    QString result = formatEpisodeColumn(50, 0, 5);
    QCOMPARE(result, QString("50/?+5"));
    
    // Scenario: Unknown total, only normal episodes
    // 15 normal episodes, no other types, eps unknown
    // Should display "15/?" to indicate unknown total
    result = formatEpisodeColumn(15, 0, 0);
    QCOMPARE(result, QString("15/?"));
    
    // Scenario: Only specials/OVAs collected
    // 0 normal episodes, 3 other types, eps unknown
    // Should display "0/?+3" to indicate unknown total
    result = formatEpisodeColumn(0, 0, 3);
    QCOMPARE(result, QString("0/?+3"));
}

void TestEpisodeColumnFormat::testMovieWithSpecials()
{
    // Scenario: Movie (Evangelion Shin Gekijouban: Ha)
    // Has 1 normal episode (the movie), user owns it, no specials in mylist
    // Anime has Eps=1 (normal), EpsSpecial=20, EpsTotal=23
    // Should show "1/1" not "1/23"
    QString result = formatEpisodeColumn(1, 1, 0);
    QCOMPARE(result, QString("1/1"));
    
    // Scenario: Movie with specials in mylist
    // 1 normal episode out of 1 total, plus 2 specials
    result = formatEpisodeColumn(1, 1, 2);
    QCOMPARE(result, QString("1/1+2"));
    
    // Scenario: OVA series with single main episode
    // 1 normal episode out of 1, plus 5 specials
    result = formatEpisodeColumn(1, 1, 5);
    QCOMPARE(result, QString("1/1+5"));
}

void TestEpisodeColumnFormat::testFormatWithOnlyOtherEpisodes()
{
    // Scenario: Anime where user only has specials/OVAs
    // 0 normal episodes out of 12 total, but 2 specials
    QString result = formatEpisodeColumn(0, 12, 2);
    QCOMPARE(result, QString("0/12+2"));
    
    // Scenario: OVA-only collection
    // 0 normal out of 6 total, 6 OVAs
    result = formatEpisodeColumn(0, 6, 6);
    QCOMPARE(result, QString("0/6+6"));
}

void TestEpisodeColumnFormat::testFormatWithNoEpisodes()
{
    // Edge case: No episodes at all (shouldn't normally happen)
    // Should display "0/?" to indicate unknown total
    QString result = formatEpisodeColumn(0, 0, 0);
    QCOMPARE(result, QString("0/?"));
    
    // Edge case: Only eptotal available, no mylist entries yet
    result = formatEpisodeColumn(0, 12, 0);
    QCOMPARE(result, QString("0/12"));
}

QTEST_MAIN(TestEpisodeColumnFormat)
#include "test_episode_column_format.moc"
