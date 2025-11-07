#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryFile>
#include "../usagi/src/playbackmanager.h"

class TestPlaybackManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testMediaPlayerPathSettings();
    void testGetDefaultPath();

private:
    QTemporaryFile *tempDbFile;
};

void TestPlaybackManager::initTestCase()
{
    // Create a temporary database for testing
    tempDbFile = new QTemporaryFile();
    QVERIFY(tempDbFile->open());
    tempDbFile->close();
    
    // Setup database connection
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(tempDbFile->fileName());
    QVERIFY(db.open());
    
    // Create settings table
    QSqlQuery q(db);
    q.exec("CREATE TABLE IF NOT EXISTS `settings`(`id` INTEGER PRIMARY KEY, `name` TEXT UNIQUE, `value` TEXT)");
}

void TestPlaybackManager::cleanupTestCase()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    delete tempDbFile;
}

void TestPlaybackManager::testMediaPlayerPathSettings()
{
    // Test setting and getting media player path
    QString testPath = "C:\\Test\\Path\\player.exe";
    
    PlaybackManager::setMediaPlayerPath(testPath);
    QString retrievedPath = PlaybackManager::getMediaPlayerPath();
    
    QCOMPARE(retrievedPath, testPath);
}

void TestPlaybackManager::testGetDefaultPath()
{
    // Clear any existing setting
    QSqlQuery q(QSqlDatabase::database());
    q.exec("DELETE FROM settings WHERE name = 'media_player_path'");
    
    // Test that default path is returned when no setting exists
    QString defaultPath = PlaybackManager::getMediaPlayerPath();
    QVERIFY(!defaultPath.isEmpty());
    QVERIFY(defaultPath.contains("mpc-hc64_nvo.exe"));
}

QTEST_MAIN(TestPlaybackManager)
#include "test_playbackmanager.moc"
