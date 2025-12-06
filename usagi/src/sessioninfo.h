#ifndef SESSIONINFO_H
#define SESSIONINFO_H

#include <QSet>

/**
 * @brief SessionInfo - Encapsulates watch session data for an anime
 * 
 * This class replaces the SessionInfo struct with:
 * - Proper encapsulation of session state
 * - Validation of data
 * - Methods for session lifecycle management
 * - State change validation
 * 
 * Follows SOLID principles:
 * - Single Responsibility: Manages session state only
 * - Encapsulation: Private members with validated setters
 * - Type Safety: Proper validation and state management
 * 
 * Usage:
 *   SessionInfo session;
 *   session.setAid(123);
 *   session.setActive(true);
 *   session.markEpisodeWatched(1);
 *   if (session.isEpisodeWatched(1)) {
 *       session.advanceToNextEpisode();
 *   }
 */
class SessionInfo
{
public:
    /**
     * @brief Default constructor creates inactive session
     */
    SessionInfo();
    
    /**
     * @brief Construct session for an anime
     * @param aid Anime ID
     * @param startAid First anime in series (prequel)
     */
    SessionInfo(int aid, int startAid);
    
    // Core identification
    int aid() const { return m_aid; }
    int startAid() const { return m_startAid; }
    
    void setAid(int aid) { m_aid = aid; }
    void setStartAid(int startAid) { m_startAid = startAid; }
    
    // Session state
    bool isActive() const { return m_isActive; }
    int currentEpisode() const { return m_currentEpisode; }
    
    void setActive(bool active) { m_isActive = active; }
    void setCurrentEpisode(int episode);
    
    // Episode tracking
    bool isEpisodeWatched(int episode) const;
    void markEpisodeWatched(int episode);
    void unmarkEpisodeWatched(int episode);
    QSet<int> watchedEpisodes() const { return m_watchedEpisodes; }
    int watchedEpisodesCount() const { return m_watchedEpisodes.size(); }
    
    // Session lifecycle methods
    void start(int startEpisode = 0);
    void pause();
    void resume();
    void end();
    bool advanceToNextEpisode();  // Returns true if advanced, false if no more episodes
    
    // Validation
    bool isValid() const { return m_aid > 0; }
    
    // Reset to default state
    void reset();
    
private:
    int m_aid;                      // Anime ID
    int m_startAid;                 // First anime in series (prequel)
    int m_currentEpisode;           // Current episode in session
    bool m_isActive;                // Whether session is active
    QSet<int> m_watchedEpisodes;    // Episodes watched in this session
};

#endif // SESSIONINFO_H
