#include <QtTest/QtTest>
#include "../usagi/src/anidbfileinfo.h"

class TestAniDBFileInfo : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor();
    void testFromApiResponse();
    void testTypeConversions();
    void testHashValidation();
    void testFormatting();
    void testVersionExtraction();
    void testLegacyConversion();
};

void TestAniDBFileInfo::testDefaultConstructor()
{
    AniDBFileInfo info;
    QVERIFY(!info.isValid());
    QCOMPARE(info.fileId(), 0);
    QCOMPARE(info.animeId(), 0);
    QCOMPARE(info.size(), 0);
    QVERIFY(!info.hasHash());
}

void TestAniDBFileInfo::testFromApiResponse()
{
    // Simulate a minimal FILE response with fmask
    // fAID (0x40000000) | fEID (0x20000000) | fGID (0x10000000) | fSIZE (0x00800000) | fED2K (0x00400000)
    unsigned int fmask = 0x40000000 | 0x20000000 | 0x10000000 | 0x00800000 | 0x00400000;
    
    QStringList tokens;
    tokens << "123"   // aid
           << "456"   // eid
           << "789"   // gid
           << "367001600"  // size (350 MB)
           << "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";  // ed2k hash
    
    int index = 0;
    AniDBFileInfo info = AniDBFileInfo::fromApiResponse(tokens, fmask, index);
    
    QCOMPARE(info.animeId(), 123);
    QCOMPARE(info.episodeId(), 456);
    QCOMPARE(info.groupId(), 789);
    QCOMPARE(info.size(), 367001600LL);
    QCOMPARE(info.ed2kHash(), QString("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4"));
    QVERIFY(info.hasHash());
    QCOMPARE(index, 5);  // All 5 fields consumed
}

void TestAniDBFileInfo::testTypeConversions()
{
    // Test that bitrates are properly converted from string to int
    unsigned int fmask = 0x00001000 | 0x00000400;  // fBITRATE_AUDIO | fBITRATE_VIDEO
    QStringList tokens;
    tokens << "192"    // audio bitrate (kbps)
           << "2500";  // video bitrate (kbps)
    
    int index = 0;
    AniDBFileInfo info = AniDBFileInfo::fromApiResponse(tokens, fmask, index);
    
    QCOMPARE(info.audioBitrate(), 192);
    QCOMPARE(info.videoBitrate(), 2500);
}

void TestAniDBFileInfo::testHashValidation()
{
    AniDBFileInfo info;
    
    // Valid hash (32 hex chars)
    info.setEd2kHash("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4");
    QVERIFY(info.hasHash());
    QCOMPARE(info.ed2kHash(), QString("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4"));
    
    // Invalid hash (wrong length) - should be rejected silently
    info.setEd2kHash("invalidhash");
    QCOMPARE(info.ed2kHash(), QString("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4"));  // unchanged
    
    // Empty hash is valid (clears the hash)
    info.setEd2kHash("");
    QVERIFY(!info.hasHash());
}

void TestAniDBFileInfo::testFormatting()
{
    AniDBFileInfo info;
    
    // Test size formatting
    info.setSize(1024);
    QCOMPARE(info.formatSize(), QString("1.00 KB"));
    
    info.setSize(1024 * 1024);
    QCOMPARE(info.formatSize(), QString("1.00 MB"));
    
    info.setSize(367001600LL);  // ~350 MB
    QString sizeStr = info.formatSize();
    QVERIFY(sizeStr.contains("MB"));
    
    // Test duration formatting
    info.setLength(90);  // 1:30
    QCOMPARE(info.formatDuration(), QString("1:30"));
    
    info.setLength(3665);  // 1:01:05
    QCOMPARE(info.formatDuration(), QString("1:01:05"));
    
    info.setLength(0);
    QVERIFY(info.formatDuration().isEmpty());
}

void TestAniDBFileInfo::testVersionExtraction()
{
    AniDBFileInfo info;
    
    // Version 1 (no flags)
    info.setState(0x01);  // CRC OK only
    QCOMPARE(info.getVersion(), 1);
    
    // Version 2
    info.setState(0x04);  // ISV2 flag
    QCOMPARE(info.getVersion(), 2);
    
    // Version 3
    info.setState(0x08);  // ISV3 flag
    QCOMPARE(info.getVersion(), 3);
    
    // Version 4
    info.setState(0x10);  // ISV4 flag
    QCOMPARE(info.getVersion(), 4);
    
    // Version 5 (highest priority)
    info.setState(0x20 | 0x10 | 0x08);  // Multiple version flags
    QCOMPARE(info.getVersion(), 5);
}

void TestAniDBFileInfo::testLegacyConversion()
{
    AniDBFileInfo original;
    original.setFileId(12345);
    original.setAnimeId(678);
    original.setSize(1000000);
    original.setEd2kHash("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4");
    original.setResolution("1920x1080");
    original.setAudioCodec("AAC");
    
    // Convert to legacy struct
    auto legacy = original.toLegacyStruct();
    QCOMPARE(legacy.fid, QString("12345"));
    QCOMPARE(legacy.aid, QString("678"));
    QCOMPARE(legacy.size, QString("1000000"));
    QCOMPARE(legacy.ed2k, QString("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4"));
    QCOMPARE(legacy.resolution, QString("1920x1080"));
    QCOMPARE(legacy.codec_audio, QString("AAC"));
    
    // Convert back from legacy struct
    AniDBFileInfo restored = AniDBFileInfo::fromLegacyStruct(legacy);
    QCOMPARE(restored.fileId(), 12345);
    QCOMPARE(restored.animeId(), 678);
    QCOMPARE(restored.size(), 1000000LL);
    QCOMPARE(restored.ed2kHash(), QString("a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4"));
    QCOMPARE(restored.resolution(), QString("1920x1080"));
    QCOMPARE(restored.audioCodec(), QString("AAC"));
}

QTEST_MAIN(TestAniDBFileInfo)
#include "test_anidbfileinfo.moc"
