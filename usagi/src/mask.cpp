#include "mask.h"

Mask::Mask() : mask(0)
{
}

Mask::Mask(const QString& hexString) : mask(0)
{
	setFromString(hexString);
}

Mask::Mask(uint64_t value) : mask(value & SEVEN_BYTE_MASK)
{
	// Mask off byte 8 (keep only lower 56 bits)
}

void Mask::setFrom32Bit(uint32_t value)
{
	// 32-bit enum values go into bytes 1-4 (lower 32 bits)
	mask = static_cast<uint64_t>(value);
}

void Mask::setFromString(const QString& hexString)
{
	if (hexString.isEmpty())
	{
		mask = 0;
		return;
	}
	
	// Parse hex string byte by byte
	// The hex string has Byte 1 first (leftmost), Byte 7 last (rightmost)
	// We need to store Byte 1 in bits 7-0 (LSB), Byte 7 in bits 55-48
	QString padded = hexString.leftJustified(14, '0').left(14); // Ensure exactly 14 chars
	
	mask = 0;
	for (int i = 0; i < 7; i++)
	{
		bool ok = false;
		QString byteStr = padded.mid(i * 2, 2);
		uint8_t byte = byteStr.toUInt(&ok, 16);
		if (!ok)
		{
			mask = 0;
			return;
		}
		// Store this byte at position i (Byte 1 at i=0 goes to bits 7-0, etc.)
		mask |= (static_cast<uint64_t>(byte) << (i * 8));
	}
	
	// Ensure byte 8 is 0 (mask off upper 8 bits)
	mask &= SEVEN_BYTE_MASK;
}

void Mask::setValue(uint64_t value)
{
	mask = value & SEVEN_BYTE_MASK;
}

void Mask::setByte(int byteIndex, uint8_t value)
{
	if (byteIndex < 0 || byteIndex >= 7)
	{
		return; // Invalid index
	}
	
	// Clear the target byte and set new value
	// byteIndex 0 = rightmost byte (bits 0-7)
	// byteIndex 6 = leftmost byte (bits 48-55)
	int shiftAmount = byteIndex * 8;
	uint64_t byteMask = 0xFFULL << shiftAmount;
	mask = (mask & ~byteMask) | (static_cast<uint64_t>(value) << shiftAmount);
	
	// Ensure byte 8 stays 0
	mask &= SEVEN_BYTE_MASK;
}

QString Mask::toString() const
{
	// Extract each byte from LSB to MSB and format as hex string
	// The enum values store Byte 1 in bits 7-0 (LSB), Byte 7 in bits 55-48 (MSB)
	// But AniDB expects the hex string with Byte 1 first (leftmost), Byte 7 last (rightmost)
	// So we need to extract bytes in reverse order compared to standard hex output
	QString result;
	for (int i = 0; i < 7; i++)
	{
		uint8_t byte = (mask >> (i * 8)) & 0xFF;
		result += QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
	}
	return result;
}

uint64_t Mask::getValue() const
{
	return mask;
}

bool Mask::isEmpty() const
{
	return mask == 0;
}

Mask Mask::operator|(const Mask& other) const
{
	return Mask(mask | other.mask);
}

Mask Mask::operator&(const Mask& other) const
{
	return Mask(mask & other.mask);
}

Mask Mask::operator~() const
{
	// NOT operation, but keep byte 8 as 0
	return Mask((~mask) & SEVEN_BYTE_MASK);
}

Mask& Mask::operator|=(const Mask& other)
{
	mask |= other.mask;
	return *this;
}

Mask& Mask::operator&=(const Mask& other)
{
	mask &= other.mask;
	return *this;
}

bool Mask::operator==(const Mask& other) const
{
	return mask == other.mask;
}

bool Mask::operator!=(const Mask& other) const
{
	return mask != other.mask;
}
