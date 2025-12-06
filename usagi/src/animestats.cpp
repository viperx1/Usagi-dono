#include "animestats.h"
#include <algorithm>

AnimeStats::AnimeStats()
    : m_normalEpisodes(0)
    , m_totalNormalEpisodes(0)
    , m_normalViewed(0)
    , m_otherEpisodes(0)
    , m_otherViewed(0)
{
}

AnimeStats::AnimeStats(int normalEpisodes, int totalNormalEpisodes, int normalViewed,
                       int otherEpisodes, int otherViewed)
    : m_normalEpisodes(std::max(0, normalEpisodes))
    , m_totalNormalEpisodes(std::max(0, totalNormalEpisodes))
    , m_normalViewed(std::max(0, normalViewed))
    , m_otherEpisodes(std::max(0, otherEpisodes))
    , m_otherViewed(std::max(0, otherViewed))
{
}

void AnimeStats::setNormalEpisodes(int count)
{
    m_normalEpisodes = std::max(0, count);
}

void AnimeStats::setTotalNormalEpisodes(int count)
{
    m_totalNormalEpisodes = std::max(0, count);
}

void AnimeStats::setNormalViewed(int count)
{
    m_normalViewed = std::max(0, count);
}

void AnimeStats::setOtherEpisodes(int count)
{
    m_otherEpisodes = std::max(0, count);
}

void AnimeStats::setOtherViewed(int count)
{
    m_otherViewed = std::max(0, count);
}

bool AnimeStats::isComplete() const
{
    // Complete if all normal episodes and other episodes are viewed
    return m_normalViewed >= m_normalEpisodes && m_otherViewed >= m_otherEpisodes;
}

double AnimeStats::normalCompletionPercent() const
{
    if (m_normalEpisodes == 0) {
        return 0.0;
    }
    return (static_cast<double>(m_normalViewed) / m_normalEpisodes) * 100.0;
}

bool AnimeStats::isValid() const
{
    // Viewed counts should not exceed episode counts
    return m_normalViewed <= m_normalEpisodes && m_otherViewed <= m_otherEpisodes;
}

void AnimeStats::reset()
{
    m_normalEpisodes = 0;
    m_totalNormalEpisodes = 0;
    m_normalViewed = 0;
    m_otherEpisodes = 0;
    m_otherViewed = 0;
}
