#include <QTest>
#include "../usagi/src/animechain.h"

/**
 * Test suite for AnimeChain class
 * 
 * This test validates that:
 * 1. Chains are ordered correctly from prequel to sequel
 * 2. Merging chains preserves correct order
 * 3. Multiple disconnected roots are ordered deterministically
 * 4. The Inuyasha chain ordering bug is fixed
 */
class TestAnimeChain : public QObject
{
    Q_OBJECT

private slots:
    void testSimpleChainOrder();
    void testMergePreservesOrder();
    void testInuyashaChainOrdering();
    void testMultipleRootsOrdered();
    void testDisconnectedComponents();
};

void TestAnimeChain::testSimpleChainOrder()
{
    // Create a simple chain: 100 -> 200 -> 300
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        if (aid == 100) return QPair<int,int>(0, 200);      // 100: prequel=0, sequel=200
        if (aid == 200) return QPair<int,int>(100, 300);    // 200: prequel=100, sequel=300
        if (aid == 300) return QPair<int,int>(200, 0);      // 300: prequel=200, sequel=0
        return QPair<int,int>(0, 0);
    };
    
    // Create chain starting from middle anime
    AnimeChain chain(200, lookup);
    
    // Expand the chain
    chain.expand(lookup);
    
    // Verify order: 100 -> 200 -> 300
    QList<int> ids = chain.getAnimeIds();
    QCOMPARE(ids.size(), 3);
    QCOMPARE(ids[0], 100);
    QCOMPARE(ids[1], 200);
    QCOMPARE(ids[2], 300);
}

void TestAnimeChain::testMergePreservesOrder()
{
    // Create two chains that should merge
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        if (aid == 100) return QPair<int,int>(0, 200);
        if (aid == 200) return QPair<int,int>(100, 300);
        if (aid == 300) return QPair<int,int>(200, 0);
        return QPair<int,int>(0, 0);
    };
    
    // Chain 1: just 100
    AnimeChain chain1(100, lookup);
    
    // Chain 2: just 200
    AnimeChain chain2(200, lookup);
    
    // Merge chain2 into chain1
    chain1.mergeWith(chain2, lookup);
    
    // Verify order after merge and ordering
    QList<int> ids = chain1.getAnimeIds();
    QVERIFY(ids.size() >= 2);
    QVERIFY(ids.contains(100));
    QVERIFY(ids.contains(200));
    
    // After orderChain(), 100 should come before 200
    int idx100 = ids.indexOf(100);
    int idx200 = ids.indexOf(200);
    QVERIFY(idx100 < idx200);
}

void TestAnimeChain::testInuyashaChainOrdering()
{
    // Reproduce the exact Inuyasha chain issue
    // 144 (Inuyasha) -> 6716 (Kanketsuhen) -> 15546 (Yashahime) -> 16141 (Yashahime S2)
    
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        if (aid == 144) return QPair<int,int>(0, 6716);         // Inuyasha: sequel=6716
        if (aid == 6716) return QPair<int,int>(144, 15546);     // Kanketsuhen: prequel=144, sequel=15546
        if (aid == 15546) return QPair<int,int>(6716, 16141);   // Yashahime: prequel=6716, sequel=16141
        if (aid == 16141) return QPair<int,int>(15546, 0);      // Yashahime S2: prequel=15546
        return QPair<int,int>(0, 0);
    };
    
    // Simulate the chain building process from the issue:
    // 1. Chain 13 starts with 15546, finds prequel 6716
    AnimeChain chain13(15546, lookup);
    AnimeChain chain6716(6716, lookup);
    chain13.mergeWith(chain6716, lookup);
    
    // 2. Chain 528 starts with 144, finds sequel 6716
    AnimeChain chain528(144, lookup);
    
    // 3. Later, chain 528 finds that 6716 is already in chain 13, so merge
    chain528.mergeWith(chain13, lookup);
    
    // 4. Chain 528 also has 16141 somehow
    AnimeChain chain16141(16141, lookup);
    chain528.mergeWith(chain16141, lookup);
    
    // Final order should be: 144 -> 6716 -> 15546 -> 16141
    QList<int> ids = chain528.getAnimeIds();
    QCOMPARE(ids.size(), 4);
    
    // Verify correct order
    QCOMPARE(ids[0], 144);    // Inuyasha first
    QCOMPARE(ids[1], 6716);   // Kanketsuhen second
    QCOMPARE(ids[2], 15546);  // Yashahime third
    QCOMPARE(ids[3], 16141);  // Yashahime S2 fourth
}

void TestAnimeChain::testMultipleRootsOrdered()
{
    // Test that when there are multiple disconnected roots, they are ordered deterministically
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        // Two separate chains: 500 and 100->200
        if (aid == 100) return QPair<int,int>(0, 200);
        if (aid == 200) return QPair<int,int>(100, 0);
        if (aid == 500) return QPair<int,int>(0, 0);  // Standalone anime
        return QPair<int,int>(0, 0);
    };
    
    // Create a chain with disconnected components
    AnimeChain chain(200, lookup);
    AnimeChain chain2(100, lookup);
    AnimeChain chain3(500, lookup);
    
    chain.mergeWith(chain2, lookup);
    chain.mergeWith(chain3, lookup);
    
    QList<int> ids = chain.getAnimeIds();
    QCOMPARE(ids.size(), 3);
    
    // Roots should be ordered by ID: 100 comes before 500
    // Since 100 has a sequel 200, final order should be: 100, 200, 500
    QCOMPARE(ids[0], 100);
    QCOMPARE(ids[1], 200);
    QCOMPARE(ids[2], 500);
}

void TestAnimeChain::testDisconnectedComponents()
{
    // Test two completely disconnected chains merged together
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        // Chain A: 10 -> 20
        if (aid == 10) return QPair<int,int>(0, 20);
        if (aid == 20) return QPair<int,int>(10, 0);
        // Chain B: 30 -> 40
        if (aid == 30) return QPair<int,int>(0, 40);
        if (aid == 40) return QPair<int,int>(30, 0);
        return QPair<int,int>(0, 0);
    };
    
    AnimeChain chainA(10, lookup);
    chainA.expand(lookup);
    
    AnimeChain chainB(30, lookup);
    chainB.expand(lookup);
    
    // Merge B into A
    chainA.mergeWith(chainB, lookup);
    
    QList<int> ids = chainA.getAnimeIds();
    QCOMPARE(ids.size(), 4);
    
    // Roots (10 and 30) should be ordered by ID
    // Since 10 < 30, chain A should come first
    QCOMPARE(ids[0], 10);
    QCOMPARE(ids[1], 20);
    QCOMPARE(ids[2], 30);
    QCOMPARE(ids[3], 40);
}

QTEST_MAIN(TestAnimeChain)
#include "test_animechain.moc"
