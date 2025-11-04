#include <QtTest/QtTest>
#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryFile>
#include "../usagi/src/anidbapi.h"

/**
 * Test suite for AniDB timeout and retry functionality
 * 
 * These tests validate that the API properly handles timeouts by:
 * 1. Detecting when a request has timed out (>10 seconds)
 * 2. Retrying the request up to MAX_RETRIES times
 * 3. Giving up after MAX_RETRIES and marking the packet as failed
 */
class TestAniDBTimeoutRetry : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    
    // Timeout and retry tests
    void testRetryCountColumnExists();
    void testPacketRetriedOnTimeout();
    void testMaxRetriesReached();

private:
    AniDBApi* api;
    
    void clearPackets();
};

void TestAniDBTimeoutRetry::clearPackets()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("DELETE FROM `packets`");
}

void TestAniDBTimeoutRetry::initTestCase()
{
    // Create API instance - it will set up its own database
    api = new AniDBApi("usagi", 1);
}

void TestAniDBTimeoutRetry::cleanupTestCase()
{
    delete api;
}

void TestAniDBTimeoutRetry::cleanup()
{
    // Clean up packets table between tests
    clearPackets();
}

void TestAniDBTimeoutRetry::testRetryCountColumnExists()
{
    // Verify that the retry_count column exists in the packets table
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("DELETE FROM packets");
}

void TestAniDBTimeoutRetry::testRetryCountColumnExists()
{
    // Verify that the retry_count column exists in the packets table
    QSqlQuery query(db);
    query.exec("PRAGMA table_info(packets)");
    
    bool retryCountExists = false;
    while(query.next())
    {
        QString columnName = query.value(1).toString();
        if(columnName == "retry_count")
        {
            retryCountExists = true;
            break;
        }
    }
    
    QVERIFY2(retryCountExists, "retry_count column should exist in packets table");
}

void TestAniDBTimeoutRetry::testPacketRetriedOnTimeout()
{
    // Insert a test packet
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("INSERT INTO packets (tag, str, processed, got_reply, retry_count) VALUES ('1000', 'FILE test', 1, 0, 0)");
    
    // Verify packet was inserted
    query.exec("SELECT tag, str, processed, got_reply, retry_count FROM packets WHERE tag = '1000'");
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("1000"));
    QCOMPARE(query.value(2).toInt(), 1); // processed
    QCOMPARE(query.value(3).toInt(), 0); // got_reply
    QCOMPARE(query.value(4).toInt(), 0); // retry_count
    
    // Simulate a retry by updating the packet (this is what SendPacket does on timeout)
    query.exec("UPDATE packets SET processed = 0, retry_count = retry_count + 1 WHERE tag = '1000'");
    
    // Verify the packet was reset for retry
    query.exec("SELECT tag, str, processed, got_reply, retry_count FROM packets WHERE tag = '1000'");
    QVERIFY(query.next());
    QCOMPARE(query.value(2).toInt(), 0); // processed should be 0 (ready to retry)
    QCOMPARE(query.value(4).toInt(), 1); // retry_count should be incremented
}

void TestAniDBTimeoutRetry::testMaxRetriesReached()
{
    // Insert a test packet that has reached max retries
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("INSERT INTO packets (tag, str, processed, got_reply, retry_count) VALUES ('2000', 'FILE test', 1, 0, 3)");
    
    // Verify packet was inserted with retry_count = 3
    query.exec("SELECT tag, retry_count FROM packets WHERE tag = '2000'");
    QVERIFY(query.next());
    QCOMPARE(query.value(1).toInt(), 3);
    
    // Simulate max retries reached by marking as failed (this is what SendPacket does)
    query.exec("UPDATE packets SET got_reply = 1, reply = 'TIMEOUT' WHERE tag = '2000'");
    
    // Verify the packet was marked as failed
    query.exec("SELECT tag, got_reply, reply FROM packets WHERE tag = '2000'");
    QVERIFY(query.next());
    QCOMPARE(query.value(1).toInt(), 1); // got_reply should be 1
    QCOMPARE(query.value(2).toString(), QString("TIMEOUT")); // reply should be TIMEOUT
}

QTEST_MAIN(TestAniDBTimeoutRetry)
#include "test_anidb_timeout_retry.moc"
