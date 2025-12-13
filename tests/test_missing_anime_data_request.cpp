#include <QTest>
#include "../usagi/src/mylistcardmanager.h"
#include "../usagi/src/animecard.h"
#include "../usagi/src/flowlayout.h"
#include "../usagi/src/main.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSignalSpy>

/**
 * Test suite for missing anime data detection and API request
 * 
 * This test validates that:
 * 1. Missing related anime are detected during chain expansion
 * 2. missingAnimeDataDetected signal is emitted with correct AIDs
 * 3. Anime data is reloaded into cache when API response arrives
 */
class TestMissingAnimeDataRequest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Test cases
    void testDetectMissingPrequel();
    void testDetectMissingSequel();
    void testDetectMultipleMissingAnime();
    void testNoSignalWhenAllAnimePresent();
    void testAnimeDataReloadOnUpdate();
    
private:
    MyListCardManager *manager;
    FlowLayout *layout;
    QWidget *container;
    QSqlDatabase db;
    
    void createTestDatabase();
    void insertTestAnime(int aid, const QString &name, int prequelAid = 0, int sequelAid = 0);
};

void TestMissingAnimeDataRequest::initTestCase()
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

void TestMissingAnimeDataRequest::cleanupTestCase()
{
    delete adbapi;
    adbapi = nullptr;
    
    if (db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void TestMissingAnimeDataRequest::init()
{
    container = new QWidget();
    layout = new FlowLayout(container);
    manager = new MyListCardManager();
    manager->setCardLayout(layout);
}

void TestMissingAnimeDataRequest::cleanup()
{
    delete manager;
    delete container;
}

void TestMissingAnimeDataRequest::createTestDatabase()
{
    QSqlQuery q(db);
    
    // Create anime table with relation fields
    q.exec("CREATE TABLE anime ("
           "aid INTEGER PRIMARY KEY, "
           "nameromaji TEXT, "
           "nameenglish TEXT, "
           "eptotal INTEGER, "
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
           "relaidtype TEXT)");
    
    q.exec("CREATE TABLE episode ("
           "eid INTEGER PRIMARY KEY, "
           "aid INTEGER, "
           "epno TEXT, "
           "name TEXT)");
    
    q.exec("CREATE TABLE mylist ("
           "lid INTEGER PRIMARY KEY, "
           "aid INTEGER, "
           "eid INTEGER, "
           "fid INTEGER, "
           "state INTEGER, "
           "viewed INTEGER, "
           "storage TEXT)");
    
    q.exec("CREATE TABLE anime_titles ("
           "aid INTEGER, "
           "type INTEGER, "
           "language TEXT, "
           "title TEXT)");
}

void TestMissingAnimeDataRequest::insertTestAnime(int aid, const QString &name, int prequelAid, int sequelAid)
{
    QSqlQuery q(db);
    
    // Build relation lists
    QStringList relaidList;
    QStringList relaidType;
    
    if (prequelAid > 0) {
        relaidList.append(QString::number(prequelAid));
        relaidType.append("2");  // 2 = prequel
    }
    if (sequelAid > 0) {
        relaidList.append(QString::number(sequelAid));
        relaidType.append("1");  // 1 = sequel
    }
    
    QString relaidListStr = relaidList.isEmpty() ? "" : "'" + relaidList.join("','") + "'";
    QString relaidTypeStr = relaidType.isEmpty() ? "" : "'" + relaidType.join("','") + "'";
    
    q.prepare("INSERT INTO anime (aid, nameromaji, eptotal, typename, startdate, enddate, relaidlist, relaidtype) "
              "VALUES (?, ?, 12, 'TV Series', '2020-01-01', '2020-03-31', ?, ?)");
    q.addBindValue(aid);
    q.addBindValue(name);
    q.addBindValue(relaidListStr);
    q.addBindValue(relaidTypeStr);
    
    if (!q.exec()) {
        qDebug() << "Failed to insert anime:" << q.lastError().text();
    }
    
    // Insert title
    q.prepare("INSERT INTO anime_titles (aid, type, language, title) VALUES (?, 1, 'x-jat', ?)");
    q.addBindValue(aid);
    q.addBindValue(name);
    q.exec();
    
    // Insert a mylist entry so it's considered "in mylist"
    q.prepare("INSERT INTO mylist (lid, aid, eid, fid, state, viewed, storage) "
              "VALUES (?, ?, 1, 1, 1, 0, '/test')");
    q.addBindValue(aid);
    q.addBindValue(aid);
    
    if (!q.exec()) {
        qDebug() << "Failed to insert mylist:" << q.lastError().text();
    }
}

void TestMissingAnimeDataRequest::testDetectMissingPrequel()
{
    // Anime 2 has prequel 1, but anime 1 is NOT in database
    insertTestAnime(2, "Anime 2 (has missing prequel)", 1, 0);
    
    // Create signal spy to catch missing anime signal
    QSignalSpy spy(manager, &MyListCardManager::missingAnimeDataDetected);
    
    // Preload anime 2 (this should trigger preloadRelationDataForChainExpansion)
    QList<int> aids;
    aids.append(2);
    manager->preloadCardCreationData(aids);
    
    // Wait for signal
    QVERIFY(spy.wait(1000));
    
    // Verify signal was emitted with the missing anime ID
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QList<int> missingAids = arguments.at(0).value<QList<int>>();
    QCOMPARE(missingAids.size(), 1);
    QCOMPARE(missingAids.first(), 1);
}

void TestMissingAnimeDataRequest::testDetectMissingSequel()
{
    // Anime 1 has sequel 2, but anime 2 is NOT in database
    insertTestAnime(1, "Anime 1 (has missing sequel)", 0, 2);
    
    QSignalSpy spy(manager, &MyListCardManager::missingAnimeDataDetected);
    
    QList<int> aids;
    aids.append(1);
    manager->preloadCardCreationData(aids);
    
    QVERIFY(spy.wait(1000));
    
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QList<int> missingAids = arguments.at(0).value<QList<int>>();
    QCOMPARE(missingAids.size(), 1);
    QCOMPARE(missingAids.first(), 2);
}

void TestMissingAnimeDataRequest::testDetectMultipleMissingAnime()
{
    // Anime 2 is in the middle of a chain: prequel 1 (missing), sequel 3 (missing)
    insertTestAnime(2, "Anime 2 (has missing prequel and sequel)", 1, 3);
    
    QSignalSpy spy(manager, &MyListCardManager::missingAnimeDataDetected);
    
    QList<int> aids;
    aids.append(2);
    manager->preloadCardCreationData(aids);
    
    QVERIFY(spy.wait(1000));
    
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QList<int> missingAids = arguments.at(0).value<QList<int>>();
    QCOMPARE(missingAids.size(), 2);
    QVERIFY(missingAids.contains(1));
    QVERIFY(missingAids.contains(3));
}

void TestMissingAnimeDataRequest::testNoSignalWhenAllAnimePresent()
{
    // Both anime 1 and 2 are in database
    insertTestAnime(1, "Anime 1", 0, 2);
    insertTestAnime(2, "Anime 2", 1, 0);
    
    QSignalSpy spy(manager, &MyListCardManager::missingAnimeDataDetected);
    
    QList<int> aids;
    aids.append(1);
    manager->preloadCardCreationData(aids);
    
    // Wait a bit to ensure no signal is emitted
    QTest::qWait(500);
    
    // No signal should be emitted since all related anime are present
    QCOMPARE(spy.count(), 0);
}

void TestMissingAnimeDataRequest::testAnimeDataReloadOnUpdate()
{
    // Insert anime 1 with basic data
    insertTestAnime(1, "Anime 1 Original", 0, 0);
    
    // Preload and verify initial data
    QList<int> aids;
    aids.append(1);
    manager->preloadCardCreationData(aids);
    
    auto cachedData = manager->getCachedAnimeData(1);
    QVERIFY(cachedData.hasData());
    QVERIFY(cachedData.animeName().contains("Original"));
    
    // Update the anime in database
    QSqlQuery q(db);
    q.prepare("UPDATE anime SET nameromaji = ? WHERE aid = ?");
    q.addBindValue("Anime 1 Updated");
    q.addBindValue(1);
    q.exec();
    
    // Simulate API response by calling onAnimeUpdated
    manager->onAnimeUpdated(1);
    
    // Wait for preload to complete
    QTest::qWait(500);
    
    // Verify data was reloaded
    cachedData = manager->getCachedAnimeData(1);
    QVERIFY(cachedData.hasData());
    QVERIFY(cachedData.animeName().contains("Updated"));
}

QTEST_MAIN(TestMissingAnimeDataRequest)
#include "test_missing_anime_data_request.moc"
