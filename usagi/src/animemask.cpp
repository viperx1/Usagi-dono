#include "animemask.h"

AnimeMask::AnimeMask() : mask(0)
{
}

AnimeMask::AnimeMask(const QString& hexString) : mask(0)
{
	setFromString(hexString);
}

AnimeMask::AnimeMask(uint64_t value) : mask(value & 0x00FFFFFFFFFFFFFFULL)
{
	// Mask off byte 8 (keep only lower 56 bits)
}

void AnimeMask::setFrom32Bit(uint32_t value)
{
	// 32-bit enum values go into bytes 1-4 (lower 32 bits)
	mask = static_cast<uint64_t>(value);
}

void AnimeMask::setFromString(const QString& hexString)
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

void AnimeMask::setValue(uint64_t value)
{
	mask = value & 0x00FFFFFFFFFFFFFFULL;
}

QString AnimeMask::toString() const
{
	// Format as 14 hex characters (7 bytes), always uppercase, zero-padded
	return QString("%1").arg(mask, 14, 16, QChar('0')).toUpper();
}

uint64_t AnimeMask::getValue() const
{
	return mask;
}

bool AnimeMask::isEmpty() const
{
	return mask == 0;
}

AnimeMask AnimeMask::operator|(const AnimeMask& other) const
{
	return AnimeMask(mask | other.mask);
}

AnimeMask AnimeMask::operator&(const AnimeMask& other) const
{
	return AnimeMask(mask & other.mask);
}

AnimeMask AnimeMask::operator~() const
{
	// NOT operation, but keep byte 8 as 0
	return AnimeMask((~mask) & 0x00FFFFFFFFFFFFFFULL);
}

AnimeMask& AnimeMask::operator|=(const AnimeMask& other)
{
	mask |= other.mask;
	return *this;
}

AnimeMask& AnimeMask::operator&=(const AnimeMask& other)
{
	mask &= other.mask;
	return *this;
}

bool AnimeMask::operator==(const AnimeMask& other) const
{
	return mask == other.mask;
}

bool AnimeMask::operator!=(const AnimeMask& other) const
{
	return mask != other.mask;
}
