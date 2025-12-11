#include "animechain.h"
#include <QSet>
#include <QMap>
#include <algorithm>

// Construct from a single anime ID with relation lookup
AnimeChain::AnimeChain(int aid, RelationLookupFunc lookupFunc)
{
    m_animeIds.append(aid);
    if (lookupFunc) {
        auto relations = lookupFunc(aid);
        m_relations[aid] = relations;
    }
}

// Get unbound relations for an anime (not already in this chain)
QPair<int,int> AnimeChain::getUnboundRelations(int aid) const
{
    if (!m_relations.contains(aid)) {
        return QPair<int,int>(0, 0);
    }
    
    QPair<int,int> relations = m_relations[aid];
    int prequelAid = relations.first;
    int sequelAid = relations.second;
    
    // Filter out relations that are already in this chain
    if (prequelAid > 0 && m_animeIds.contains(prequelAid)) {
        prequelAid = 0;
    }
    if (sequelAid > 0 && m_animeIds.contains(sequelAid)) {
        sequelAid = 0;
    }
    
    return QPair<int,int>(prequelAid, sequelAid);
}

// Check if this chain can merge with an anime
bool AnimeChain::canMergeWith(int aid) const
{
    // Check if any anime in this chain has the target aid as a relation
    for (int chainAid : m_animeIds) {
        auto unbound = getUnboundRelations(chainAid);
        if (unbound.first == aid || unbound.second == aid) {
            return true;
        }
    }
    return false;
}

// Check if this chain can merge with another chain
bool AnimeChain::canMergeWith(const AnimeChain& other) const
{
    // Check if any anime in this chain connects to any anime in the other chain
    for (int otherAid : other.m_animeIds) {
        if (canMergeWith(otherAid)) {
            return true;
        }
    }
    return false;
}

// Merge another chain into this one
bool AnimeChain::mergeWith(const AnimeChain& other, RelationLookupFunc lookupFunc)
{
    if (other.isEmpty()) {
        return false;
    }
    
    // Add all anime from other chain
    for (int aid : other.m_animeIds) {
        if (!m_animeIds.contains(aid)) {
            m_animeIds.append(aid);
        }
    }
    
    // Merge relation data
    for (auto it = other.m_relations.constBegin(); it != other.m_relations.constEnd(); ++it) {
        if (!m_relations.contains(it.key())) {
            m_relations[it.key()] = it.value();
        }
    }
    
    // Reorder the chain after merging
    orderChain();
    
    return true;
}

// Expand chain by following unbound relations
void AnimeChain::expand(RelationLookupFunc lookupFunc)
{
    if (!lookupFunc) {
        return;
    }
    
    QSet<int> processed;
    bool changed = true;
    int iterations = 0;
    const int MAX_ITERATIONS = 100;  // Safety limit
    
    while (changed && iterations < MAX_ITERATIONS) {
        changed = false;
        iterations++;
        
        // Check all anime in current chain for unbound relations
        QList<int> currentAnime = m_animeIds;  // Copy to avoid modification during iteration
        
        for (int aid : currentAnime) {
            if (processed.contains(aid)) {
                continue;
            }
            processed.insert(aid);
            
            // Get or fetch relations for this anime
            if (!m_relations.contains(aid)) {
                m_relations[aid] = lookupFunc(aid);
            }
            
            auto unbound = getUnboundRelations(aid);
            
            // Add prequel if found
            if (unbound.first > 0) {
                m_animeIds.append(unbound.first);
                m_relations[unbound.first] = lookupFunc(unbound.first);
                changed = true;
            }
            
            // Add sequel if found
            if (unbound.second > 0) {
                m_animeIds.append(unbound.second);
                m_relations[unbound.second] = lookupFunc(unbound.second);
                changed = true;
            }
        }
    }
    
    // Order the chain after expansion
    orderChain();
}

