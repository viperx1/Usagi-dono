#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "../usagi/src/aired.h"

class TestMylistTypeAired : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testDatabaseSchema();
    void testAnimeDataStorage();
    void testMylistQuery();
    void testUpdateWithExistingEpisodeCounts();

private:
    QSqlDatabase db;
};

void TestMylistTypeAired::initTestCase()
{
    // Create an in-memory database for testing
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    
    if(!db.open())
    {
        QFAIL("Failed to open database");
    }
    
    // Create tables with new schema
    QSqlQuery query(db);
    query.exec("CREATE TABLE `anime`(`aid` INTEGER PRIMARY KEY, `eptotal` INTEGER, `eps` INTEGER, "
               "`nameromaji` TEXT, `nameenglish` TEXT, `typename` TEXT, `startdate` TEXT, `enddate` TEXT)");
    query.exec("CREATE TABLE `mylist`(`lid` INTEGER PRIMARY KEY, `aid` INTEGER, `eid` INTEGER, "
               "`state` INTEGER, `viewed` INTEGER, `storage` TEXT)");
    query.exec("CREATE TABLE `episode`(`eid` INTEGER PRIMARY KEY, `name` TEXT, `epno` TEXT)");
}

void TestMylistTypeAired::cleanupTestCase()
{
    db.close();
}

void TestMylistTypeAired::testDatabaseSchema()
{
    // Verify that typename, startdate, enddate columns exist
    QSqlQuery query(db);
    query.exec("PRAGMA table_info(anime)");
    
    QStringList columns;
    while(query.next())
    {
        columns << query.value(1).toString();
    }
    
    QVERIFY(columns.contains("typename"));
    QVERIFY(columns.contains("startdate"));
    QVERIFY(columns.contains("enddate"));
}

void TestMylistTypeAired::testAnimeDataStorage()
{
    // Insert test anime data with type and dates
    QSqlQuery query(db);
    query.prepare("INSERT INTO anime (aid, eptotal, eps, nameromaji, typename, startdate, enddate) "
                  "VALUES (:aid, :eptotal, :eps, :name, :typename, :startdate, :enddate)");
    query.bindValue(":aid", 1135);
    query.bindValue(":eptotal", 1);
    query.bindValue(":eps", 1);
    query.bindValue(":name", ".hack//Gift");
    query.bindValue(":typename", "OVA");
    query.bindValue(":startdate", "2003-11-16Z");
    query.bindValue(":enddate", "2003-11-16Z");
    
    QVERIFY(query.exec());
    
    // Verify data was stored correctly
    query.exec("SELECT typename, startdate, enddate FROM anime WHERE aid = 1135");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OVA"));
    QCOMPARE(query.value(1).toString(), QString("2003-11-16Z"));
    QCOMPARE(query.value(2).toString(), QString("2003-11-16Z"));
}

void TestMylistTypeAired::testMylistQuery()
{
    // Insert test mylist entry
    QSqlQuery query(db);
    query.exec("INSERT INTO mylist (lid, aid, eid, state, viewed, storage) "
               "VALUES (1, 1135, 12814, 2, 1, 'a005')");
    
    // Run the mylist query to ensure it works with new columns
    QString queryStr = "SELECT m.lid, m.aid, m.eid, m.state, m.viewed, m.storage, "
                      "a.nameromaji, a.nameenglish, a.eptotal, "
                      "a.eps, a.typename, a.startdate, a.enddate "
                      "FROM mylist m "
                      "LEFT JOIN anime a ON m.aid = a.aid";
    
    query.exec(queryStr);
    QVERIFY(query.next());
    
    // Verify all fields are accessible
    QCOMPARE(query.value(0).toInt(), 1);      // lid
    QCOMPARE(query.value(1).toInt(), 1135);   // aid
    QCOMPARE(query.value(6).toString(), QString(".hack//Gift"));  // nameromaji
    QCOMPARE(query.value(10).toString(), QString("OVA"));         // typename
    QCOMPARE(query.value(11).toString(), QString("2003-11-16Z")); // startdate
    QCOMPARE(query.value(12).toString(), QString("2003-11-16Z")); // enddate
    
    // Test aired date formatting
    QString startDate = query.value(11).toString();
    QString endDate = query.value(12).toString();
    aired airedDates(startDate, endDate);
    QString displayString = airedDates.toDisplayString();
    
    // Finished anime from 2003 should show date range
    QCOMPARE(displayString, QString("16.11.2003-16.11.2003"));
}

void TestMylistTypeAired::testUpdateWithExistingEpisodeCounts()
{
    // This test simulates the bug scenario:
    // 1. User had an old database with eptotal/eps already set
    // 2. Upgraded to new version with typename/startdate/enddate columns (NULL)
    // 3. Loads mylist XML which should update typename/startdate/enddate
    
    // Insert anime with eptotal/eps set but typename/startdate/enddate as NULL
    QSqlQuery query(db);
    query.prepare("INSERT INTO anime (aid, eptotal, eps, nameromaji) "
                  "VALUES (:aid, :eptotal, :eps, :name)");
    query.bindValue(":aid", 222);
    query.bindValue(":eptotal", 4);
    query.bindValue(":eps", 4);
    query.bindValue(":name", ".hack//Liminality");
    QVERIFY(query.exec());
    
    // Verify typename/startdate/enddate are NULL
    query.exec("SELECT typename, startdate, enddate FROM anime WHERE aid = 222");
    QVERIFY(query.next());
    QVERIFY(query.value(0).isNull());  // typename should be NULL
    QVERIFY(query.value(1).isNull());  // startdate should be NULL
    QVERIFY(query.value(2).isNull());  // enddate should be NULL
    
    // Simulate the UPDATE query that should run when loading mylist XML
    // This is what the fixed code does - update typename/startdate/enddate independently
    query.prepare("UPDATE `anime` SET `typename` = :typename, "
                  "`startdate` = :startdate, `enddate` = :enddate WHERE `aid` = :aid");
    query.bindValue(":typename", QString("OVA"));
    query.bindValue(":startdate", QString("2002-06-20Z"));
    query.bindValue(":enddate", QString("2003-04-10Z"));
    query.bindValue(":aid", 222);
    QVERIFY(query.exec());
    
    // Verify that typename/startdate/enddate were updated successfully
    query.exec("SELECT typename, startdate, enddate, eptotal, eps FROM anime WHERE aid = 222");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("OVA"));          // typename updated
    QCOMPARE(query.value(1).toString(), QString("2002-06-20Z"));  // startdate updated
    QCOMPARE(query.value(2).toString(), QString("2003-04-10Z"));  // enddate updated
    QCOMPARE(query.value(3).toInt(), 4);                          // eptotal unchanged
    QCOMPARE(query.value(4).toInt(), 4);                          // eps unchanged
    
    // Verify aired date formatting works
    aired airedDates(query.value(1).toString(), query.value(2).toString());
    QString displayString = airedDates.toDisplayString();
    QCOMPARE(displayString, QString("20.06.2002-10.04.2003"));
}

QTEST_MAIN(TestMylistTypeAired)
#include "test_mylist_type_aired.moc"
