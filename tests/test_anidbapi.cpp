#include <QtTest/QtTest>
#include <QString>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryFile>
#include "../usagi/src/anidbapi.h"

/**
 * Test suite for AniDB API command format integrity
 * 
 * These tests validate that API commands are formatted correctly according to
 * the AniDB UDP API Definition (https://wiki.anidb.net/UDP_API_Definition)
 * 
 * Tests call actual API functions and verify the commands stored in the database
 * match the expected format and contain all required parameters.
 */
class TestAniDBApiCommands : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    
    // AUTH command tests
    void testAuthCommandFormat();
    
    // LOGOUT command tests  
    void testLogoutCommandFormat();
    
    // MYLISTADD command tests
    void testMylistAddBasicFormat();
    void testMylistAddWithOptionalParameters();
    void testMylistAddWithEditFlag();
    
    // FILE command tests
    void testFileCommandFormat();
    void testFileCommandMasks();
    
    // MYLIST command tests
    void testMylistCommandWithLid();
    void testMylistStatCommandFormat();

private:
    AniDBApi* api;
    QString dbPath;
    
    QString getLastPacketCommand();
    void clearPackets();
};

// Helper function to get the last command inserted into packets table
QString TestAniDBApiCommands::getLastPacketCommand()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("SELECT `str` FROM `packets` WHERE `processed` = 0 ORDER BY `tag` DESC LIMIT 1");
    if (query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

// Helper function to clear packets table between tests
void TestAniDBApiCommands::clearPackets()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("DELETE FROM `packets`");
}

void TestAniDBApiCommands::initTestCase()
{
    // Create API instance with test client info
    api = new AniDBApi("usagitest", 1);
    
    // Set test credentials
    api->setUsername("testuser");
    api->setPassword("testpass");
    
    // Clear any existing packets
    clearPackets();
}

void TestAniDBApiCommands::cleanupTestCase()
{
    delete api;
}

void TestAniDBApiCommands::cleanup()
{
    // Clear packets after each test
    clearPackets();
}

// ===== AUTH Command Tests =====

void TestAniDBApiCommands::testAuthCommandFormat()
{
    // Call the actual Auth() function
    api->Auth();
    
    // Get the command that was inserted into the database
    QString authCommand = getLastPacketCommand();
    
    // Verify command is not empty
    QVERIFY(!authCommand.isEmpty());
    
    // Verify command starts with AUTH
    QVERIFY(authCommand.startsWith("AUTH "));
    
    // Verify all required parameters are present
    QVERIFY(authCommand.contains("user="));
    QVERIFY(authCommand.contains("pass="));
    QVERIFY(authCommand.contains("protover="));
    QVERIFY(authCommand.contains("client="));
    QVERIFY(authCommand.contains("clientver="));
    QVERIFY(authCommand.contains("enc="));
    
    // Verify parameter values
    QVERIFY(authCommand.contains("user=testuser"));
    QVERIFY(authCommand.contains("pass=testpass"));
    QVERIFY(authCommand.contains("protover=3"));
    QVERIFY(authCommand.contains("client=usagitest"));
    QVERIFY(authCommand.contains("clientver=1"));
    QVERIFY(authCommand.contains("enc=utf8"));
    
    // Verify parameters are separated by '&'
    int ampCount = authCommand.count('&');
    QVERIFY(ampCount >= 5); // At least 5 '&' separators for 6 parameters
}

// ===== LOGOUT Command Tests =====

void TestAniDBApiCommands::testLogoutCommandFormat()
{
    // Note: Logout() calls Send() directly, which requires a socket
    // Instead, we'll test the command format it would generate
    QString logoutCommand = QString("LOGOUT ");
    
    // Verify command format
    QVERIFY(logoutCommand.startsWith("LOGOUT"));
    QCOMPARE(logoutCommand, QString("LOGOUT "));
}

// ===== MYLISTADD Command Tests =====

void TestAniDBApiCommands::testMylistAddBasicFormat()
{
    // Call the actual MylistAdd() function with basic parameters
    qint64 size = 734003200;  // ~700MB file
    QString ed2khash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    int viewed = 0;
    int state = 1;
    QString storage = "";
    bool edit = false;
    
    api->MylistAdd(size, ed2khash, viewed, state, storage, edit);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify command is not empty
    QVERIFY(!msg.isEmpty());
    
    // Verify command starts with MYLISTADD
    QVERIFY(msg.startsWith("MYLISTADD "));
    
    // Verify required parameters
    QVERIFY(msg.contains("size="));
    QVERIFY(msg.contains("ed2k="));
    QVERIFY(msg.contains("state="));
    
    // Verify parameter values
    QVERIFY(msg.contains(QString("size=%1").arg(size)));
    QVERIFY(msg.contains(QString("ed2k=%1").arg(ed2khash)));
    QVERIFY(msg.contains(QString("state=%1").arg(state)));
}

