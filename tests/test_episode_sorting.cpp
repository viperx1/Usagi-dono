#include <QtTest/QtTest>
#include <QString>
#include <QList>

// Test class for episode sorting logic
class TestEpisodeSorting : public QObject
{
    Q_OBJECT

private:
    // Helper struct to represent episode data
    struct Episode {
        QString epno;      // Raw episode number from database (e.g., "01", "S01", "C01")
        int eptype;        // Episode type (1=regular, 2=special, 3=credit, 4=trailer, 5=parody, 6=other)
        QString display;   // Expected display string
        int sortType;      // Expected sort type
        int sortNumber;    // Expected sort number
    };

    // Helper function to parse episode number and type (mimics window.cpp logic)
    void parseEpisode(const QString& epno, int eptype, QString& display, int& sortType, int& sortNumber)
    {
        QString numericPart;
        
        // Parse episode number based on type field
        if(eptype == 2)  // Special
        {
            numericPart = epno.mid(1);  // Remove 'S' prefix
        }
        else if(eptype == 3)  // Credit
        {
            numericPart = epno.mid(1);  // Remove 'C' prefix
        }
        else if(eptype == 4)  // Trailer
        {
            numericPart = epno.mid(1);  // Remove 'T' prefix
        }
        else if(eptype == 5)  // Parody
        {
            numericPart = epno.mid(1);  // Remove 'P' prefix
        }
        else if(eptype == 6)  // Other
        {
            numericPart = epno.mid(1);  // Remove 'O' prefix
        }
        else  // Regular episode (type == 1 or 0)
        {
            numericPart = epno;
        }
        
        // Remove leading zeros by converting to int and back to string
        bool ok;
        int epNum = numericPart.toInt(&ok);
        if(ok)
        {
            numericPart = QString::number(epNum);
            sortNumber = epNum;
        }
        else
        {
            sortNumber = 0;
        }
        
        // Build display string based on type
        if(eptype == 2)
            display = QString("Special %1").arg(numericPart);
        else if(eptype == 3)
            display = QString("Credit %1").arg(numericPart);
        else if(eptype == 4)
            display = QString("Trailer %1").arg(numericPart);
        else if(eptype == 5)
            display = QString("Parody %1").arg(numericPart);
        else if(eptype == 6)
            display = QString("Other %1").arg(numericPart);
        else
            display = numericPart;  // Regular episode - just the number
        
        sortType = eptype;
    }

private slots:
    void testLeadingZeroRemoval()
    {
        // Test that leading zeros are removed from regular episodes
        QString display;
        int sortType, sortNumber;
        
        parseEpisode("01", 1, display, sortType, sortNumber);
        QCOMPARE(display, QString("1"));
        QCOMPARE(sortNumber, 1);
        
        parseEpisode("001", 1, display, sortType, sortNumber);
        QCOMPARE(display, QString("1"));
        QCOMPARE(sortNumber, 1);
        
        parseEpisode("10", 1, display, sortType, sortNumber);
        QCOMPARE(display, QString("10"));
        QCOMPARE(sortNumber, 10);
        
        parseEpisode("100", 1, display, sortType, sortNumber);
        QCOMPARE(display, QString("100"));
        QCOMPARE(sortNumber, 100);
    }

    void testSpecialEpisodeFormatting()
    {
        QString display;
        int sortType, sortNumber;
        
        // Test special episodes (type 2)
        parseEpisode("S01", 2, display, sortType, sortNumber);
        QCOMPARE(display, QString("Special 1"));
        QCOMPARE(sortType, 2);
        QCOMPARE(sortNumber, 1);
        
        parseEpisode("S10", 2, display, sortType, sortNumber);
        QCOMPARE(display, QString("Special 10"));
        QCOMPARE(sortType, 2);
        QCOMPARE(sortNumber, 10);
    }

    void testCreditEpisodeFormatting()
    {
        QString display;
        int sortType, sortNumber;
        
        // Test credit episodes (type 3)
        parseEpisode("C01", 3, display, sortType, sortNumber);
        QCOMPARE(display, QString("Credit 1"));
        QCOMPARE(sortType, 3);
        QCOMPARE(sortNumber, 1);
    }

