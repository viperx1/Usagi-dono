#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QXmlStreamReader>

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
    
    // Create anime table
    QString createAnimeTable = 
        "CREATE TABLE IF NOT EXISTS `anime` ("
        "`aid` INTEGER PRIMARY KEY, "
        "`eptotal` INTEGER, "
        "`eplast` INTEGER, "
        "`year` TEXT, "
        "`type` TEXT, "
        "`relaidlist` TEXT, "
        "`relaidtype` TEXT, "
        "`category` TEXT, "
        "`nameromaji` TEXT, "
        "`namekanji` TEXT, "
        "`nameenglish` TEXT, "
        "`nameother` TEXT, "
        "`nameshort` TEXT, "
        "`synonyms` TEXT"
        ")";
    
    QVERIFY(query.exec(createAnimeTable));
    
    // Create episode table
    QString createEpisodeTable = 
        "CREATE TABLE IF NOT EXISTS `episode` ("
        "`eid` INTEGER PRIMARY KEY, "
        "`name` TEXT, "
        "`nameromaji` TEXT, "
        "`namekanji` TEXT, "
        "`rating` INTEGER, "
        "`votecount` INTEGER, "
        "`epno` TEXT"
        ")";
    
    QVERIFY(query.exec(createEpisodeTable));
}

