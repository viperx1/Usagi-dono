#include <QtTest/QtTest>
#include <QString>
#include <QRegularExpression>

/**
 * Test suite for URL extraction from AniDB notification messages
 * 
 * Tests that URLs can be extracted from both plain text and BBCode-formatted
 * notification messages, as AniDB uses BBCode format in their notifications.
 */
class TestUrlExtraction : public QObject
{
    Q_OBJECT

private slots:
    void testPlainUrlExtraction();
    void testBBCodeUrlExtraction();
    void testNoUrlInMessage();
    void testMultipleBBCodeUrls();
    void testMixedContent();

private:
    QString extractExportUrl(const QString& message);
};

QString TestUrlExtraction::extractExportUrl(const QString& message)
{
    QString exportUrl;
    
    // First try to match BBCode format: [url=...]...[/url]
    QRegularExpression bbcodeRegex("\\[url=(https?://[^\\]]+\\.tgz)\\]");
    QRegularExpressionMatch bbcodeMatch = bbcodeRegex.match(message);
    
    if(bbcodeMatch.hasMatch())
    {
        exportUrl = bbcodeMatch.captured(1);  // Capture group 1 is the URL inside [url=...]
    }
    else
    {
        // Fallback to plain URL format
        QRegularExpression plainRegex("https?://[^\\s]+\\.tgz");
        QRegularExpressionMatch plainMatch = plainRegex.match(message);
        if(plainMatch.hasMatch())
        {
            exportUrl = plainMatch.captured(0);
        }
    }
    
    return exportUrl;
}

void TestUrlExtraction::testPlainUrlExtraction()
{
    QString message = "Your mylist export is ready: https://anidb.net/export/12345-user-export.tgz";
    QString url = extractExportUrl(message);
    
    QCOMPARE(url, QString("https://anidb.net/export/12345-user-export.tgz"));
}

void TestUrlExtraction::testBBCodeUrlExtraction()
{
    QString message = "Your export is ready: [url=https://anidb.net/export/12345-export.tgz]Download here[/url]";
    QString url = extractExportUrl(message);
    
    QCOMPARE(url, QString("https://anidb.net/export/12345-export.tgz"));
}

void TestUrlExtraction::testNoUrlInMessage()
{
    QString message = "A new anime relation has been added, linking an anime in your Mylist";
    QString url = extractExportUrl(message);
    
    QVERIFY(url.isEmpty());
}

void TestUrlExtraction::testMultipleBBCodeUrls()
{
    // When multiple URLs exist, should match the first .tgz URL
    QString message = "Anime 1: [url=https://anidb.net/a2996]Ranma[/url] Export: [url=https://anidb.net/export/12345.tgz]Download[/url]";
    QString url = extractExportUrl(message);
    
    QCOMPARE(url, QString("https://anidb.net/export/12345.tgz"));
}

void TestUrlExtraction::testMixedContent()
{
    // Real notification from issue
    QString message = "A new anime relation has been added, linking an anime in your Mylist and/or Wishlist to an anime that is not in your Mylist and/or Wishlist\n\n"
                     "Anime 1: [url=https://anidb.net/a2996]Ranma 1/2 Super[/url]\n"
                     "Anime 2: [url=https://anidb.net/a6141]Ranma 1/2: Akumu! Shunmin Kou[/url]\n\n"
                     "Relation Type: prequel\n\n"
                     "You can disable these notifications in your profile settings.";
    QString url = extractExportUrl(message);
    
    // This message does not contain any .tgz export link
    QVERIFY(url.isEmpty());
}

QTEST_MAIN(TestUrlExtraction)
#include "test_url_extraction.moc"
