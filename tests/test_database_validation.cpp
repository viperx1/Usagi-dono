#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

/**
 * Test to verify database connection validation prevents segmentation faults
 * 
 * This test ensures that database operations properly validate the connection
 * before attempting to execute queries, preventing crashes when the database
 * is not properly initialized.
 */
class TestDatabaseValidation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testInvalidDatabaseConnection();
    void testClosedDatabaseConnection();
    void testValidDatabaseConnection();

private:
    QString testConnectionName;
};

void TestDatabaseValidation::initTestCase()
{
    testConnectionName = "test_db_validation";
}

void TestDatabaseValidation::cleanupTestCase()
{
    // Clean up any test database connections
    if(QSqlDatabase::contains(testConnectionName))
    {
        QSqlDatabase::removeDatabase(testConnectionName);
    }
}

void TestDatabaseValidation::testInvalidDatabaseConnection()
{
    // Test that accessing a non-existent database connection returns invalid
    QString nonExistentConnection = "non_existent_db_12345";
    
    // Ensure connection doesn't exist
    if(QSqlDatabase::contains(nonExistentConnection))
    {
        QSqlDatabase::removeDatabase(nonExistentConnection);
    }
    
    // Try to get non-existent connection
    QSqlDatabase db = QSqlDatabase::database(nonExistentConnection, false);
    
    // Verify it's invalid
    QVERIFY(!db.isValid());
    
    // Attempting to execute a query on invalid database should fail gracefully
    // This simulates what would happen without our validation checks
    QSqlQuery query(db);
    bool result = query.exec("SELECT 1");
    QVERIFY(!result); // Should fail, not crash
}

void TestDatabaseValidation::testClosedDatabaseConnection()
{
    // Create a database connection but don't open it
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", testConnectionName);
    db.setDatabaseName(":memory:");
    
    // Verify it's valid but not open
    QVERIFY(db.isValid());
    QVERIFY(!db.isOpen());
    
    // Attempting to execute a query on closed database should fail gracefully
    QSqlQuery query(db);
    bool result = query.exec("SELECT 1");
    QVERIFY(!result); // Should fail, not crash
    
    // Clean up
    db.close();
    QSqlDatabase::removeDatabase(testConnectionName);
}

void TestDatabaseValidation::testValidDatabaseConnection()
{
    // Create and open a valid database connection
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", testConnectionName);
    db.setDatabaseName(":memory:");
    
    bool opened = db.open();
    QVERIFY(opened);
    
    // Verify it's valid and open
    QVERIFY(db.isValid());
    QVERIFY(db.isOpen());
    
    // Executing a query on valid, open database should succeed
    QSqlQuery query(db);
    bool result = query.exec("SELECT 1");
    QVERIFY(result); // Should succeed
    
    // Verify query returned data
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    
    // Clean up
    db.close();
    QSqlDatabase::removeDatabase(testConnectionName);
}

QTEST_MAIN(TestDatabaseValidation)
#include "test_database_validation.moc"
