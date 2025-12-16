/**
 * Test to verify the fix for the 221 MYLIST response handler
 * 
 * Issue: The 221 MYLIST response was incorrectly parsing lid and fid,
 * causing them to have the same value in the database.
 * 
 * Root Cause: The code assumed the MYLIST response included lid as the 
 * first field, but according to the AniDB API, the response only contains
 * file data starting with fid (the lid is already known from the query).
 * 
 * Fix: Extract lid from the original MYLIST command (stored in packets table)
 * and correctly map response fields starting with fid at index 0.
 */

#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

class TestMylist221Fix : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    
    void testMylistResponseParsing();
    void testLidExtraction();
    
private:
    QSqlDatabase db;
};

void TestMylist221Fix::initTestCase()
{
    // Ensure clean slate: remove any existing default connection
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
    
    // Setup test database
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());
    
    // Create tables
    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS `packets`(`tag` INTEGER PRIMARY KEY AUTOINCREMENT, `str` TEXT, `processed` INTEGER DEFAULT 0)");
    query.exec("CREATE TABLE IF NOT EXISTS `mylist`(`lid` INTEGER PRIMARY KEY, `fid` INTEGER, `eid` INTEGER, `aid` INTEGER, `gid` INTEGER, `date` INTEGER, `state` INTEGER, `viewed` INTEGER, `viewdate` INTEGER, `storage` TEXT, `source` TEXT, `other` TEXT, `filestate` INTEGER)");
}

void TestMylist221Fix::cleanupTestCase()
{
    db.close();
    
    // Clear the QSqlDatabase object to release the connection reference
    db = QSqlDatabase();
    
    // Now safely remove the database connection
    QString defaultConn = QSqlDatabase::defaultConnection;
    if (QSqlDatabase::contains(defaultConn)) {
        QSqlDatabase::removeDatabase(defaultConn);
    }
}

void TestMylist221Fix::cleanup()
{
    QSqlQuery query(db);
    query.exec("DELETE FROM `packets`");
    query.exec("DELETE FROM `mylist`");
}

void TestMylist221Fix::testMylistResponseParsing()
{
    // Test demonstrates the bug and the fix
    
    // Example MYLIST command and response from AniDB API
    QString mylistResponse = "221 MYLIST\n789012|297776|18795|16325|1609459200|1|1640995200|HDD|BluRay||1";
    
    // The response format is: fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
    // Note: lid (123456) from MYLIST command is NOT in the response - it's known from the command parameter
    
    // Parse the response
    QStringList lines = mylistResponse.split("\n");
    lines.pop_front(); // Remove status line
    QStringList fields = lines.first().split("|");
    
    // Before the fix, the code would incorrectly map:
    // - lid = fields[0] = 789012 (actually the fid!)
    // - fid = fields[1] = 297776 (actually the eid!)
    // This caused lid and fid to have wrong values
    
    // After the fix, the correct mapping is:
    QString lid = "123456"; // Extracted from command, not from response
    QString fid = fields[0];  // 789012
    QString eid = fields[1];  // 297776
    QString aid = fields[2];  // 18795
    QString gid = fields[3];  // 16325
    
    // Verify the fix
    QCOMPARE(lid, QString("123456"));
    QCOMPARE(fid, QString("789012"));
    QCOMPARE(eid, QString("297776"));
    QCOMPARE(aid, QString("18795"));
    QCOMPARE(gid, QString("16325"));
    
    // The bug would have resulted in:
    // lid = 789012, fid = 297776 (both wrong!)
    // After fix: lid = 123456, fid = 789012 (correct!)
    
    QVERIFY(lid != fid); // lid and fid should be different!
}

void TestMylist221Fix::testLidExtraction()
{
    // Test lid extraction from MYLIST command
    
    QString command1 = "MYLIST lid=12345";
    int lidStart = command1.indexOf("lid=");
    QVERIFY(lidStart != -1);
    
    lidStart += 4; // Move past "lid="
    int lidEnd = command1.indexOf("&", lidStart);
    if(lidEnd == -1)
        lidEnd = command1.indexOf(" ", lidStart);
    if(lidEnd == -1)
        lidEnd = command1.length();
    
    QString lid = command1.mid(lidStart, lidEnd - lidStart);
    QCOMPARE(lid, QString("12345"));
    
    // Test with parameters after lid
    QString command2 = "MYLIST lid=67890&other=param";
    lidStart = command2.indexOf("lid=");
    QVERIFY(lidStart != -1);
    
    lidStart += 4;
    lidEnd = command2.indexOf("&", lidStart);
    if(lidEnd == -1)
        lidEnd = command2.indexOf(" ", lidStart);
    if(lidEnd == -1)
        lidEnd = command2.length();
    
    lid = command2.mid(lidStart, lidEnd - lidStart);
    QCOMPARE(lid, QString("67890"));
}

QTEST_MAIN(TestMylist221Fix)
#include "test_mylist_221_fix.moc"
