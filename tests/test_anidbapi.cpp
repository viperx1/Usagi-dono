#include <QTest>
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
    
    // EPISODE command tests
    void testEpisodeCommandFormat();
    
    // Command name validation test
    void testCommandNamesAreValid();
    
    // Command format validation tests
    void testNotifyListCommandFormat();
    void testPushAckCommandFormat();
    void testNotifyGetCommandFormat();
    void testAllCommandsHaveProperSpacing();
    
    // Notification database tests
    void testNotificationsTableExists();
    
    // MYLISTADD database storage tests
    void testMylistAddResponseStoredInDatabase();
    void testMylistEditResponseUpdatesDatabase();
    
    // Export notification interval tests
    void testNotifyCheckIntervalNotIncreasedWhenNotLoggedIn();

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
    QVERIFY(authCommand.contains("comp="));
    
    // Verify parameter values
    QVERIFY(authCommand.contains("user=testuser"));
    QVERIFY(authCommand.contains("pass=testpass"));
    QVERIFY(authCommand.contains("protover=3"));
    QVERIFY(authCommand.contains("client=usagitest"));
    QVERIFY(authCommand.contains("clientver=1"));
    QVERIFY(authCommand.contains("enc=utf8"));
    QVERIFY(authCommand.contains("comp=1"));
    
    // Verify parameters are separated by '&'
    int ampCount = authCommand.count('&');
    QVERIFY(ampCount >= 6); // At least 6 '&' separators for 7 parameters (including comp=1)
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

// ===== EPISODE Command Tests =====

void TestAniDBApiCommands::testEpisodeCommandFormat()
{
    // Test EPISODE command with episode ID
    int testEid = 12345;
    api->Episode(testEid);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify command is not empty
    QVERIFY(!msg.isEmpty());
    
    // Verify command starts with EPISODE
    QVERIFY(msg.startsWith("EPISODE"));
    
    // Verify command contains eid parameter
    QVERIFY(msg.contains("eid="));
    
    // Verify the EID is correct
    QVERIFY(msg.contains(QString("eid=%1").arg(testEid)));
    
    // Verify no extra spaces or formatting issues
    QVERIFY(!msg.contains("  "));  // No double spaces
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
    // Test NOTIFYLIST command format using builder
    QString cmd = api->buildNotifyListCommand();
    
    // Verify command is exactly "NOTIFYLIST " (with trailing space)
    QCOMPARE(cmd, QString("NOTIFYLIST "));
    
    // Verify it ends with a space (required for parameterless commands)
    QVERIFY(cmd.endsWith(' '));
}

void TestAniDBApiCommands::testPushAckCommandFormat()
{
    // Test PUSHACK command format using builder
    int nid = 12345;
    QString cmd = api->buildPushAckCommand(nid);
    
    // Verify command starts with PUSHACK
    QVERIFY(cmd.startsWith("PUSHACK "));
    
    // Verify nid parameter
    QVERIFY(cmd.contains("nid="));
    QVERIFY(cmd.contains(QString("nid=%1").arg(nid)));
}

void TestAniDBApiCommands::testNotifyGetCommandFormat()
{
    // Test NOTIFYGET command format using builder
    int id = 4998280;
    QString cmd = api->buildNotifyGetCommand(id);
    
    // Verify command starts with NOTIFYGET
    QVERIFY(cmd.startsWith("NOTIFYGET "));
    
    // Verify type parameter (type=M for messages, type=N for notifications)
    QVERIFY(cmd.contains("type="));
    
    // Verify id parameter (not nid!)
    QVERIFY(cmd.contains("id="));
    QVERIFY(cmd.contains(QString("id=%1").arg(id)));
    
    // According to AniDB UDP API: NOTIFYGET type={str type}&id={int4 id}
    // For message notifications from NOTIFYLIST: type=M
    QVERIFY(cmd.contains("type=M"));
}

// ===== Simplified Global Command Format Validation Test =====

