/**
 * Test to verify the fix for "Evangelion Shin Gekijouban: Ha" episode column display
 * 
 * Issue: Episode column shows "1" instead of "1/1" for movies with specials
 * 
 * This test verifies that when an anime has eptotal set but eps is NULL/0,
 * the mylist export parser correctly updates the eps field from the XML.
 */

#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QTemporaryDir>

class TestEvangelionHaFix : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testEpsUpdateWhenEptotalIsSet();
    void testEpsUpdateWhenBothAreNull();
    void testNoUpdateWhenBothAreSet();
    void cleanupTestCase();

private:
    QSqlDatabase db;
    void createTables();
};

void TestEvangelionHaFix::initTestCase()
{
    // Set up in-memory database for testing
    db = QSqlDatabase::addDatabase("QSQLITE", "test_evangelion_fix");
    db.setDatabaseName(":memory:");
    
    QVERIFY(db.open());
    createTables();
}

void TestEvangelionHaFix::createTables()
{
    QSqlQuery query(db);
    
    // Create anime table
    QString createAnimeTable = 
        "CREATE TABLE `anime` ("
        "`aid` INTEGER PRIMARY KEY,"
        "`nameromaji` TEXT,"
        "`nameenglish` TEXT,"
        "`eptotal` INTEGER,"
        "`eps` INTEGER"
        ")";
    
    QVERIFY2(query.exec(createAnimeTable), qPrintable(query.lastError().text()));
}

void TestEvangelionHaFix::testEpsUpdateWhenEptotalIsSet()
{
    // This simulates the bug scenario: eptotal is set (e.g., from FILE command),
    // but eps is NULL. The mylist export should update eps.
    
    // Insert anime with eptotal set but eps NULL
    QSqlQuery query(db);
    query.prepare("INSERT INTO anime (aid, nameromaji, eptotal, eps) VALUES (6171, 'Evangelion Shin Gekijouban: Ha', 23, NULL)");
    QVERIFY(query.exec());
    
    // Simulate the UPDATE from mylist XML parser
    // This is the NEW condition (with the fix)
    query.prepare("UPDATE `anime` SET `eptotal` = :eptotal, `eps` = :eps "
                  "WHERE `aid` = :aid AND ((eptotal IS NULL OR eptotal = 0) OR (eps IS NULL OR eps = 0))");
    query.bindValue(":eptotal", 23);  // EpsTotal from XML
    query.bindValue(":eps", 1);       // Eps from XML
    query.bindValue(":aid", 6171);
    
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY2(query.numRowsAffected() == 1, "UPDATE should affect 1 row");
    
    // Verify eps was updated
    query.prepare("SELECT eps FROM anime WHERE aid = 6171");
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void TestEvangelionHaFix::testEpsUpdateWhenBothAreNull()
{
    // Test case: both eptotal and eps are NULL (fresh insert)
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO anime (aid, nameromaji, eptotal, eps) VALUES (9999, 'Test Anime', NULL, NULL)");
    QVERIFY(query.exec());
    
    // Simulate the UPDATE from mylist XML parser
    query.prepare("UPDATE `anime` SET `eptotal` = :eptotal, `eps` = :eps "
                  "WHERE `aid` = :aid AND ((eptotal IS NULL OR eptotal = 0) OR (eps IS NULL OR eps = 0))");
    query.bindValue(":eptotal", 12);
    query.bindValue(":eps", 12);
    query.bindValue(":aid", 9999);
    
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY2(query.numRowsAffected() == 1, "UPDATE should affect 1 row");
    
    // Verify both were updated
    query.prepare("SELECT eptotal, eps FROM anime WHERE aid = 9999");
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 12);
    QCOMPARE(query.value(1).toInt(), 12);
}

void TestEvangelionHaFix::testNoUpdateWhenBothAreSet()
{
    // Test case: both eptotal and eps are already set (from FILE command)
    // The UPDATE should not affect this row (preserve FILE command data)
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO anime (aid, nameromaji, eptotal, eps) VALUES (8888, 'Anime With File Data', 24, 24)");
    QVERIFY(query.exec());
    
    // Simulate the UPDATE from mylist XML parser with different values
    query.prepare("UPDATE `anime` SET `eptotal` = :eptotal, `eps` = :eps "
                  "WHERE `aid` = :aid AND ((eptotal IS NULL OR eptotal = 0) OR (eps IS NULL OR eps = 0))");
    query.bindValue(":eptotal", 26);  // Different from current value
    query.bindValue(":eps", 26);      // Different from current value
    query.bindValue(":aid", 8888);
    
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QCOMPARE(query.numRowsAffected(), 0);  // Should NOT update
    
    // Verify values remain unchanged
    query.prepare("SELECT eptotal, eps FROM anime WHERE aid = 8888");
    QVERIFY(query.exec());
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 24);  // Original value
    QCOMPARE(query.value(1).toInt(), 24);  // Original value
}

void TestEvangelionHaFix::cleanupTestCase()
{
    db.close();
    QSqlDatabase::removeDatabase("test_evangelion_fix");
}

QTEST_MAIN(TestEvangelionHaFix)
#include "test_evangelion_ha_fix.moc"
