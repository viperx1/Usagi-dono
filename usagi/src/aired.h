#ifndef AIRED_H
#define AIRED_H

#include <QString>
#include <QDate>

/**
 * @brief Aired date type that handles anime airing dates and formats them appropriately
 * 
 * This class represents the airing period of an anime and provides proper formatting
 * for display in the mylist.
 * 
 * Formats:
 * - "DD.MM.YYYY-DD.MM.YYYY" - for finished releases (both start and end dates set)
 * - "DD.MM.YYYY-ongoing" - for still airing (start date set, end date in future or not set)
 * - "Airs DD.MM.YYYY" - for future releases (start date in the future)
 * - "" - for unknown dates
 */
class aired
{
public:
    // Constructors
    aired();
    aired(const QString& startDateStr, const QString& endDateStr);
    aired(const QDate& startDate, const QDate& endDate);
    
    // Getters
    QDate startDate() const { return m_startDate; }
    QDate endDate() const { return m_endDate; }
    bool hasStartDate() const { return m_startDate.isValid(); }
    bool hasEndDate() const { return m_endDate.isValid(); }
    
    // Display string formatted according to requirements
    QString toDisplayString() const;
    
    // Check if this has valid data
    bool isValid() const { return m_startDate.isValid(); }
    
    // Comparison operators for sorting
    bool operator<(const aired& other) const;
    bool operator>(const aired& other) const;
    bool operator==(const aired& other) const;
    bool operator!=(const aired& other) const;
    bool operator<=(const aired& other) const;
    bool operator>=(const aired& other) const;

private:
    QDate m_startDate;  // Start date of airing
    QDate m_endDate;    // End date of airing (may be invalid for ongoing/future)
    
    // Parse date string in format "YYYY-MM-DDZ" or "YYYY-MM-DD"
    static QDate parseDate(const QString& dateStr);
    
    // Format date in DD.MM.YYYY format
    static QString formatDate(const QDate& date);
};

#endif // AIRED_H
