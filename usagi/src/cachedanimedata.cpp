#include "cachedanimedata.h"

CachedAnimeData::CachedAnimeData()
    : m_isHidden(false)
    , m_is18Restricted(false)
    , m_eptotal(0)
    , m_lastPlayed(0)
    , m_hasData(false)
{
}

CachedAnimeData::CachedAnimeData(const QString& animeName, const QString& typeName,
                                 const QString& startDate, const QString& endDate,
                                 bool isHidden, bool is18Restricted, int eptotal,
                                 const AnimeStats& stats, qint64 lastPlayed)
    : m_animeName(animeName)
    , m_typeName(typeName)
    , m_startDate(startDate)
    , m_endDate(endDate)
    , m_isHidden(isHidden)
    , m_is18Restricted(is18Restricted)
    , m_eptotal(eptotal)
    , m_stats(stats)
    , m_lastPlayed(lastPlayed)
    , m_hasData(true)
{
}

bool CachedAnimeData::isValid() const
{
    return m_hasData && !m_animeName.isEmpty();
}

bool CachedAnimeData::isAiring() const
{
    // If end date is empty or in future, anime is still airing
    // This is a simple check - could be enhanced with actual date parsing
    return m_endDate.isEmpty();
}

double CachedAnimeData::completionPercent() const
{
    return m_stats.normalCompletionPercent();
}

void CachedAnimeData::reset()
{
    m_animeName.clear();
    m_typeName.clear();
    m_startDate.clear();
    m_endDate.clear();
    m_isHidden = false;
    m_is18Restricted = false;
    m_eptotal = 0;
    m_stats.reset();
    m_lastPlayed = 0;
    m_hasData = false;
}
