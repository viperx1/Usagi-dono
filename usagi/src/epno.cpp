#include "epno.h"
#include "logger.h"

// Static flag to log only once
static bool s_epnoLogged = false;

epno::epno()
    : m_type(0), m_number(0), m_rawString("")
{
    // Log only once when first epno object is created
    if (!s_epnoLogged) {
        Logger::log("epno type system initialized [epno.cpp]");
        s_epnoLogged = true;
    }
}

epno::epno(const QString& epnoString)
    : m_type(0), m_number(0), m_rawString(epnoString)
{
    parse(epnoString);
}

epno::epno(int type, int number)
    : m_type(type), m_number(number), m_rawString("")
{
    // Generate raw string from type and number
    QString prefix;
    if(type == 2)
        prefix = "S";
    else if(type == 3)
        prefix = "C";
    else if(type == 4)
        prefix = "T";
    else if(type == 5)
        prefix = "P";
    else if(type == 6)
        prefix = "O";
    
    if(!prefix.isEmpty())
        m_rawString = prefix + QString::number(number);
    else
        m_rawString = QString::number(number);
}

void epno::parse(const QString& str)
{
    if(str.isEmpty())
    {
        m_type = 0;
        m_number = 0;
        return;
    }
    
    QString upper = str.toUpper();
    QString numericPart;
    
    // Determine type based on prefix
    if(upper.startsWith("S"))
    {
        m_type = 2;  // Special
        numericPart = str.mid(1);
    }
    else if(upper.startsWith("C"))
    {
        m_type = 3;  // Credit
        numericPart = str.mid(1);
    }
    else if(upper.startsWith("T"))
    {
        m_type = 4;  // Trailer
        numericPart = str.mid(1);
    }
    else if(upper.startsWith("P"))
    {
        m_type = 5;  // Parody
        numericPart = str.mid(1);
    }
    else if(upper.startsWith("O"))
    {
        m_type = 6;  // Other
        numericPart = str.mid(1);
    }
    else
    {
        m_type = 1;  // Regular episode
        numericPart = str;
    }
    
    // Extract numeric value (removes leading zeros automatically)
    bool ok;
    m_number = numericPart.toInt(&ok);
    if(!ok)
    {
        m_number = 0;
        m_type = 0;  // Invalid
    }
}

QString epno::toDisplayString() const
{
    if(!isValid())
        return "";
    
    // Format based on type
    QString numStr = QString::number(m_number);
    
    if(m_type == 2)
        return QString("Special %1").arg(numStr);
    else if(m_type == 3)
        return QString("Credit %1").arg(numStr);
    else if(m_type == 4)
        return QString("Trailer %1").arg(numStr);
    else if(m_type == 5)
        return QString("Parody %1").arg(numStr);
    else if(m_type == 6)
        return QString("Other %1").arg(numStr);
    else
        return numStr;  // Regular episode - just the number
}

bool epno::operator<(const epno& other) const
{
    // Sort by type first, then by number
    if(m_type != other.m_type)
        return m_type < other.m_type;
    return m_number < other.m_number;
}

bool epno::operator>(const epno& other) const
{
    return other < *this;
}

bool epno::operator==(const epno& other) const
{
    return m_type == other.m_type && m_number == other.m_number;
}

bool epno::operator!=(const epno& other) const
{
    return !(*this == other);
}

bool epno::operator<=(const epno& other) const
{
    return !(*this > other);
}

bool epno::operator>=(const epno& other) const
{
    return !(*this < other);
}

epno epno::fromString(const QString& str)
{
    return epno(str);
}
