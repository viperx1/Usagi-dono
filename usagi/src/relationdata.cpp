#include "relationdata.h"
#include <QStringList>

void RelationData::setRelations(const QString& aidList, const QString& typeList)
{
    m_relaidlist = aidList;
    m_relaidtype = typeList;
    m_cacheParsed = false;  // Invalidate cache
    m_relationCache.clear();
}

void RelationData::parseRelations() const
{
    if (m_cacheParsed) {
        return;
    }
    
    m_relationCache.clear();
    
    if (m_relaidlist.isEmpty() || m_relaidtype.isEmpty()) {
        m_cacheParsed = true;
        return;
    }
    
    QStringList aidList = m_relaidlist.split('\'', Qt::SkipEmptyParts);
    QStringList typeList = m_relaidtype.split('\'', Qt::SkipEmptyParts);
    
    int count = qMin(aidList.size(), typeList.size());
    for (int i = 0; i < count; ++i) {
        bool aidOk = false;
        bool typeOk = false;
        int aid = aidList[i].toInt(&aidOk);
        int type = typeList[i].toInt(&typeOk);
        
        if (aidOk && typeOk && aid > 0) {
            m_relationCache[aid] = static_cast<RelationType>(type);
        }
    }
    
    m_cacheParsed = true;
}

int RelationData::getPrequel() const
{
    parseRelations();
    
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
    parseRelations();
    
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
    parseRelations();
    
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
    parseRelations();
    return m_relationCache;
}

bool RelationData::hasRelations() const
{
    parseRelations();
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
    m_cacheParsed = false;
}
