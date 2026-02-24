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
 * Test suite for missing anime data request deduplication
 *
 * Tests verify the production code path for:
 * 1. Detecting missing prequels during chain building
 * 2. Detecting missing sequels during chain building
 * 3. Detecting multiple missing anime during chain building
 *
 * All tests exercise production code without test-mode overrides.
 */
class TestMissingAnimeDataRequest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testDetectMissingPrequel();
    void testDetectMissingSequel();
    void testDetectMultipleMissingAnime();

private:
    MyListCardManager *manager;
    FlowLayout *layout;
    QWidget *container;
    QSqlDatabase db;

    void createTestDatabase();
    void insertAnime(int aid, const QString &name,
                     const QString &relaidlist = QString(),
                     const QString &relaidtype = QString());
    void insertEpisode(int aid, int eid, const QString &name, const QString &epno);
    void insertMylistEntry(int lid, int aid, int eid);
};

void TestMissingAnimeDataRequest::initTestCase()
{
    qputenv("USAGI_TEST_MODE", "1");

    adbapi = new myAniDBApi("test", 1);

    {
        QString defaultConn = QSqlDatabase::defaultConnection;
        if (QSqlDatabase::contains(defaultConn)) {
            QSqlDatabase existingDb = QSqlDatabase::database(defaultConn, false);
            if (existingDb.isOpen()) {
                existingDb.close();
            }
            QSqlDatabase::removeDatabase(defaultConn);
        }
    }

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
    db = QSqlDatabase();

    QString defaultConn = QSqlDatabase::defaultConnection;
    if (QSqlDatabase::contains(defaultConn)) {
        QSqlDatabase::removeDatabase(defaultConn);
    }
}

void TestMissingAnimeDataRequest::init()
{
    container = new QWidget();
    layout = new FlowLayout(container);
    manager = new MyListCardManager();
    manager->setCardLayout(layout);

    // Clear test data between runs
    QSqlQuery q(db);
    q.exec("DELETE FROM anime");
    q.exec("DELETE FROM episode");
    q.exec("DELETE FROM mylist");
    q.exec("DELETE FROM anime_titles");
}

void TestMissingAnimeDataRequest::cleanup()
{
    delete manager;
    delete container;
}

void TestMissingAnimeDataRequest::createTestDatabase()
{
    QSqlQuery q(db);

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
           "is_18_restricted INTEGER DEFAULT 0, "
           "relaidlist TEXT, "
           "relaidtype TEXT)");

    q.exec("CREATE TABLE episode ("
           "eid INTEGER PRIMARY KEY, "
           "aid INTEGER, "
           "epno TEXT, "
           "name TEXT)");

    q.exec("CREATE TABLE file ("
           "fid INTEGER PRIMARY KEY, "
           "filename TEXT, "
           "resolution TEXT, "
           "quality TEXT)");

    q.exec("CREATE TABLE `group` ("
           "gid INTEGER PRIMARY KEY, "
           "name TEXT)");

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

    q.exec("CREATE TABLE anime_titles ("
           "aid INTEGER, "
           "type INTEGER, "
           "language TEXT, "
           "title TEXT)");

    q.exec("CREATE TABLE local_files ("
           "id INTEGER PRIMARY KEY, "
           "path TEXT)");
}

void TestMissingAnimeDataRequest::insertAnime(int aid, const QString &name,
                                               const QString &relaidlist,
                                               const QString &relaidtype)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO anime (aid, nameromaji, eptotal, typename, startdate, enddate, relaidlist, relaidtype) "
              "VALUES (?, ?, 12, 'TV Series', '2020-01-01', '2020-03-31', ?, ?)");
    q.addBindValue(aid);
    q.addBindValue(name);
    q.addBindValue(relaidlist);
    q.addBindValue(relaidtype);
    q.exec();

    q.prepare("INSERT INTO anime_titles (aid, type, language, title) VALUES (?, 1, 'x-jat', ?)");
    q.addBindValue(aid);
    q.addBindValue(name);
    q.exec();
}

void TestMissingAnimeDataRequest::insertEpisode(int aid, int eid, const QString &name, const QString &epno)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO episode (eid, aid, epno, name) VALUES (?, ?, ?, ?)");
    q.addBindValue(eid);
    q.addBindValue(aid);
    q.addBindValue(epno);
    q.addBindValue(name);
    q.exec();
}

void TestMissingAnimeDataRequest::insertMylistEntry(int lid, int aid, int eid)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO mylist (lid, aid, eid, fid, state, viewed, storage) "
              "VALUES (?, ?, ?, 1, 1, 0, '/test/path')");
    q.addBindValue(lid);
    q.addBindValue(aid);
    q.addBindValue(eid);
    q.exec();
}

