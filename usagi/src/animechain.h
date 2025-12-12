#ifndef ANIMECHAIN_H
#define ANIMECHAIN_H

#include <QList>
#include <QString>
#include <QPair>
#include <QMap>
#include <functional>

// Forward declaration - use full namespace path in MyListCardManager
class MyListCardManager;

/**
 * AnimeChain - Manages anime series chains with relation data
 * 
 * This class encapsulates chain building logic and holds relation data
 * for efficient merging and expansion operations.
 * 
 * Design principles:
 * - Holds anime IDs AND their prequel/sequel relations
 * - Provides interface for checking relations and merging chains
 * - No internal gaps allowed - all relations must be resolved
 * - Can expand to include related anime not in original input
 */
class AnimeChain
{
public:
    // Sort criteria for chain comparison
    enum class SortCriteria {
        ByRepresentativeTitle,
        ByRepresentativeDate,
        ByRepresentativeType,
        ByChainLength,
        ByRepresentativeId,
        ByRepresentativeEpisodeCount,
        ByRepresentativeCompletion,
        ByRepresentativeLastPlayed
    };
    
    // Forward declaration of CardCreationData from MyListCardManager
    template<typename T>
    using CardDataMap = QMap<int, T>;
    
    // Relation lookup function type: aid -> (prequel_aid, sequel_aid)
    using RelationLookupFunc = std::function<QPair<int,int>(int)>;
    
    AnimeChain() = default;
    
    // Construct from a single anime ID with relation lookup function
    explicit AnimeChain(int aid, RelationLookupFunc lookupFunc);
    
    // Construct from a list of anime IDs (for backward compatibility)
    explicit AnimeChain(const QList<int>& animeIds) : m_animeIds(animeIds) {}
    
    // Get the list of anime IDs in this chain (ordered from prequel to sequel)
    QList<int> getAnimeIds() const { return m_animeIds; }
    
    // Get the first (representative) anime ID for external sorting
    int getRepresentativeAnimeId() const {
        return m_animeIds.isEmpty() ? 0 : m_animeIds.first();
    }
    
    // Get the last anime ID in the chain
    int getLastAnimeId() const {
        return m_animeIds.isEmpty() ? 0 : m_animeIds.last();
    }
    
    // Get chain size
    int size() const { return m_animeIds.size(); }
    
    // Check if chain is empty
    bool isEmpty() const { return m_animeIds.isEmpty(); }
    
    // Check if chain contains a specific anime ID
    bool contains(int aid) const { return m_animeIds.contains(aid); }
    
    // Get unbound relations for an anime (not already in this chain)
    // Returns (prequel_aid, sequel_aid) where 0 means no relation
    QPair<int,int> getUnboundRelations(int aid) const;
    
    // Check if this chain can merge with an anime (has connecting relation)
    bool canMergeWith(int aid) const;
    
    // Check if this chain can merge with another chain
    bool canMergeWith(const AnimeChain& other) const;
    
    // Merge another chain into this one
    // Returns true if successful, false if chains don't connect
    bool mergeWith(const AnimeChain& other, RelationLookupFunc lookupFunc);
    
    // Expand chain by following unbound relations
    // Continues until no more relations can be found
    void expand(RelationLookupFunc lookupFunc);
    
    // Order the anime IDs in the chain from prequel to sequel
    void orderChain();
    
    // Comparison operators for sorting (default: by representative ID)
    bool operator<(const AnimeChain& other) const;
    bool operator>(const AnimeChain& other) const;
    bool operator==(const AnimeChain& other) const;
    
    // Compare with another chain using specified criteria (template version for CardCreationData)
    // Returns: <0 if this < other, 0 if equal, >0 if this > other
    template<typename CardCreationData>
    int compareWith(const AnimeChain& other,
                    const QMap<int, CardCreationData>& dataCache,
                    SortCriteria criteria,
                    bool ascending) const;
    
