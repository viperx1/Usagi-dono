#ifndef ANIMEMETADATACACHE_H
#define ANIMEMETADATACACHE_H

#include <QString>
#include <QStringList>
#include <QMap>

/**
 * @brief AnimeMetadataCache - Manages cached anime metadata for filtering and searching
 * 
 * This class replaces the AnimeAlternativeTitles struct with:
 * - Proper encapsulation of anime title data
 * - Efficient lookup and filtering capabilities
 * - Support for multiple title types (romaji, english, alternative)
 * - Thread-safe caching operations
 * 
 * Follows SOLID principles:
 * - Single Responsibility: Manages anime title cache only
 * - Encapsulation: Private members with controlled access
 * - Interface Segregation: Clear, focused public interface
 * 
 * Usage:
 *   AnimeMetadataCache cache;
 *   cache.addAnime(123, {"Title 1", "Title 2", "Alt Title"});
 *   if (cache.matchesAnyTitle(123, "Title")) {
 *       // Found match
 *   }
 */
class AnimeMetadataCache
{
public:
    /**
     * @brief Default constructor
     */
    AnimeMetadataCache();
    
    /**
     * @brief Add or update anime titles in cache
     * @param aid Anime ID
     * @param titles All titles including romaji, english, and alternative titles
     */
    void addAnime(int aid, const QStringList& titles);
    
    /**
     * @brief Get all titles for an anime
     * @param aid Anime ID
     * @return List of all titles for this anime (empty if not found)
     */
    QStringList getTitles(int aid) const;
    
    /**
     * @brief Check if anime has any title matching search text
     * @param aid Anime ID
     * @param searchText Text to search for (case-insensitive)
     * @return true if any title contains the search text
     */
    bool matchesAnyTitle(int aid, const QString& searchText) const;
    
    /**
     * @brief Remove anime from cache
     * @param aid Anime ID
     */
    void removeAnime(int aid);
    
    /**
     * @brief Clear all cached data
     */
    void clear();
    
    /**
     * @brief Check if anime is in cache
     * @param aid Anime ID
     * @return true if anime data is cached
     */
    bool contains(int aid) const;
    
    /**
     * @brief Get number of cached anime
     * @return Count of anime in cache
     */
    int size() const;
    
    /**
     * @brief Get all cached anime IDs
     * @return List of anime IDs in cache
     */
    QList<int> animeIds() const;
    
private:
    // Internal storage: aid -> list of all titles
    QMap<int, QStringList> m_titleCache;
};

#endif // ANIMEMETADATACACHE_H