// Order the anime IDs in the chain from prequel to sequel
void AnimeChain::orderChain()
{
    if (m_animeIds.size() <= 1) {
        return;
    }
    
    // Build adjacency graph for topological sort
    QMap<int, QList<int>> graph;
    QMap<int, int> inDegree;
    
    // Initialize
    for (int aid : m_animeIds) {
        graph[aid] = QList<int>();
        inDegree[aid] = 0;
    }
    
    // Build graph edges based on sequel relationships
    for (int aid : m_animeIds) {
        if (m_relations.contains(aid)) {
            int sequelAid = m_relations[aid].second;
            if (sequelAid > 0 && m_animeIds.contains(sequelAid)) {
                graph[aid].append(sequelAid);
                inDegree[sequelAid]++;
            }
        }
    }
    
    // Topological sort using Kahn's algorithm
    QList<int> orderedChain;
    QList<int> queue;
    
    // Start with anime that have no prequels (in-degree = 0)
    for (int aid : m_animeIds) {
        if (inDegree[aid] == 0) {
            queue.append(aid);
        }
    }
    
    while (!queue.isEmpty()) {
        int current = queue.takeFirst();
        orderedChain.append(current);
        
        for (int sequel : graph[current]) {
            inDegree[sequel]--;
            if (inDegree[sequel] == 0) {
                queue.append(sequel);
            }
        }
    }
    
    // Add any remaining anime (cycles or disconnected)
    if (orderedChain.size() < m_animeIds.size()) {
        for (int aid : m_animeIds) {
            if (!orderedChain.contains(aid)) {
                orderedChain.append(aid);
            }
        }
    }
    
    m_animeIds = orderedChain;
}

bool AnimeChain::operator<(const AnimeChain& other) const
{
    // Default comparison: by representative anime ID
    return getRepresentativeAnimeId() < other.getRepresentativeAnimeId();
}

bool AnimeChain::operator>(const AnimeChain& other) const
{
    return getRepresentativeAnimeId() > other.getRepresentativeAnimeId();
}

bool AnimeChain::operator==(const AnimeChain& other) const
{
    return getRepresentativeAnimeId() == other.getRepresentativeAnimeId();
}

QList<int> AnimeChain::buildChainFromRelations(
    int startAid,
    const QMap<int, QPair<QString, QString>>& relationData)
{
    QList<int> chain;
    QSet<int> visited;
    
    if (!relationData.contains(startAid)) {
        // No relation data for this anime, return single-item chain
        chain.append(startAid);
        return chain;
    }
    
    // Find the original prequel (first anime in the chain)
    int currentAid = startAid;
    QSet<int> prequelSearchVisited;
    
    while (currentAid > 0 && !prequelSearchVisited.contains(currentAid)) {
        prequelSearchVisited.insert(currentAid);
        
        if (!relationData.contains(currentAid)) {
            break;
        }
        
        const QPair<QString, QString>& relations = relationData[currentAid];
        QStringList relAids = relations.first.split("'", Qt::SkipEmptyParts);
        QStringList relTypes = relations.second.split("'", Qt::SkipEmptyParts);
        
        // Look for prequel (type code 2)
        int prequelAid = 0;
        for (int i = 0; i < qMin(relAids.size(), relTypes.size()); i++) {
            QString typeStr = relTypes[i].toLower();
            if (typeStr == "2" || typeStr.contains("prequel", Qt::CaseInsensitive)) {
                prequelAid = relAids[i].toInt();
                break;
            }
        }
        
        if (prequelAid > 0) {
            currentAid = prequelAid;
        } else {
            break;
        }
    }
    
    // Now build the chain from the original prequel following sequels
    int chainStart = currentAid;
    currentAid = chainStart;
    
    while (currentAid > 0 && !visited.contains(currentAid)) {
        chain.append(currentAid);
        visited.insert(currentAid);
        
        if (!relationData.contains(currentAid)) {
            break;
        }
        
        const QPair<QString, QString>& relations = relationData[currentAid];
        QStringList relAids = relations.first.split("'", Qt::SkipEmptyParts);
        QStringList relTypes = relations.second.split("'", Qt::SkipEmptyParts);
        
        // Look for sequel (type code 1)
        int sequelAid = 0;
        for (int i = 0; i < qMin(relAids.size(), relTypes.size()); i++) {
            QString typeStr = relTypes[i].toLower();
            if (typeStr == "1" || typeStr.contains("sequel", Qt::CaseInsensitive)) {
                sequelAid = relAids[i].toInt();
                break;
            }
        }
        
        if (sequelAid > 0) {
            currentAid = sequelAid;
        } else {
            break;
        }
    }
    
    return chain;
}
