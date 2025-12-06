#include "cardepisodeinfo.h"

CardEpisodeInfo::CardEpisodeInfo()
    : m_eid(0)
{
}

CardEpisodeInfo::CardEpisodeInfo(int eid, const epno& episodeNumber, const QString& episodeTitle)
    : m_eid(eid)
    , m_episodeNumber(episodeNumber)
    , m_episodeTitle(episodeTitle)
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
}
