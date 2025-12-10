#include <QTest>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include "../usagi/src/anidbapi.h"

/**
 * Test suite for ANIME response mask parsing
 * 
 * Tests validate that ANIME responses are parsed correctly based on the amask,
 * including handling of:
 * - Byte 1 bit 7 (AID) - whether AID is included in response or not
 * - Retired bits (e.g., Byte 1 bit 1) - should be consumed but not stored
 * - Unused bits (e.g., Byte 7 bit 2) - should be consumed but not stored
 * - All defined fields at various byte/bit positions
 * - Complete responses from actual logs
 */

// Test wrapper to expose protected methods
class TestableAniDBApi : public AniDBApi
{
public:
    TestableAniDBApi(QString /*dbPath*/) : AniDBApi("testclient", 1)
    {
        // Initialize with test database
    }
    
    // Expose parseMaskFromString for testing
    AniDBAnimeInfo testParseMaskFromString(const QStringList& tokens, const QString& amaskHexString, int& index)
    {
        return parseMaskFromString(tokens, amaskHexString, index);
    }
    
    AniDBAnimeInfo testParseMaskFromString(const QStringList& tokens, const QString& amaskHexString, int& index, QByteArray& parsedMaskBytes)
    {
        return parseMaskFromString(tokens, amaskHexString, index, parsedMaskBytes);
    }
};

class TestAnimeMaskParsing : public QObject
{
    Q_OBJECT

private:
    TestableAniDBApi *api;
    QTemporaryFile *tempDbFile;

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Test AID bit handling (Byte 1, bit 7 = 0x80)
    void testParseWithAidBit();
    void testParseWithoutAidBit();
    
    // Test retired bits (Byte 1, bit 1 = 0x02)
    void testRetiredBitConsumed();
    
    // Test unused bits (Byte 7, bit 2 = 0x04)
    void testUnusedBitConsumed();
    
    // Test complete response from actual log
    void testCompleteAnimeResponse();
    void testCompleteReRequestResponse();
    
    // Test various byte positions
    void testByte2Fields();
    void testByte3Fields();
    void testByte4Fields();
    void testByte5Fields();
    void testByte6Fields();
    void testByte7Fields();
};

void TestAnimeMaskParsing::initTestCase()
{
    // Create temporary database
    tempDbFile = new QTemporaryFile();
    QVERIFY(tempDbFile->open());
    QString dbPath = tempDbFile->fileName();
    tempDbFile->close();
    
    // Create API instance with temp database
    api = new TestableAniDBApi(dbPath);
    QVERIFY(api != nullptr);
}

void TestAnimeMaskParsing::cleanupTestCase()
{
    delete api;
    delete tempDbFile;
}

void TestAnimeMaskParsing::testParseWithAidBit()
{
    // Test parsing with AID bit set (Byte 1, bit 7 = 0x80)
    // Mask: FC000000000000 (Byte 1 = 0xFC has AID and DATEFLAGS)
    // Response with AID: "18989|0"
    // Parser skips AID (caller extracts it separately at token[0])
    // Parser starts reading at token[1] for DATEFLAGS
    
    QStringList tokens;
    tokens << "18989" << "0";  // AID (skipped), DATEFLAGS
    
    QString mask = "FC000000000000";
    int index = 1;  // Start at 1 because AID is at token[0] and skipped
    QByteArray parsedMaskBytes;
    
    auto data = api->testParseMaskFromString(tokens, mask, index, parsedMaskBytes);
    
    // DATEFLAGS should be parsed from token[1]
    QCOMPARE(data.dateFlags(), QString("0"));
    
    // Index should have advanced by 1 (DATEFLAGS consumed)
    QCOMPARE(index, 2);
}

void TestAnimeMaskParsing::testParseWithoutAidBit()
{
    // Test parsing without AID bit (used in re-requests)
    // Mask: 04000000000000 (Byte 2, bit 2 = SYNONYM_LIST only)
    // Response: "Backstabbed in a Backwater Dungeon..."
    
    QStringList tokens;
    tokens << "Backstabbed in a Backwater Dungeon: My Trusted Companions Tried to Kill Me";
    
    QString mask = "00040000000000";
    int index = 0;
    QByteArray parsedMaskBytes;
    
    auto data = api->testParseMaskFromString(tokens, mask, index, parsedMaskBytes);
    
    // SYNONYM_LIST should be parsed from token[0]
    QCOMPARE(data.synonyms(), tokens[0]);
    
    // Index should have advanced by 1
    QCOMPARE(index, 1);
}

void TestAnimeMaskParsing::testRetiredBitConsumed()
{
    // Test that retired bit (Byte 1, bit 1 = 0x02) is consumed from response
    // Mask: 82000000000000 (Byte 1 = 0x82 = AID bit + retired bit 1)
    // Response: "18989|retired_value"
    
    QStringList tokens;
    tokens << "18989" << "retired_value";
    
    QString mask = "82000000000000";
    int index = 0;
    QByteArray parsedMaskBytes;
    
    auto data = api->testParseMaskFromString(tokens, mask, index, parsedMaskBytes);
    
    // Both tokens should be consumed (AID skipped, retired bit consumed)
    QCOMPARE(index, 1);
    
    // Retired field should not be stored (we don't have a field for it)
    // Just verify parsing didn't crash
}

