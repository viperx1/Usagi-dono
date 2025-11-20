#include "epno.h"
#include "logger.h"
#include <QMap>

// Static flag to log only once
static bool s_epnoLogged = false;

epno::epno()
    : m_type(0), m_number(0), m_rawString("")
{
    // Log only once when first epno object is created
    if (!s_epnoLogged) {
        LOG("epno type system initialized [epno.cpp]");
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
    // Map of type to prefix
    static const QMap<int, QString> typePrefixes = {
        {2, "S"}, // Special
        {3, "C"}, // Credit
        {4, "T"}, // Trailer
        {5, "P"}, // Parody
        {6, "O"}  // Other
    };
    
    // Generate raw string from type and number
    if(typePrefixes.contains(type))
        m_rawString = typePrefixes[type] + QString::number(number);
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
    
    // Map of prefix to type
    static const QMap<QString, int> prefixTypes = {
        {"S", 2}, // Special
        {"C", 3}, // Credit
        {"T", 4}, // Trailer
        {"P", 5}, // Parody
        {"O", 6}  // Other
    };
    
    QString upper = str.toUpper();
    QString numericPart;
    
    // Check for type prefix
    if(!upper.isEmpty())
    {
        QString firstChar = upper.left(1);
        if(prefixTypes.contains(firstChar))
        {
            m_type = prefixTypes[firstChar];
            numericPart = str.mid(1);
        }
        else
        {
            m_type = 1;  // Regular episode
            numericPart = str;
        }
    }
    else
    {
        m_type = 1;
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
    
    // Map of type to display name
    static const QMap<int, QString> typeNames = {
        {2, "Special"},
        {3, "Credit"},
        {4, "Trailer"},
        {5, "Parody"},
        {6, "Other"}
    };
    
    QString numStr = QString::number(m_number);
    
    if(typeNames.contains(m_type))
        return QString("%1 %2").arg(typeNames[m_type], numStr);
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