// Verify chain expansion discovers a prequel that the user does not own.
// Anime 200 (sequel) owns a prequel relation to 100.  The user only has
// anime 200 in their mylist, so 100 must be discovered via relation data.
void TestMissingAnimeDataRequest::testDetectMissingPrequel()
{
    // Anime 100: the prequel – user does NOT have it in mylist
    // relaidlist "200", relaidtype "1" means anime 200 is a sequel of 100
    insertAnime(100, "Prequel Anime", "200", "1");
    insertEpisode(100, 1001, "Episode 1", "1");

    // Anime 200: the sequel – user HAS this in mylist
    // relaidlist "100", relaidtype "2" means anime 100 is a prequel of 200
    insertAnime(200, "Sequel Anime", "100", "2");
    insertEpisode(200, 2001, "Episode 1", "1");
    insertMylistEntry(1, 200, 2001);

    // Preload only anime that the user owns
    QList<int> userAids;
    userAids << 200;
    manager->preloadCardCreationData(userAids);

    // buildChainsFromCache internally calls preloadRelationDataForChainExpansion
    // which should discover anime 100 as a prequel of 200
    manager->buildChainsFromCache();

    // After chain building, the cache should now contain both anime
    QVERIFY2(manager->hasCachedData(200), "User anime 200 must be in cache");
    QVERIFY2(manager->hasCachedData(100), "Prequel anime 100 must be discovered via chain expansion");

    // The chain containing anime 200 should also contain anime 100
    AnimeChain chain = manager->getChainForAnime(200);
    QVERIFY2(!chain.isEmpty(), "Chain for anime 200 must not be empty");
    QVERIFY2(chain.contains(100), "Chain must include prequel anime 100");
    QVERIFY2(chain.contains(200), "Chain must include user anime 200");

    // Prequel should come first in the chain
    QList<int> ids = chain.getAnimeIds();
    int idx100 = ids.indexOf(100);
    int idx200 = ids.indexOf(200);
    QVERIFY2(idx100 < idx200, "Prequel 100 must appear before sequel 200 in the chain");
}

// Verify chain expansion discovers a sequel that the user does not own.
void TestMissingAnimeDataRequest::testDetectMissingSequel()
{
    // Anime 300: the prequel – user HAS this in mylist
    // relaidlist "400", relaidtype "1" means anime 400 is a sequel of 300
    insertAnime(300, "Prequel Anime", "400", "1");
    insertEpisode(300, 3001, "Episode 1", "1");
    insertMylistEntry(2, 300, 3001);

    // Anime 400: the sequel – user does NOT have it in mylist
    // relaidlist "300", relaidtype "2" means anime 300 is a prequel of 400
    insertAnime(400, "Sequel Anime", "300", "2");
    insertEpisode(400, 4001, "Episode 1", "1");

    QList<int> userAids;
    userAids << 300;
    manager->preloadCardCreationData(userAids);
    manager->buildChainsFromCache();

    QVERIFY2(manager->hasCachedData(300), "User anime 300 must be in cache");
    QVERIFY2(manager->hasCachedData(400), "Sequel anime 400 must be discovered via chain expansion");

    AnimeChain chain = manager->getChainForAnime(300);
    QVERIFY2(!chain.isEmpty(), "Chain for anime 300 must not be empty");
    QVERIFY2(chain.contains(300), "Chain must include user anime 300");
    QVERIFY2(chain.contains(400), "Chain must include sequel anime 400");

    QList<int> ids = chain.getAnimeIds();
    int idx300 = ids.indexOf(300);
    int idx400 = ids.indexOf(400);
    QVERIFY2(idx300 < idx400, "Prequel 300 must appear before sequel 400 in the chain");
}

// Verify chain expansion discovers multiple missing anime (both prequel and sequel).
void TestMissingAnimeDataRequest::testDetectMultipleMissingAnime()
{
    // Chain: 500 (prequel) -> 600 (middle, user owns) -> 700 (sequel)
    // Anime 500: first in chain, user does NOT own
    insertAnime(500, "First Anime", "600", "1");
    insertEpisode(500, 5001, "Episode 1", "1");

    // Anime 600: middle, user OWNS, has prequel 500 and sequel 700
    insertAnime(600, "Middle Anime", "500'700", "2'1");
    insertEpisode(600, 6001, "Episode 1", "1");
    insertMylistEntry(3, 600, 6001);

    // Anime 700: last in chain, user does NOT own
    insertAnime(700, "Last Anime", "600", "2");
    insertEpisode(700, 7001, "Episode 1", "1");

    QList<int> userAids;
    userAids << 600;
    manager->preloadCardCreationData(userAids);
    manager->buildChainsFromCache();

    QVERIFY2(manager->hasCachedData(600), "User anime 600 must be in cache");
    QVERIFY2(manager->hasCachedData(500), "Prequel anime 500 must be discovered via chain expansion");
    QVERIFY2(manager->hasCachedData(700), "Sequel anime 700 must be discovered via chain expansion");

    AnimeChain chain = manager->getChainForAnime(600);
    QVERIFY2(!chain.isEmpty(), "Chain for anime 600 must not be empty");
    QCOMPARE(chain.size(), 3);
    QVERIFY2(chain.contains(500), "Chain must include prequel anime 500");
    QVERIFY2(chain.contains(600), "Chain must include user anime 600");
    QVERIFY2(chain.contains(700), "Chain must include sequel anime 700");

    QList<int> ids = chain.getAnimeIds();
    int idx500 = ids.indexOf(500);
    int idx600 = ids.indexOf(600);
    int idx700 = ids.indexOf(700);
    QVERIFY2(idx500 < idx600, "Prequel 500 must appear before middle 600");
    QVERIFY2(idx600 < idx700, "Middle 600 must appear before sequel 700");
}

QTEST_MAIN(TestMissingAnimeDataRequest)
#include "test_missing_anime_data_request.moc"
