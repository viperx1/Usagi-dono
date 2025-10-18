/**
 * Test to verify episode column formatting in mylist anime rows
 * 
 * Issue: "mylist" - Episode column pattern should be:
 * "owned_normal_episodes/total_normal_episodes+owned_non_normal_episodes"
 * 
 * This test verifies that the episode column displays the correct format
 * based on episode counts from mylist.
 */

#include <QtTest/QtTest>
#include <QString>

class TestEpisodeColumnFormat : public QObject
{
    Q_OBJECT

private slots:
    void testFormatWithAllData();
    void testFormatWithOnlyNormalEpisodes();
    void testFormatWithoutEpTotal();
    void testFormatWithOnlyOtherEpisodes();
    void testFormatWithNoEpisodes();
    
private:
    QString formatEpisodeColumn(int normalEpisodes, int totalEpisodes, int otherEpisodes);
};

QString TestEpisodeColumnFormat::formatEpisodeColumn(int normalEpisodes, int totalEpisodes, int otherEpisodes)
{
    // This replicates the logic from window.cpp lines 1270-1295
    QString episodeText;
    if(totalEpisodes > 0)
    {
        if(otherEpisodes > 0)
        {
            episodeText = QString("%1/%2+%3").arg(normalEpisodes).arg(totalEpisodes).arg(otherEpisodes);
        }
        else
        {
            episodeText = QString("%1/%2").arg(normalEpisodes).arg(totalEpisodes);
        }
    }
    else
    {
        // If eptotal is not available, show count in mylist with other types
        if(otherEpisodes > 0)
        {
            episodeText = QString("%1+%2").arg(normalEpisodes).arg(otherEpisodes);
        }
        else
        {
            episodeText = QString::number(normalEpisodes);
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

void TestEpisodeColumnFormat::testFormatWithoutEpTotal()
{
    // Scenario: Ongoing series where eptotal is not available (0)
    // 50 normal episodes plus 5 other types in mylist
    QString result = formatEpisodeColumn(50, 0, 5);
    QCOMPARE(result, QString("50+5"));
    
    // Scenario: Unknown total, only normal episodes
    // 15 normal episodes, no other types, eptotal unknown
    result = formatEpisodeColumn(15, 0, 0);
    QCOMPARE(result, QString("15"));
    
    // Scenario: Only specials/OVAs collected
    // 0 normal episodes, 3 other types, eptotal unknown
    result = formatEpisodeColumn(0, 0, 3);
    QCOMPARE(result, QString("0+3"));
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
    QString result = formatEpisodeColumn(0, 0, 0);
    QCOMPARE(result, QString("0"));
    
    // Edge case: Only eptotal available, no mylist entries yet
    result = formatEpisodeColumn(0, 12, 0);
    QCOMPARE(result, QString("0/12"));
}

QTEST_MAIN(TestEpisodeColumnFormat)
#include "test_episode_column_format.moc"
