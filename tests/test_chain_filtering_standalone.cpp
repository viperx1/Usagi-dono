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
    
private:
    MyListCardManager *manager;
    FlowLayout *layout;
    QWidget *container;
    QSqlDatabase db;
    
    void createTestDatabase();
    void insertTestAnime(int aid, const QString &name, int prequelAid = 0, int sequelAid = 0);
};

void TestChainFilteringStandalone::initTestCase()
{
    // Set environment variable to signal test mode
    qputenv("USAGI_TEST_MODE", "1");
    
    // Initialize the global adbapi object
    adbapi = new myAniDBApi("test", 1);
    
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
    
    if (db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
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
    
    // Create anime table with relation columns
    QString createAnime = "CREATE TABLE anime ("
        "aid INTEGER PRIMARY KEY, "
        "romaji_name TEXT, "
        "kanji_name TEXT, "
        "english_name TEXT, "
        "type TEXT, "
        "year INTEGER, "
        "enddate TEXT, "
        "picture TEXT, "
        "rating INTEGER, "
        "temprating INTEGER, "
        "reviewrating INTEGER"
    ")";
    
    if (!query.exec(createAnime)) {
        qWarning() << "Failed to create anime table:" << query.lastError().text();
        QFAIL("Could not create anime table");
    }
    
    // Create relation table
    QString createRelation = "CREATE TABLE relation ("
        "rid INTEGER PRIMARY KEY AUTOINCREMENT, "
        "aid INTEGER, "
        "related_aid INTEGER, "
        "relation_type TEXT"
    ")";
    
    if (!query.exec(createRelation)) {
        qWarning() << "Failed to create relation table:" << query.lastError().text();
        QFAIL("Could not create relation table");
    }
}

void TestChainFilteringStandalone::insertTestAnime(int aid, const QString &name, int prequelAid, int sequelAid)
{
    QSqlQuery query(db);
    
    // Insert anime
    query.prepare("INSERT INTO anime (aid, romaji_name, type, year) VALUES (?, ?, ?, ?)");
    query.addBindValue(aid);
    query.addBindValue(name);
    query.addBindValue("TV Series");
    query.addBindValue(2019);
    
    if (!query.exec()) {
        qWarning() << "Failed to insert anime:" << query.lastError().text();
        QFAIL("Could not insert anime");
    }
    
    // Insert prequel relation if specified
    if (prequelAid > 0) {
        query.prepare("INSERT INTO relation (aid, related_aid, relation_type) VALUES (?, ?, 'prequel')");
        query.addBindValue(aid);
        query.addBindValue(prequelAid);
        
        if (!query.exec()) {
            qWarning() << "Failed to insert prequel relation:" << query.lastError().text();
        }
    }
    
    // Insert sequel relation if specified
    if (sequelAid > 0) {
        query.prepare("INSERT INTO relation (aid, related_aid, relation_type) VALUES (?, ?, 'sequel')");
        query.addBindValue(aid);
        query.addBindValue(sequelAid);
        
        if (!query.exec()) {
            qWarning() << "Failed to insert sequel relation:" << query.lastError().text();
        }
    }
}

void TestChainFilteringStandalone::testStandaloneAnimeInChainMode()
{
    // Reproduce the Arifureta scenario:
    // - aid 13624: standalone (no relation data)
    // - aid 15135: has sequel relation to 17615
    // - aid 17615: has prequel relation to 15135
    
    insertTestAnime(13624, "Arifureta Shokugyou de Sekai Saikyou", 0, 0);  // No relations
    insertTestAnime(15135, "Arifureta Shokugyou de Sekai Saikyou 2nd Season", 0, 17615);  // sequel=17615
    insertTestAnime(17615, "Arifureta Shokugyou de Sekai Saikyou Season 3", 15135, 0);  // prequel=15135
    
    // Preload data for all 3 anime
    QList<int> allAnime = {13624, 15135, 17615};
    manager->preloadCardCreationData(allAnime);
    
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
    QList<int> chainIndices;
    for (int aid : displayedAnime) {
        int chainIdx = manager->getChainIndexForAnime(aid);
        if (!chainIndices.contains(chainIdx)) {
            chainIndices.append(chainIdx);
        }
    }
    QCOMPARE(chainIndices.size(), 2);
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
    
    // Set chain mode with all anime
    manager->setAnimeIdList(allAnime, true);
    
    QList<int> displayedAnime = manager->getAnimeIdList();
    
    // All 7 anime should be displayed
    QCOMPARE(displayedAnime.size(), 7);
    
    // Verify we have 4 chains (2 multi-anime chains + 2 standalone)
    QList<int> chainIndices;
    for (int aid : displayedAnime) {
        int chainIdx = manager->getChainIndexForAnime(aid);
        if (!chainIndices.contains(chainIdx)) {
            chainIndices.append(chainIdx);
        }
    }
    QCOMPARE(chainIndices.size(), 4);
}

void TestChainFilteringStandalone::testMultipleStandaloneAnime()
{
    // Test with only standalone anime (no chains)
    insertTestAnime(1, "Anime 1", 0, 0);
    insertTestAnime(2, "Anime 2", 0, 0);
    insertTestAnime(3, "Anime 3", 0, 0);
    
    QList<int> allAnime = {1, 2, 3};
    manager->preloadCardCreationData(allAnime);
    
    // Enable chain mode
    manager->setAnimeIdList(allAnime, true);
    
    QList<int> displayedAnime = manager->getAnimeIdList();
    
    // All 3 anime should be displayed as standalone chains
    QCOMPARE(displayedAnime.size(), 3);
    
    // Verify we have 3 chains (all standalone)
    QList<int> chainIndices;
    for (int aid : displayedAnime) {
        int chainIdx = manager->getChainIndexForAnime(aid);
        if (!chainIndices.contains(chainIdx)) {
            chainIndices.append(chainIdx);
        }
    }
    QCOMPARE(chainIndices.size(), 3);
}

QTEST_MAIN(TestChainFilteringStandalone)
#include "test_chain_filtering_standalone.moc"
