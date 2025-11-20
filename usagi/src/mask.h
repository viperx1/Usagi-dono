#ifndef MASK_H
#define MASK_H

#include <QString>
#include <cstdint>

/**
 * @brief Mask class for proper handling of 7-byte masks
 * 
 * AniDB ANIME and FILE commands use 7-byte masks (56 bits), but C++ enums
 * are typically 32-bit. This class uses uint64_t to properly represent
 * all 7 bytes and provides conversion to/from hex strings.
 * 
 * The extra byte (byte 8) is always kept at 0.
 * 
 * Byte layout (from left to right in hex string):
 * - Bytes 1-4: Low 32 bits (enum constants work here)
 * - Bytes 5-7: High 24 bits (would require enum extension or string parsing)
 * - Byte 8: Always 0 (unused, for alignment)
 */
class Mask
{
public:
	/**
	 * @brief Construct empty Mask (all bits 0)
	 */
	Mask();
	
	/**
	 * @brief Construct Mask from hex string
	 * @param hexString Hex string representing 7 bytes (e.g., "fffffcfc000000")
	 */
	explicit Mask(const QString& hexString);
	
	/**
	 * @brief Construct Mask from 64-bit value
	 * @param value 64-bit value where lower 56 bits represent the mask
	 */
	explicit Mask(uint64_t value);
	
	/**
	 * @brief Set mask from 32-bit enum value (bytes 1-4)
	 * @param value 32-bit value from enum constants
	 */
	void setFrom32Bit(uint32_t value);
	
	/**
	 * @brief Set mask from hex string
	 * @param hexString Hex string representing 7 bytes
	 */
	void setFromString(const QString& hexString);
	
	/**
	 * @brief Set mask from 64-bit value
	 * @param value 64-bit value where lower 56 bits represent the mask
	 */
	void setValue(uint64_t value);
	
	/**
	 * @brief Set a specific byte in the mask (0-6)
	 * @param byteIndex Index of the byte to set (0 = rightmost byte, 6 = leftmost byte)
	 * @param value Byte value to set
	 */
	void setByte(int byteIndex, uint8_t value);
	
	/**
	 * @brief Convert mask to hex string (always 14 characters for 7 bytes)
	 * @return Hex string representation (e.g., "fffffcfc000000")
	 */
	QString toString() const;
	
	/**
	 * @brief Get 64-bit value of mask
	 * @return 64-bit value (lower 56 bits are the mask)
	 */
	uint64_t getValue() const;
	
	/**
	 * @brief Check if mask is empty (all bits 0)
	 * @return true if mask is empty
	 */
	bool isEmpty() const;
	
	/**
	 * @brief Bitwise OR operation
	 */
	Mask operator|(const Mask& other) const;
	
	/**
	 * @brief Bitwise AND operation
	 */
	Mask operator&(const Mask& other) const;
	
	/**
	 * @brief Bitwise NOT operation
	 */
	Mask operator~() const;
	
	/**
	 * @brief Bitwise OR assignment
	 */
	Mask& operator|=(const Mask& other);
	
	/**
	 * @brief Bitwise AND assignment
	 */
	Mask& operator&=(const Mask& other);
	
	/**
	 * @brief Equality comparison
	 */
	bool operator==(const Mask& other) const;
	
	/**
	 * @brief Inequality comparison
	 */
	bool operator!=(const Mask& other) const;

private:
	// Mask for 7 bytes (56 bits), byte 8 always 0
	static constexpr uint64_t SEVEN_BYTE_MASK = 0x00FFFFFFFFFFFFFFULL;
	
	uint64_t mask; ///< 64-bit storage for 7-byte mask (byte 8 always 0)
};

#endif // MASK_H