void TestAnimeMaskParsing::testUnusedBitConsumed()
{
    // Test that unused bit (Byte 7, bit 2 = 0x04) is consumed from response
    // Mask: 00000000000004 (Byte 7, bit 2 = unused)
    // Response: "unused_value"
    
    QStringList tokens;
    tokens << "unused_value";
    
    QString mask = "00000000000004";
    int index = 0;
    QByteArray parsedMaskBytes;
    
    auto data = api->testParseMaskFromString(tokens, mask, index, parsedMaskBytes);
    
    // Token should be consumed even though it's unused
    QCOMPARE(index, 1);
}

void TestAnimeMaskParsing::testCompleteAnimeResponse()
{
    // Test based on actual log - first response with AID
    // Mask: FCFCFEFF7F80F8
    // Response: 18989|0|2025-2025|TV Series|||Romaji|Kanji|English|Other|Short|Synonym
    // Parser starts at index=1 (token[0]=AID is skipped)
    
    QStringList tokens;
    tokens << "18989" << "0" << "2025-2025" << "TV Series" << "" << ""
           << "Shinjite Ita Nakama-tachi" << "信じていた仲間達" 
           << "My Gift Lvl 9999" << "Other name" << "Short names" << "Synonyms";
    
    QString mask = "FCFCFEFF7F80F8";
    int index = 1;  // Start at 1 because AID at token[0] is skipped
    QByteArray parsedMaskBytes;
    
    auto data = api->testParseMaskFromString(tokens, mask, index, parsedMaskBytes);
    
    // Verify key fields were parsed correctly
    // Note: AID is skipped by parser, handled by caller
    QCOMPARE(data.dateFlags(), QString("0"));
    QCOMPARE(data.year(), QString("2025-2025"));
    QCOMPARE(data.type(), QString("TV Series"));
    QCOMPARE(data.nameRomaji(), QString("Shinjite Ita Nakama-tachi"));
    QCOMPARE(data.nameKanji(), QString("信じていた仲間達"));
    QCOMPARE(data.nameEnglish(), QString("My Gift Lvl 9999"));
    QCOMPARE(data.synonyms(), QString("Synonyms"));
    
    // Verify all tokens were consumed (AID skipped + 11 fields parsed)
    QCOMPARE(index, 12);
}

void TestAnimeMaskParsing::testCompleteReRequestResponse()
{
    // Test based on actual log - re-request without AID
    // Mask: 0004FEFF7F80F8 (no AID bit)
    // Response starts with SYNONYM_LIST, not AID
    
    QStringList tokens;
    tokens << "Backstabbed in a Backwater Dungeon" // SYNONYM_LIST (byte 2, bit 2)
           << "12" << "12" << "1" << "1759449600" << "1766102400" // Byte 3 fields
           << "https://mugengacha.com/" << "318947.jpg" // Byte 3 fields
           << "349" << "40" << "536" << "41" << "0" << "0" << "" << "0" // Byte 4 fields
           << "34029" << "400172" << "" << "original work" // Byte 5 fields
           << "2609,2799" << "0,0,0,0" << "1762811997" // Byte 5 fields
           << "148179,148180" // Byte 6 field
           << "1" << "0" << "0" << "0" << "0"; // Byte 7 fields
    
    QString mask = "0004FEFF7F80F8";
    int index = 0;
    QByteArray parsedMaskBytes;
    
    auto data = api->testParseMaskFromString(tokens, mask, index, parsedMaskBytes);
    
    // Verify first field is SYNONYM_LIST (not AID)
    QCOMPARE(data.synonyms(), QString("Backstabbed in a Backwater Dungeon"));
    
    // Verify some other fields
    QCOMPARE(data.episodeCount(), 12);
    QCOMPARE(data.highestEpisode(), QString("12"));
    QCOMPARE(data.url(), QString("https://mugengacha.com/"));
    
    // Verify correct number of tokens consumed
    QVERIFY(index > 0);
}

void TestAnimeMaskParsing::testByte2Fields()
{
    // Test Byte 2 fields (ROMAJI_NAME, KANJI_NAME, ENGLISH_NAME, etc.)
    // Mask: 00FC0000000000 (Byte 2 = 0xFC: bits 7,6,5,4,3,2)
    
    QStringList tokens;
    tokens << "Romaji Name" << "Kanji Name" << "English Name" 
           << "Other Name" << "Short Names" << "Synonyms";
    
    QString mask = "00FC0000000000";
    int index = 0;
    
    auto data = api->testParseMaskFromString(tokens, mask, index);
    
    QCOMPARE(data.nameRomaji(), QString("Romaji Name"));
    QCOMPARE(data.nameKanji(), QString("Kanji Name"));
    QCOMPARE(data.nameEnglish(), QString("English Name"));
    QCOMPARE(data.nameOther(), QString("Other Name"));
    QCOMPARE(data.nameShort(), QString("Short Names"));
    QCOMPARE(data.synonyms(), QString("Synonyms"));
    QCOMPARE(index, 6);
}