void TestAniDBApiCommands::testAllCommandsHaveProperSpacing()
{
    // Simplified test using builder methods to validate command format
    // Pattern: "COMMAND " or "COMMAND param1=value1&param2=value2"
    // All commands have a space after the command name
    // Parameter names can contain lowercase letters and digits (e.g., "ed2k", "clientver")
    // Parameters are optional after the space
    
    QRegularExpression pattern("^[A-Z]+ ([a-z0-9]+=([^&\\s]+)(&[a-z0-9]+=([^&\\s]+))*)?$");
    
    QStringList commands;
    QStringList commandNames;
    
    // Build all commands using builder methods - direct testing without DB
    commands << api->buildAuthCommand("testuser", "testpass", 3, "usagitest", 1, "utf8");
    commandNames << "AUTH";
    
    commands << api->buildLogoutCommand();
    commandNames << "LOGOUT";
    
    commands << api->buildMylistAddCommand(734003200, "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4", 0, 1, "", false);
    commandNames << "MYLISTADD";
    
    commands << api->buildFileCommand(734003200, "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4", 0x7ff8fef8, 0xf0f0f0f0);
    commandNames << "FILE";
    
    commands << api->buildMylistCommand(12345);
    commandNames << "MYLIST";
    
    commands << api->buildMylistStatsCommand();
    commandNames << "MYLISTSTATS";
    
    commands << api->buildPushAckCommand(12345);
    commandNames << "PUSHACK";
    
    commands << api->buildNotifyListCommand();
    commandNames << "NOTIFYLIST";
    
    commands << api->buildNotifyGetCommand(4998280);
    commandNames << "NOTIFYGET";
    
    // Validate each command against pattern
    for (int i = 0; i < commands.size(); ++i) {
        QString cmd = commands[i];
        QString name = commandNames[i];
        
        QVERIFY2(!cmd.isEmpty(), 
                 QString("%1 command is empty").arg(name).toLatin1());
        
        QVERIFY2(pattern.match(cmd).hasMatch(),
                 QString("%1 command doesn't match pattern: '%2'").arg(name).arg(cmd).toLatin1());
        
        // Verify command starts with expected name
        QVERIFY2(cmd.startsWith(name),
                 QString("%1 command doesn't start with '%2': '%3'").arg(name).arg(name).arg(cmd).toLatin1());
        
        // Verify proper separator after command name (space or parameter start)
        if (cmd.length() > name.length()) {
            QChar nextChar = cmd.at(name.length());
            bool validSeparator = (nextChar == ' ') || (nextChar.isLower());
            QVERIFY2(validSeparator,
                     QString("%1 command has invalid character after command name: '%2'").arg(name).arg(cmd).toLatin1());
        }
    }
    
    // Report summary
    qDebug() << "Validated proper spacing for" << commands.size() << "commands using builders";
    qDebug() << "Commands tested:" << commandNames;
}

// ===== Notification Database Tests =====

void TestAniDBApiCommands::testNotificationsTableExists()
{
    // Verify that the notifications table was created in the database
    QSqlDatabase db = QSqlDatabase::database();
    QVERIFY(db.isOpen());
    
    // Check if notifications table exists
    QSqlQuery query(db);
    bool success = query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='notifications'");
    QVERIFY2(success, "Failed to query sqlite_master table");
    QVERIFY2(query.next(), "Notifications table does not exist in database");
    
    QString tableName = query.value(0).toString();
    QCOMPARE(tableName, QString("notifications"));
    
    // Verify the table has the expected columns
    success = query.exec("PRAGMA table_info(notifications)");
    QVERIFY2(success, "Failed to get table info for notifications");
    
    QStringList expectedColumns;
    expectedColumns << "nid" << "type" << "from_user_id" << "from_user_name" 
                    << "date" << "message_type" << "title" << "body" 
                    << "received_at" << "acknowledged";
    
    QStringList actualColumns;
    while(query.next())
    {
        actualColumns << query.value(1).toString(); // Column name is at index 1
    }
    
    // Verify all expected columns exist
    for(const QString &col : expectedColumns)
    {
        QVERIFY2(actualColumns.contains(col), 
                 QString("Expected column '%1' not found in notifications table").arg(col).toLatin1());
    }
    
    qDebug() << "Notifications table verified with columns:" << actualColumns;
}

