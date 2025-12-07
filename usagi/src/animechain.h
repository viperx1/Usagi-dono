#ifndef ANIMECHAIN_H
#define ANIMECHAIN_H

#include <QList>
#include <QString>
#include <QPair>
#include <QMap>

// Forward declaration - use full namespace path in MyListCardManager
class MyListCardManager;

/**
 * AnimeChain - Data structure for managing anime series chains
 * 
 * This is a data layer (not a widget) that represents a series of related anime
 * (prequels/sequels) and provides helper methods for chain-based operations
 * like sorting and filtering.
 * 
 * Design principles:
 * - Pure data structure with no UI logic
 * - Stores ordered list of anime IDs from prequel to sequel (immutable internal order)
 * - Provides comparison operators for external sorting
 * - Used to simplify sorting/filtering logic when chain mode is enabled
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
    
    AnimeChain() = default;
    
    // Construct from a list of anime IDs (should be ordered prequel to sequel)
    explicit AnimeChain(const QList<int>& animeIds) : m_animeIds(animeIds) {}
    
    // Get the list of anime IDs in this chain (ordered from prequel to sequel, immutable)
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
    QList<int> m_animeIds;  // Ordered list of anime IDs (prequel to sequel, immutable)
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
