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
    
    // Verify order: 300 -> 200 -> 100 (reversed: sequel to prequel)
    QList<int> ids = chain.getAnimeIds();
    QCOMPARE(ids.size(), 3);
    QCOMPARE(ids[0], 300);
    QCOMPARE(ids[1], 200);
    QCOMPARE(ids[2], 100);
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
    
    // After orderChain(), 200 should come before 100 (reversed order)
    int idx100 = ids.indexOf(100);
    int idx200 = ids.indexOf(200);
    QVERIFY(idx200 < idx100);
}

void TestAnimeChain::testInuyashaChainOrdering()
{
    // Reproduce the exact Inuyasha chain issue with realistic scenario:
    // - 144, 15546, 16141 are in mylist (have relation data loaded)
    // - 6716 is NOT in mylist (no relation data initially)
    // Expected final order: 144 -> 6716 -> 15546 -> 16141
    
    AnimeChain::RelationLookupFunc lookup = [](int aid) -> QPair<int,int> {
        if (aid == 144) return QPair<int,int>(0, 6716);         // Inuyasha: sequel=6716
        if (aid == 6716) return QPair<int,int>(0, 0);           // Kanketsuhen: NO RELATION DATA (not in mylist)
        if (aid == 15546) return QPair<int,int>(6716, 16141);   // Yashahime: prequel=6716, sequel=16141
        if (aid == 16141) return QPair<int,int>(15546, 0);      // Yashahime S2: prequel=15546
        return QPair<int,int>(0, 0);
    };
    
    // Simulate buildChainsFromAnimeIds with only mylist anime (144, 15546, 16141)
    // This matches the actual scenario from the logs
    QList<int> mylistAnime = {144, 15546, 16141};
    
    // Create initial chains - one per anime in mylist
    QMap<int, int> animeToChainIdx;
    QList<AnimeChain> chains;
    QSet<int> deletedChains;
    
    int idx = 0;
    for (int aid : mylistAnime) {
        AnimeChain chain(aid, lookup);
        chains.append(chain);
        animeToChainIdx[aid] = idx;
        idx++;
    }
    
    // Expand and merge chains (simulating MyListCardManager::buildChainsFromAnimeIds)
    for (int i = 0; i < chains.size(); i++) {
        if (deletedChains.contains(i)) {
            continue;
        }
        
        QSet<int> processed;
        bool changed = true;
        int iterations = 0;
        const int MAX_ITERATIONS = 100;
        
        while (changed && iterations < MAX_ITERATIONS) {
            changed = false;
            iterations++;
            
            QList<int> currentAnime = chains[i].getAnimeIds();
            for (int aid : currentAnime) {
                if (processed.contains(aid)) {
                    continue;
                }
                processed.insert(aid);
                
                auto unbound = chains[i].getUnboundRelations(aid);
                
                // Process prequel
                if (unbound.first > 0) {
                    if (animeToChainIdx.contains(unbound.first)) {
                        // Prequel is in another chain - merge
                        int otherIdx = animeToChainIdx[unbound.first];
                        if (otherIdx != i && !deletedChains.contains(otherIdx)) {
                            chains[i].mergeWith(chains[otherIdx], lookup);
                            deletedChains.insert(otherIdx);
                            for (int aid2 : chains[i].getAnimeIds()) {
                                animeToChainIdx[aid2] = i;
                            }
                            changed = true;
                        }
                    } else {
                        // Prequel not in any chain yet - add it
                        AnimeChain prequelChain(unbound.first, lookup);
                        chains[i].mergeWith(prequelChain, lookup);
                        animeToChainIdx[unbound.first] = i;
                        changed = true;
                    }
                }
                
                // Process sequel
                if (unbound.second > 0) {
                    if (animeToChainIdx.contains(unbound.second)) {
                        // Sequel is in another chain - merge
                        int otherIdx = animeToChainIdx[unbound.second];
                        if (otherIdx != i && !deletedChains.contains(otherIdx)) {
                            chains[i].mergeWith(chains[otherIdx], lookup);
                            deletedChains.insert(otherIdx);
                            for (int aid2 : chains[i].getAnimeIds()) {
                                animeToChainIdx[aid2] = i;
                            }
                            changed = true;
                        }
                    } else {
                        // Sequel not in any chain yet - add it
                        AnimeChain sequelChain(unbound.second, lookup);
                        chains[i].mergeWith(sequelChain, lookup);
                        animeToChainIdx[unbound.second] = i;
                        changed = true;
                    }
                }
            }
        }
    }
    
    // Find the final merged chain (should contain all 4 anime)
    AnimeChain finalChain;
    for (int i = 0; i < chains.size(); i++) {
        if (!deletedChains.contains(i)) {
            if (chains[i].getAnimeIds().size() > finalChain.getAnimeIds().size()) {
                finalChain = chains[i];
            }
        }
    }
    
    QList<int> ids = finalChain.getAnimeIds();
    QCOMPARE(ids.size(), 4);
    
    // Verify correct order (reversed): 16141 -> 15546 -> 6716 -> 144
    QCOMPARE(ids[0], 16141);  // Yashahime S2 first (most recent sequel)
    QCOMPARE(ids[1], 15546);  // Yashahime second
    QCOMPARE(ids[2], 6716);   // Kanketsuhen third (even with no relation data)
    QCOMPARE(ids[3], 144);    // Inuyasha last (original prequel)
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
    
    // Reversed order: dependents first, then roots in reverse ID order
    // Order (reversed): 200 (depends on 100), 500 (root), 100 (root)
    QCOMPARE(ids[0], 200);
    QCOMPARE(ids[1], 500);
    QCOMPARE(ids[2], 100);
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
    
    // Reversed order: dependents first, then roots in reverse ID order
    // Order (reversed): 40 (depends on 30), 20 (depends on 10), 30 (root), 10 (root)
    QCOMPARE(ids[0], 40);
    QCOMPARE(ids[1], 20);
    QCOMPARE(ids[2], 30);
    QCOMPARE(ids[3], 10);
}

QTEST_MAIN(TestAnimeChain)
#include "test_animechain.moc"
