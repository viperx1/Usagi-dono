#ifndef ANIMESTATS_H
#define ANIMESTATS_H

/**
 * @class AnimeStats
 * @brief Statistics about anime episodes and viewing progress
 * 
 * This class encapsulates episode statistics for an anime including
 * total episodes, viewed episodes, and separate tracking for special episodes.
 * 
 * SOLID Principles Applied:
 * - Single Responsibility: Only manages anime statistics
 * - Encapsulation: Private fields with controlled access
 * - Validation: Ensures consistent state
 */
class AnimeStats
{
public:
    /**
     * @brief Construct empty AnimeStats with all counts at zero
     */
    AnimeStats();
    
    /**
     * @brief Construct AnimeStats with all fields
     * @param normalEpisodes Number of normal episodes in mylist
     * @param totalNormalEpisodes Total normal episodes for anime
     * @param normalViewed Number of normal episodes viewed
     * @param otherEpisodes Number of special/other episodes in mylist
     * @param otherViewed Number of special/other episodes viewed
     */
    AnimeStats(int normalEpisodes, int totalNormalEpisodes, int normalViewed,
               int otherEpisodes, int otherViewed);
    
    // Getters
    int normalEpisodes() const { return m_normalEpisodes; }
    int totalNormalEpisodes() const { return m_totalNormalEpisodes; }
    int normalViewed() const { return m_normalViewed; }
    int otherEpisodes() const { return m_otherEpisodes; }
    int otherViewed() const { return m_otherViewed; }
    
    // Setters with validation
    void setNormalEpisodes(int count);
    void setTotalNormalEpisodes(int count);
    void setNormalViewed(int count);
    void setOtherEpisodes(int count);
    void setOtherViewed(int count);
    
    /**
     * @brief Get total episode count (normal + other)
     * @return Total episode count
     */
    int totalEpisodes() const { return m_normalEpisodes + m_otherEpisodes; }
    
    /**
     * @brief Get total viewed count (normal + other)
     * @return Total viewed count
     */
    int totalViewed() const { return m_normalViewed + m_otherViewed; }
    
    /**
     * @brief Check if all episodes have been viewed
     * @return true if all episodes are viewed
     */
    bool isComplete() const;
    
    /**
     * @brief Get completion percentage for normal episodes
     * @return Percentage (0-100)
     */
    double normalCompletionPercent() const;
    
    /**
     * @brief Check if stats are valid (no inconsistencies)
     * @return true if viewed counts don't exceed totals
     */
    bool isValid() const;
    
    /**
     * @brief Reset all counts to zero
     */
    void reset();
    
private:
    int m_normalEpisodes;        ///< Number of normal episodes in mylist
    int m_totalNormalEpisodes;   ///< Total normal episodes for anime
    int m_normalViewed;          ///< Number of normal episodes viewed
    int m_otherEpisodes;         ///< Number of special/other episodes in mylist
    int m_otherViewed;           ///< Number of special/other episodes viewed
};

#endif // ANIMESTATS_H