    // Parse relation data and build a series chain
    // Returns a list of anime IDs in order from prequel to sequel
    static QList<int> buildChainFromRelations(
        int startAid,
        const QMap<int, QPair<QString, QString>>& relationData);
    
private:
    QList<int> m_animeIds;  // Ordered list of anime IDs (prequel to sequel)
    QMap<int, QPair<int,int>> m_relations;  // aid -> (prequel_aid, sequel_aid)
};

// Template implementation must be in header
template<typename CardCreationData>
int AnimeChain::compareWith(
    const AnimeChain& other,
    const QMap<int, CardCreationData>& dataCache,
    SortCriteria criteria,
    bool ascending) const
{
    int result = 0;
    int myAid = getRepresentativeAnimeId();
    int otherAid = other.getRepresentativeAnimeId();
    
    if (!dataCache.contains(myAid) || !dataCache.contains(otherAid)) {
        // Fallback to ID comparison if data not available
        result = myAid - otherAid;
    } else {
        const CardCreationData& myData = dataCache[myAid];
        const CardCreationData& otherData = dataCache[otherAid];
        
        switch (criteria) {
            case SortCriteria::ByRepresentativeTitle:
                result = myData.animeTitle.compare(otherData.animeTitle, Qt::CaseInsensitive);
                break;
            case SortCriteria::ByRepresentativeDate:
                result = myData.startDate.compare(otherData.startDate);
                break;
            case SortCriteria::ByRepresentativeType:
                result = myData.typeName.compare(otherData.typeName);
                break;
            case SortCriteria::ByChainLength:
                result = size() - other.size();
                break;
            case SortCriteria::ByRepresentativeEpisodeCount: {
                int myEpisodes = myData.stats.normalEpisodes() + myData.stats.otherEpisodes();
                int otherEpisodes = otherData.stats.normalEpisodes() + otherData.stats.otherEpisodes();
                result = myEpisodes - otherEpisodes;
                break;
            }
            case SortCriteria::ByRepresentativeCompletion: {
                int myTotal = myData.stats.normalEpisodes() + myData.stats.otherEpisodes();
                int myViewed = myData.stats.normalViewed() + myData.stats.otherViewed();
                double myCompletion = (myTotal > 0) ? static_cast<double>(myViewed) / myTotal : 0.0;
                
                int otherTotal = otherData.stats.normalEpisodes() + otherData.stats.otherEpisodes();
                int otherViewed = otherData.stats.normalViewed() + otherData.stats.otherViewed();
                double otherCompletion = (otherTotal > 0) ? static_cast<double>(otherViewed) / otherTotal : 0.0;
                
                if (myCompletion < otherCompletion) result = -1;
                else if (myCompletion > otherCompletion) result = 1;
                else result = 0;
                break;
            }
            case SortCriteria::ByRepresentativeLastPlayed: {
                qint64 myLastPlayed = myData.lastPlayed;
                qint64 otherLastPlayed = otherData.lastPlayed;
                
                // Never played items (0) go to the end regardless of sort order
                // We need to handle this specially since it shouldn't be affected by ascending/descending
                if (myLastPlayed == 0 && otherLastPlayed == 0) {
                    result = 0;
                } else if (myLastPlayed == 0) {
                    // Unplayed always at end: return result that puts this after other
                    // Since the template will negate based on ascending, we need to return a value
                    // that after potential negation still puts unplayed at end
                    // In ascending order: want this > other, so result should be > 0
                    // In descending order: want this > other, but result will be negated, so need < 0
                    result = ascending ? 1 : -1;
                } else if (otherLastPlayed == 0) {
                    // Unplayed always at end: return result that puts other after this
                    result = ascending ? -1 : 1;
                } else {
                    // Normal comparison
                    if (myLastPlayed < otherLastPlayed) result = -1;
                    else if (myLastPlayed > otherLastPlayed) result = 1;
                    else result = 0;
                }
                break;
            }
            case SortCriteria::ByRepresentativeId:
            default:
                result = myAid - otherAid;
                break;
        }
    }
    
    return ascending ? result : -result;
}

#endif // ANIMECHAIN_H