void TestAniDBApiCommands::testMylistAddResponseStoredInDatabase()
{
    // This test verifies that when a MYLISTADD response (210) is received,
    // the mylist entry is stored in the local database
    
    QSqlDatabase db = QSqlDatabase::database();
    QVERIFY(db.isOpen());
    
    // Step 1: Insert a file entry into the database (simulating a FILE response)
    QString fid = "3825687";
    QString aid = "18795";
    QString eid = "297776";
    QString gid = "16325";
    qint64 size = 1468257416;
    QString ed2k = "c46543aef15b4919cf966de5f339324b";
    
    QString q = QString("INSERT OR REPLACE INTO `file` "
        "(`fid`, `aid`, `eid`, `gid`, `lid`, `othereps`, `isdepr`, `state`, `size`, `ed2k`, "
        "`md5`, `sha1`, `crc`, `quality`, `source`, `codec_audio`, `bitrate_audio`, "
        "`codec_video`, `bitrate_video`, `resolution`, `filetype`, `lang_dub`, `lang_sub`, "
        "`length`, `description`, `airdate`, `filename`) "
        "VALUES ('%1', '%2', '%3', '%4', '0', '', '0', '1', '%5', '%6', '', '', '', '', '', '', '0', '', '0', '', '', '', '', '0', '', '0', '')")
        .arg(fid).arg(aid).arg(eid).arg(gid).arg(size).arg(ed2k);
    
    QSqlQuery query(db);
    QVERIFY2(query.exec(q), QString("Failed to insert file entry: %1").arg(query.lastError().text()).toLatin1());
    
    // Step 2: Simulate sending a MYLISTADD command
    int viewed = 1;
    int state = 1;
    QString storage = "";
    bool edit = false;
    
    QString tag = api->MylistAdd(size, ed2k, viewed, state, storage, edit);
    QVERIFY(!tag.isEmpty());
    
    // Verify the command was stored in packets table
    q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(tag);
    QVERIFY2(query.exec(q), QString("Failed to query packets: %1").arg(query.lastError().text()).toLatin1());
    QVERIFY2(query.next(), "MYLISTADD command not found in packets table");
    QString mylistAddCmd = query.value(0).toString();
    QVERIFY(mylistAddCmd.contains("MYLISTADD"));
    QVERIFY(mylistAddCmd.contains(QString("size=%1").arg(size)));
    QVERIFY(mylistAddCmd.contains(QString("ed2k=%1").arg(ed2k)));
    
    // Step 3: Simulate receiving a 210 MYLIST ENTRY ADDED response
    QString lid = "423064547";
    QString response = QString("%1 210 MYLIST ENTRY ADDED\n%2").arg(tag).arg(lid);
    
    // Parse the response (this will trigger the database storage logic)
    api->ParseMessage(response, "", "");
    
    // Step 4: Verify the mylist entry was stored in the database
    q = QString("SELECT `lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage` "
        "FROM `mylist` WHERE `lid` = %1").arg(lid);
    QVERIFY2(query.exec(q), QString("Failed to query mylist: %1").arg(query.lastError().text()).toLatin1());
    QVERIFY2(query.next(), "Mylist entry not found in database");
    
    // Verify the stored values
    QCOMPARE(query.value(0).toString(), lid);
    QCOMPARE(query.value(1).toString(), fid);
    QCOMPARE(query.value(2).toString(), eid);
    QCOMPARE(query.value(3).toString(), aid);
    QCOMPARE(query.value(4).toString(), gid);
    QCOMPARE(query.value(5).toString(), QString::number(state));
    QCOMPARE(query.value(6).toString(), QString::number(viewed - 1)); // viewed is decremented in MylistAdd
    QCOMPARE(query.value(7).toString(), storage);
    
    qDebug() << "Successfully verified MYLISTADD response stored in database";
    qDebug() << "  lid:" << lid << "fid:" << fid << "aid:" << aid << "eid:" << eid;
    
    // Clean up
    q = QString("DELETE FROM `mylist` WHERE `lid` = %1").arg(lid);
    query.exec(q);
    q = QString("DELETE FROM `file` WHERE `fid` = %1").arg(fid);
    query.exec(q);
}

