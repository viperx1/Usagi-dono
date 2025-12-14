#ifndef CACHEDANIMEDATA_H
#define CACHEDANIMEDATA_H

#include <QString>
#include "animestats.h"

/**
 * @class CachedAnimeData
 * @brief Cached anime information for filtering and sorting without card widgets
 * 
 * This class stores essential anime information that can be used for filtering
 * and sorting operations without needing to create or access card widgets.
 * This is particularly important for virtual scrolling where cards may not exist yet.
 * 
 * SOLID Principles Applied:
 * - Single Responsibility: Only manages cached anime display data
 * - Encapsulation: Private fields with controlled access
 * - Composition: Uses AnimeStats for episode statistics
 */
class CachedAnimeData
{
public:
    /**
     * @brief Construct empty CachedAnimeData with no data
     */
    CachedAnimeData();
    
    /**
     * @brief Construct CachedAnimeData with all fields
     */
    CachedAnimeData(const QString& animeName, const QString& typeName,
                    const QString& startDate, const QString& endDate,
                    bool isHidden, bool is18Restricted, int eptotal,
                    const AnimeStats& stats, qint64 lastPlayed);
    
    // Getters
    QString animeName() const { return m_animeName; }
    QString typeName() const { return m_typeName; }
    QString startDate() const { return m_startDate; }
    QString endDate() const { return m_endDate; }
    bool isHidden() const { return m_isHidden; }
    bool is18Restricted() const { return m_is18Restricted; }
    int eptotal() const { return m_eptotal; }
    const AnimeStats& stats() const { return m_stats; }
    qint64 lastPlayed() const { return m_lastPlayed; }
    qint64 recentEpisodeAirDate() const { return m_recentEpisodeAirDate; }
    bool hasData() const { return m_hasData; }
    
    // Setters
    void setAnimeName(const QString& name) { m_animeName = name; m_hasData = true; }
    void setTypeName(const QString& type) { m_typeName = type; }
    void setStartDate(const QString& date) { m_startDate = date; }
    void setEndDate(const QString& date) { m_endDate = date; }
    void setIsHidden(bool hidden) { m_isHidden = hidden; }
    void setIs18Restricted(bool restricted) { m_is18Restricted = restricted; }
    void setEptotal(int total) { m_eptotal = total; }
    void setStats(const AnimeStats& stats) { m_stats = stats; }
    void setLastPlayed(qint64 timestamp) { m_lastPlayed = timestamp; }
    void setRecentEpisodeAirDate(qint64 timestamp) { m_recentEpisodeAirDate = timestamp; }
    void setHasData(bool hasData) { m_hasData = hasData; }
    
    /**
     * @brief Check if this anime data is valid
     * @return true if has data and anime name is not empty
     */
    bool isValid() const;
    
    /**
     * @brief Check if anime is currently airing (based on end date)
     * @return true if end date is empty or in future
     */
    bool isAiring() const;
    
    /**
     * @brief Check if anime has been played
     * @return true if lastPlayed timestamp is set
     */
    bool hasBeenPlayed() const { return m_lastPlayed > 0; }
    
    /**
     * @brief Get completion percentage
     * @return Percentage of episodes viewed (0-100)
     */
    double completionPercent() const;
    
    /**
     * @brief Reset to empty state
     */
    void reset();
    
private:
    QString m_animeName;        ///< The resolved display name
    QString m_typeName;         ///< Anime type (TV, Movie, OVA, etc.)
    QString m_startDate;        ///< Start air date
    QString m_endDate;          ///< End air date
    bool m_isHidden;            ///< Whether anime is marked as hidden
    bool m_is18Restricted;      ///< Whether anime is 18+ restricted
    int m_eptotal;              ///< Total episode count from anime data
    AnimeStats m_stats;         ///< Episode statistics
    qint64 m_lastPlayed;        ///< Most recent play timestamp from episodes
    qint64 m_recentEpisodeAirDate; ///< Most recent episode air date from files
    bool m_hasData;             ///< Whether this cache entry has been populated
};

#endif // CACHEDANIMEDATA_H
