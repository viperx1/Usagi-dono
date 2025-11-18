#include <QTest>
#include <QString>

/**
 * Test suite for verifying export template in notification messages
 * 
 * Tests that the application verifies notification messages contain the
 * correct template name before downloading export files.
 */
class TestExportTemplateVerification : public QObject
{
    Q_OBJECT

private slots:
    void testMessageContainsCorrectTemplate();
    void testMessageContainsWrongTemplate();
    void testMessageContainsNoTemplate();
    void testMessageContainsMultipleFormats();

private:
    bool messageMatchesTemplate(const QString& message, const QString& expectedTemplate);
};

bool TestExportTemplateVerification::messageMatchesTemplate(const QString& message, const QString& expectedTemplate)
{
    // If no template is expected, accept any message with .tgz
    if(expectedTemplate.isEmpty())
    {
        return message.contains(".tgz", Qt::CaseInsensitive);
    }
    
    // Check if message contains both .tgz and the expected template name
    return message.contains(".tgz", Qt::CaseInsensitive) && 
           message.contains(expectedTemplate, Qt::CaseInsensitive);
}

void TestExportTemplateVerification::testMessageContainsCorrectTemplate()
{
    QString message = "Your mylist export (xml-plain-cs) is ready: https://anidb.net/export/12345-user-export.tgz";
    QString expectedTemplate = "xml-plain-cs";
    
    bool matches = messageMatchesTemplate(message, expectedTemplate);
    
    QVERIFY(matches);
}

void TestExportTemplateVerification::testMessageContainsWrongTemplate()
{
    QString message = "Your mylist export (csv-adborg) is ready: https://anidb.net/export/12345-user-export.tgz";
    QString expectedTemplate = "xml-plain-cs";
    
    bool matches = messageMatchesTemplate(message, expectedTemplate);
    
    QVERIFY(!matches);
}

void TestExportTemplateVerification::testMessageContainsNoTemplate()
{
    // Old-style notification without template information
    QString message = "Your mylist export is ready: https://anidb.net/export/12345-user-export.tgz";
    QString expectedTemplate = "xml-plain-cs";
    
    bool matches = messageMatchesTemplate(message, expectedTemplate);
    
    // Should not match if template is expected but not present
    QVERIFY(!matches);
}

void TestExportTemplateVerification::testMessageContainsMultipleFormats()
{
    // Message mentions multiple template formats
    QString message = "Your exports are ready: xml-plain-cs at https://anidb.net/export/12345-xml.tgz and csv-adborg at https://anidb.net/export/12345-csv.tgz";
    QString expectedTemplate = "xml-plain-cs";
    
    bool matches = messageMatchesTemplate(message, expectedTemplate);
    
    // Should match because it contains the expected template
    QVERIFY(matches);
}

QTEST_MAIN(TestExportTemplateVerification)
#include "test_export_template_verification.moc"