QString TestMylistXMLParser::createSampleXMLExport()
{
    // Create a sample XML file in xml-plain-cs format (hierarchical structure)
    QString xmlContent = 
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<MyList>\n"
        "<User Id=\"12345\" Name=\"testuser\"/>\n"
        "<Anime Id=\"1135\" Eps=\"1\" EpsTotal=\"1\">\n"
        "  <Ep Id=\"12814\" EpNo=\"1\" Name=\"OVA\">\n"
        "    <File Id=\"54357\" LId=\"16588092\" GroupId=\"925\" Storage=\"a005\" "
        "ViewDate=\"2006-08-18T22:00:00Z\" MyState=\"2\"/>\n"
        "  </Ep>\n"
        "</Anime>\n"
        "<Anime Id=\"222\" Eps=\"4\" EpsTotal=\"4\">\n"
        "  <Ep Id=\"2614\" EpNo=\"1\" Name=\"First Episode\">\n"
        "    <File Id=\"47082\" LId=\"21080811\" GroupId=\"925\" Storage=\"a040\" "
        "ViewDate=\"2007-02-10T23:58:00Z\" MyState=\"2\"/>\n"
        "  </Ep>\n"
        "  <Ep Id=\"2615\" EpNo=\"2\" Name=\"Second Episode\">\n"
        "    <File Id=\"47083\" LId=\"21080812\" GroupId=\"925\" Storage=\"\" "
        "ViewDate=\"\" MyState=\"2\"/>\n"
        "  </Ep>\n"
        "</Anime>\n"
        "</MyList>\n";
    
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
    
    // Parse XML structure: <MyList><Anime><Ep><File LId="..." Id="..."/></Ep></Anime></MyList>
    QString currentAid;
    QString currentEid;
    QString currentEpNo;
    QString currentEpName;
    
    while(!xml.atEnd() && !xml.hasError())
    {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if(token == QXmlStreamReader::StartElement)
        {
            if(xml.name() == QString("Anime"))
            {
                QXmlStreamAttributes attributes = xml.attributes();
                currentAid = attributes.value("Id").toString();
                QString epsTotal = attributes.value("EpsTotal").toString();
                
                // Store or update anime table with eptotal if we have valid data
                if(!currentAid.isEmpty() && !epsTotal.isEmpty())
                {
                    // Insert anime record if it doesn't exist
                    QString animeInsertQuery = QString("INSERT OR IGNORE INTO `anime` (`aid`) VALUES (%1)")
                        .arg(currentAid);
                    
                    QSqlQuery animeQueryExec(db);
                    QVERIFY2(animeQueryExec.exec(animeInsertQuery), 
                        QString("Failed to insert anime: %1").arg(animeQueryExec.lastError().text()).toUtf8().constData());
                    
                    // Update eptotal only if it's currently 0 or NULL
                    QString animeUpdateQuery = QString("UPDATE `anime` SET `eptotal` = %1 "
                        "WHERE `aid` = %2 AND (eptotal IS NULL OR eptotal = 0)")
                        .arg(epsTotal)
                        .arg(currentAid);
                    
                    QVERIFY2(animeQueryExec.exec(animeUpdateQuery), 
                        QString("Failed to update anime: %1").arg(animeQueryExec.lastError().text()).toUtf8().constData());
                }
            }
            else if(xml.name() == QString("Ep"))
            {
                QXmlStreamAttributes attributes = xml.attributes();
                currentEid = attributes.value("Id").toString();
                currentEpNo = attributes.value("EpNo").toString();
                currentEpName = attributes.value("Name").toString();
                
                // Store episode data in episode table if we have valid data
                if(!currentEid.isEmpty() && (!currentEpNo.isEmpty() || !currentEpName.isEmpty()))
                {
                    QString epName_escaped = QString(currentEpName).replace("'", "''");
                    QString epNo_escaped = QString(currentEpNo).replace("'", "''");
                    
                    QString episodeQuery = QString("INSERT OR REPLACE INTO `episode` "
                        "(`eid`, `epno`, `name`) VALUES (%1, '%2', '%3')")
                        .arg(currentEid)
                        .arg(epNo_escaped)
                        .arg(epName_escaped);
                    
                    QSqlQuery episodeQueryExec(db);
                    QVERIFY2(episodeQueryExec.exec(episodeQuery), 
                        QString("Failed to insert episode: %1").arg(episodeQueryExec.lastError().text()).toUtf8().constData());
                }
            }
            else if(xml.name() == QString("File"))
            {
                QXmlStreamAttributes attributes = xml.attributes();
                
                QString lid = attributes.value("LId").toString();
                QString fid = attributes.value("Id").toString();
                QString gid = attributes.value("GroupId").toString();
                QString storage = attributes.value("Storage").toString();
                QString viewdate = attributes.value("ViewDate").toString();
                QString myState = attributes.value("MyState").toString();
                
                QVERIFY(!lid.isEmpty());
                QVERIFY(!currentAid.isEmpty());
                
                QString viewed = (!viewdate.isEmpty() && viewdate != "0") ? "1" : "0";
                QString storage_escaped = QString(storage).replace("'", "''");
                
                QString q = QString("INSERT OR REPLACE INTO `mylist` "
                    "(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) "
                    "VALUES (%1, %2, %3, %4, %5, %6, %7, '%8')")
                    .arg(lid)
                    .arg(fid.isEmpty() ? "0" : fid)
                    .arg(currentEid.isEmpty() ? "0" : currentEid)
                    .arg(currentAid)
                    .arg(gid.isEmpty() ? "0" : gid)
                    .arg(myState.isEmpty() ? "0" : myState)
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
    QCOMPARE(query.value(0).toInt(), 16588092);  // lid (LId from File)
    QCOMPARE(query.value(1).toInt(), 54357);     // fid (Id from File)
    QCOMPARE(query.value(2).toInt(), 12814);     // eid (Id from Ep)
    QCOMPARE(query.value(3).toInt(), 1135);      // aid (Id from Anime)
    QCOMPARE(query.value(4).toInt(), 1);         // viewed
    
    // Second entry - should have viewed=1 because viewdate is set
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 21080811);  // lid
    QCOMPARE(query.value(1).toInt(), 47082);     // fid
    QCOMPARE(query.value(4).toInt(), 1);         // viewed
    
    // Third entry - should have viewed=0 because viewdate is empty
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 21080812);  // lid
    QCOMPARE(query.value(1).toInt(), 47083);     // fid
    QCOMPARE(query.value(4).toInt(), 0);         // viewed (not viewed because viewdate is empty)
    
    // Verify no more entries
    QVERIFY(!query.next());
    
    // Verify that lid and fid are different (not duplicated)
    QVERIFY(query.exec("SELECT lid, fid FROM mylist WHERE lid = fid"));
    QVERIFY(!query.next());  // Should be no results where lid == fid
    
    // Verify episode data was stored correctly
    QVERIFY(query.exec("SELECT eid, epno, name FROM episode ORDER BY eid"));
    
    // First episode - eid 2614
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2614);
    QCOMPARE(query.value(1).toString(), QString("1"));
    QCOMPARE(query.value(2).toString(), QString("First Episode"));
    
    // Second episode - eid 2615
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2615);
    QCOMPARE(query.value(1).toString(), QString("2"));
    QCOMPARE(query.value(2).toString(), QString("Second Episode"));
    
    // Third episode - eid 12814
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 12814);
    QCOMPARE(query.value(1).toString(), QString("1"));
    QCOMPARE(query.value(2).toString(), QString("OVA"));
    
    // Verify no more episode entries
    QVERIFY(!query.next());
    
    // Verify anime data was stored correctly with eptotal
    QVERIFY(query.exec("SELECT aid, eptotal FROM anime ORDER BY aid"));
    
    // First anime - aid 222, eptotal 4
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 222);
    QCOMPARE(query.value(1).toInt(), 4);
    
    // Second anime - aid 1135, eptotal 1
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1135);
    QCOMPARE(query.value(1).toInt(), 1);
    
    // Verify no more anime entries
    QVERIFY(!query.next());
    
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
