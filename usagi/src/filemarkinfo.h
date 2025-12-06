#ifndef FILEMARKINFO_H
#define FILEMARKINFO_H

/**
 * @brief Represents how a file is marked for queue management
 */
enum class FileMarkType {
    None = 0,           // No special marking
    ForDownload = 1,    // Marked for download (priority)
    ForDeletion = 2     // Marked for soft deletion (can be removed when space needed)
};

/**
 * @brief FileMarkInfo - Encapsulates information about a file's marking status
 * 
 * This class replaces the FileMarkInfo struct with:
 * - Proper encapsulation of file marking data
 * - Validation of data
 * - Clear interface for querying and updating mark status
 * 
 * Follows SOLID principles:
 * - Single Responsibility: Represents file marking status only
 * - Encapsulation: Private members with validated setters
 * - Type Safety: Proper types and enums
 * 
 * Usage:
 *   FileMarkInfo markInfo;
 *   markInfo.setLid(123);
 *   markInfo.setMarkType(FileMarkType::ForDeletion);
 *   markInfo.setMarkScore(500);
 *   if (markInfo.isMarkedForDeletion()) {
 *       // Handle deletion
 *   }
 */
class FileMarkInfo
{
public:
    /**
     * @brief Default constructor creates unmarked file info
     */
    FileMarkInfo();
    
    /**
     * @brief Construct file mark info with IDs
     * @param lid MyList ID
     * @param aid Anime ID
     */
    FileMarkInfo(int lid, int aid);
    
    // Core identification
    int lid() const { return m_lid; }
    int aid() const { return m_aid; }
    
    void setLid(int lid) { m_lid = lid; }
    void setAid(int aid) { m_aid = aid; }
    
    // Mark type and score
    FileMarkType markType() const { return m_markType; }
    int markScore() const { return m_markScore; }
    
    void setMarkType(FileMarkType type) { m_markType = type; }
    void setMarkScore(int score) { m_markScore = score; }
    
    // File status
    bool hasLocalFile() const { return m_hasLocalFile; }
    bool isWatched() const { return m_isWatched; }
    bool isInActiveSession() const { return m_isInActiveSession; }
    
    void setHasLocalFile(bool has) { m_hasLocalFile = has; }
    void setIsWatched(bool watched) { m_isWatched = watched; }
    void setIsInActiveSession(bool inSession) { m_isInActiveSession = inSession; }
    
    // Convenience methods
    bool isMarkedForDeletion() const { return m_markType == FileMarkType::ForDeletion; }
    bool isMarkedForDownload() const { return m_markType == FileMarkType::ForDownload; }
    bool isUnmarked() const { return m_markType == FileMarkType::None; }
    
    // Validation
    bool isValid() const { return m_lid > 0 && m_aid > 0; }
    
    // Reset to default state
    void reset();
    
private:
    int m_lid;                      // MyList ID
    int m_aid;                      // Anime ID
    FileMarkType m_markType;        // Current marking
    int m_markScore;                // Calculated score for deletion priority (higher = more likely to delete)
    bool m_hasLocalFile;            // Whether file exists locally
    bool m_isWatched;               // Whether file has been watched (local_watched)
    bool m_isInActiveSession;       // Whether this file's anime has an active session
};

#endif // FILEMARKINFO_H
