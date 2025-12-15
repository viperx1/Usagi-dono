#include <QTest>
#include "../usagi/src/animeutils.h"

class TestFileVersionExtraction : public QObject
{
    Q_OBJECT

private slots:
    void testVersion1_NoVersionBits()
    {
        // No version bits set = version 1
        int state = 0;
        QCOMPARE(AnimeUtils::extractFileVersion(state), 1);
        
        // State with only CRC bits set (bits 0-1) = version 1
        state = 1;  // Bit 0: FILE_CRCOK
        QCOMPARE(AnimeUtils::extractFileVersion(state), 1);
        
        state = 2;  // Bit 1: FILE_CRCERR
        QCOMPARE(AnimeUtils::extractFileVersion(state), 1);
        
        state = 3;  // Bits 0-1: CRCOK + CRCERR
        QCOMPARE(AnimeUtils::extractFileVersion(state), 1);
        
        // State with only censorship bits set (bits 6-7) = version 1
        state = 64;  // Bit 6: FILE_UNC
        QCOMPARE(AnimeUtils::extractFileVersion(state), 1);
        
        state = 128;  // Bit 7: FILE_CEN
        QCOMPARE(AnimeUtils::extractFileVersion(state), 1);
        
        state = 192;  // Bits 6-7: UNC + CEN
        QCOMPARE(AnimeUtils::extractFileVersion(state), 1);
    }
    
    void testVersion2()
    {
        // Bit 2 (4): FILE_ISV2
        int state = 4;
        QCOMPARE(AnimeUtils::extractFileVersion(state), 2);
        
        // Version 2 with CRC flag
        state = 5;  // Bits 0,2: CRCOK + ISV2
        QCOMPARE(AnimeUtils::extractFileVersion(state), 2);
        
        // Version 2 with censorship flag
        state = 68;  // Bits 2,6: ISV2 + UNC
        QCOMPARE(AnimeUtils::extractFileVersion(state), 2);
    }
    
    void testVersion3()
    {
        // Bit 3 (8): FILE_ISV3
        int state = 8;
        QCOMPARE(AnimeUtils::extractFileVersion(state), 3);
        
        // Version 3 with CRC flag
        state = 9;  // Bits 0,3: CRCOK + ISV3
        QCOMPARE(AnimeUtils::extractFileVersion(state), 3);
        
        // Version 3 with censorship flag
        state = 136;  // Bits 3,7: ISV3 + CEN
        QCOMPARE(AnimeUtils::extractFileVersion(state), 3);
    }
    
    void testVersion4()
    {
        // Bit 4 (16): FILE_ISV4
        int state = 16;
        QCOMPARE(AnimeUtils::extractFileVersion(state), 4);
        
        // Version 4 with CRC flag
        state = 17;  // Bits 0,4: CRCOK + ISV4
        QCOMPARE(AnimeUtils::extractFileVersion(state), 4);
        
        // Version 4 with censorship flag
        state = 80;  // Bits 4,6: ISV4 + UNC
        QCOMPARE(AnimeUtils::extractFileVersion(state), 4);
    }
    
    void testVersion5()
    {
        // Bit 5 (32): FILE_ISV5
        int state = 32;
        QCOMPARE(AnimeUtils::extractFileVersion(state), 5);
        
        // Version 5 with CRC flag
        state = 33;  // Bits 0,5: CRCOK + ISV5
        QCOMPARE(AnimeUtils::extractFileVersion(state), 5);
        
        // Version 5 with censorship flag
        state = 160;  // Bits 5,7: ISV5 + CEN
        QCOMPARE(AnimeUtils::extractFileVersion(state), 5);
    }
    
    void testVersionPriority()
    {
        // When multiple version bits are set, higher versions take priority
        
        // v5 > v4
        int state = 48;  // Bits 4,5: ISV4 + ISV5
        QCOMPARE(AnimeUtils::extractFileVersion(state), 5);
        
        // v5 > v3
        state = 40;  // Bits 3,5: ISV3 + ISV5
        QCOMPARE(AnimeUtils::extractFileVersion(state), 5);
        
        // v4 > v3
        state = 24;  // Bits 3,4: ISV3 + ISV4
        QCOMPARE(AnimeUtils::extractFileVersion(state), 4);
        
        // v4 > v2
        state = 20;  // Bits 2,4: ISV2 + ISV4
        QCOMPARE(AnimeUtils::extractFileVersion(state), 4);
        
        // v3 > v2
        state = 12;  // Bits 2,3: ISV2 + ISV3
        QCOMPARE(AnimeUtils::extractFileVersion(state), 3);
        
        // All version bits set - v5 should win
        state = 60;  // Bits 2,3,4,5: all version bits
        QCOMPARE(AnimeUtils::extractFileVersion(state), 5);
    }
    
    void testRealWorldCombinations()
    {
        // Common real-world combinations
        
        // Version 1 file with CRC OK and uncensored
        int state = 65;  // Bits 0,6: CRCOK + UNC
        QCOMPARE(AnimeUtils::extractFileVersion(state), 1);
        
        // Version 2 file with CRC OK and uncensored
        state = 69;  // Bits 0,2,6: CRCOK + ISV2 + UNC
        QCOMPARE(AnimeUtils::extractFileVersion(state), 2);
        
        // Version 3 file with CRC OK and censored
        state = 137;  // Bits 0,3,7: CRCOK + ISV3 + CEN
        QCOMPARE(AnimeUtils::extractFileVersion(state), 3);
        
        // Version 4 file with all flags
        state = 209;  // Bits 0,4,6,7: CRCOK + ISV4 + UNC + CEN
        QCOMPARE(AnimeUtils::extractFileVersion(state), 4);
        
        // Version 5 file with CRC ERROR
        state = 34;  // Bits 1,5: CRCERR + ISV5
        QCOMPARE(AnimeUtils::extractFileVersion(state), 5);
    }
};

QTEST_MAIN(TestFileVersionExtraction)
#include "test_file_version_extraction.moc"
