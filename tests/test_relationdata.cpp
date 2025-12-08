#include <QTest>
#include "../usagi/src/relationdata.h"

/**
 * Test suite for RelationData class
 * 
 * This test validates that:
 * 1. Relations can be set and retrieved correctly
 * 2. Prequel and sequel can be identified
 * 3. Relation parsing handles various formats correctly
 * 4. API follows the requirements: setRelations, getPrequel, getSequel
 */
class TestRelationData : public QObject
{
    Q_OBJECT

private slots:
    void testSetRelations();
    void testGetPrequel();
    void testGetSequel();
    void testEmptyRelations();
    void testMultipleRelations();
    void testInvalidData();
    void testCaching();
    void testClear();
};

void TestRelationData::testSetRelations()
{
    RelationData data;
    
    // Test setting relations
    data.setRelations("123'456'789", "1'2'11");
    
    QVERIFY(data.hasRelations());
    QCOMPARE(data.getRelationAidList(), QString("123'456'789"));
    QCOMPARE(data.getRelationTypeList(), QString("1'2'11"));
}

void TestRelationData::testGetPrequel()
{
    RelationData data;
    
    // Prequel is type 2
    data.setRelations("123'456", "1'2");
    
    QVERIFY(data.hasPrequel());
    QCOMPARE(data.getPrequel(), 456);
}

void TestRelationData::testGetSequel()
{
    RelationData data;
    
    // Sequel is type 1
    data.setRelations("123'456", "1'2");
    
    QVERIFY(data.hasSequel());
    QCOMPARE(data.getSequel(), 123);
}

void TestRelationData::testEmptyRelations()
{
    RelationData data;
    
    // Test with no relations
    QVERIFY(!data.hasRelations());
    QVERIFY(!data.hasPrequel());
    QVERIFY(!data.hasSequel());
    QCOMPARE(data.getPrequel(), 0);
    QCOMPARE(data.getSequel(), 0);
}

void TestRelationData::testMultipleRelations()
{
    RelationData data;
    
    // Multiple relations including prequel, sequel, and other types
    data.setRelations("100'200'300'400", "1'2'11'32");
    
    QVERIFY(data.hasRelations());
    QVERIFY(data.hasPrequel());
    QVERIFY(data.hasSequel());
    
    // Should get the first sequel (type 1)
    QCOMPARE(data.getSequel(), 100);
    
    // Should get the first prequel (type 2)
    QCOMPARE(data.getPrequel(), 200);
    
    // Test getAllRelations
    auto allRelations = data.getAllRelations();
    QCOMPARE(allRelations.size(), 4);
    QCOMPARE(allRelations[100], RelationData::RelationType::Sequel);
    QCOMPARE(allRelations[200], RelationData::RelationType::Prequel);
    QCOMPARE(allRelations[300], RelationData::RelationType::SameSetting);
    QCOMPARE(allRelations[400], RelationData::RelationType::AlternativeVersion);
}

void TestRelationData::testInvalidData()
{
    RelationData data;
    
    // Test with mismatched list sizes
    data.setRelations("123'456", "1");
    
    // Should still work with the valid pair
    QVERIFY(data.hasSequel());
    QCOMPARE(data.getSequel(), 123);
    QVERIFY(!data.hasPrequel());
    
    // Test with empty strings
    data.setRelations("", "");
    QVERIFY(!data.hasRelations());
    
    // Test with only one empty
    data.setRelations("123", "");
    QVERIFY(!data.hasRelations());
    
    data.setRelations("", "1");
    QVERIFY(!data.hasRelations());
}

void TestRelationData::testCaching()
{
    RelationData data;
    
    // Set initial relations
    data.setRelations("100'200", "1'2");
    
    // Access data (should parse and cache)
    QCOMPARE(data.getSequel(), 100);
    QCOMPARE(data.getPrequel(), 200);
    
    // Change relations (should invalidate cache)
    data.setRelations("300'400", "2'1");
    
    // Verify new data is returned
    QCOMPARE(data.getSequel(), 400);
    QCOMPARE(data.getPrequel(), 300);
}

void TestRelationData::testClear()
{
    RelationData data;
    
    // Set some relations
    data.setRelations("100'200", "1'2");
    QVERIFY(data.hasRelations());
    
    // Clear relations
    data.clear();
    
    // Verify everything is cleared
    QVERIFY(!data.hasRelations());
    QVERIFY(!data.hasPrequel());
    QVERIFY(!data.hasSequel());
    QCOMPARE(data.getRelationAidList(), QString(""));
    QCOMPARE(data.getRelationTypeList(), QString(""));
}

QTEST_MAIN(TestRelationData)
#include "test_relationdata.moc"
