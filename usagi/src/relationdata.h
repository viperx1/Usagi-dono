#ifndef RELATIONDATA_H
#define RELATIONDATA_H

#include <QString>
#include <QStringList>
#include <QMap>

/**
 * RelationData - Encapsulates anime relation data following SOLID principles
 * 
 * This class provides:
 * - Proper encapsulation of relation data (apostrophe-separated strings from AniDB)
 * - Convenient getters for common relation types (prequel, sequel)
 * - Type-safe access to relation information
 * - Efficient caching of parsed relations
 * 
 * Design principles:
 * - Single Responsibility: Manages only relation data parsing and access
 * - Open/Closed: Can be extended with new relation types without modifying existing code
 * - Encapsulation: Internal data is private, accessed through controlled interfaces
 */
class RelationData
{
public:
    // Relation types as defined by AniDB
    enum class RelationType {
        Sequel = 1,           // This anime is a sequel to the related anime
        Prequel = 2,          // This anime is a prequel to the related anime
        SameSetting = 11,     // Same setting/universe
        AlternativeSetting = 12,  // Alternative setting
        AlternativeVersion = 32,  // Alternative version
        CharacterAnime = 41,  // Character appears in related anime
        SideStory = 51,       // Side story
        ParentStory = 52,     // Parent story
        Summary = 61,         // Summary
        FullStory = 62,       // Full story
        Other = 100           // Other/Unknown
    };
    
    RelationData() = default;
    
    /**
     * Set relation data from apostrophe-separated strings (AniDB format)
     * @param aidList Apostrophe-separated anime IDs (e.g., "123'456'789")
     * @param typeList Apostrophe-separated relation types (e.g., "1'2'11")
     */
    void setRelations(const QString& aidList, const QString& typeList);
    
    /**
     * Get the anime ID of the prequel (if exists)
     * @return Anime ID of prequel, or 0 if no prequel exists
     */
    int getPrequel() const;
    
    /**
     * Get the anime ID of the sequel (if exists)
     * @return Anime ID of sequel, or 0 if no sequel exists
     */
    int getSequel() const;
    
    /**
     * Get all related anime IDs for a specific relation type
     * @param type Relation type to filter by
     * @return List of anime IDs with that relation type
     */
    QList<int> getRelatedAnimeByType(RelationType type) const;
    
    /**
     * Get all relations as a map (anime ID -> relation type)
     * @return Map of all relations
     */
    QMap<int, RelationType> getAllRelations() const;
    
    /**
     * Check if this anime has any relations
     * @return true if at least one relation exists
     */
    bool hasRelations() const;
    
    /**
     * Check if this anime has a prequel
     * @return true if prequel exists
     */
    bool hasPrequel() const;
    
    /**
     * Check if this anime has a sequel
     * @return true if sequel exists
     */
    bool hasSequel() const;
    
    /**
     * Get the raw relation data (for database storage/serialization)
     */
    QString getRelationAidList() const { return m_relaidlist; }
    QString getRelationTypeList() const { return m_relaidtype; }
    
    /**
     * Clear all relation data
     */
    void clear();

private:
    // Parse the relation strings and populate the cache
    void parseRelations() const;
    
    // Raw data from database (apostrophe-separated strings)
    QString m_relaidlist;
    QString m_relaidtype;
    
    // Parsed cache (mutable to allow lazy parsing in const methods)
    mutable QMap<int, RelationType> m_relationCache;
    mutable bool m_cacheParsed = false;
};

#endif // RELATIONDATA_H
