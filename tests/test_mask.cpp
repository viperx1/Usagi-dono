#include <QtTest/QtTest>
#include "../usagi/src/mask.h"

/**
 * @brief Test suite for Mask class byte order
 * 
 * Verifies that Mask::toString() outputs bytes in the correct order:
 * - Byte 1 (LSB) should appear first (leftmost) in the hex string
 * - Byte 7 (MSB) should appear last (rightmost) in the hex string
 */
class TestMask : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief Test basic byte order with simple values
     */
    void testByteOrderSimple();
    
    /**
     * @brief Test byte order with mixed values
     */
    void testByteOrderMixed();
    
    /**
     * @brief Test byte order with all bytes set
     */
    void testByteOrderFull();
    
    /**
     * @brief Test that setFromString and toString are inverse operations
     */
    void testRoundTrip();
    
    /**
     * @brief Test individual byte setting
     */
    void testSetByte();
};

void TestMask::testByteOrderSimple()
{
    // Test with a value where Byte 1 = 0x30, all others = 0x00
    // In uint64_t: bits 7-0 = 0x30, so value = 0x0000000000000030
    Mask mask(0x0000000000000030ULL);
    QString result = mask.toString();
    
    // Expected: Byte 1 first, so "30" at the beginning
    QCOMPARE(result, QString("30000000000000"));
}

void TestMask::testByteOrderMixed()
{
    // Test with multiple bytes set:
    // Byte 1 (bits 7-0)   = 0x30 = 0x0000000000000030
    // Byte 2 (bits 15-8)  = 0x80 = 0x0000000000008000
    // Byte 3 (bits 23-16) = 0x80 = 0x0000000000800000
    // Combined = 0x0000000000808030
    Mask mask(0x0000000000808030ULL);
    QString result = mask.toString();
    
    // Expected: Bytes in order 1, 2, 3, 4, 5, 6, 7
    QCOMPARE(result, QString("30808000000000"));
}

void TestMask::testByteOrderFull()
{
    // Test with all 7 bytes set to different values
    // Byte 1 = 0x01, Byte 2 = 0x02, ..., Byte 7 = 0x07
    // In uint64_t: 0x0007060504030201
    Mask mask(0x0007060504030201ULL);
    QString result = mask.toString();
    
    // Expected: "01020304050607" (Byte 1 first, Byte 7 last)
    QCOMPARE(result, QString("01020304050607"));
}

void TestMask::testRoundTrip()
{
    // Test that setFromString and toString are inverse operations
    QString original = "AABBCCDDEEFF00";
    
    Mask mask(original);
    QString result = mask.toString();
    
    QCOMPARE(result, original);
}

void TestMask::testSetByte()
{
    // Test that setByte works with correct byte order
    Mask mask;
    
    // Set Byte 1 (index 0) to 0xAA
    mask.setByte(0, 0xAA);
    QString result1 = mask.toString();
    QCOMPARE(result1, QString("AA000000000000"));
    
    // Set Byte 2 (index 1) to 0xBB
    mask.setByte(1, 0xBB);
    QString result2 = mask.toString();
    QCOMPARE(result2, QString("AABB0000000000"));
    
    // Set Byte 7 (index 6) to 0xFF
    mask.setByte(6, 0xFF);
    QString result3 = mask.toString();
    QCOMPARE(result3, QString("AABB00000000FF"));
}

QTEST_MAIN(TestMask)
#include "test_mask.moc"