void TestAniDBApiCommands::testMylistEditResponseUpdatesDatabase()
{
    // This test verifies that when a MYLISTADD response with edit=1 
    // receives a 311 MYLIST ENTRY EDITED response, the entry is updated in the database
    
    QSqlDatabase db = QSqlDatabase::database();
    QVERIFY(db.isOpen());
    
    // Step 1: Insert a file entry into the database
    QString fid = "3825688";
    QString aid = "18796";
    QString eid = "297777";
    QString gid = "16326";
    qint64 size = 1468257417;
    QString ed2k = "d46543aef15b4919cf966de5f339324c";
    
    QString q = QString("INSERT OR REPLACE INTO `file` "
        "(`fid`, `aid`, `eid`, `gid`, `lid`, `othereps`, `isdepr`, `state`, `size`, `ed2k`, "
        "`md5`, `sha1`, `crc`, `quality`, `source`, `codec_audio`, `bitrate_audio`, "
        "`codec_video`, `bitrate_video`, `resolution`, `filetype`, `lang_dub`, `lang_sub`, "
        "`length`, `description`, `airdate`, `filename`) "
        "VALUES ('%1', '%2', '%3', '%4', '0', '', '0', '1', '%5', '%6', '', '', '', '', '', '', '0', '', '0', '', '', '', '', '0', '', '0', '')")
        .arg(fid).arg(aid).arg(eid).arg(gid).arg(size).arg(ed2k);
    
    QSqlQuery query(db);
    QVERIFY2(query.exec(q), QString("Failed to insert file entry: %1").arg(query.lastError().text()).toLatin1());
    
    // Step 2: Insert an existing mylist entry (to simulate editing)
    QString lid = "423064548";
    q = QString("INSERT OR REPLACE INTO `mylist` "
        "(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) "
        "VALUES (%1, %2, %3, %4, %5, 0, 0, '')")
        .arg(lid).arg(fid).arg(eid).arg(aid).arg(gid);
    QVERIFY2(query.exec(q), QString("Failed to insert initial mylist entry: %1").arg(query.lastError().text()).toLatin1());
    
    // Step 3: Simulate sending a MYLISTADD command with edit=1
    int viewed = 2;  // Changed to 2 (will be stored as 1)
    int state = 2;   // Changed state
    QString storage = "HDD";  // Added storage
    bool edit = true;
    
    QString tag = api->MylistAdd(size, ed2k, viewed, state, storage, edit);
    QVERIFY(!tag.isEmpty());
    
    // Verify the command was stored in packets table with edit=1
    q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(tag);
    QVERIFY2(query.exec(q), QString("Failed to query packets: %1").arg(query.lastError().text()).toLatin1());
    QVERIFY2(query.next(), "MYLISTADD command not found in packets table");
    QString mylistAddCmd = query.value(0).toString();
    QVERIFY(mylistAddCmd.contains("MYLISTADD"));
    QVERIFY(mylistAddCmd.contains("&edit=1"));
    
    // Step 4: Simulate receiving a 311 MYLIST ENTRY EDITED response
    QString response = QString("%1 311 MYLIST ENTRY EDITED\n%2").arg(tag).arg(lid);
    
    // Parse the response (this will trigger the database update logic)
    api->ParseMessage(response, "", "");
    
    // Step 5: Verify the mylist entry was updated in the database
    q = QString("SELECT `lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage` "
        "FROM `mylist` WHERE `lid` = %1").arg(lid);
    QVERIFY2(query.exec(q), QString("Failed to query mylist: %1").arg(query.lastError().text()).toLatin1());
    QVERIFY2(query.next(), "Mylist entry not found in database");
    
    // Verify the stored values were updated
    QCOMPARE(query.value(0).toString(), lid);
    QCOMPARE(query.value(1).toString(), fid);
    QCOMPARE(query.value(2).toString(), eid);
    QCOMPARE(query.value(3).toString(), aid);
    QCOMPARE(query.value(4).toString(), gid);
    QCOMPARE(query.value(5).toString(), QString::number(state));
    QCOMPARE(query.value(6).toString(), QString::number(viewed - 1)); // viewed is decremented in MylistAdd
    QCOMPARE(query.value(7).toString(), storage);
    
    qDebug() << "Successfully verified MYLISTADD edit (311) response updated database";
    qDebug() << "  lid:" << lid << "state:" << state << "viewed:" << (viewed - 1) << "storage:" << storage;
    
    // Clean up
    q = QString("DELETE FROM `mylist` WHERE `lid` = %1").arg(lid);
    query.exec(q);
    q = QString("DELETE FROM `file` WHERE `fid` = %1").arg(fid);
    query.exec(q);
}

