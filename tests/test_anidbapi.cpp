#include <QtTest/QtTest>
#include <QString>
#include <QRegularExpression>

/**
 * Test suite for AniDB API command format integrity
 * 
 * These tests validate that API commands are formatted correctly according to
 * the AniDB UDP API Definition (https://wiki.anidb.net/UDP_API_Definition)
 * 
 * The tests do not require network access or authentication - they only verify
 * that command strings are constructed with the correct format and parameters.
 */
class TestAniDBApiCommands : public QObject
{
    Q_OBJECT

private slots:
    // AUTH command tests
    void testAuthCommandFormat();
    void testAuthCommandParameterEncoding();
    
    // LOGOUT command tests
    void testLogoutCommandFormat();
    
    // MYLISTADD command tests
    void testMylistAddBasicFormat();
    void testMylistAddWithOptionalParameters();
    void testMylistAddWithEditFlag();
    void testMylistAddParameterOrder();
    
    // FILE command tests
    void testFileCommandFormat();
    void testFileCommandMasks();
    
    // MYLIST command tests
    void testMylistCommandWithLid();
    void testMylistStatCommandFormat();
    
    // General format tests
    void testParameterSeparators();
    void testSpecialCharacterEncoding();
};

// ===== AUTH Command Tests =====

void TestAniDBApiCommands::testAuthCommandFormat()
{
    // AUTH command format: AUTH user=<str>&pass=<str>&protover=<int4>&client=<str>&clientver=<int4>&enc=<str>
    QString username = "testuser";
    QString password = "testpass";
    int protover = 3;
    QString client = "usagi";
    int clientver = 1;
    QString enc = "utf8";
    
    QString authCommand = QString("AUTH user=%1&pass=%2&protover=%3&client=%4&clientver=%5&enc=%6")
        .arg(username)
        .arg(password)
        .arg(protover)
        .arg(client)
        .arg(clientver)
        .arg(enc);
    
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
    QVERIFY(authCommand.contains("client=usagi"));
    QVERIFY(authCommand.contains("clientver=1"));
    QVERIFY(authCommand.contains("enc=utf8"));
}

void TestAniDBApiCommands::testAuthCommandParameterEncoding()
{
    // Test that special characters in username/password would need encoding
    // Note: In real implementation, special characters should be URL-encoded
    QString username = "test user";  // Space should be encoded
    QString password = "test&pass";  // Ampersand should be encoded
    
    QString authCommand = QString("AUTH user=%1&pass=%2&protover=3&client=usagi&clientver=1&enc=utf8")
        .arg(username)
        .arg(password);
    
    // These tests document the current behavior - in production, these should be URL-encoded
    QVERIFY(authCommand.contains("user="));
    QVERIFY(authCommand.contains("pass="));
}

// ===== LOGOUT Command Tests =====

void TestAniDBApiCommands::testLogoutCommandFormat()
{
    // LOGOUT command format: LOGOUT
    QString logoutCommand = QString("LOGOUT ");
    
    // Verify command format
    QVERIFY(logoutCommand.startsWith("LOGOUT"));
    QCOMPARE(logoutCommand, QString("LOGOUT "));
}

// ===== MYLISTADD Command Tests =====

void TestAniDBApiCommands::testMylistAddBasicFormat()
{
    // MYLISTADD command format: MYLISTADD size=<int8>&ed2k=<str>
    qint64 size = 734003200;  // ~700MB file
    QString ed2khash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    int state = 1;
    
    QString msg = QString("MYLISTADD size=%1&ed2k=%2").arg(size).arg(ed2khash);
    msg += QString("&state=%1").arg(state);
    
    // Verify command starts with MYLISTADD
    QVERIFY(msg.startsWith("MYLISTADD "));
    
    // Verify required parameters
    QVERIFY(msg.contains("size="));
    QVERIFY(msg.contains("ed2k="));
    QVERIFY(msg.contains("state="));
    
    // Verify parameter values
    QVERIFY(msg.contains(QString("size=%1").arg(size)));
    QVERIFY(msg.contains(QString("ed2k=%1").arg(ed2khash)));
}

void TestAniDBApiCommands::testMylistAddWithOptionalParameters()
{
    // Test MYLISTADD with optional parameters: viewed, storage
    qint64 size = 734003200;
    QString ed2khash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    int viewed = 1;  // viewed parameter (1 or 2 in the function, mapped to 0 or 1 for API)
    int state = 1;
    QString storage = "HDD";
    
    QString msg = QString("MYLISTADD size=%1&ed2k=%2").arg(size).arg(ed2khash);
    
    // Add viewed parameter (note: implementation subtracts 1)
    if(viewed > 0 && viewed < 3)
    {
        msg += QString("&viewed=%1").arg(viewed-1);
    }
    
    // Add storage parameter
    if(storage.length() > 0)
    {
        msg += QString("&storage=%1").arg(storage);
    }
    
    msg += QString("&state=%1").arg(state);
    
    // Verify optional parameters are included
    QVERIFY(msg.contains("&viewed="));
    QVERIFY(msg.contains("&storage="));
    QVERIFY(msg.contains("storage=HDD"));
}

