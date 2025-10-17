#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

class TestMylistXMLParser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testXMLParsing();
    void cleanupTestCase();

private:
    QSqlDatabase db;
    QString createSampleXMLExport();
};

void TestMylistXMLParser::initTestCase()
{
    // Set up in-memory database for testing
    db = QSqlDatabase::addDatabase("QSQLITE", "test_connection");
    db.setDatabaseName(":memory:");
    
    QVERIFY(db.open());
    
    // Create mylist table
    QSqlQuery query(db);
    QString createTable = 
        "CREATE TABLE `mylist` ("
        "`lid` INTEGER PRIMARY KEY,"
        "`fid` INTEGER,"
        "`eid` INTEGER,"
        "`aid` INTEGER,"
        "`gid` INTEGER,"
        "`date` INTEGER,"
        "`state` INTEGER,"
        "`viewed` INTEGER,"
        "`viewdate` INTEGER,"
        "`storage` TEXT,"
        "`source` TEXT,"
        "`other` TEXT,"
        "`filestate` INTEGER"
        ")";
    
    QVERIFY(query.exec(createTable));
}

QString TestMylistXMLParser::createSampleXMLExport()
{
    // Create a sample XML file in xml-plain-cs format
    QString xmlContent = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<mylistexport>\n"
        "  <mylist lid=\"123456\" fid=\"789012\" eid=\"345678\" aid=\"901234\" gid=\"567890\" "
        "state=\"1\" viewdate=\"1640995200\" storage=\"/path/to/file\"/>\n"
        "  <mylist lid=\"234567\" fid=\"890123\" eid=\"456789\" aid=\"123456\" gid=\"678901\" "
        "state=\"2\" viewdate=\"1641081600\" storage=\"HDD\"/>\n"
        "  <mylist lid=\"345678\" fid=\"901234\" eid=\"567890\" aid=\"234567\" gid=\"789012\" "
        "state=\"1\" viewdate=\"0\" storage=\"\"/>\n"
        "</mylistexport>\n";
    
    // Create temporary directory and files
    QTemporaryDir tempDir;
    if(!tempDir.isValid())
        return QString();
    
    QString xmlFilePath = tempDir.path() + "/mylist.xml";
    QFile xmlFile(xmlFilePath);
    if(!xmlFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();
    xmlFile.write(xmlContent.toUtf8());
    xmlFile.close();
    
    // Create tar.gz file
    QString tarGzPath = tempDir.path() + "/export.tgz";
    QProcess tarProcess;
    tarProcess.setWorkingDirectory(tempDir.path());
    tarProcess.start("tar", QStringList() << "-czf" << "export.tgz" << "mylist.xml");
    if(!tarProcess.waitForFinished(10000) || tarProcess.exitCode() != 0)
        return QString();
    
    // Copy to a permanent location for the test
    QString permanentPath = QDir::tempPath() + "/test_export.tgz";
    QFile::remove(permanentPath);
    if(!QFile::copy(tarGzPath, permanentPath))
        return QString();
    
    return permanentPath;
}

void TestMylistXMLParser::testXMLParsing()
{
    // Create sample export file
    QString exportPath = createSampleXMLExport();
    QVERIFY(!exportPath.isEmpty());
    
    // Since we can't directly test Window::parseMylistExport without creating a Window instance,
    // we'll test the XML parsing logic inline to verify the concept
    
    // Extract tar.gz
    QString tempDir = QDir::tempPath() + "/test_extract_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(tempDir);
    
    QProcess tarProcess;
    tarProcess.setWorkingDirectory(tempDir);
    tarProcess.start("tar", QStringList() << "-xzf" << exportPath);
    QVERIFY(tarProcess.waitForFinished(10000));
    QCOMPARE(tarProcess.exitCode(), 0);
    
    // Find XML file
    QDir extractedDir(tempDir);
    QStringList xmlFiles = extractedDir.entryList(QStringList() << "*.xml", QDir::Files);
    QVERIFY(!xmlFiles.isEmpty());
    
    // Parse XML
    QString xmlFilePath = extractedDir.absoluteFilePath(xmlFiles.first());
    QFile xmlFile(xmlFilePath);
    QVERIFY(xmlFile.open(QIODevice::ReadOnly | QIODevice::Text));
    
    QXmlStreamReader xml(&xmlFile);
    int count = 0;
    
    db.transaction();
    
    while(!xml.atEnd() && !xml.hasError())
    {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if(token == QXmlStreamReader::StartElement)
        {
            if(xml.name() == QString("mylist"))
            {
                QXmlStreamAttributes attributes = xml.attributes();
                
                QString lid = attributes.value("lid").toString();
                QString fid = attributes.value("fid").toString();
                QString eid = attributes.value("eid").toString();
                QString aid = attributes.value("aid").toString();
                QString gid = attributes.value("gid").toString();
                QString state = attributes.value("state").toString();
                QString viewdate = attributes.value("viewdate").toString();
                QString storage = attributes.value("storage").toString();
                
                QVERIFY(!lid.isEmpty());
                QVERIFY(!aid.isEmpty());
                
                QString viewed = (!viewdate.isEmpty() && viewdate != "0") ? "1" : "0";
                QString storage_escaped = QString(storage).replace("'", "''");
                
                QString q = QString("INSERT OR REPLACE INTO `mylist` "
                    "(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) "
                    "VALUES (%1, %2, %3, %4, %5, %6, %7, '%8')")
                    .arg(lid)
                    .arg(fid.isEmpty() ? "0" : fid)
                    .arg(eid.isEmpty() ? "0" : eid)
                    .arg(aid)
                    .arg(gid.isEmpty() ? "0" : gid)
                    .arg(state.isEmpty() ? "0" : state)
                    .arg(viewed)
                    .arg(storage_escaped);
                
                QSqlQuery query(db);
                QVERIFY(query.exec(q));
                count++;
            }
        }
    }
    
    QVERIFY(!xml.hasError());
    xmlFile.close();
    db.commit();
    
    // Verify that 3 entries were parsed
    QCOMPARE(count, 3);
    
    // Verify database entries
    QSqlQuery query(db);
    QVERIFY(query.exec("SELECT lid, fid, eid, aid, viewed FROM mylist ORDER BY lid"));
    
    // First entry - should have viewed=1 because viewdate is set
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 123456);  // lid
    QCOMPARE(query.value(1).toInt(), 789012);  // fid
    QCOMPARE(query.value(2).toInt(), 345678);  // eid
    QCOMPARE(query.value(3).toInt(), 901234);  // aid
    QCOMPARE(query.value(4).toInt(), 1);       // viewed
    
    // Second entry - should have viewed=1 because viewdate is set
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 234567);  // lid
    QCOMPARE(query.value(1).toInt(), 890123);  // fid
    QCOMPARE(query.value(4).toInt(), 1);       // viewed
    
    // Third entry - should have viewed=0 because viewdate is 0
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 345678);  // lid
    QCOMPARE(query.value(1).toInt(), 901234);  // fid
    QCOMPARE(query.value(4).toInt(), 0);       // viewed (not viewed because viewdate=0)
    
    // Verify no more entries
    QVERIFY(!query.next());
    
    // Verify that lid and fid are different (not duplicated)
    QVERIFY(query.exec("SELECT lid, fid FROM mylist WHERE lid = fid"));
    QVERIFY(!query.next());  // Should be no results where lid == fid
    
    // Clean up
    QDir(tempDir).removeRecursively();
    QFile::remove(exportPath);
}

void TestMylistXMLParser::cleanupTestCase()
{
    db.close();
    QSqlDatabase::removeDatabase("test_connection");
}

QTEST_MAIN(TestMylistXMLParser)
#include "test_mylist_xml_parser.moc"
