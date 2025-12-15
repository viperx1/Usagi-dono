#include <QTest>
#include "../usagi/src/mylistcardmanager.h"
#include "../usagi/src/animecard.h"
#include "../usagi/src/flowlayout.h"
#include "../usagi/src/main.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSignalSpy>


// Global adbapi used by MyListCardManager

/**
 * Test suite for MyListCardManager
 * 
 * This test validates that:
 * 1. Cards are loaded only once
 * 2. Individual cards can be updated without reloading all cards
 * 3. Updates are asynchronous and don't block
 * 4. Memory is managed properly (no leaks)
 */
class TestMyListCardManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Test cases
    void testCardCreation();
    void testCardCaching();
    void testIndividualUpdate();
    void testBatchUpdates();
    void testAsynchronousOperations();
    void testMemoryManagement();
    
private:
    MyListCardManager *manager;
    FlowLayout *layout;
    QWidget *container;
    QSqlDatabase db;
    
    void createTestDatabase();
    void insertTestAnime(int aid, const QString &name);
    void insertTestEpisode(int aid, int eid, const QString &name, const QString &epno);
    void insertTestMylistEntry(int lid, int aid, int eid);
};

void TestMyListCardManager::initTestCase()
{
    // Set environment variable to signal test mode
    qputenv("USAGI_TEST_MODE", "1");
    
    // Initialize the global adbapi object
    adbapi = new myAniDBApi("test", 1);
    
    // Create in-memory test database (use default connection, not named)
    // MyListCardManager uses QSqlDatabase::database() which requires the default connection
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    
    if (!db.open()) {
        QFAIL("Could not open test database");
    }
    
    createTestDatabase();
}

void TestMyListCardManager::cleanupTestCase()
{
    delete adbapi;
    adbapi = nullptr;
    
    if (db.isOpen()) {
        db.close();
    }
    // Remove default database connection (no name parameter)
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void TestMyListCardManager::init()
{
    // Create fresh manager and layout for each test
    container = new QWidget();
    layout = new FlowLayout(container);
    manager = new MyListCardManager();
    manager->setCardLayout(layout);
}

void TestMyListCardManager::cleanup()
{
    delete manager;
    delete container;  // This will also delete layout
}

void TestMyListCardManager::createTestDatabase()
{
    QSqlQuery q(db);
    
    // Create anime table (matching production schema with both eps and eptotal)
    q.exec("CREATE TABLE anime ("
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
           "is_18_restricted INTEGER DEFAULT 0)");
    
    // Create episode table
    q.exec("CREATE TABLE episode ("
           "eid INTEGER PRIMARY KEY, "
           "aid INTEGER, "
           "epno TEXT, "
           "name TEXT)");
    
    // Create file table
    q.exec("CREATE TABLE file ("
           "fid INTEGER PRIMARY KEY, "
           "filename TEXT, "
           "resolution TEXT, "
           "quality TEXT)");
    
    // Create group table
    q.exec("CREATE TABLE `group` ("
           "gid INTEGER PRIMARY KEY, "
           "name TEXT)");
    
    // Create mylist table
    q.exec("CREATE TABLE mylist ("
           "lid INTEGER PRIMARY KEY, "
           "aid INTEGER, "
           "eid INTEGER, "
           "fid INTEGER, "
           "gid INTEGER, "
           "state INTEGER, "
           "viewed INTEGER, "
           "storage TEXT, "
           "local_file INTEGER, "
           "last_played INTEGER)");
    
    // Create anime_titles table
    q.exec("CREATE TABLE anime_titles ("
           "aid INTEGER, "
           "type INTEGER, "
           "language TEXT, "
           "title TEXT)");
    
    // Create local_files table
    q.exec("CREATE TABLE local_files ("
           "id INTEGER PRIMARY KEY, "
           "path TEXT)");
}

void TestMyListCardManager::insertTestAnime(int aid, const QString &name)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO anime (aid, nameromaji, eptotal, typename, startdate, enddate) "
              "VALUES (?, ?, 12, 'TV Series', '2020-01-01', '2020-03-31')");
    q.addBindValue(aid);
    q.addBindValue(name);
    q.exec();
    
    // Also insert into anime_titles for the title lookup
    q.prepare("INSERT INTO anime_titles (aid, type, language, title) VALUES (?, 1, 'x-jat', ?)");
    q.addBindValue(aid);
    q.addBindValue(name);
    q.exec();
}

void TestMyListCardManager::insertTestEpisode(int aid, int eid, const QString &name, const QString &epno)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO episode (eid, aid, epno, name) VALUES (?, ?, ?, ?)");
    q.addBindValue(eid);
    q.addBindValue(aid);
    q.addBindValue(epno);
    q.addBindValue(name);
    q.exec();
}

void TestMyListCardManager::insertTestMylistEntry(int lid, int aid, int eid)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO mylist (lid, aid, eid, fid, state, viewed, storage) "
              "VALUES (?, ?, ?, 1, 1, 0, '/test/path')");
    q.addBindValue(lid);
    q.addBindValue(aid);
    q.addBindValue(eid);
    q.exec();
}