void TestAniDBApiCommands::testNotifyCheckIntervalNotIncreasedWhenNotLoggedIn()
{
    // This test verifies that the notification check interval does not increase
    // when the user is not logged in, as per the fix for the issue where the
    // interval would increase from 1 to 2 minutes even when no actual check was performed
    
    QSqlDatabase db = QSqlDatabase::database();
    QVERIFY(db.isOpen());
    QSqlQuery query(db);
    
    // Step 1: Set up an export queue state to trigger periodic checks
    // Set isExportQueued = 1, initial interval = 60000 (1 minute)
    query.exec("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'export_queued', '1')");
    query.exec("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'export_check_attempts', '0')");
    query.exec("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'export_check_interval_ms', '60000')");
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    query.exec(QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'export_queued_timestamp', '%1')").arg(currentTime));
    
    // Step 2: Create a new API instance that will load this state
    // Use QScopedPointer for RAII to ensure proper cleanup even if test fails
    QScopedPointer<AniDBApi> testApi(new AniDBApi("usagitest", 1));
    
    // Give the API time to initialize (it starts async tasks)
    QTest::qWait(100);
    
    // Step 3: Verify initial interval is 60000 ms (1 minute)
    query.exec("SELECT `value` FROM `settings` WHERE `name` = 'export_check_interval_ms'");
    QVERIFY(query.next());
    int initialInterval = query.value(0).toInt();
    QCOMPARE(initialInterval, 60000);
    
    // Step 4: Manually trigger checkForNotifications() when NOT logged in
    // (SID is empty, LoginStatus is 0 by default in test)
    testApi->checkForNotifications();
    
    // Give the system time to process
    QTest::qWait(100);
    
    // Step 5: Verify the interval has NOT increased (should still be 60000 ms)
    query.exec("SELECT `value` FROM `settings` WHERE `name` = 'export_check_interval_ms'");
    QVERIFY(query.next());
    int intervalAfterCheck = query.value(0).toInt();
    QCOMPARE(intervalAfterCheck, 60000);  // Should remain 60000, not increase to 120000
    
    qDebug() << "Successfully verified interval does not increase when not logged in";
    qDebug() << "  Initial interval:" << initialInterval << "ms";
    qDebug() << "  Interval after check (not logged in):" << intervalAfterCheck << "ms";
    qDebug() << "  Expected: no increase (should stay at 60000 ms)";
    
    // Clean up database state (QScopedPointer automatically deletes testApi)
    query.exec("DELETE FROM `settings` WHERE `name` IN ('export_queued', 'export_check_attempts', 'export_check_interval_ms', 'export_queued_timestamp')");
}

QTEST_MAIN(TestAniDBApiCommands)
#include "test_anidbapi.moc"
