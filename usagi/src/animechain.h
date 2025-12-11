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
        ByRepresentativeId
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
            case SortCriteria::ByRepresentativeId:
            default:
                result = myAid - otherAid;
                break;
        }
    }
    
    return ascending ? result : -result;
}

#endif // ANIMECHAIN_H
