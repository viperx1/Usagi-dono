#include "aired.h"
#include "logger.h"
#include <QDateTime>

// Static flag to log only once
static bool s_airedLogged = false;

aired::aired()
    : m_startDate(), m_endDate()
{
    // Log only once when first aired object is created
    if (!s_airedLogged) {
        LOG("aired date system initialized [aired.cpp]");
        s_airedLogged = true;
    }
}

aired::aired(const QString& startDateStr, const QString& endDateStr)
    : m_startDate(parseDate(startDateStr)), m_endDate(parseDate(endDateStr))
{
}

aired::aired(const QDate& startDate, const QDate& endDate)
    : m_startDate(startDate), m_endDate(endDate)
{
}

QDate aired::parseDate(const QString& dateStr)
{
    if(dateStr.isEmpty())
        return QDate();
    
    // Handle format like "2003-11-16Z" or "2003-11-16"
    QString cleanStr = dateStr;
    if(cleanStr.endsWith("Z"))
        cleanStr.chop(1);
    
    // Try to parse as ISO date format (YYYY-MM-DD)
    QDate date = QDate::fromString(cleanStr, Qt::ISODate);
    return date;
}

QString aired::formatDate(const QDate& date)
{
    if(!date.isValid())
        return "";
    
    // Format as DD.MM.YYYY
    return date.toString("dd.MM.yyyy");
}

QString aired::toDisplayString() const
{
    if(!m_startDate.isValid())
        return "";  // No start date - return empty
    
    QDate today = QDate::currentDate();
    
    // Check if start date is in the future
    if(m_startDate > today)
    {
        // Future release
        return QString("Airs %1").arg(formatDate(m_startDate));
    }
    
    // Start date is in the past or today
    if(m_endDate.isValid())
    {
        // Both dates are set
        if(m_endDate >= today)
        {
            // Still airing (end date is today or future)
            return QString("%1-ongoing").arg(formatDate(m_startDate));
        }
        else
        {
            // Finished (end date is in the past)
            return QString("%1-%2").arg(formatDate(m_startDate), formatDate(m_endDate));
        }
    }
    else
    {
        // Only start date is set, no end date
        // Treat as ongoing
        return QString("%1-ongoing").arg(formatDate(m_startDate));
    }
}

bool aired::operator<(const aired& other) const
{
    // Sort by start date first, then by end date
    if(m_startDate != other.m_startDate)
        return m_startDate < other.m_startDate;
    return m_endDate < other.m_endDate;
}

bool aired::operator>(const aired& other) const
{
    return other < *this;
}

bool aired::operator==(const aired& other) const
{
    return m_startDate == other.m_startDate && m_endDate == other.m_endDate;
}

bool aired::operator!=(const aired& other) const
{
    return !(*this == other);
}

bool aired::operator<=(const aired& other) const
{
    return !(*this > other);
}

bool aired::operator>=(const aired& other) const
{
    return !(*this < other);
}
