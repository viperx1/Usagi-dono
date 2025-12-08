#include <QTest>
#include "../usagi/src/relationdata.h"

/**
 * Test to demonstrate the API usage as specified in the issue
 * 
 * This test validates the issue requirements work:
 * - anime1->setRelations(QString, QString)
 * - int aid = anime1->getPrequel()
 * - int aid = anime1->getSequel()
 * 
 * Note: CardCreationData is a private inner class of MyListCardManager,
 * but it delegates to RelationData which provides the same API.
 * This test demonstrates the API works correctly.
 */
class TestCardCreationDataAPI : public QObject
{
    Q_OBJECT

private slots:
    void testAPIUsageAsSpecifiedInIssue();
};

void TestCardCreationDataAPI::testAPIUsageAsSpecifiedInIssue()
{
    // Create a RelationData instance (CardCreationData uses this internally)
    RelationData anime1;
    
    // Test the API as specified in the issue: anime1->setRelations(QString, QString)
    anime1.setRelations("100'200'300", "1'2'11");
    
    // Test the API as specified in the issue: int aid = anime1->getPrequel()
    int prequelAid = anime1.getPrequel();
    QCOMPARE(prequelAid, 200);
    
    // Test the API as specified in the issue: int aid = anime1->getSequel()
    int sequelAid = anime1.getSequel();
    QCOMPARE(sequelAid, 100);
    
    // Verify the has methods work
    QVERIFY(anime1.hasPrequel());
    QVERIFY(anime1.hasSequel());
    
    // Test with no prequel
    RelationData anime2;
    anime2.setRelations("500", "1");
    
    QVERIFY(anime2.hasSequel());
    QVERIFY(!anime2.hasPrequel());
    QCOMPARE(anime2.getSequel(), 500);
    QCOMPARE(anime2.getPrequel(), 0);
    
    // Test with no sequel
    RelationData anime3;
    anime3.setRelations("600", "2");
    
    QVERIFY(anime3.hasPrequel());
    QVERIFY(!anime3.hasSequel());
    QCOMPARE(anime3.getPrequel(), 600);
    QCOMPARE(anime3.getSequel(), 0);
}

QTEST_MAIN(TestCardCreationDataAPI)
#include "test_cardcreationdata_api.moc"
