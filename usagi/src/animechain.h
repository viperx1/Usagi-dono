#ifndef ANIMECHAIN_H
#define ANIMECHAIN_H

#include <QList>
#include <QString>
#include <QPair>

/**
 * AnimeChain - Data structure for managing anime series chains
 * 
 * This is a data layer (not a widget) that represents a series of related anime
 * (prequels/sequels) and provides helper methods for chain-based operations
 * like sorting and filtering.
 * 
 * Design principles:
 * - Pure data structure with no UI logic
 * - Stores ordered list of anime IDs from prequel to sequel
 * - Provides utility methods for chain operations
 * - Used to simplify sorting/filtering logic when chain mode is enabled
 */
class AnimeChain
{
public:
    AnimeChain() = default;
    
    // Construct from a list of anime IDs (should be ordered prequel to sequel)
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
    
    // Add an anime ID to the chain (at the end, maintaining order)
    void addAnime(int aid) { m_animeIds.append(aid); }
    
    // Clear the chain
    void clear() { m_animeIds.clear(); }
    
    // Parse relation data and build a series chain
    // Returns a list of anime IDs in order from prequel to sequel
    static QList<int> buildChainFromRelations(
        int startAid,
        const QMap<int, QPair<QString, QString>>& relationData);
    
private:
    QList<int> m_animeIds;  // Ordered list of anime IDs (prequel to sequel)
};

#endif // ANIMECHAIN_H
