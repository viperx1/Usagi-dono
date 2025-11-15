#include <QtTest/QtTest>
#include <QString>
#include <QByteArray>
#include <zlib.h>
#include "../usagi/src/anidbapi.h"

/**
 * Test suite for AniDB compression support (comp=1 feature)
 * 
 * According to AniDB UDP API specification:
 * - comp=1 parameter in AUTH enables compression support
 * - Compressed datagrams always start with two zero bytes (0x00 0x00)
 * - Compression algorithm is DEFLATE (RFC 1951)
 * - Tags should never start with zero, so 0x00 0x00 is a reliable indicator
 */
class TestCompression : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Compression detection tests
    void testNonCompressedDataNotModified();
    void testCompressedDataDetectedAndDecompressed();
    void testEmptyDataHandled();
    void testTooSmallDataNotProcessed();
    void testInvalidCompressedDataHandled();
    
    // AUTH command tests
    void testAuthCommandIncludesComp1();

private:
    AniDBApi* api;
    
    // Helper to create DEFLATE-compressed data with 0x00 0x00 prefix
    QByteArray compressData(const QString& input);
};

void TestCompression::initTestCase()
{
    // Create API instance with test client info
    api = new AniDBApi("usagitest", 1);
    api->setUsername("testuser");
    api->setPassword("testpass");
}

void TestCompression::cleanupTestCase()
{
    delete api;
}

/**
 * Helper function to compress data using DEFLATE algorithm
 * with the AniDB comp=1 format (0x00 0x00 prefix + DEFLATE-compressed data)
 */
QByteArray TestCompression::compressData(const QString& input)
{
    QByteArray inputData = input.toUtf8();
    
    // Initialize zlib stream for DEFLATE compression
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = inputData.size();
    stream.next_in = (Bytef*)inputData.data();
    
    // Initialize deflator with raw DEFLATE format (no zlib or gzip headers)
    // windowBits = -15 means raw DEFLATE (negative value removes zlib wrapper)
    int ret = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if(ret != Z_OK)
    {
        qWarning() << "deflateInit2 failed with code" << ret;
        return QByteArray();
    }
    
    // Compress the data
    QByteArray compressed;
    compressed.reserve(inputData.size());
    
    const int CHUNK_SIZE = 4096;
    unsigned char outBuffer[CHUNK_SIZE];
    
    do
    {
        stream.avail_out = CHUNK_SIZE;
        stream.next_out = outBuffer;
        
        ret = deflate(&stream, Z_FINISH);
        
        if(ret == Z_STREAM_ERROR)
        {
            qWarning() << "deflate failed with Z_STREAM_ERROR";
            deflateEnd(&stream);
            return QByteArray();
        }
        
        int have = CHUNK_SIZE - stream.avail_out;
        compressed.append((char*)outBuffer, have);
        
    } while(ret != Z_STREAM_END);
    
    deflateEnd(&stream);
    
    // Prepend the two zero bytes as per AniDB spec
    QByteArray result;
    result.append((char)0x00);
    result.append((char)0x00);
    result.append(compressed);
    
    return result;
}

// ===== Compression Detection Tests =====

void TestCompression::testNonCompressedDataNotModified()
{
    // Test that normal (non-compressed) data is not modified
    // Normal AniDB responses start with a tag (non-zero number)
    QString normalResponse = "T123 200 LOGIN ACCEPTED\nsessionkey";
    QByteArray data = normalResponse.toUtf8();
    
    // Call decompressIfNeeded - should return data unchanged
    QByteArray result = api->decompressIfNeeded(data);
    
    // Verify data was not modified
    QCOMPARE(result, data);
    QCOMPARE(QString::fromUtf8(result), normalResponse);
}

void TestCompression::testCompressedDataDetectedAndDecompressed()
{
    // Test that compressed data is detected and decompressed correctly
    QString originalText = "T456 220 FILE\nfid|aid|eid|gid|state|size|ed2k|groupname";
    
    // Compress the data using DEFLATE with 0x00 0x00 prefix
    QByteArray compressed = compressData(originalText);
    
    // Verify compression worked
    QVERIFY(!compressed.isEmpty());
    QVERIFY(compressed.size() < originalText.size() + 10); // Sanity check
    
    // Verify it starts with 0x00 0x00
    QCOMPARE((unsigned char)compressed[0], (unsigned char)0x00);
    QCOMPARE((unsigned char)compressed[1], (unsigned char)0x00);
    
    // Call decompressIfNeeded - should detect and decompress
    QByteArray decompressed = api->decompressIfNeeded(compressed);
    
    // Verify decompression worked
    QString decompressedText = QString::fromUtf8(decompressed);
    QCOMPARE(decompressedText, originalText);
}

void TestCompression::testEmptyDataHandled()
{
    // Test that empty data is handled gracefully
    QByteArray emptyData;
    
    QByteArray result = api->decompressIfNeeded(emptyData);
    
    // Should return empty data unchanged
    QVERIFY(result.isEmpty());
}

void TestCompression::testTooSmallDataNotProcessed()
{
    // Test that data smaller than 2 bytes is not processed
    QByteArray tooSmall;
    tooSmall.append((char)0x00);
    
    QByteArray result = api->decompressIfNeeded(tooSmall);
    
    // Should return data unchanged
    QCOMPARE(result, tooSmall);
}

void TestCompression::testInvalidCompressedDataHandled()
{
    // Test that invalid compressed data is handled gracefully
    // Create data that starts with 0x00 0x00 but is not valid DEFLATE
    QByteArray invalidData;
    invalidData.append((char)0x00);
    invalidData.append((char)0x00);
    invalidData.append("not valid deflate data");
    
    QByteArray result = api->decompressIfNeeded(invalidData);
    
    // Should return original data on decompression error
    QCOMPARE(result, invalidData);
}

// ===== AUTH Command Tests =====

void TestCompression::testAuthCommandIncludesComp1()
{
    // Verify that buildAuthCommand includes comp=1 parameter
    QString authCmd = api->buildAuthCommand("testuser", "testpass", 3, "usagitest", 1, "utf8");
    
    // Verify command format
    QVERIFY(authCmd.startsWith("AUTH "));
    QVERIFY(authCmd.contains("user=testuser"));
    QVERIFY(authCmd.contains("pass=testpass"));
    QVERIFY(authCmd.contains("protover=3"));
    QVERIFY(authCmd.contains("client=usagitest"));
    QVERIFY(authCmd.contains("clientver=1"));
    QVERIFY(authCmd.contains("enc=utf8"));
    
    // Verify comp=1 parameter is present
    QVERIFY2(authCmd.contains("comp=1"), 
             QString("AUTH command missing comp=1 parameter: %1").arg(authCmd).toLatin1());
}

QTEST_MAIN(TestCompression)
#include "test_compression.moc"
