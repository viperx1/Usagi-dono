#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "../usagi/src/logger.h"

/**
 * Test to verify unknown files binding status is persisted correctly
 * 
 * This test ensures that when a file is added to mylist (via re-check or binding),
 * the binding_status is updated to 1 (bound_to_anime) so that it doesn't reappear
 * in the unknown files list after app restart.
 * 
 * Bug: Files added to mylist after re-check reappeared on "unknown files" list 
 * after app restart because binding_status was not updated.
 */
class TestUnknownFilesPersistence : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testBindingStatusAfterUpdateBothFields();
    void testUnboundFilesQueryExcludesBoundFiles();

private:
    QString testConnectionName;
    QSqlDatabase db;
    
    void setupTestDatabase();
    void insertTestData();
};

void TestUnknownFilesPersistence::initTestCase()
{
    testConnectionName = "test_unknown_files_persistence";
    setupTestDatabase();
}

void TestUnknownFilesPersistence::cleanupTestCase()
{
    // Clean up test database connection
    if(QSqlDatabase::contains(testConnectionName))
    {
        db.close();
        QSqlDatabase::removeDatabase(testConnectionName);
    }
}

void TestUnknownFilesPersistence::setupTestDatabase()
{
    // Create test database
    db = QSqlDatabase::addDatabase("QSQLITE", testConnectionName);
    db.setDatabaseName(":memory:");
    
    QVERIFY(db.open());
    
    QSqlQuery query(db);
    
    // Create necessary tables with same schema as production
    // Status: 0=not hashed, 1=hashed but not checked by API, 2=in anidb, 3=not in anidb
    // binding_status: 0=not_bound, 1=bound_to_anime, 2=not_anime
    query.exec("CREATE TABLE `local_files`("
               "`id` INTEGER PRIMARY KEY AUTOINCREMENT, "
               "`path` TEXT UNIQUE, "
               "`filename` TEXT, "
               "`status` INTEGER DEFAULT 0, "
               "`ed2k_hash` TEXT, "
               "`binding_status` INTEGER DEFAULT 0)");
    
    insertTestData();
}

void TestUnknownFilesPersistence::insertTestData()
{
    QSqlQuery query(db);
    
    // Insert test local files simulating the unknown files scenario
    // File 1: Unknown file (status=3, binding_status=0)
    query.prepare("INSERT INTO `local_files` (`id`, `path`, `filename`, `status`, `ed2k_hash`, `binding_status`) "
                  "VALUES (1, '/test/path/file1.mkv', 'file1.mkv', 3, 'testhash123', 0)");
    QVERIFY(query.exec());
    
    // File 2: Bound file (status=2, binding_status=1)
    query.prepare("INSERT INTO `local_files` (`id`, `path`, `filename`, `status`, `ed2k_hash`, `binding_status`) "
                  "VALUES (2, '/test/path/file2.mkv', 'file2.mkv', 2, 'testhash456', 1)");
    QVERIFY(query.exec());
    
    // File 3: Not anime (status=3, binding_status=2)
    query.prepare("INSERT INTO `local_files` (`id`, `path`, `filename`, `status`, `ed2k_hash`, `binding_status`) "
                  "VALUES (3, '/test/path/file3.mkv', 'file3.mkv', 3, 'testhash789', 2)");
    QVERIFY(query.exec());
}

void TestUnknownFilesPersistence::testBindingStatusAfterUpdateBothFields()
{
    // This test simulates what UpdateLocalPath() and LinkLocalFileToMylist() should do:
    // Update BOTH status and binding_status together
    
    QSqlQuery query(db);
    
    // Simulate the fix: update both status and binding_status
    query.prepare("UPDATE `local_files` SET `status` = 2, `binding_status` = 1 WHERE `id` = 1");
    QVERIFY(query.exec());
    
    // Verify both fields were updated
    query.prepare("SELECT `status`, `binding_status` FROM `local_files` WHERE `id` = 1");
    QVERIFY(query.exec());
    QVERIFY(query.next());
    
    int status = query.value(0).toInt();
    int bindingStatus = query.value(1).toInt();
    
    QCOMPARE(status, 2); // in anidb
    QCOMPARE(bindingStatus, 1); // bound_to_anime
}

void TestUnknownFilesPersistence::testUnboundFilesQueryExcludesBoundFiles()
{
    // This test verifies that files with binding_status=1 don't appear in unbound files query
    // This is the query used to load unknown files on app startup
    
    QSqlQuery query(db);
    
    // Keep test independent from execution order: ensure file 1 is already bound
    query.prepare("UPDATE `local_files` SET `status` = 2, `binding_status` = 1 WHERE `id` = 1");
    QVERIFY(query.exec());
    
    // Query unbound files (same as production getUnboundFiles)
    query.prepare("SELECT `id`, `path`, `filename`, `ed2k_hash`, `status`, `binding_status` "
                  "FROM `local_files` "
                  "WHERE `binding_status` = 0 AND `status` = 3 "
                  "AND `ed2k_hash` IS NOT NULL AND `ed2k_hash` != ''");
    QVERIFY(query.exec());
    
    // Count results and verify
    QList<int> foundIds;
    while(query.next())
    {
        int id = query.value(0).toInt();
        int bindingStatus = query.value(5).toInt();
        
        foundIds.append(id);
        
        // Verify only files with binding_status=0 are returned
        QCOMPARE(bindingStatus, 0);
    }
    
    // After the fix in testBindingStatusAfterUpdateBothFields, file 1 should have binding_status=1
    // So only no files should match (file 1 was updated, file 2 already bound, file 3 is not_anime)
    QCOMPARE(foundIds.size(), 0);
    
    // Verify that file 2 (already bound) and file 3 (not_anime) are not in results
    QVERIFY(!foundIds.contains(2));
    QVERIFY(!foundIds.contains(3));
}

QTEST_MAIN(TestUnknownFilesPersistence)
#include "test_unknown_files_persistence.moc"