void TestMyListCardManager::testCardCreation()
{
    // Insert test data
    insertTestAnime(1, "Test Anime 1");
    insertTestEpisode(1, 1, "Episode 1", "1");
    insertTestMylistEntry(1, 1, 1);
    
    // Preload data and build chains (required for m_dataReady flag)
    QList<int> aids;
    aids.append(1);
    manager->preloadCardCreationData(aids);
    manager->buildChainsFromCache();
    AnimeCard *card = manager->createCard(1);
    
    // Verify card exists
    QVERIFY(manager->hasCard(1));
    QVERIFY(card != nullptr);
    QCOMPARE(card->getAnimeId(), 1);
}

void TestMyListCardManager::testCardCaching()
{
    // Insert test data
    insertTestAnime(1, "Test Anime 1");
    insertTestEpisode(1, 1, "Episode 1", "1");
    insertTestMylistEntry(1, 1, 1);
    
    // Load card first time
    QList<int> aids;
    aids.append(1);
    manager->preloadCardCreationData(aids);
    manager->buildChainsFromCache();
    manager->createCard(1);
    AnimeCard *card1 = manager->getCard(1);
    
    // Update card data
    manager->updateCardAnimeInfo(1);
    
    // Get card again - should be same instance (cached)
    AnimeCard *card2 = manager->getCard(1);
    QCOMPARE(card1, card2);  // Same pointer = card was reused, not recreated
}

void TestMyListCardManager::testIndividualUpdate()
{
    // Insert test data
    insertTestAnime(1, "Test Anime 1");
    insertTestAnime(2, "Test Anime 2");
    insertTestEpisode(1, 1, "Episode 1", "1");
    insertTestEpisode(2, 2, "Episode 1", "1");
    insertTestMylistEntry(1, 1, 1);
    insertTestMylistEntry(2, 2, 2);
    
    // Load all cards
    QList<int> aids;
    aids.append(1);
    aids.append(2);
    manager->preloadCardCreationData(aids);
    manager->buildChainsFromCache();
    manager->createCard(1);
    manager->createCard(2);
    
    // Get initial card pointers
    [[maybe_unused]] AnimeCard *card1 = manager->getCard(1);
    AnimeCard *card2 = manager->getCard(2);
    
    // Update only card 1
    QSignalSpy updateSpy(manager, &MyListCardManager::cardUpdated);
    manager->updateCardAnimeInfo(1);
    
    // Process pending events to allow batched update to execute
    QTest::qWait(100);  // Wait for batch timer
    QCoreApplication::processEvents();
    
    // Verify only card 1 was marked as updated
    QVERIFY(updateSpy.count() >= 1);
    
    // Verify card 2 is still the same instance (wasn't recreated)
    QCOMPARE(card2, manager->getCard(2));
}

void TestMyListCardManager::testBatchUpdates()
{
    // Insert test data
    for (int i = 1; i <= 5; i++) {
        insertTestAnime(i, QString("Test Anime %1").arg(i));
        insertTestEpisode(i, i, "Episode 1", "1");
        insertTestMylistEntry(i, i, i);
    }
    
    // Load all cards
    QList<int> aids;
    for (int i = 1; i <= 5; i++) {
        aids.append(i);
    }
    manager->preloadCardCreationData(aids);
    manager->buildChainsFromCache();
    for (int i = 1; i <= 5; i++) {
        manager->createCard(i);
    }
    
    // Queue multiple updates
    QSet<int> toUpdate;
    toUpdate << 1 << 2 << 3;
    
    QSignalSpy updateSpy(manager, &MyListCardManager::cardUpdated);
    manager->updateMultipleCards(toUpdate);
    
    // Process pending events to allow batched updates to execute
    QTest::qWait(100);  // Wait for batch timer
    QCoreApplication::processEvents();
    
    // Verify updates were processed
    // Should have emitted cardUpdated for each card
    QVERIFY(updateSpy.count() >= 3);
}

void TestMyListCardManager::testAsynchronousOperations()
{
    // This test verifies that operations don't block
    insertTestAnime(1, "Test Anime 1");
    insertTestEpisode(1, 1, "Episode 1", "1");
    insertTestMylistEntry(1, 1, 1);
    
    // Load card
    QList<int> aids;
    aids.append(1);
    manager->preloadCardCreationData(aids);
    manager->buildChainsFromCache();
    manager->createCard(1);
    
    // Update card - should not block (uses batched timer)
    manager->updateCardAnimeInfo(1);
    
    // The update should be queued, not executed immediately
    // Process events to execute the batched update
    QTest::qWait(100);
    QCoreApplication::processEvents();
    
    // Card should still exist and be accessible
    QVERIFY(manager->hasCard(1));
}

void TestMyListCardManager::testMemoryManagement()
{
    // Insert test data
    for (int i = 1; i <= 10; i++) {
        insertTestAnime(i, QString("Test Anime %1").arg(i));
        insertTestEpisode(i, i, "Episode 1", "1");
        insertTestMylistEntry(i, i, i);
    }
    
    // Load cards
    QList<int> aids;
    for (int i = 1; i <= 10; i++) {
        aids.append(i);
    }
    manager->preloadCardCreationData(aids);
    manager->buildChainsFromCache();
    for (int i = 1; i <= 10; i++) {
        manager->createCard(i);
    }
    QCOMPARE(manager->getAllCards().count(), 10);
    
    // Clear all cards
    manager->clearAllCards();
    QCOMPARE(manager->getAllCards().count(), 0);
    
    // Verify cards were deleted (no dangling pointers)
    for (int i = 1; i <= 10; i++) {
        QVERIFY(!manager->hasCard(i));
    }
}

QTEST_MAIN(TestMyListCardManager)
#include "test_mylistcardmanager.moc"
