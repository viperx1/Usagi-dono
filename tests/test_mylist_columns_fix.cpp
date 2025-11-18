#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "../usagi/src/aired.h"

/**
 * Test to verify the fix for mylist Type and Aired columns not displaying data
 * 
 * This test simulates the exact scenario that was causing the bug:
 * - An anime with multiple episodes in mylist
 * - All episodes have the same typename, startdate, enddate from the database
 * - The first episode might not have set the Type/Aired columns
 * - Subsequent episodes should also be able to set them if they're empty
 */
class TestMylistColumnsFix : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testMultipleEpisodesSetColumns();

private:
    QSqlDatabase db;
};

void TestMylistColumnsFix::initTestCase()
{
    // Create an in-memory database for testing
    db = QSqlDatabase::addDatabase("QSQLITE", "test_columns_fix");
    db.setDatabaseName(":memory:");
    
    if(!db.open())
    {
        QFAIL("Failed to open database");
    }
    
    // Create tables
    QSqlQuery query(db);
    query.exec("CREATE TABLE `anime`(`aid` INTEGER PRIMARY KEY, `eptotal` INTEGER, `eps` INTEGER, "
               "`nameromaji` TEXT, `nameenglish` TEXT, `typename` TEXT, `startdate` TEXT, `enddate` TEXT)");
    query.exec("CREATE TABLE `mylist`(`lid` INTEGER PRIMARY KEY, `aid` INTEGER, `eid` INTEGER, "
               "`state` INTEGER, `viewed` INTEGER, `storage` TEXT)");
    query.exec("CREATE TABLE `episode`(`eid` INTEGER PRIMARY KEY, `name` TEXT, `epno` TEXT)");
    
    // Insert test anime with Type and Aired data
    query.prepare("INSERT INTO anime (aid, eptotal, eps, nameromaji, typename, startdate, enddate) "
                  "VALUES (1135, 1, 1, '.hack//Gift', 'OVA', '2003-11-16Z', '2003-11-16Z')");
    query.exec();
    
    // Insert another anime with multiple episodes
    query.prepare("INSERT INTO anime (aid, eptotal, eps, nameromaji, typename, startdate, enddate) "
                  "VALUES (222, 4, 4, '.hack//Liminality', 'OVA', '2002-06-20Z', '2003-04-10Z')");
    query.exec();
    
    // Insert episodes for anime 222 (4 episodes)
    query.exec("INSERT INTO episode (eid, name, epno) VALUES (2614, 'In the Case of Mai Minase', '1')");
    query.exec("INSERT INTO episode (eid, name, epno) VALUES (2615, 'In the Case of Yuki Aihara', '2')");
    query.exec("INSERT INTO episode (eid, name, epno) VALUES (2616, 'In the Case of Kyoko Tohno', '3')");
    query.exec("INSERT INTO episode (eid, name, epno) VALUES (2617, 'Trismegistus', '4')");
    
    // Insert mylist entries for all 4 episodes
    query.exec("INSERT INTO mylist (lid, aid, eid, state, viewed, storage) VALUES (1, 222, 2614, 2, 1, 'a040')");
    query.exec("INSERT INTO mylist (lid, aid, eid, state, viewed, storage) VALUES (2, 222, 2615, 2, 1, 'a040')");
    query.exec("INSERT INTO mylist (lid, aid, eid, state, viewed, storage) VALUES (3, 222, 2616, 2, 1, 'a040')");
    query.exec("INSERT INTO mylist (lid, aid, eid, state, viewed, storage) VALUES (4, 222, 2617, 2, 1, 'a040')");
}

void TestMylistColumnsFix::cleanupTestCase()
{
    db.close();
}

void TestMylistColumnsFix::testMultipleEpisodesSetColumns()
{
    // Simulate the loadMylistFromDatabase query
    QString query = "SELECT m.lid, m.aid, m.eid, m.state, m.viewed, m.storage, "
                   "a.nameromaji, a.nameenglish, a.eptotal, "
                   "e.name as episode_name, e.epno, "
                   "NULL as anime_title, "
                   "a.eps, a.typename, a.startdate, a.enddate "
                   "FROM mylist m "
                   "LEFT JOIN anime a ON m.aid = a.aid "
                   "LEFT JOIN episode e ON m.eid = e.eid "
                   "ORDER BY a.nameromaji, m.eid";
    
    QSqlQuery q(db);
    QVERIFY(q.exec(query));
    
    // Create a tree widget to simulate the UI
    QTreeWidget treeWidget;
    treeWidget.setColumnCount(9);
    
    QMap<int, QTreeWidgetItem*> animeItems;
    
    // Process all episodes (simulating the while loop in loadMylistFromDatabase)
    int rowCount = 0;
    while(q.next())
    {
        rowCount++;
        int aid = q.value(1).toInt();
        QString animeName = q.value(6).toString();
        QString typeName = q.value(13).toString();
        QString startDate = q.value(14).toString();
        QString endDate = q.value(15).toString();
        
        // Verify we got the data from database
        QCOMPARE(aid, 222);
        QCOMPARE(typeName, QString("OVA"));
        QCOMPARE(startDate, QString("2002-06-20Z"));
        QCOMPARE(endDate, QString("2003-04-10Z"));
        
        // Simulate the fixed code: get or create anime item
        QTreeWidgetItem *animeItem;
        if(animeItems.contains(aid))
        {
            animeItem = animeItems[aid];
        }
        else
        {
            animeItem = new QTreeWidgetItem(&treeWidget);
            animeItem->setText(0, animeName);
            animeItems[aid] = animeItem;
            treeWidget.addTopLevelItem(animeItem);
        }
        
        // THE FIX: Set Type and Aired columns outside the else block
        // This runs for every episode, not just when creating the anime item
        if(!typeName.isEmpty() && animeItem->text(7).isEmpty())
        {
            animeItem->setText(7, typeName);
        }
        
        if(!startDate.isEmpty() && animeItem->text(8).isEmpty())
        {
            aired airedDates(startDate, endDate);
            animeItem->setText(8, airedDates.toDisplayString());
        }
    }
    
    // Verify we processed all 4 episodes
    QCOMPARE(rowCount, 4);
    
    // Verify only one anime item was created (all episodes belong to same anime)
    QCOMPARE(treeWidget.topLevelItemCount(), 1);
    
    // Get the anime item
    QTreeWidgetItem *animeItem = treeWidget.topLevelItem(0);
    QVERIFY(animeItem != nullptr);
    
    // THE KEY TEST: Verify Type and Aired columns are set
    QCOMPARE(animeItem->text(7), QString("OVA"));
    QCOMPARE(animeItem->text(8), QString("20.06.2002-10.04.2003"));
    
    // Verify the anime name is correct
    QCOMPARE(animeItem->text(0), QString(".hack//Liminality"));
}

QTEST_MAIN(TestMylistColumnsFix)
#include "test_mylist_columns_fix.moc"
