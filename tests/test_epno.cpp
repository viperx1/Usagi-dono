#include <QtTest/QtTest>
#include "../usagi/src/epno.h"

class TestEpno : public QObject
{
    Q_OBJECT

private slots:
    void testConstructorFromString()
    {
        // Test regular episodes
        epno ep1("1");
        QCOMPARE(ep1.type(), 1);
        QCOMPARE(ep1.number(), 1);
        QCOMPARE(ep1.toDisplayString(), QString("1"));
        
        epno ep2("01");
        QCOMPARE(ep2.type(), 1);
        QCOMPARE(ep2.number(), 1);
        QCOMPARE(ep2.toDisplayString(), QString("1"));
        
        epno ep10("10");
        QCOMPARE(ep10.type(), 1);
        QCOMPARE(ep10.number(), 10);
        QCOMPARE(ep10.toDisplayString(), QString("10"));
        
        // Test special episodes
        epno epS1("S01");
        QCOMPARE(epS1.type(), 2);
        QCOMPARE(epS1.number(), 1);
        QCOMPARE(epS1.toDisplayString(), QString("Special 1"));
        
        // Test credit episodes
        epno epC1("C01");
        QCOMPARE(epC1.type(), 3);
        QCOMPARE(epC1.number(), 1);
        QCOMPARE(epC1.toDisplayString(), QString("Credit 1"));
        
        // Test trailer episodes
        epno epT1("T01");
        QCOMPARE(epT1.type(), 4);
        QCOMPARE(epT1.number(), 1);
        QCOMPARE(epT1.toDisplayString(), QString("Trailer 1"));
        
        // Test parody episodes
        epno epP1("P01");
        QCOMPARE(epP1.type(), 5);
        QCOMPARE(epP1.number(), 1);
        QCOMPARE(epP1.toDisplayString(), QString("Parody 1"));
        
        // Test other episodes
        epno epO1("O01");
        QCOMPARE(epO1.type(), 6);
        QCOMPARE(epO1.number(), 1);
        QCOMPARE(epO1.toDisplayString(), QString("Other 1"));
    }
    
    void testConstructorFromTypeAndNumber()
    {
        epno ep1(1, 1);
        QCOMPARE(ep1.type(), 1);
        QCOMPARE(ep1.number(), 1);
        QCOMPARE(ep1.toDisplayString(), QString("1"));
        
        epno ep2(2, 1);
        QCOMPARE(ep2.type(), 2);
        QCOMPARE(ep2.number(), 1);
        QCOMPARE(ep2.toDisplayString(), QString("Special 1"));
    }
    
    void testLeadingZeroRemoval()
    {
        epno ep1("01");
        QCOMPARE(ep1.toDisplayString(), QString("1"));
        
        epno ep2("001");
        QCOMPARE(ep2.toDisplayString(), QString("1"));
        
        epno ep3("010");
        QCOMPARE(ep3.toDisplayString(), QString("10"));
        
        epno ep4("S01");
        QCOMPARE(ep4.toDisplayString(), QString("Special 1"));
        
        epno ep5("S001");
        QCOMPARE(ep5.toDisplayString(), QString("Special 1"));
    }
    
    void testComparisonOperators()
    {
        epno ep1("1");
        epno ep2("2");
        epno ep10("10");
        epno epS1("S01");
        epno epS2("S02");
        epno epC1("C01");
        
        // Test less than
        QVERIFY(ep1 < ep2);
        QVERIFY(ep2 < ep10);
        QVERIFY(ep1 < ep10);
        
        // Regular episodes come before specials (type 1 < type 2)
        QVERIFY(ep1 < epS1);
        QVERIFY(ep10 < epS1);
        
        // Specials come before credits (type 2 < type 3)
        QVERIFY(epS1 < epC1);
        
        // Within same type, sort by number
        QVERIFY(epS1 < epS2);
        
        // Test greater than
        QVERIFY(ep2 > ep1);
        QVERIFY(ep10 > ep2);
        QVERIFY(epS1 > ep1);
        
        // Test equality
        epno ep1_copy("01");
        QVERIFY(ep1 == ep1_copy);
        
        epno epS1_copy("S1");
        QVERIFY(epS1 == epS1_copy);
        
        // Test inequality
        QVERIFY(ep1 != ep2);
        QVERIFY(ep1 != epS1);
    }
    
    void testSorting()
    {
        QList<epno> episodes;
        episodes << epno("2") << epno("010") << epno("S01") << epno("01") 
                 << epno("100") << epno("S02") << epno("C01");
        
        std::sort(episodes.begin(), episodes.end());
        
        // Expected order: 1, 2, 10, 100, S1, S2, C1
        QCOMPARE(episodes[0].toDisplayString(), QString("1"));
        QCOMPARE(episodes[1].toDisplayString(), QString("2"));
        QCOMPARE(episodes[2].toDisplayString(), QString("10"));
        QCOMPARE(episodes[3].toDisplayString(), QString("100"));
        QCOMPARE(episodes[4].toDisplayString(), QString("Special 1"));
        QCOMPARE(episodes[5].toDisplayString(), QString("Special 2"));
        QCOMPARE(episodes[6].toDisplayString(), QString("Credit 1"));
    }
    
    void testIsValid()
    {
        epno ep1("1");
        QVERIFY(ep1.isValid());
        
        epno ep2("S01");
        QVERIFY(ep2.isValid());
        
        epno ep3("");
        QVERIFY(!ep3.isValid());
        
        epno ep4;
        QVERIFY(!ep4.isValid());
    }
};

QTEST_MAIN(TestEpno)
#include "test_epno.moc"
