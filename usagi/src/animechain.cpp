#include "animechain.h"
#include <QSet>
#include <QMap>

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
