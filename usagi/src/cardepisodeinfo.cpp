#include "cardepisodeinfo.h"

CardEpisodeInfo::CardEpisodeInfo()
    : m_eid(0)
    , m_episodeWatched(false)
{
}

CardEpisodeInfo::CardEpisodeInfo(int eid, const epno& episodeNumber, const QString& episodeTitle)
    : m_eid(eid)
    , m_episodeNumber(episodeNumber)
    , m_episodeTitle(episodeTitle)
    , m_episodeWatched(false)
{
}

void CardEpisodeInfo::addFile(const CardFileInfo& fileInfo)
{
    m_files.append(fileInfo);
}

bool CardEpisodeInfo::isValid() const
{
    return m_eid > 0;
}

bool CardEpisodeInfo::isWatched() const
{
    // Episode is watched if either:
    // 1. Episode is marked as watched at episode level (persists across file replacements)
    // 2. At least one file is watched
    if (m_episodeWatched) {
        return true;
    }
    
    for (const CardFileInfo& file : m_files) {
        if (file.isWatched()) {
            return true;
        }
    }
    return false;
}

void CardEpisodeInfo::clearFiles()
{
    m_files.clear();
}

void CardEpisodeInfo::reset()
{
    m_eid = 0;
    m_episodeNumber = epno();
    m_episodeTitle.clear();
    m_files.clear();
    m_episodeWatched = false;
}
