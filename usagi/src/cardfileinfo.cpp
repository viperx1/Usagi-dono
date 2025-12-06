#include "cardfileinfo.h"

CardFileInfo::CardFileInfo()
    : m_lid(0)
    , m_fid(0)
    , m_viewed(false)
    , m_localWatched(false)
    , m_lastPlayed(0)
    , m_version(0)
    , m_markType(FileMarkType::None)
{
}

CardFileInfo::CardFileInfo(int lid, int fid, const QString& fileName)
    : m_lid(lid)
    , m_fid(fid)
    , m_fileName(fileName)
    , m_viewed(false)
    , m_localWatched(false)
    , m_lastPlayed(0)
    , m_version(0)
    , m_markType(FileMarkType::None)
{
}

bool CardFileInfo::isValid() const
{
    return m_lid > 0 && m_fid > 0;
}

void CardFileInfo::reset()
{
    m_lid = 0;
    m_fid = 0;
    m_fileName.clear();
    m_state.clear();
    m_viewed = false;
    m_localWatched = false;
    m_storage.clear();
    m_localFilePath.clear();
    m_lastPlayed = 0;
    m_resolution.clear();
    m_quality.clear();
    m_groupName.clear();
    m_version = 0;
    m_markType = FileMarkType::None;
}
