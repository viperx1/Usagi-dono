#ifndef FILEHASHINFO_H
#define FILEHASHINFO_H

#include <QString>

/**
 * @class FileHashInfo
 * @brief Represents information about a file's hash and binding status
 * 
 * This class encapsulates file hash information retrieved from batch operations
 * including the file path, ED2K hash, and various status flags related to
 * AniDB identification and binding.
 * 
 * Status codes:
 * - 0: Unknown/not processed
 * - 1-2: Processing states
 * - 3+: Identified in AniDB
 * 
 * Binding status codes:
 * - 0: Not bound
 * - 1+: Bound to anime
 * 
 * SOLID Principles Applied:
 * - Single Responsibility: Only manages file hash and status information
 * - Encapsulation: All fields are private with controlled access
 * - Type Safety: Proper validation of hash format
 */
class FileHashInfo
{
public:
    /**
     * @brief Construct an empty FileHashInfo object
     */
    FileHashInfo();
    
    /**
     * @brief Construct a FileHashInfo with all fields
     * @param path File path
     * @param hash ED2K hash (hexadecimal string)
     * @param status File status code (0=unknown, 3+=identified)
     * @param bindingStatus Binding status code (0=not bound, 1+=bound)
     */
    FileHashInfo(const QString& path, const QString& hash, int status, int bindingStatus);
    
    // Getters
    QString path() const { return m_path; }
    QString hash() const { return m_hash; }
    int status() const { return m_status; }
    int bindingStatus() const { return m_bindingStatus; }
    
    // Setters with validation
    void setPath(const QString& path);
    void setHash(const QString& hash);
    void setStatus(int status) { m_status = status; }
    void setBindingStatus(int bindingStatus) { m_bindingStatus = bindingStatus; }
    
    /**
     * @brief Check if this object has valid data
     * @return true if path and hash are not empty
     */
    bool isValid() const;
    
    /**
     * @brief Check if the hash is properly formatted
     * @return true if hash is a valid hexadecimal string
     */
    bool hasValidHash() const;
    
    /**
     * @brief Check if file has been identified in AniDB (status >= 3)
     * @return true if file has been identified in AniDB
     */
    bool isIdentified() const { return m_status >= 3; }
    
    /**
     * @brief Check if file is bound to an anime (bindingStatus > 0)
     * @return true if binding status indicates bound
     */
    bool isBound() const { return m_bindingStatus > 0; }
    
    /**
     * @brief Reset to default empty state
     */
    void reset();
    
private:
    static constexpr int ED2K_HASH_LENGTH = 32;  ///< ED2K hash length in hex characters
    
    QString m_path;           ///< File path
    QString m_hash;           ///< ED2K hash (hexadecimal)
    int m_status;             ///< File status code (0=unknown, 3+=identified)
    int m_bindingStatus;      ///< Binding status code (0=not bound, 1+=bound)
    
    /**
     * @brief Validate hash format (hexadecimal)
     * @param hash Hash string to validate
     * @return true if hash is valid hexadecimal
     */
    static bool isValidHexHash(const QString& hash);
};

#endif // FILEHASHINFO_H