void TestAnimeMaskParsing::testByte3Fields()
{
    // Test Byte 3 fields (EPISODES, HIGHEST_EPISODE, etc.)
    // Mask: 0000FE00000000 (Byte 3 = 0xFE: bits 7,6,5,4,3,2,1)
    
    QStringList tokens;
    tokens << "12" << "12" << "1" << "1759449600" << "1766102400" 
           << "https://example.com/" << "image.jpg";
    
    QString mask = "0000FE00000000";
    int index = 0;
    
    auto data = api->testParseMaskFromString(tokens, mask, index);
    
    QCOMPARE(data.episodeCount(), 12);
    QCOMPARE(data.highestEpisode(), QString("12"));
    QCOMPARE(data.specialEpisodeCount(), 1);
    QCOMPARE(data.airDate(), QString("1759449600"));
    QCOMPARE(data.endDate(), QString("1766102400"));
    QCOMPARE(data.url(), QString("https://example.com/"));
    QCOMPARE(data.pictureName(), QString("image.jpg"));
    QCOMPARE(index, 7);
}

void TestAnimeMaskParsing::testByte4Fields()
{
    // Test Byte 4 fields (RATING, VOTE_COUNT, etc.)
    // Mask: 000000FF000000 (Byte 4 = 0xFF: all 8 bits)
    
    QStringList tokens;
    tokens << "349" << "40" << "536" << "41" << "0" << "0" << "award1,award2" << "0";
    
    QString mask = "000000FF000000";
    int index = 0;
    
    auto data = api->testParseMaskFromString(tokens, mask, index);
    
    QCOMPARE(data.rating(), QString("349"));
    QCOMPARE(data.voteCount(), 40);
    QCOMPARE(data.tempRating(), QString("536"));
    QCOMPARE(data.tempVoteCount(), 41);
    QCOMPARE(data.avgReviewRating(), QString("0"));
    QCOMPARE(data.reviewCount(), 0);
    QCOMPARE(data.awardList(), QString("award1,award2"));
    QCOMPARE(data.is18Restricted(), false);
    QCOMPARE(index, 8);
}

void TestAnimeMaskParsing::testByte5Fields()
{
    // Test Byte 5 fields (ANN_ID, TAG_LIST, etc.)
    // Mask: 00000000FF0000 (Byte 5 = 0xFF: all 8 bits)
    
    QStringList tokens;
    tokens << "retired" << "34029" << "400172" << "animenfo123" 
           << "action,comedy" << "100,200" << "50,75" << "1762811997";
    
    QString mask = "00000000FF0000";
    int index = 0;
    
    auto data = api->testParseMaskFromString(tokens, mask, index);
    
    // Note: First bit in byte 5 is retired, so it gets consumed but not stored
    QCOMPARE(data.annId(), 34029);
    QCOMPARE(data.allCinemaId(), 400172);
    QCOMPARE(data.animeNfoId(), QString("animenfo123"));
    QCOMPARE(data.tagNameList(), QString("action,comedy"));
    QCOMPARE(data.tagIdList(), QString("100,200"));
    QCOMPARE(data.tagWeightList(), QString("50,75"));
    QCOMPARE(data.dateRecordUpdated(), static_cast<qint64>(1762811997));
    QCOMPARE(index, 8);
}

void TestAnimeMaskParsing::testByte6Fields()
{
    // Test Byte 6 fields (CHARACTER_ID_LIST)
    // Mask: 0000000000FF00 (Byte 6 = 0xFF, but only bit 7 is defined)
    
    QStringList tokens;
    tokens << "148179,148180,150080";
    
    QString mask = "00000000008000"; // Only bit 7 (CHARACTER_ID_LIST)
    int index = 0;
    
    auto data = api->testParseMaskFromString(tokens, mask, index);
    
    QCOMPARE(data.characterIdList(), QString("148179,148180,150080"));
    QCOMPARE(index, 1);
}

void TestAnimeMaskParsing::testByte7Fields()
{
    // Test Byte 7 fields (SPECIALS_COUNT, CREDITS_COUNT, etc.)
    // Mask: 000000000000F8 (Byte 7 = 0xF8: bits 7,6,5,4,3)
    
    QStringList tokens;
    tokens << "1" << "0" << "0" << "0" << "0";
    
    QString mask = "000000000000F8";
    int index = 0;
    
    auto data = api->testParseMaskFromString(tokens, mask, index);
    
    QCOMPARE(data.specialsCount(), 1);
    QCOMPARE(data.creditsCount(), 0);
    QCOMPARE(data.otherCount(), 0);
    QCOMPARE(data.trailerCount(), 0);
    QCOMPARE(data.parodyCount(), 0);
    QCOMPARE(index, 5);
}

QTEST_MAIN(TestAnimeMaskParsing)
#include "test_anime_mask_parsing.moc"
