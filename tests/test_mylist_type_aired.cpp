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

QTEST_MAIN(TestMylistTypeAired)
#include "test_mylist_type_aired.moc"
