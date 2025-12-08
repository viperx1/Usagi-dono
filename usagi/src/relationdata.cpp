#include "relationdata.h"
#include <QStringList>

void RelationData::setRelations(const QString& aidList, const QString& typeList)
{
    m_relaidlist = aidList;
    m_relaidtype = typeList;
    m_relationCache.clear();
    
    // Parse immediately (merged parseRelations logic)
    if (m_relaidlist.isEmpty() || m_relaidtype.isEmpty()) {
        return;
    }
    
    QStringList aidStrList = m_relaidlist.split('\'', Qt::SkipEmptyParts);
    QStringList typeStrList = m_relaidtype.split('\'', Qt::SkipEmptyParts);
    
    int count = qMin(aidStrList.size(), typeStrList.size());
    for (int i = 0; i < count; ++i) {
        bool aidOk = false;
        bool typeOk = false;
        int aid = aidStrList[i].toInt(&aidOk);
        int type = typeStrList[i].toInt(&typeOk);
        
        if (aidOk && typeOk && aid > 0) {
            m_relationCache[aid] = static_cast<RelationType>(type);
        }
    }
}

int RelationData::getPrequel() const
{
    // Find first prequel (type 2)
    for (auto it = m_relationCache.constBegin(); it != m_relationCache.constEnd(); ++it) {
        if (it.value() == RelationType::Prequel) {
            return it.key();
        }
    }
    
    return 0;
}

int RelationData::getSequel() const
{
    // Find first sequel (type 1)
    for (auto it = m_relationCache.constBegin(); it != m_relationCache.constEnd(); ++it) {
        if (it.value() == RelationType::Sequel) {
            return it.key();
        }
    }
    
    return 0;
}

QList<int> RelationData::getRelatedAnimeByType(RelationType type) const
{
    QList<int> result;
    for (auto it = m_relationCache.constBegin(); it != m_relationCache.constEnd(); ++it) {
        if (it.value() == type) {
            result.append(it.key());
        }
    }
    
    return result;
}

QMap<int, RelationData::RelationType> RelationData::getAllRelations() const
{
    return m_relationCache;
}

bool RelationData::hasRelations() const
{
    return !m_relationCache.isEmpty();
}

bool RelationData::hasPrequel() const
{
    return getPrequel() != 0;
}

bool RelationData::hasSequel() const
{
    return getSequel() != 0;
}

void RelationData::clear()
{
    m_relaidlist.clear();
    m_relaidtype.clear();
    m_relationCache.clear();
}
