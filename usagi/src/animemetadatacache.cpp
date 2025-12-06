#include "animemetadatacache.h"

AnimeMetadataCache::AnimeMetadataCache()
{
}

void AnimeMetadataCache::addAnime(int aid, const QStringList& titles)
{
    if (aid <= 0) {
        return;
    }
    
    m_titleCache[aid] = titles;
}

QStringList AnimeMetadataCache::getTitles(int aid) const
{
    return m_titleCache.value(aid, QStringList());
}

bool AnimeMetadataCache::matchesAnyTitle(int aid, const QString& searchText) const
{
    if (searchText.isEmpty()) {
        return true; // Empty search matches everything
    }
    
    QStringList titles = getTitles(aid);
    if (titles.isEmpty()) {
        return false;
    }
    
    // Case-insensitive search
    QString lowerSearch = searchText.toLower();
    
    for (const QString& title : titles) {
        if (title.toLower().contains(lowerSearch)) {
            return true;
        }
    }
    
    return false;
}

void AnimeMetadataCache::removeAnime(int aid)
{
    m_titleCache.remove(aid);
}

void AnimeMetadataCache::clear()
{
    m_titleCache.clear();
}

bool AnimeMetadataCache::contains(int aid) const
{
    return m_titleCache.contains(aid);
}

int AnimeMetadataCache::size() const
{
    return m_titleCache.size();
}

QList<int> AnimeMetadataCache::animeIds() const
{
    return m_titleCache.keys();
}
