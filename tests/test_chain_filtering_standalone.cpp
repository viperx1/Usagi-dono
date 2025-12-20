#include <QTest>
#include "../usagi/src/mylistcardmanager.h"
#include "../usagi/src/animecard.h"
#include "../usagi/src/flowlayout.h"
#include "../usagi/src/main.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

/**
 * Test suite for chain filtering with standalone anime
 * 
 * This test reproduces the Arifureta issue where:
 * - 3 anime are found by search (13624, 15135, 17615)
 * - Anime 13624 is NOT in any chain (no relation data or isolated)
 * - Anime 15135 and 17615 form a chain
 * 
 * Expected behavior with chain mode enabled:
 * - Should show 2 chains: one standalone chain with 13624, one chain with 15135 and 17615
 * - Total: 3 anime should be displayed
 */
class TestChainFilteringStandalone : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Test cases
    void testStandaloneAnimeInChainMode();
    void testMixedChainAndStandalone();
    void testMultipleStandaloneAnime();
    void testStandaloneChainSurvivesSorting();
    
private:
    MyListCardManager *manager;
    FlowLayout *layout;
    QWidget *container;
    QSqlDatabase db;
    
    void createTestDatabase();
    void insertTestAnime(int aid, const QString &name, int prequelAid = 0, int sequelAid = 0);
    int countChains(const QList<int>& animeIds);
};

void TestChainFilteringStandalone::initTestCase()
{
    // Set environment variable to signal test mode
    qputenv("USAGI_TEST_MODE", "1");
    
    // Initialize the global adbapi object
    adbapi = new myAniDBApi("test", 1);
    
    // Ensure clean slate: remove any existing default connection
    // This is critical when running tests in sequence
    {
        QString defaultConn = QSqlDatabase::defaultConnection;
        if (QSqlDatabase::contains(defaultConn)) {
            // Close any existing default database first
            QSqlDatabase existingDb = QSqlDatabase::database(defaultConn, false);
            if (existingDb.isOpen()) {
                existingDb.close();
            }
            // Now remove the connection
            QSqlDatabase::removeDatabase(defaultConn);
        }
    }
    
    // Create in-memory test database
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    
    if (!db.open()) {
        QFAIL("Could not open test database");
    }
    
    createTestDatabase();
}

void TestChainFilteringStandalone::cleanupTestCase()
{
    delete adbapi;
    adbapi = nullptr;
    
    // Close the database before removing the connection
    if (db.isOpen()) {
        db.close();
    }
    
    // Clear the QSqlDatabase object to release the connection reference
    db = QSqlDatabase();
    
    // Now safely remove the database connection
    QString defaultConn = QSqlDatabase::defaultConnection;
    if (QSqlDatabase::contains(defaultConn)) {
        QSqlDatabase::removeDatabase(defaultConn);
    }
}

void TestChainFilteringStandalone::init()
{
    container = new QWidget();
    layout = new FlowLayout(container);
    manager = new MyListCardManager();
    manager->setCardLayout(layout);
}

void TestChainFilteringStandalone::cleanup()
{
    delete manager;
    delete container;
}

void TestChainFilteringStandalone::createTestDatabase()
{
    QSqlQuery query(db);
    
    // Create anime table (matching production schema)
    QString createAnime = "CREATE TABLE anime ("
        "aid INTEGER PRIMARY KEY, "
        "nameromaji TEXT, "
        "nameenglish TEXT, "
        "eptotal INTEGER, "
        "eps INTEGER, "
        "typename TEXT, "
        "startdate TEXT, "
        "enddate TEXT, "
        "picname TEXT, "
        "poster_image BLOB, "
        "category TEXT, "
        "rating TEXT, "
        "tag_name_list TEXT, "
        "tag_id_list TEXT, "
        "tag_weight_list TEXT, "
        "hidden INTEGER DEFAULT 0, "
        "is_18_restricted INTEGER DEFAULT 0, "
        "relaidlist TEXT, "
        "relaidtype TEXT"
    ")";
    
    if (!query.exec(createAnime)) {
        qWarning() << "Failed to create anime table:" << query.lastError().text();
        QFAIL("Could not create anime table");
    }
    
    // Create anime_titles table
    QString createTitles = "CREATE TABLE anime_titles ("
        "aid INTEGER, "
        "type INTEGER, "
        "language TEXT, "
        "title TEXT"
    ")";
    
    if (!query.exec(createTitles)) {
        qWarning() << "Failed to create anime_titles table:" << query.lastError().text();
        QFAIL("Could not create anime_titles table");
    }
    
    // Create episode table (required by preload)
    QString createEpisode = "CREATE TABLE episode ("
        "eid INTEGER PRIMARY KEY, "
        "aid INTEGER, "
        "epno TEXT, "
        "name TEXT"
    ")";
    
    if (!query.exec(createEpisode)) {
        qWarning() << "Failed to create episode table:" << query.lastError().text();
        QFAIL("Could not create episode table");
    }
    
    // Create mylist table (required by preload)
    QString createMylist = "CREATE TABLE mylist ("
        "lid INTEGER PRIMARY KEY, "
        "aid INTEGER, "
        "eid INTEGER, "
        "fid INTEGER, "
        "state INTEGER, "
        "viewed INTEGER, "
        "storage TEXT"
    ")";
    
    if (!query.exec(createMylist)) {
        qWarning() << "Failed to create mylist table:" << query.lastError().text();
        QFAIL("Could not create mylist table");
    }
}

