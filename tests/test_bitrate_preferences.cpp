#include <QTest>
#include "../usagi/src/anidbapi.h"
#include "../usagi/src/watchsessionmanager.h"
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

class TestBitratePreferences : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testDefaultValues();
    void testSetAndGetBitrate();
    void testSetAndGetResolution();
    void testCalculateExpectedBitrate();
    // calculateBitrateScore tests removed — method removed in Phase 5

private:
    AniDBApi* api;
    WatchSessionManager* watchManager;
    QTemporaryDir* tempDir;
};

void TestBitratePreferences::initTestCase()
{
    // Create temporary directory for test database
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());
    
    // Create test database with unique connection name
    QString dbPath = tempDir->filePath("test.db");
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_bitrate_connection");
    db.setDatabaseName(dbPath);
    QVERIFY(db.open());
    
    // Initialize API with test database (this creates the tables)
    api = new AniDBApi("testclient", 1);
    
    // Initialize WatchSessionManager
    watchManager = new WatchSessionManager();
}

void TestBitratePreferences::cleanupTestCase()
{
    delete watchManager;
    delete api;
    delete tempDir;
    QSqlDatabase::removeDatabase("test_bitrate_connection");
}

void TestBitratePreferences::testDefaultValues()
{
    // Test default bitrate is 3.5 Mbps
    QCOMPARE(api->getPreferredBitrate(), 3.5);
    
    // Test default resolution is 1080p
    QCOMPARE(api->getPreferredResolution(), QString("1080p"));
}

void TestBitratePreferences::testSetAndGetBitrate()
{
    // Test setting and getting bitrate
    api->setPreferredBitrate(5.0);
    QCOMPARE(api->getPreferredBitrate(), 5.0);
    
    // Reset to default
    api->setPreferredBitrate(3.5);
    QCOMPARE(api->getPreferredBitrate(), 3.5);
}

void TestBitratePreferences::testSetAndGetResolution()
{
    // Test setting and getting resolution
    api->setPreferredResolution("1440p");
    QCOMPARE(api->getPreferredResolution(), QString("1440p"));
    
    // Test with custom resolution
    api->setPreferredResolution("1920x1080");
    QCOMPARE(api->getPreferredResolution(), QString("1920x1080"));
    
    // Reset to default
    api->setPreferredResolution("1080p");
    QCOMPARE(api->getPreferredResolution(), QString("1080p"));
}

void TestBitratePreferences::testCalculateExpectedBitrate()
{
    // Set baseline bitrate to 3.5 Mbps for 1080p
    api->setPreferredBitrate(3.5);
    
    // Test 1080p (2.07 MP) - should return baseline
    double bitrate1080p = watchManager->calculateExpectedBitrate("1080p", "H.264");
    QVERIFY(qAbs(bitrate1080p - 3.5) < 0.01);
    
    // Test 720p (0.92 MP) - should be ~1.6 Mbps
    double bitrate720p = watchManager->calculateExpectedBitrate("720p", "H.264");
    QVERIFY(qAbs(bitrate720p - 1.56) < 0.1);
    
    // Test 1440p (3.69 MP) - should be ~6.2 Mbps
    double bitrate1440p = watchManager->calculateExpectedBitrate("1440p", "H.264");
    QVERIFY(qAbs(bitrate1440p - 6.24) < 0.1);
    
    // Test 4K (8.29 MP) - should be ~14 Mbps
    double bitrate4K = watchManager->calculateExpectedBitrate("4K", "H.264");
    QVERIFY(qAbs(bitrate4K - 14.0) < 0.5);
    
    // Test with WxH format
    double bitrate1920x1080 = watchManager->calculateExpectedBitrate("1920x1080", "H.264");
    QVERIFY(qAbs(bitrate1920x1080 - 3.5) < 0.01);
}

// calculateBitrateScore tests removed — method removed in Phase 5

QTEST_MAIN(TestBitratePreferences)
#include "test_bitrate_preferences.moc"
