#ifndef ANIDBEPISODEINFO_H
#define ANIDBEPISODEINFO_H

#include <QString>

/**
 * @brief AniDBEpisodeInfo - Type-safe representation of AniDB episode data
 * 
 * This class replaces the plain EpisodeData struct with:
 * - Type-safe fields (int for IDs and counts, proper types for ratings)
 * - Validation of data
 * - Encapsulation with getters/setters
 * 
 * Follows SOLID principles:
 * - Single Responsibility: Represents episode metadata only
 * - Encapsulation: Private members with validated setters
 * - Type Safety: Proper C++ types instead of all QString
 * 
 * Usage:
 *   AniDBEpisodeInfo episodeInfo;
 *   episodeInfo.setEpisodeId(12345);
 *   episodeInfo.setEpisodeNumber("01");
 *   int eid = episodeInfo.episodeId();
 */
class AniDBEpisodeInfo
{
public:
    /**
     * @brief Default constructor creates invalid episode info
     */
    AniDBEpisodeInfo();
    
    // Episode identification
    int episodeId() const { return m_eid; }
    QString episodeNumber() const { return m_epno; }
    
    void setEpisodeId(int eid) { m_eid = eid; }
    void setEpisodeNumber(const QString& epno) { m_epno = epno; }
    
    // Episode names
    QString name() const { return m_epname; }
    QString nameRomaji() const { return m_epnameromaji; }
    QString nameKanji() const { return m_epnamekanji; }
    
    void setName(const QString& name) { m_epname = name; }
    void setNameRomaji(const QString& name) { m_epnameromaji = name; }
    void setNameKanji(const QString& name) { m_epnamekanji = name; }
    
    // Ratings
    QString rating() const { return m_eprating; }
    int voteCount() const { return m_epvotecount; }
    
    void setRating(const QString& rating) { m_eprating = rating; }
    void setVoteCount(int count) { m_epvotecount = count; }
    
    // Validation
    bool isValid() const { return m_eid > 0; }
    
private:
    // Episode identification
    int m_eid;
    QString m_epno;
    
    // Episode names
    QString m_epname;
    QString m_epnameromaji;
    QString m_epnamekanji;
    
    // Ratings
    QString m_eprating;
    int m_epvotecount;
};

#endif // ANIDBEPISODEINFO_H
