#include "taginfo.h"

TagInfo::TagInfo()
    : m_id(0)
    , m_weight(0)
{
}

TagInfo::TagInfo(const QString& name, int id, int weight)
    : m_name(name)
    , m_id(id)
    , m_weight(weight)
{
}

bool TagInfo::isValid() const
{
    return !m_name.isEmpty() && m_id > 0;
}

void TagInfo::reset()
{
    m_name.clear();
    m_id = 0;
    m_weight = 0;
}