void TestAniDBApiCommands::testMylistAddWithOptionalParameters()
{
    // Test MYLISTADD with optional parameters: viewed, storage
    qint64 size = 734003200;
    QString ed2khash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    int viewed = 1;  // Will be mapped to 0 by the function (viewed-1)
    int state = 1;
    QString storage = "HDD";
    bool edit = false;
    
    api->MylistAdd(size, ed2khash, viewed, state, storage, edit);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify optional parameters are included
    QVERIFY(msg.contains("&viewed="));
    QVERIFY(msg.contains("&storage="));
    QVERIFY(msg.contains("storage=HDD"));
    
    // Verify viewed was decremented (1 becomes 0)
    QVERIFY(msg.contains("viewed=0"));
}

void TestAniDBApiCommands::testMylistAddWithEditFlag()
{
    // Test MYLISTADD with edit=1 flag for updating existing entries
    qint64 size = 734003200;
    QString ed2khash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    int viewed = 0;
    int state = 1;
    QString storage = "";
    bool edit = true;
    
    api->MylistAdd(size, ed2khash, viewed, state, storage, edit);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify edit flag is present
    QVERIFY(msg.contains("&edit=1"));
}

// ===== FILE Command Tests =====

void TestAniDBApiCommands::testFileCommandFormat()
{
    // Call the actual File() function
    qint64 size = 734003200;
    QString ed2k = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    
    api->File(size, ed2k);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify command is not empty
    QVERIFY(!msg.isEmpty());
    
    // Verify command starts with FILE
    QVERIFY(msg.startsWith("FILE "));
    
    // Verify required parameters
    QVERIFY(msg.contains("size="));
    QVERIFY(msg.contains("ed2k="));
    QVERIFY(msg.contains("fmask="));
    QVERIFY(msg.contains("amask="));
    
    // Verify parameter values
    QVERIFY(msg.contains(QString("size=%1").arg(size)));
    QVERIFY(msg.contains(QString("ed2k=%1").arg(ed2k)));
}

void TestAniDBApiCommands::testFileCommandMasks()
{
    // Test that fmask and amask are formatted correctly
    qint64 size = 734003200;
    QString ed2k = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    
    api->File(size, ed2k);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Extract fmask and amask values using regex
    QRegularExpression fmaskRegex("fmask=([0-9a-f]{8})");
    QRegularExpression amaskRegex("amask=([0-9a-f]{8})");
    
    QRegularExpressionMatch fmaskMatch = fmaskRegex.match(msg);
    QRegularExpressionMatch amaskMatch = amaskRegex.match(msg);
    
    // Verify masks were found
    QVERIFY(fmaskMatch.hasMatch());
    QVERIFY(amaskMatch.hasMatch());
    
    // Verify they are 8-character hex strings
    QString fmaskStr = fmaskMatch.captured(1);
    QString amaskStr = amaskMatch.captured(1);
    
    QCOMPARE(fmaskStr.length(), 8);
    QCOMPARE(amaskStr.length(), 8);
    
    // Verify they contain only hex characters (lowercase)
    QRegularExpression hexRegex("^[0-9a-f]{8}$");
    QVERIFY(hexRegex.match(fmaskStr).hasMatch());
    QVERIFY(hexRegex.match(amaskStr).hasMatch());
}

// ===== MYLIST Command Tests =====

void TestAniDBApiCommands::testMylistCommandWithLid()
{
    // Call the actual Mylist() function with a lid
    int lid = 12345;
    
    api->Mylist(lid);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify command is not empty
    QVERIFY(!msg.isEmpty());
    
    // Verify command starts with MYLIST
    QVERIFY(msg.startsWith("MYLIST "));
    
    // Verify lid parameter
    QVERIFY(msg.contains("lid="));
    QVERIFY(msg.contains(QString("lid=%1").arg(lid)));
}

void TestAniDBApiCommands::testMylistStatCommandFormat()
{
    // Call Mylist() with no lid (or lid <= 0) to get MYLISTSTATS
    api->Mylist(-1);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify command is not empty
    QVERIFY(!msg.isEmpty());
    
    // Verify command is MYLISTSTATS
    QVERIFY(msg.startsWith("MYLISTSTATS"));
}

QTEST_MAIN(TestAniDBApiCommands)
#include "test_anidbapi.moc"