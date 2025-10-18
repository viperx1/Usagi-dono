#ifndef EPNO_H
#define EPNO_H

#include <QString>

/**
 * @brief Episode number type that properly handles sorting by type and numeric value
 * 
 * This class represents an episode number with its type (regular, special, credit, etc.)
 * and provides proper comparison operators for sorting.
 * 
 * Episode types:
 * 1 = regular episode (no prefix)
 * 2 = special ("S" prefix)
 * 3 = credit ("C" prefix)
 * 4 = trailer ("T" prefix)
 * 5 = parody ("P" prefix)
 * 6 = other ("O" prefix)
 */
class epno
{
public:
    // Constructors
    epno();
    epno(const QString& epnoString);
    epno(int type, int number);
    
    // Getters
    int type() const { return m_type; }
    int number() const { return m_number; }
    QString toString() const { return m_rawString; }
    
    // Display string (without leading zeros and with type prefix for specials)
    QString toDisplayString() const;
    
    // Comparison operators for sorting
    bool operator<(const epno& other) const;
    bool operator>(const epno& other) const;
    bool operator==(const epno& other) const;
    bool operator!=(const epno& other) const;
    bool operator<=(const epno& other) const;
    bool operator>=(const epno& other) const;
    
    // Conversion from/to QString
    static epno fromString(const QString& str);
    
    // Check if this is a valid episode number
    bool isValid() const { return m_type > 0 && m_number >= 0; }

private:
    int m_type;          // Episode type (1-6)
    int m_number;        // Numeric episode number (without prefix or leading zeros)
    QString m_rawString; // Original string from database/API
    
    // Parse the episode string to extract type and number
    void parse(const QString& str);
};

#endif // EPNO_H