void TestChainFilteringStandalone::insertTestAnime(int aid, const QString &name, int prequelAid, int sequelAid)
{
    QSqlQuery query(db);
    
    // Build relation lists
    QStringList relAidList;
    QStringList relTypeList;
    
    if (prequelAid > 0) {
        relAidList.append(QString::number(prequelAid));
        relTypeList.append("2");  // 2 = prequel (from relation type enum)
    }
    
    if (sequelAid > 0) {
        relAidList.append(QString::number(sequelAid));
        relTypeList.append("1");  // 1 = sequel (from relation type enum)
    }
    
    QString relaidlist = relAidList.join("'");
    QString relaidtype = relTypeList.join("'");
    
    // Insert anime with relation data
    query.prepare("INSERT INTO anime (aid, nameromaji, eptotal, typename, startdate, enddate, relaidlist, relaidtype) "
                  "VALUES (?, ?, 12, 'TV Series', '2019-01-01', '2019-03-31', ?, ?)");
    query.addBindValue(aid);
    query.addBindValue(name);
    query.addBindValue(relaidlist);
    query.addBindValue(relaidtype);
    
    if (!query.exec()) {
        qWarning() << "Failed to insert anime:" << query.lastError().text();
        QFAIL("Could not insert anime");
    }
    
    // Also insert into anime_titles for the title lookup
    query.prepare("INSERT INTO anime_titles (aid, type, language, title) VALUES (?, 1, 'x-jat', ?)");
    query.addBindValue(aid);
    query.addBindValue(name);
    
    if (!query.exec()) {
        qWarning() << "Failed to insert anime title:" << query.lastError().text();
    }
}

int TestChainFilteringStandalone::countChains(const QList<int>& animeIds)
{
    // Helper function to count unique chains from anime IDs
    QSet<int> chainIndices;
    for (int aid : animeIds) {
        int chainIdx = manager->getChainIndexForAnime(aid);
        chainIndices.insert(chainIdx);
    }
    return chainIndices.size();
}

void TestChainFilteringStandalone::testStandaloneAnimeInChainMode()
{
    // Reproduce the Arifureta scenario:
    // - aid 13624: standalone (no relation data)
    // - aid 15135: Season 2, has sequel → 17615 (Season 3)
    // - aid 17615: Season 3, has prequel ← 15135 (Season 2)
    // Chain: 15135 → 17615
    
    insertTestAnime(13624, "Arifureta Shokugyou de Sekai Saikyou", 0, 0);  // No relations
    insertTestAnime(15135, "Arifureta Shokugyou de Sekai Saikyou 2nd Season", 0, 17615);  // sequel=17615
    insertTestAnime(17615, "Arifureta Shokugyou de Sekai Saikyou Season 3", 15135, 0);  // prequel=15135
    
    // Preload data for all 3 anime and build chains
    QList<int> allAnime = {13624, 15135, 17615};
    manager->preloadCardCreationData(allAnime);
    manager->buildChainsFromCache();
    
    // Simulate the search result: all 3 anime found
    QList<int> searchResults = {13624, 15135, 17615};
    
    // Enable chain mode
    manager->setAnimeIdList(searchResults, true);
    
    // Get the filtered anime list
    QList<int> displayedAnime = manager->getAnimeIdList();
    
    // CRITICAL: All 3 anime should be displayed
    // - 13624 as a standalone chain (1 anime)
    // - 15135 and 17615 as a connected chain (2 anime)
    QCOMPARE(displayedAnime.size(), 3);
    
    // Verify all anime are present
    QVERIFY(displayedAnime.contains(13624));
    QVERIFY(displayedAnime.contains(15135));
    QVERIFY(displayedAnime.contains(17615));
    
    // Verify we have 2 chains
    QCOMPARE(countChains(displayedAnime), 2);
}

