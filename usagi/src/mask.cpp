#include "mask.h"

Mask::Mask() : mask(0)
{
}

Mask::Mask(const QString& hexString) : mask(0)
{
	setFromString(hexString);
}

Mask::Mask(uint64_t value) : mask(value & 0x00FFFFFFFFFFFFFFULL)
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
	
	// Parse hex string as 64-bit value
	bool ok = false;
	// Take first 14 characters (7 bytes), pad if shorter
	QString padded = hexString.leftJustified(14, '0');
	mask = padded.toULongLong(&ok, 16);
	
	if (!ok)
	{
		mask = 0;
	}
	
	// Ensure byte 8 is 0 (mask off upper 8 bits)
	mask &= 0x00FFFFFFFFFFFFFFULL;
}

void Mask::setValue(uint64_t value)
{
	mask = value & 0x00FFFFFFFFFFFFFFULL;
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
	mask &= 0x00FFFFFFFFFFFFFFULL;
}

QString Mask::toString() const
{
	// Format as 14 hex characters (7 bytes), always uppercase, zero-padded
	return QString("%1").arg(mask, 14, 16, QChar('0')).toUpper();
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
	return Mask((~mask) & 0x00FFFFFFFFFFFFFFULL);
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