    void testTrailerEpisodeFormatting()
    {
        QString display;
        int sortType, sortNumber;
        
        // Test trailer episodes (type 4)
        parseEpisode("T01", 4, display, sortType, sortNumber);
        QCOMPARE(display, QString("Trailer 1"));
        QCOMPARE(sortType, 4);
        QCOMPARE(sortNumber, 1);
    }

    void testParodyEpisodeFormatting()
    {
        QString display;
        int sortType, sortNumber;
        
        // Test parody episodes (type 5)
        parseEpisode("P01", 5, display, sortType, sortNumber);
        QCOMPARE(display, QString("Parody 1"));
        QCOMPARE(sortType, 5);
        QCOMPARE(sortNumber, 1);
    }

    void testOtherEpisodeFormatting()
    {
        QString display;
        int sortType, sortNumber;
        
        // Test other episodes (type 6)
        parseEpisode("O01", 6, display, sortType, sortNumber);
        QCOMPARE(display, QString("Other 1"));
        QCOMPARE(sortType, 6);
        QCOMPARE(sortNumber, 1);
    }

    void testEpisodeSortingOrder()
    {
        // Test that episodes sort correctly by type then by number
        QList<Episode> episodes = {
            {"02", 1, "", 0, 0},     // Regular episode 2
            {"S01", 2, "", 0, 0},    // Special 1
            {"01", 1, "", 0, 0},     // Regular episode 1
            {"10", 1, "", 0, 0},     // Regular episode 10
            {"S02", 2, "", 0, 0},    // Special 2
            {"C01", 3, "", 0, 0},    // Credit 1
        };
        
        // Parse all episodes
        for(auto& ep : episodes)
        {
            parseEpisode(ep.epno, ep.eptype, ep.display, ep.sortType, ep.sortNumber);
        }
        
        // Sort by type first, then by number
        std::sort(episodes.begin(), episodes.end(), [](const Episode& a, const Episode& b) {
            if(a.sortType != b.sortType)
                return a.sortType < b.sortType;
            return a.sortNumber < b.sortNumber;
        });
        
        // Verify sort order: Regular episodes (1, 2, 10), then Specials (1, 2), then Credits (1)
        QCOMPARE(episodes[0].display, QString("1"));      // Regular 1
        QCOMPARE(episodes[1].display, QString("2"));      // Regular 2
        QCOMPARE(episodes[2].display, QString("10"));     // Regular 10
        QCOMPARE(episodes[3].display, QString("Special 1")); // Special 1
        QCOMPARE(episodes[4].display, QString("Special 2")); // Special 2
        QCOMPARE(episodes[5].display, QString("Credit 1"));  // Credit 1
    }

    void testLeadingZerosSortingCorrectly()
    {
        // Test that episodes with leading zeros sort numerically, not alphabetically
        QList<Episode> episodes = {
            {"02", 1, "", 0, 0},
            {"010", 1, "", 0, 0},
            {"01", 1, "", 0, 0},
            {"100", 1, "", 0, 0},
        };
        
        // Parse all episodes
        for(auto& ep : episodes)
        {
            parseEpisode(ep.epno, ep.eptype, ep.display, ep.sortType, ep.sortNumber);
        }
        
        // Sort by number
        std::sort(episodes.begin(), episodes.end(), [](const Episode& a, const Episode& b) {
            return a.sortNumber < b.sortNumber;
        });
        
        // Verify numeric sort order: 1, 2, 10, 100
        QCOMPARE(episodes[0].sortNumber, 1);
        QCOMPARE(episodes[1].sortNumber, 2);
        QCOMPARE(episodes[2].sortNumber, 10);
        QCOMPARE(episodes[3].sortNumber, 100);
        
        // Verify display strings don't have leading zeros
        QCOMPARE(episodes[0].display, QString("1"));
        QCOMPARE(episodes[1].display, QString("2"));
        QCOMPARE(episodes[2].display, QString("10"));
        QCOMPARE(episodes[3].display, QString("100"));
    }
};

QTEST_MAIN(TestEpisodeSorting)
#include "test_episode_sorting.moc"