void TestChainFilteringStandalone::testMixedChainAndStandalone()
{
    // Test with multiple standalone and chained anime
    // Chain 1: 100 -> 101 -> 102
    // Standalone: 200
    // Chain 2: 300 -> 301
    // Standalone: 400
    
    insertTestAnime(100, "Series A S1", 0, 101);
    insertTestAnime(101, "Series A S2", 100, 102);
    insertTestAnime(102, "Series A S3", 101, 0);
    insertTestAnime(200, "Standalone Series B", 0, 0);
    insertTestAnime(300, "Series C S1", 0, 301);
    insertTestAnime(301, "Series C S2", 300, 0);
    insertTestAnime(400, "Standalone Series D", 0, 0);
    
    QList<int> allAnime = {100, 101, 102, 200, 300, 301, 400};
    manager->preloadCardCreationData(allAnime);
    manager->buildChainsFromCache();
    
    // Set chain mode with all anime
    manager->setAnimeIdList(allAnime, true);
    
    QList<int> displayedAnime = manager->getAnimeIdList();
    
    // All 7 anime should be displayed
    QCOMPARE(displayedAnime.size(), 7);
    
    // Verify we have 4 chains (2 multi-anime chains + 2 standalone)
    QCOMPARE(countChains(displayedAnime), 4);
}

void TestChainFilteringStandalone::testMultipleStandaloneAnime()
{
    // Test with only standalone anime (no chains)
    insertTestAnime(1, "Anime 1", 0, 0);
    insertTestAnime(2, "Anime 2", 0, 0);
    insertTestAnime(3, "Anime 3", 0, 0);
    
    QList<int> allAnime = {1, 2, 3};
    manager->preloadCardCreationData(allAnime);
    manager->buildChainsFromCache();
    
    // Enable chain mode
    manager->setAnimeIdList(allAnime, true);
    
    QList<int> displayedAnime = manager->getAnimeIdList();
    
    // All 3 anime should be displayed as standalone chains
    QCOMPARE(displayedAnime.size(), 3);
    
    // Verify we have 3 chains (all standalone)
    QCOMPARE(countChains(displayedAnime), 3);
}

void TestChainFilteringStandalone::testStandaloneChainSurvivesSorting()
{
    // This test reproduces the exact Arifureta issue:
    // - Anime 13624 is standalone (no relation data)
    // - Anime 15135 and 17615 form a chain
    // - After sorting, the standalone chain should NOT be lost
    
    // Setup: Create the exact Arifureta scenario
    insertTestAnime(13624, "Arifureta Shokugyou de Sekai Saikyou", 0, 0);  // Standalone
    insertTestAnime(15135, "Arifureta Shokugyou de Sekai Saikyou 2nd Season", 0, 17615);
    insertTestAnime(17615, "Arifureta Shokugyou de Sekai Saikyou Season 3", 15135, 0);
    
    QList<int> allAnime = {13624, 15135, 17615};
    manager->preloadCardCreationData(allAnime);
    manager->buildChainsFromCache();
    
    // Step 1: Enable chain mode - should create standalone chain for 13624
    manager->setAnimeIdList(allAnime, true);
    
    QList<int> afterChainMode = manager->getAnimeIdList();
    QCOMPARE(afterChainMode.size(), 3);
    QVERIFY(afterChainMode.contains(13624));
    QVERIFY(afterChainMode.contains(15135));
    QVERIFY(afterChainMode.contains(17615));
    
    // Should have 2 chains: one standalone (13624) and one connected (15135-17615)
    QCOMPARE(countChains(afterChainMode), 2);
    
    // Step 2: Trigger sorting - this is where the bug would occur
    // Sort by title (criteria 0) in ascending order
    manager->sortChains(AnimeChain::SortCriteria::ByRepresentativeTitle, true);
    
    QList<int> afterSorting = manager->getAnimeIdList();
    
    // CRITICAL: After sorting, all 3 anime must still be present
    QCOMPARE(afterSorting.size(), 3);
    QVERIFY(afterSorting.contains(13624));
    QVERIFY(afterSorting.contains(15135));
    QVERIFY(afterSorting.contains(17615));
    
    // Still should have 2 chains after sorting
    QCOMPARE(countChains(afterSorting), 2);
    
    // Test sorting in different directions and criteria to ensure robustness
    manager->sortChains(AnimeChain::SortCriteria::ByRepresentativeTitle, false);
    QList<int> afterDescSort = manager->getAnimeIdList();
    QCOMPARE(afterDescSort.size(), 3);
    QVERIFY(afterDescSort.contains(13624));
    
    // Sort by type
    manager->sortChains(AnimeChain::SortCriteria::ByRepresentativeType, true);
    QList<int> afterTypeSort = manager->getAnimeIdList();
    QCOMPARE(afterTypeSort.size(), 3);
    QVERIFY(afterTypeSort.contains(13624));
}

QTEST_MAIN(TestChainFilteringStandalone)
#include "test_chain_filtering_standalone.moc"
