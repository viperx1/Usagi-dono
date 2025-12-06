#ifndef TRUNCATEDRESPONSEINFO_H
#define TRUNCATEDRESPONSEINFO_H

#include <QString>

/**
 * @class TruncatedResponseInfo
 * @brief Manages the state of truncated AniDB API responses that require multiple parts
 * 
 * This class encapsulates the logic for handling multi-part responses from the AniDB API.
 * When a response is too large to fit in a single UDP packet, the API sends it in chunks
 * with a truncation flag. This class tracks the state across multiple response parts.
 * 
 * SOLID Principles Applied:
 * - Single Responsibility: Only manages truncated response state
 * - Encapsulation: All fields are private with controlled access
 * - Open/Closed: Easy to extend with new fields without breaking existing code
 */
class TruncatedResponseInfo
{
public:
    /**
     * @brief Construct a new TruncatedResponseInfo object in non-truncated state
     */
    TruncatedResponseInfo();
    
    /**
     * @brief Check if currently handling a truncated response
     * @return true if response is truncated and awaiting continuation
     */
    bool isTruncated() const { return m_isTruncated; }
    
    /**
     * @brief Get the tag associated with this truncated response
     * @return Tag string used to match continuation packets
     */
    QString tag() const { return m_tag; }
    
    /**
     * @brief Get the original command that produced this truncated response
     * @return Command string for reference
     */
    QString command() const { return m_command; }
    
    /**
     * @brief Get the number of fields successfully parsed so far
     * @return Field count
     */
    int fieldsParsed() const { return m_fieldsParsed; }
    
    /**
     * @brief Get the file mask received in the response
     * @return File mask bits
     */
    unsigned int fmaskReceived() const { return m_fmaskReceived; }
    
    /**
     * @brief Get the anime mask received in the response
     * @return Anime mask bits
     */
    unsigned int amaskReceived() const { return m_amaskReceived; }
    
    /**
     * @brief Start tracking a new truncated response
     * @param tag Response tag for matching continuation
     * @param command Original command that was sent
     */
    void beginTruncatedResponse(const QString& tag, const QString& command);
    
    /**
     * @brief Update the parsing progress
     * @param fieldsParsed Number of fields successfully parsed
     * @param fmask File mask bits from response
     * @param amask Anime mask bits from response
     */
    void updateProgress(int fieldsParsed, unsigned int fmask, unsigned int amask);
    
    /**
     * @brief Reset to non-truncated state (response complete or cancelled)
     */
    void reset();
    
    /**
     * @brief Check if this object is in a valid state
     * @return true if state is consistent
     */
    bool isValid() const;
    
private:
    bool m_isTruncated;           ///< Whether currently handling truncated response
    QString m_tag;                 ///< Tag for matching continuation packets
    QString m_command;             ///< Original command that was sent
    int m_fieldsParsed;            ///< Number of fields parsed so far
    unsigned int m_fmaskReceived;  ///< File mask from response
    unsigned int m_amaskReceived;  ///< Anime mask from response
};

#endif // TRUNCATEDRESPONSEINFO_H
