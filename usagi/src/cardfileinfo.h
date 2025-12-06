#ifndef CARDFILEINFO_H
#define CARDFILEINFO_H

#include <QString>
#include "filemarkinfo.h"  // For FileMarkType enum

/**
 * @class CardFileInfo
 * @brief Represents file information displayed in anime cards
 * 
 * This class encapsulates all information about a file associated with an anime episode,
 * including viewing status, local file tracking, quality information, and file marking.
 * 
 * Renamed from FileInfo to CardFileInfo to avoid confusion with other file info classes.
 * 
 * SOLID Principles Applied:
 * - Single Responsibility: Only manages file display information for cards
 * - Encapsulation: Private fields with controlled access
 * - Type Safety: Proper types instead of all strings
 */
class CardFileInfo
{
public:
    /**
     * @brief Construct an empty CardFileInfo
     */
    CardFileInfo();
    
    /**
     * @brief Construct a CardFileInfo with basic fields
     * @param lid MyList ID
     * @param fid File ID
     * @param fileName File name
     */
    CardFileInfo(int lid, int fid, const QString& fileName);
    
    // Getters
    int lid() const { return m_lid; }
    int fid() const { return m_fid; }
    QString fileName() const { return m_fileName; }
    QString state() const { return m_state; }
    bool viewed() const { return m_viewed; }
    bool localWatched() const { return m_localWatched; }
    QString storage() const { return m_storage; }
    QString localFilePath() const { return m_localFilePath; }
    qint64 lastPlayed() const { return m_lastPlayed; }
    QString resolution() const { return m_resolution; }
    QString quality() const { return m_quality; }
    QString groupName() const { return m_groupName; }
    int version() const { return m_version; }
    FileMarkType markType() const { return m_markType; }
    
    // Setters
    void setLid(int lid) { m_lid = lid; }
    void setFid(int fid) { m_fid = fid; }
    void setFileName(const QString& fileName) { m_fileName = fileName; }
    void setState(const QString& state) { m_state = state; }
    void setViewed(bool viewed) { m_viewed = viewed; }
    void setLocalWatched(bool localWatched) { m_localWatched = localWatched; }
    void setStorage(const QString& storage) { m_storage = storage; }
    void setLocalFilePath(const QString& path) { m_localFilePath = path; }
    void setLastPlayed(qint64 timestamp) { m_lastPlayed = timestamp; }
    void setResolution(const QString& resolution) { m_resolution = resolution; }
    void setQuality(const QString& quality) { m_quality = quality; }
    void setGroupName(const QString& groupName) { m_groupName = groupName; }
    void setVersion(int version) { m_version = version; }
    void setMarkType(FileMarkType markType) { m_markType = markType; }
    
    /**
     * @brief Check if file has valid IDs
     * @return true if lid and fid are valid
     */
    bool isValid() const;
    
    /**
     * @brief Check if file has been watched (either viewed or localWatched)
     * @return true if watched by any method
     */
    bool isWatched() const { return m_viewed || m_localWatched; }
    
    /**
     * @brief Check if file has local tracking
     * @return true if local file path is set
     */
    bool hasLocalFile() const { return !m_localFilePath.isEmpty(); }
    
    /**
     * @brief Check if file has been marked for action
     * @return true if mark type is not None
     */
    bool isMarked() const { return m_markType != FileMarkType::None; }
    
    /**
     * @brief Reset to default empty state
     */
    void reset();
    
private:
    int m_lid;                      ///< MyList ID
    int m_fid;                      ///< File ID
    QString m_fileName;             ///< File name
    QString m_state;                ///< State string
    bool m_viewed;                  ///< AniDB API watch status (synced from server)
    bool m_localWatched;            ///< Local watch status (chunk-based playback tracking)
    QString m_storage;              ///< Storage location
    QString m_localFilePath;        ///< Path to local file if tracked
    qint64 m_lastPlayed;            ///< Timestamp of last playback session (for resume)
    QString m_resolution;           ///< Video resolution
    QString m_quality;              ///< Quality string
    QString m_groupName;            ///< Release group name
    int m_version;                  ///< File version (1, 2, 3, etc.)
    FileMarkType m_markType;        ///< File marking for download/deletion
};

#endif // CARDFILEINFO_H