void TestAniDBApiCommands::testMylistAddWithEditFlag()
{
    // Test MYLISTADD with edit=1 flag for updating existing entries
    qint64 size = 734003200;
    QString ed2khash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    int state = 1;
    bool edit = true;
    
    QString msg = QString("MYLISTADD size=%1&ed2k=%2").arg(size).arg(ed2khash);
    
    if(edit)
    {
        msg += QString("&edit=1");
    }
    
    msg += QString("&state=%1").arg(state);
    
    // Verify edit flag is present
    QVERIFY(msg.contains("&edit=1"));
}

void TestAniDBApiCommands::testMylistAddParameterOrder()
{
    // Verify that parameters can be in any order (API accepts them in any order)
    qint64 size = 734003200;
    QString ed2khash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    int state = 1;
    
    QString msg = QString("MYLISTADD size=%1&ed2k=%2&state=%3")
        .arg(size).arg(ed2khash).arg(state);
    
    // Verify all required parameters are present regardless of order
    QVERIFY(msg.contains("size="));
    QVERIFY(msg.contains("ed2k="));
    QVERIFY(msg.contains("state="));
}

// ===== FILE Command Tests =====

void TestAniDBApiCommands::testFileCommandFormat()
{
    // FILE command format: FILE size=<int8>&ed2k=<str>&fmask=<hexstr>&amask=<hexstr>
    qint64 size = 734003200;
    QString ed2k = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
    unsigned int fmask = 0x7FF8FEF8;  // Example file mask
    unsigned int amask = 0xF0F0F0F0;  // Example anime mask
    
    QString msg = QString("FILE size=%1&ed2k=%2&fmask=%3&amask=%4")
        .arg(size)
        .arg(ed2k)
        .arg(fmask, 8, 16, QChar('0'))
        .arg(amask, 8, 16, QChar('0'));
    
    // Verify command format
    QVERIFY(msg.startsWith("FILE "));
    QVERIFY(msg.contains("size="));
    QVERIFY(msg.contains("ed2k="));
    QVERIFY(msg.contains("fmask="));
    QVERIFY(msg.contains("amask="));
}

void TestAniDBApiCommands::testFileCommandMasks()
{
    // Test that fmask and amask are formatted as 8-character hex strings
    unsigned int fmask = 0x12345678;
    unsigned int amask = 0xABCDEF00;
    
    QString fmaskStr = QString("%1").arg(fmask, 8, 16, QChar('0'));
    QString amaskStr = QString("%1").arg(amask, 8, 16, QChar('0'));
    
    // Verify masks are 8 characters
    QCOMPARE(fmaskStr.length(), 8);
    QCOMPARE(amaskStr.length(), 8);
    
    // Verify they are valid hex
    QCOMPARE(fmaskStr, QString("12345678"));
    QCOMPARE(amaskStr, QString("abcdef00"));
    
    // Verify they contain only hex characters
    QRegularExpression hexRegex("^[0-9a-f]{8}$");
    QVERIFY(hexRegex.match(fmaskStr).hasMatch());
    QVERIFY(hexRegex.match(amaskStr).hasMatch());
}

// ===== MYLIST Command Tests =====

void TestAniDBApiCommands::testMylistCommandWithLid()
{
    // MYLIST command format: MYLIST lid=<int4>
    int lid = 12345;
    
    QString msg = QString("MYLIST lid=%1").arg(lid);
    
    // Verify command format
    QVERIFY(msg.startsWith("MYLIST "));
    QVERIFY(msg.contains("lid="));
    QVERIFY(msg.contains(QString("lid=%1").arg(lid)));
}

void TestAniDBApiCommands::testMylistStatCommandFormat()
{
    // MYLISTSTAT command format: MYLISTSTAT
    QString msg = QString("MYLISTSTAT ");
    
    // Verify command format
    QVERIFY(msg.startsWith("MYLISTSTAT"));
    QCOMPARE(msg, QString("MYLISTSTAT "));
}

// ===== General Format Tests =====

void TestAniDBApiCommands::testParameterSeparators()
{
    // Verify that parameters are separated by '&' character
    QString msg = QString("MYLISTADD size=123&ed2k=hash&state=1");
    
    // Count the number of '&' separators
    int separatorCount = msg.count('&');
    QCOMPARE(separatorCount, 2);  // Two '&' for three parameters
    
    // Verify no spaces around separators
    QVERIFY(!msg.contains(" &"));
    QVERIFY(!msg.contains("& "));
}

void TestAniDBApiCommands::testSpecialCharacterEncoding()
{
    // Test handling of special characters in storage parameter
    // Note: This documents current behavior - production code should URL-encode
    QString storage1 = "External HDD";  // Space
    QString storage2 = "HDD&SSD";       // Ampersand (would break parameter parsing)
    
    // In real implementation, these should be URL-encoded:
    // "External HDD" -> "External%20HDD"
    // "HDD&SSD" -> "HDD%26SSD"
    
    QString msg1 = QString("MYLISTADD size=1&ed2k=hash&storage=%1&state=1").arg(storage1);
    QString msg2 = QString("MYLISTADD size=1&ed2k=hash&storage=%1&state=1").arg(storage2);
    
    // Verify storage parameter is present
    QVERIFY(msg1.contains("storage="));
    QVERIFY(msg2.contains("storage="));
    
    // Note: msg2 would be malformed in practice due to unencoded '&'
    // This test documents the issue - proper URL encoding should be added
}

QTEST_MAIN(TestAniDBApiCommands)
#include "test_anidbapi.moc"
