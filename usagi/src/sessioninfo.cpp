#include "sessioninfo.h"

SessionInfo::SessionInfo()
    : m_aid(0)
    , m_startAid(0)
    , m_currentEpisode(0)
    , m_isActive(false)
{
}

SessionInfo::SessionInfo(int aid, int startAid)
    : m_aid(aid)
    , m_startAid(startAid)
    , m_currentEpisode(0)
    , m_isActive(false)
{
}

void SessionInfo::setCurrentEpisode(int episode)
{
    if (episode >= 0) {
        m_currentEpisode = episode;
    }
}

bool SessionInfo::isEpisodeWatched(int episode) const
{
    return m_watchedEpisodes.contains(episode);
}

void SessionInfo::markEpisodeWatched(int episode)
{
    if (episode > 0) {
        m_watchedEpisodes.insert(episode);
    }
}

void SessionInfo::unmarkEpisodeWatched(int episode)
{
    m_watchedEpisodes.remove(episode);
}

void SessionInfo::start(int startEpisode)
{
    m_isActive = true;
    m_currentEpisode = startEpisode;
}

void SessionInfo::pause()
{
    m_isActive = false;
}

void SessionInfo::resume()
{
    m_isActive = true;
}

void SessionInfo::end()
{
    m_isActive = false;
    m_currentEpisode = 0;
    m_watchedEpisodes.clear();
}

bool SessionInfo::advanceToNextEpisode()
{
    // Note: Episode 0 is allowed as a special state (e.g., OVA, Special)
    // Advancing from 0 to 1 is valid for starting a new session
    if (m_currentEpisode >= 0) {
        m_currentEpisode++;
        return true;
    }
    return false;
}

void SessionInfo::reset()
{
    m_aid = 0;
    m_startAid = 0;
    m_currentEpisode = 0;
    m_isActive = false;
    m_watchedEpisodes.clear();
}
