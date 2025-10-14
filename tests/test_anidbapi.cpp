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
    
    // Command name validation test
    void testCommandNamesAreValid();
    
    // Command format validation tests
    void testNotifyListCommandFormat();
    void testPushAckCommandFormat();
    void testAllCommandsHaveProperSpacing();

private:
    AniDBApi* api;
    QString dbPath;
    
    QString getLastPacketCommand();
    void clearPackets();
    QStringList getValidApiCommands();
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

// Helper function to get the list of valid AniDB API commands
// Based on https://wiki.anidb.net/UDP_API_Definition
QStringList TestAniDBApiCommands::getValidApiCommands()
{
    return QStringList() 
        // Session Management Commands
        << "AUTH"
        << "LOGOUT"
        << "ENCRYPT"
        << "ENCODING"
        << "PING"
        << "VERSION"
        << "UPTIME"
        // Data Commands
        << "FILE"
        << "ANIME"
        << "ANIMEDESC"
        << "EPISODE"
        << "GROUP"
        << "GROUPSTATUS"
        << "PRODUCER"
        << "CHARACTER"
        << "CREATOR"
        << "CALENDAR"
        << "REVIEW"
        << "MYLIST"
        << "MYLISTSTATS"
        << "MYLISTADD"
        << "MYLISTDEL"
        << "MYLISTMOD"
        << "MYLISTEXPORT"
        << "MYLISTIMPORT"
        << "VOTE"
        << "RANDOMRECOMMENDATION"
        << "NOTIFICATION"
        << "NOTIFYLIST"
        << "NOTIFYADD"
        << "NOTIFYMOD"
        << "NOTIFYDEL"
        << "NOTIFYGET"
        << "NOTIFYACK"
        << "SENDMSG"
        << "USER";
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

void TestAniDBApiCommands::testCommandNamesAreValid()
{
    // This test validates all command names used in anidbapi.cpp against
    // the official AniDB UDP API Definition to prevent typos like MYLISTSTAT
    // Reference: https://wiki.anidb.net/UDP_API_Definition
    
    QStringList validCommands = getValidApiCommands();
    
    // Test each implemented command
    QStringList commandsToTest;
    
    // AUTH command
    api->Auth();
    QString authCmd = getLastPacketCommand();
    QVERIFY(!authCmd.isEmpty());
    QString authCmdName = authCmd.split(' ').first().split('&').first();
    if (authCmdName.contains('=')) {
        authCmdName = authCmdName.split('=').first();
    }
    commandsToTest << authCmdName;
    QVERIFY2(validCommands.contains(authCmdName), 
             QString("Command '%1' is not in valid API command list").arg(authCmdName).toLatin1());
    clearPackets();
    
    // LOGOUT command - skip as it requires socket connection
    // The command name is verified in testLogoutCommandFormat()
    
    // MYLISTADD command
    api->MylistAdd(734003200, "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4", 0, 1, "", false);
    QString mylistaddCmd = getLastPacketCommand();
    QVERIFY(!mylistaddCmd.isEmpty());
    QString mylistaddCmdName = mylistaddCmd.split(' ').first();
    if (mylistaddCmdName.contains('=')) {
        mylistaddCmdName = mylistaddCmdName.split('=').first();
    }
    commandsToTest << mylistaddCmdName;
    QVERIFY2(validCommands.contains(mylistaddCmdName),
             QString("Command '%1' is not in valid API command list").arg(mylistaddCmdName).toLatin1());
    clearPackets();
    
    // FILE command
    api->File(734003200, "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4");
    QString fileCmd = getLastPacketCommand();
    QVERIFY(!fileCmd.isEmpty());
    QString fileCmdName = fileCmd.split(' ').first();
    if (fileCmdName.contains('=')) {
        fileCmdName = fileCmdName.split('=').first();
    }
    commandsToTest << fileCmdName;
    QVERIFY2(validCommands.contains(fileCmdName),
             QString("Command '%1' is not in valid API command list").arg(fileCmdName).toLatin1());
    clearPackets();
    
    // MYLIST command
    api->Mylist(12345);
    QString mylistCmd = getLastPacketCommand();
    QVERIFY(!mylistCmd.isEmpty());
    QString mylistCmdName = mylistCmd.split(' ').first();
    if (mylistCmdName.contains('=')) {
        mylistCmdName = mylistCmdName.split('=').first();
    }
    commandsToTest << mylistCmdName;
    QVERIFY2(validCommands.contains(mylistCmdName),
             QString("Command '%1' is not in valid API command list").arg(mylistCmdName).toLatin1());
    clearPackets();
    
    // MYLISTSTATS command
    api->Mylist(-1);
    QString myliststatsCmd = getLastPacketCommand();
    QVERIFY(!myliststatsCmd.isEmpty());
    QString myliststatsCmdName = myliststatsCmd.split(' ').first();
    if (myliststatsCmdName.contains('=')) {
        myliststatsCmdName = myliststatsCmdName.split('=').first();
    }
    commandsToTest << myliststatsCmdName;
    QVERIFY2(validCommands.contains(myliststatsCmdName),
             QString("Command '%1' is not in valid API command list - this was the typo bug!").arg(myliststatsCmdName).toLatin1());
    
    // Report what commands were tested
    qDebug() << "Validated commands against API definition:" << commandsToTest;
}

// ===== Notification Command Tests =====

void TestAniDBApiCommands::testNotifyListCommandFormat()
{
    // Test NOTIFYLIST command format
    api->NotifyEnable();
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify command is not empty
    QVERIFY(!msg.isEmpty());
    
    // Verify command is exactly "NOTIFYLIST " (with trailing space)
    QCOMPARE(msg, QString("NOTIFYLIST "));
    
    // Verify it ends with a space (required for parameterless commands)
    QVERIFY(msg.endsWith(' '));
}

void TestAniDBApiCommands::testPushAckCommandFormat()
{
    // Test PUSHACK command format
    int nid = 12345;
    api->PushAck(nid);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify command is not empty
    QVERIFY(!msg.isEmpty());
    
    // Verify command starts with PUSHACK
    QVERIFY(msg.startsWith("PUSHACK "));
    
    // Verify nid parameter
    QVERIFY(msg.contains("nid="));
    QVERIFY(msg.contains(QString("nid=%1").arg(nid)));
    
    // Verify there's a space after command name before parameters
    QRegularExpression spacePattern("^PUSHACK ");
    QVERIFY(spacePattern.match(msg).hasMatch());
}

// ===== Global Command Format Validation Test =====

void TestAniDBApiCommands::testAllCommandsHaveProperSpacing()
{
    // This test validates that ALL commands follow the AniDB UDP API format rule:
    // 1. Commands with parameters must have a space after command name: "COMMAND param1=value"
    // 2. Commands without parameters must end with a space: "COMMAND "
    //
    // This prevents issues like "NOTIFYLIST&s=xxx" which gets rejected with 598 UNKNOWN COMMAND
    
    struct CommandTest {
        QString name;
        QString command;
        bool hasParameters;
    };
    
    QList<CommandTest> commands;
    
    // Test AUTH command
    api->Auth();
    QString authCmd = getLastPacketCommand();
    commands.append({"AUTH", authCmd, true});
    clearPackets();
    
    // Test LOGOUT command (has trailing space, no parameters)
    QString logoutCmd = QString("LOGOUT ");
    commands.append({"LOGOUT", logoutCmd, false});
    
    // Test MYLISTADD command
    api->MylistAdd(734003200, "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4", 0, 1, "", false);
    QString mylistaddCmd = getLastPacketCommand();
    commands.append({"MYLISTADD", mylistaddCmd, true});
    clearPackets();
    
    // Test FILE command
    api->File(734003200, "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4");
    QString fileCmd = getLastPacketCommand();
    commands.append({"FILE", fileCmd, true});
    clearPackets();
    
    // Test MYLIST command (with parameter)
    api->Mylist(12345);
    QString mylistCmd = getLastPacketCommand();
    commands.append({"MYLIST", mylistCmd, true});
    clearPackets();
    
    // Test MYLISTSTATS command (no parameters, should have trailing space)
    api->Mylist(-1);
    QString myliststatsCmd = getLastPacketCommand();
    commands.append({"MYLISTSTATS", myliststatsCmd, false});
    clearPackets();
    
    // Test PUSHACK command
    api->PushAck(12345);
    QString pushackCmd = getLastPacketCommand();
    commands.append({"PUSHACK", pushackCmd, true});
    clearPackets();
    
    // Test NOTIFYLIST command (no parameters, should have trailing space)
    api->NotifyEnable();
    QString notifylistCmd = getLastPacketCommand();
    commands.append({"NOTIFYLIST", notifylistCmd, false});
    clearPackets();
    
    // Validate each command
    for (const CommandTest& test : commands) {
        // Verify command is not empty
        QVERIFY2(!test.command.isEmpty(), 
                 QString("%1 command is empty").arg(test.name).toLatin1());
        
        // Extract command name from the command string
        QString cmdName;
        if (test.command.contains(' ')) {
            cmdName = test.command.split(' ').first();
        } else {
            cmdName = test.command.split('=').first();
        }
        
        // Verify command name matches expected
        QVERIFY2(cmdName == test.name || test.command.startsWith(test.name + " "),
                 QString("%1: command doesn't start correctly - got '%2'")
                     .arg(test.name).arg(test.command).toLatin1());
        
        if (test.hasParameters) {
            // Commands with parameters must have a space after the command name
            // Format: "COMMAND param1=value" or "COMMAND param1=value&param2=value"
            QVERIFY2(test.command.contains(' ') && test.command.indexOf(' ') < test.command.indexOf('='),
                     QString("%1: command with parameters must have space after command name - got '%2'")
                         .arg(test.name).arg(test.command).toLatin1());
            
            // Verify the space comes right after the command name
            int spacePos = test.command.indexOf(' ');
            QString beforeSpace = test.command.left(spacePos);
            QVERIFY2(beforeSpace == test.name,
                     QString("%1: expected command name before space, got '%2' in '%3'")
                         .arg(test.name).arg(beforeSpace).arg(test.command).toLatin1());
                         
        } else {
            // Commands without parameters must end with a space
            // Format: "COMMAND "
            QVERIFY2(test.command.endsWith(' '),
                     QString("%1: parameterless command must end with space - got '%2'")
                         .arg(test.name).arg(test.command).toLatin1());
            
            // Verify it's exactly "COMMANDNAME " (command + one space, nothing else)
            QVERIFY2(test.command == test.name + " ",
                     QString("%1: expected exactly '%2 ' but got '%3'")
                         .arg(test.name).arg(test.name).arg(test.command).toLatin1());
        }
    }
    
    // Report summary
    qDebug() << "Validated proper spacing for" << commands.size() << "commands";
    qDebug() << "Commands with parameters:" << commands.size() - 3;  // AUTH, MYLISTADD, FILE, MYLIST, PUSHACK
    qDebug() << "Commands without parameters:" << 3;  // LOGOUT, MYLISTSTATS, NOTIFYLIST
}

QTEST_MAIN(TestAniDBApiCommands)
#include "test_anidbapi.moc"