#ifndef ANIMEMASK_H
#define ANIMEMASK_H

#include <QString>
#include <cstdint>

/**
 * @brief AnimeMask class for proper handling of 7-byte anime masks
 * 
 * AniDB ANIME command uses a 7-byte mask (56 bits), but C++ enums
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
class AnimeMask
{
public:
	/**
	 * @brief Construct empty AnimeMask (all bits 0)
	 */
	AnimeMask();
	
	/**
	 * @brief Construct AnimeMask from hex string
	 * @param hexString Hex string representing 7 bytes (e.g., "fffffcfc000000")
	 */
	explicit AnimeMask(const QString& hexString);
	
	/**
	 * @brief Construct AnimeMask from 64-bit value
	 * @param value 64-bit value where lower 56 bits represent the mask
	 */
	explicit AnimeMask(uint64_t value);
	
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
	AnimeMask operator|(const AnimeMask& other) const;
	
	/**
	 * @brief Bitwise AND operation
	 */
	AnimeMask operator&(const AnimeMask& other) const;
	
	/**
	 * @brief Bitwise NOT operation
	 */
	AnimeMask operator~() const;
	
	/**
	 * @brief Bitwise OR assignment
	 */
	AnimeMask& operator|=(const AnimeMask& other);
	
	/**
	 * @brief Bitwise AND assignment
	 */
	AnimeMask& operator&=(const AnimeMask& other);
	
	/**
	 * @brief Equality comparison
	 */
	bool operator==(const AnimeMask& other) const;
	
	/**
	 * @brief Inequality comparison
	 */
	bool operator!=(const AnimeMask& other) const;

private:
	uint64_t mask; ///< 64-bit storage for 7-byte mask (byte 8 always 0)
};

#endif // ANIMEMASK_H
