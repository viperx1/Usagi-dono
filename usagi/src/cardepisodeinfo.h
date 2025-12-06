#ifndef CARDEPISODEINFO_H
#define CARDEPISODEINFO_H

#include <QList>
#include "epno.h"
#include "cardfileinfo.h"

/**
 * @class CardEpisodeInfo
 * @brief Represents episode information displayed in anime cards
 * 
 * This class encapsulates all information about an anime episode,
 * including episode number, title, and associated files.
 * 
 * Renamed from EpisodeInfo to CardEpisodeInfo to avoid confusion with AniDB episode info.
 * 
 * SOLID Principles Applied:
 * - Single Responsibility: Only manages episode display information for cards
 * - Encapsulation: Private fields with controlled access
 * - Composition: Contains list of CardFileInfo objects
 */
class CardEpisodeInfo
{
public:
    /**
     * @brief Construct an empty CardEpisodeInfo
     */
    CardEpisodeInfo();
    
    /**
     * @brief Construct a CardEpisodeInfo with basic fields
     * @param eid Episode ID
     * @param episodeNumber Episode number
     * @param episodeTitle Episode title
     */
    CardEpisodeInfo(int eid, const epno& episodeNumber, const QString& episodeTitle);
    
    // Getters
    int eid() const { return m_eid; }
    epno episodeNumber() const { return m_episodeNumber; }
    QString episodeTitle() const { return m_episodeTitle; }
    const QList<CardFileInfo>& files() const { return m_files; }
    QList<CardFileInfo>& files() { return m_files; }
    
    // Setters
    void setEid(int eid) { m_eid = eid; }
    void setEpisodeNumber(const epno& episodeNumber) { m_episodeNumber = episodeNumber; }
    void setEpisodeTitle(const QString& title) { m_episodeTitle = title; }
    void setFiles(const QList<CardFileInfo>& files) { m_files = files; }
    
    /**
     * @brief Add a file to this episode
     * @param fileInfo File information to add
     */
    void addFile(const CardFileInfo& fileInfo);
    
    /**
     * @brief Get number of files for this episode
     * @return File count
     */
    int fileCount() const { return m_files.size(); }
    
    /**
     * @brief Check if episode has any files
     * @return true if files list is not empty
     */
    bool hasFiles() const { return !m_files.isEmpty(); }
    
    /**
     * @brief Check if episode has valid ID
     * @return true if eid is valid
     */
    bool isValid() const;
    
    /**
     * @brief Check if any file in this episode has been watched
     * @return true if at least one file is watched
     */
    bool isWatched() const;
    
    /**
     * @brief Clear all files from this episode
     */
    void clearFiles();
    
    /**
     * @brief Reset to default empty state
     */
    void reset();
    
private:
    int m_eid;                          ///< Episode ID
    epno m_episodeNumber;               ///< Episode number (can be special episodes)
    QString m_episodeTitle;             ///< Episode title
    QList<CardFileInfo> m_files;        ///< Files associated with this episode
};

#endif // CARDEPISODEINFO_H
