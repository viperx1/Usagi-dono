#include "filemarkinfo.h"

FileMarkInfo::FileMarkInfo()
    : m_lid(0)
    , m_aid(0)
    , m_markType(FileMarkType::None)
    , m_markScore(0)
    , m_hasLocalFile(false)
    , m_isWatched(false)
    , m_isInActiveSession(false)
{
}

FileMarkInfo::FileMarkInfo(int lid, int aid)
    : m_lid(lid)
    , m_aid(aid)
    , m_markType(FileMarkType::None)
    , m_markScore(0)
    , m_hasLocalFile(false)
    , m_isWatched(false)
    , m_isInActiveSession(false)
{
}

void FileMarkInfo::reset()
{
    m_lid = 0;
    m_aid = 0;
    m_markType = FileMarkType::None;
    m_markScore = 0;
    m_hasLocalFile = false;
    m_isWatched = false;
    m_isInActiveSession = false;
}
