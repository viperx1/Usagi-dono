#include "mylistcardmanager.h"
#include "virtualflowlayout.h"
#include "animeutils.h"
#include "logger.h"
#include "main.h"
#include "watchsessionmanager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QElapsedTimer>
#include <QDateTime>
#include <QNetworkRequest>
#include <algorithm>
#include <numeric>

// External references
extern myAniDBApi *adbapi;

MyListCardManager::MyListCardManager(QObject *parent)
    : QObject(parent)
    , m_layout(nullptr)
    , m_virtualLayout(nullptr)
    , m_watchSessionManager(nullptr)
    , m_chainModeEnabled(false)  // Initialize chain mode as disabled
    , m_networkManager(nullptr)
    , m_initialLoadComplete(false)
{
    // Initialize network manager for poster downloads
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &MyListCardManager::onPosterDownloadFinished);
    
    // Initialize batch update timer
    m_batchUpdateTimer = new QTimer(this);
    m_batchUpdateTimer->setSingleShot(true);
    m_batchUpdateTimer->setInterval(BATCH_UPDATE_DELAY);
    connect(m_batchUpdateTimer, &QTimer::timeout,
            this, &MyListCardManager::processBatchedUpdates);
}

MyListCardManager::~MyListCardManager()
{
    clearAllCards();
}

void MyListCardManager::setCardLayout(FlowLayout *layout)
{
    QMutexLocker locker(&m_mutex);
    m_layout = layout;
}

void MyListCardManager::setVirtualLayout(VirtualFlowLayout *layout)
{
    QMutexLocker locker(&m_mutex);
    m_virtualLayout = layout;
    
    if (m_virtualLayout) {
        // Set the item factory to create cards on demand
        m_virtualLayout->setItemFactory([this](int index) -> QWidget* {
            return this->createCardForIndex(index);
        });
        
        // Set the item size to match anime card size
        m_virtualLayout->setItemSize(AnimeCard::getCardSize());
    }
}

QList<int> MyListCardManager::getAnimeIdList() const
{
    QMutexLocker locker(&m_mutex);
    return m_orderedAnimeIds;
}

void MyListCardManager::setAnimeIdList(const QList<int>& aids)
{
    setAnimeIdList(aids, false);  // Default: chain mode disabled
}

void MyListCardManager::setAnimeIdList(const QList<int>& aids, bool chainModeEnabled)
{
    QList<int> finalAnimeIds;
    QList<int> expandedAnimeNeedingData;
    
    {
        QMutexLocker locker(&m_mutex);
        m_chainModeEnabled = chainModeEnabled;
        m_expandedChainAnimeIds.clear();
        
        if (chainModeEnabled) {
            // Build chains BEFORE sorting - always expand to include related anime
            LOG(QString("[MyListCardManager] Building chains from %1 anime IDs WITH expansion").arg(aids.size()));
            m_chainList = buildChainsFromAnimeIds(aids);  // Always expand chains
            
            // Build aid -> chain index map and collect all anime IDs from expanded chains
            m_aidToChainIndex.clear();
            finalAnimeIds.clear();
            for (int i = 0; i < m_chainList.size(); ++i) {
                for (int aid : m_chainList[i].getAnimeIds()) {
                    m_aidToChainIndex[aid] = i;
                    // Add all anime from chains (including expanded ones not in original aids list)
                    if (!finalAnimeIds.contains(aid)) {
                        finalAnimeIds.append(aid);
                    }
                }
            }
            
            LOG(QString("[MyListCardManager] Built %1 chains from %2 anime (with expansion)")
                .arg(m_chainList.size()).arg(finalAnimeIds.size()));
        } else {
            // Normal mode: clear chain data
            m_chainList.clear();
            m_aidToChainIndex.clear();
            finalAnimeIds = aids;
        }
        
        m_orderedAnimeIds = finalAnimeIds;
        LOG(QString("[MyListCardManager] setAnimeIdList: set %1 anime IDs, chain mode %2")
            .arg(finalAnimeIds.size()).arg(chainModeEnabled ? "enabled" : "disabled"));
    }
    
    // Update virtual layout AFTER releasing the mutex to avoid deadlock
    // (setItemCount -> updateVisibleItems -> createCardForIndex -> needs mutex)
    if (m_virtualLayout) {
        m_virtualLayout->setItemCount(finalAnimeIds.size());
    }
}

QList<AnimeChain> MyListCardManager::buildChainsFromAnimeIds(const QList<int>& aids) const
{
    QList<AnimeChain> chains;
    QSet<int> availableAids = QSet<int>(aids.begin(), aids.end());
    QSet<int> processedAids;
    
    LOG(QString("[MyListCardManager] buildChainsFromAnimeIds: input has %1 anime, %2 unique, expansion=ALWAYS ON")
        .arg(aids.size()).arg(availableAids.size()));
    
    for (int aid : aids) {
        if (processedAids.contains(aid)) {
            continue;
        }
        
        // Build chain starting from this anime (always with expansion)
        QList<int> chainAids = buildChainFromAid(aid, availableAids, true);
        
        // Skip empty chains (anime not in available set)
        if (chainAids.isEmpty()) {
            continue;
        }
        
        // CRITICAL FIX: Remove anime that are already processed from the chain
        // This prevents duplicates when anime with broken relationships are appended
        // to chains containing already-processed anime
        QList<int> filteredChainAids;
        for (int chainAid : chainAids) {
            if (!processedAids.contains(chainAid)) {
                filteredChainAids.append(chainAid);
            } else {
                LOG(QString("[MyListCardManager] Skipping already-processed anime %1 in chain from %2")
                    .arg(chainAid).arg(aid));
            }
        }
        
        // Skip this chain if all anime were already processed
        if (filteredChainAids.isEmpty()) {
            LOG(QString("[MyListCardManager] Chain from aid=%1 is empty after filtering processed anime, skipping")
                .arg(aid));
            continue;
        }
        
        // Mark all anime in this chain as processed
        for (int chainAid : filteredChainAids) {
            processedAids.insert(chainAid);
        }
        
        // Create chain object
        chains.append(AnimeChain(filteredChainAids));
    }
    
    // OPTIMIZED: Build anime-to-chain-index map for O(1) lookups
    QMap<int, int> animeToChainIndex;
    for (int i = 0; i < chains.size(); ++i) {
        for (int aid : chains[i].getAnimeIds()) {
            animeToChainIndex[aid] = i;
        }
    }
    
    // Merge chains by checking each anime's prequel/sequel relations
    // Instead of O(n²) chain pair checking, this is O(n×relations) which is much faster
    QSet<int> chainsToMerge;  // Track which chains need merging
    QMap<int, QSet<int>> chainMergeGroups;  // Group chains that should be merged together
    
    for (int chainIdx = 0; chainIdx < chains.size(); ++chainIdx) {
        QList<int> chainAnime = chains[chainIdx].getAnimeIds();
        
        for (int aid : chainAnime) {
            // Load relation data for this anime
            loadRelationDataForAnime(aid);
            CardCreationData data = m_cardCreationDataCache.value(aid);
            
            QStringList relAidList = data.relaidlist.split('\'', Qt::SkipEmptyParts);
            QStringList relAidType = data.relaidtype.split('\'', Qt::SkipEmptyParts);
            
            // Check both prequel and sequel relations
            for (int k = 0; k < relAidList.size() && k < relAidType.size(); ++k) {
                int relAid = relAidList[k].toInt();
                int relType = relAidType[k].toInt();
                
                // Only interested in Prequel (1) and Sequel (2) relationships
                if (relType != 1 && relType != 2) {
                    continue;
                }
                
                // Check if this related anime is in another chain
                if (animeToChainIndex.contains(relAid)) {
                    int otherChainIdx = animeToChainIndex[relAid];
                    
                    // If in different chain, mark both chains for merging
                    if (otherChainIdx != chainIdx) {
                        if (!chainMergeGroups.contains(chainIdx)) {
                            chainMergeGroups[chainIdx] = QSet<int>();
                        }
                        chainMergeGroups[chainIdx].insert(otherChainIdx);
                        chainsToMerge.insert(chainIdx);
                        chainsToMerge.insert(otherChainIdx);
                        
                        LOG(QString("[MyListCardManager] Chain %1 (aid=%2) connects to chain %3 (aid=%4) via %5")
                            .arg(chainIdx).arg(aid).arg(otherChainIdx).arg(relAid)
                            .arg(relType == 1 ? "prequel" : "sequel"));
                    }
                }
            }
        }
    }
    
    // Perform merging if needed
    if (!chainsToMerge.isEmpty()) {
        LOG(QString("[MyListCardManager] Merging %1 chains that have connections").arg(chainsToMerge.size()));
        
        // Build connected components - chains that should be merged together
        QMap<int, QSet<int>> mergeComponents;
        for (int chainIdx : chainsToMerge) {
            mergeComponents[chainIdx].insert(chainIdx);
            if (chainMergeGroups.contains(chainIdx)) {
                mergeComponents[chainIdx].unite(chainMergeGroups[chainIdx]);
            }
        }
        
        // Expand components transitively
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (auto it = mergeComponents.begin(); it != mergeComponents.end(); ++it) {
                QSet<int> current = it.value();
                QSet<int> newSet = current;
                
                for (int idx : current) {
                    if (mergeComponents.contains(idx)) {
                        newSet.unite(mergeComponents[idx]);
                    }
                }
                
                if (newSet.size() > current.size()) {
                    it.value() = newSet;
                    expanded = true;
                }
            }
        }
        
        // Find unique merge groups
        QSet<QSet<int>> uniqueMergeGroups;
        for (const QSet<int>& component : mergeComponents.values()) {
            uniqueMergeGroups.insert(component);
        }
        
        // Create new merged chains
        QList<AnimeChain> newChains;
        QSet<int> processedChainIndices;
        
        for (const QSet<int>& group : uniqueMergeGroups) {
            // Collect all anime from chains in this group
            QSet<int> allAnime;
            for (int chainIdx : group) {
                processedChainIndices.insert(chainIdx);
                for (int aid : chains[chainIdx].getAnimeIds()) {
                    allAnime.insert(aid);
                }
            }
            
            // Rebuild merged chain using relation data for proper ordering
            QList<int> mergedList = allAnime.values();
            if (!mergedList.isEmpty()) {
                LOG(QString("[MyListCardManager] Merging group of %1 chains with %2 total anime")
                    .arg(group.size()).arg(allAnime.size()));
                newChains.append(AnimeChain(buildChainFromAid(mergedList.first(), allAnime, true)));
            }
        }
        
        // Add unmerged chains
        for (int i = 0; i < chains.size(); ++i) {
            if (!processedChainIndices.contains(i)) {
                newChains.append(chains[i]);
            }
        }
        
        chains = newChains;
    }
    
    LOG(QString("[MyListCardManager] After merging: %1 chains").arg(chains.size()));
    
    // Verify total anime count and find duplicates
    int totalAnimeInChains = 0;
    QSet<int> allAnimeInChains;
    QMap<int, int> animeOccurrences;  // Track how many times each anime appears
    
    for (int chainIdx = 0; chainIdx < chains.size(); ++chainIdx) {
        const AnimeChain& chain = chains[chainIdx];
        for (int aid : chain.getAnimeIds()) {
            totalAnimeInChains++;
            allAnimeInChains.insert(aid);
            animeOccurrences[aid]++;
            
            // Log if anime appears more than once
            if (animeOccurrences[aid] > 1) {
                LOG(QString("[MyListCardManager] ERROR: Anime %1 appears %2 times (chain %3)")
                    .arg(aid).arg(animeOccurrences[aid]).arg(chainIdx));
            }
        }
    }
    
    if (totalAnimeInChains != availableAids.size()) {
        LOG(QString("[MyListCardManager] WARNING: Chain building mismatch! Input: %1 unique anime, Chains contain: %2 anime (%3 unique)")
            .arg(availableAids.size()).arg(totalAnimeInChains).arg(allAnimeInChains.size()));
            
        // Log which anime are extra
        QSet<int> extraAnime = allAnimeInChains - availableAids;
        if (!extraAnime.isEmpty()) {
            QStringList extraList;
            for (int aid : extraAnime) {
                extraList.append(QString::number(aid));
            }
            LOG(QString("[MyListCardManager] Extra anime in chains (NOT in input): %1").arg(extraList.join(", ")));
        }
        
        // Log which anime are missing
        QSet<int> missingAnime = availableAids - allAnimeInChains;
        if (!missingAnime.isEmpty()) {
            QStringList missingList;
            for (int aid : missingAnime) {
                missingList.append(QString::number(aid));
            }
            LOG(QString("[MyListCardManager] Missing anime from chains: %1").arg(missingList.join(", ")));
        }
        
        // Log duplicates
        QStringList duplicates;
        for (QMap<int, int>::const_iterator it = animeOccurrences.constBegin(); it != animeOccurrences.constEnd(); ++it) {
            if (it.value() > 1) {
                duplicates.append(QString("%1(x%2)").arg(it.key()).arg(it.value()));
            }
        }
        if (!duplicates.isEmpty()) {
            LOG(QString("[MyListCardManager] Duplicate anime in chains: %1").arg(duplicates.join(", ")));
        }
    }
    
    return chains;
}

QList<int> MyListCardManager::buildChainFromAid(int startAid, const QSet<int>& availableAids, bool expandChain) const
{
    QList<int> chain;
    QSet<int> visited;
    const int MAX_CHAIN_LENGTH = 20;  // Safety limit to prevent infinite loops
    
    // Find the original prequel (first in chain)
    int currentAid = startAid;
    
    // Only traverse backward if startAid is in our filtered set
    if (!expandChain && !availableAids.contains(startAid)) {
        return QList<int>();  // Return empty chain - anime not in filtered list
    }
    
    // Ensure relation data is loaded for startAid
    loadRelationDataForAnime(startAid);
    
    // Track the last anime we found that's in our filtered set
    int lastAvailableAid = startAid;
    
    // Traverse backward to find the first prequel
    while (true) {
        if (visited.contains(currentAid)) {
            break;  // Cycle detected
        }
        visited.insert(currentAid);
        
        if (visited.size() > MAX_CHAIN_LENGTH) {
            LOG(QString("[MyListCardManager] WARNING: Chain too long (>%1), stopping backward traversal").arg(MAX_CHAIN_LENGTH));
            break;
        }
        
        int prequelAid = findPrequelAid(currentAid);
        if (prequelAid == 0) {
            break;  // No prequel
        }
        
        // Load relation data for the prequel
        if (expandChain) {
            loadRelationDataForAnime(prequelAid);
        }
        
        // If not expanding, stop at prequels not in available set
        if (!expandChain && !availableAids.contains(prequelAid)) {
            break;
        }
        
        // Update last available anime
        if (expandChain || availableAids.contains(prequelAid)) {
            lastAvailableAid = prequelAid;
        }
        
        currentAid = prequelAid;
    }
    
    // Now build chain forward from the last anime in our filtered set
    // (or from the first prequel if expanding)
    visited.clear();
    int chainStart = expandChain ? currentAid : lastAvailableAid;
    currentAid = chainStart;
    
    // Track if we've added startAid to ensure it's always included
    bool startAidAdded = false;
    
    while (currentAid > 0 && !visited.contains(currentAid)) {
        // When expanding, include all anime in chain
        // When not expanding, only include anime in availableAids
        if (expandChain || availableAids.contains(currentAid)) {
            chain.append(currentAid);
            visited.insert(currentAid);
            
            if (currentAid == startAid) {
                startAidAdded = true;
            }
            
            if (visited.size() > MAX_CHAIN_LENGTH) {
                LOG(QString("[MyListCardManager] WARNING: Chain too long (>%1), stopping forward traversal").arg(MAX_CHAIN_LENGTH));
                break;
            }
            
            int sequelAid = findSequelAid(currentAid);
            if (sequelAid == 0) {
                break;  // No sequel
            }
            
            // Load relation data for the sequel (only when expanding)
            if (expandChain) {
                loadRelationDataForAnime(sequelAid);
            }
            
            // If not expanding, stop at sequels not in available set
            if (!expandChain && !availableAids.contains(sequelAid)) {
                break;
            }
            
            currentAid = sequelAid;
        } else {
            // Current anime not in available set and not expanding - stop traversing
            break;
        }
    }
    
    // CRITICAL FIX: Ensure startAid is in the chain
    // If we started from a prequel but couldn't reach startAid via sequel relationships,
    // append startAid to the chain to keep it with its prequel.
    // This handles database inconsistencies where prequel->sequel relationships are not bidirectional.
    if (!startAidAdded && (expandChain || availableAids.contains(startAid))) {
        LOG(QString("[MyListCardManager] WARNING: startAid=%1 not reachable from chainStart=%2 via sequel relationships, appending to chain")
            .arg(startAid).arg(chainStart));
        
        // Append the starting anime to the chain it belongs to (based on its prequel relationship)
        // This keeps related anime together even when sequel relationships are broken
        chain.append(startAid);
    }
    
    return chain;
}

int MyListCardManager::findPrequelAid(int aid) const
{
    // Use cached relation data
    if (!m_cardCreationDataCache.contains(aid)) {
        return 0;
    }
    
    const CardCreationData& data = m_cardCreationDataCache[aid];
    if (data.relaidlist.isEmpty() || data.relaidtype.isEmpty()) {
        return 0;
    }
    
    QStringList aidList = data.relaidlist.split("'", Qt::SkipEmptyParts);
    QStringList typeList = data.relaidtype.split("'", Qt::SkipEmptyParts);
    
    for (int i = 0; i < qMin(aidList.size(), typeList.size()); ++i) {
        QString type = typeList[i].toLower();
        // Type 2 = prequel
        if (type == "2" || type.contains("prequel", Qt::CaseInsensitive)) {
            return aidList[i].toInt();
        }
    }
    
    return 0;
}

int MyListCardManager::findSequelAid(int aid) const
{
    // Similar to findPrequelAid but looks for type 1 (sequel)
    if (!m_cardCreationDataCache.contains(aid)) {
        return 0;
    }
    
    const CardCreationData& data = m_cardCreationDataCache[aid];
    if (data.relaidlist.isEmpty() || data.relaidtype.isEmpty()) {
        return 0;
    }
    
    QStringList aidList = data.relaidlist.split("'", Qt::SkipEmptyParts);
    QStringList typeList = data.relaidtype.split("'", Qt::SkipEmptyParts);
    
    for (int i = 0; i < qMin(aidList.size(), typeList.size()); ++i) {
        QString type = typeList[i].toLower();
        // Type 1 = sequel
        if (type == "1" || type.contains("sequel", Qt::CaseInsensitive)) {
            return aidList[i].toInt();
        }
    }
    
    return 0;
}

void MyListCardManager::loadRelationDataForAnime(int aid) const
{
    // If already in cache, nothing to do
    if (m_cardCreationDataCache.contains(aid)) {
        return;
    }
    
    // Query database for relation data
    // Note: This is a const method but modifies mutable cache - we need to cast away const
    MyListCardManager* mutableThis = const_cast<MyListCardManager*>(this);
    
    QSqlQuery query(QSqlDatabase::database());
    query.prepare("SELECT relaidlist, relaidtype FROM anime WHERE aid = ?");
    query.addBindValue(aid);
    
    if (!query.exec()) {
        LOG(QString("[MyListCardManager] Failed to load relation data for aid=%1: %2").arg(aid).arg(query.lastError().text()));
        return;
    }
    
    if (query.next()) {
        CardCreationData data;
        data.relaidlist = query.value(0).toString();
        data.relaidtype = query.value(1).toString();
        
        // Store in cache (cast away const)
        mutableThis->m_cardCreationDataCache[aid] = data;
        
        LOG(QString("[MyListCardManager] Loaded relation data for aid=%1 (relaidlist=%2)")
            .arg(aid).arg(data.relaidlist.isEmpty() ? "empty" : "present"));
    } else {
        LOG(QString("[MyListCardManager] No anime found in database for aid=%1").arg(aid));
    }
}

void MyListCardManager::sortChains(AnimeChain::SortCriteria criteria, bool ascending)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_chainModeEnabled || m_chainList.isEmpty()) {
        LOG("[MyListCardManager] sortChains: chain mode not enabled or no chains");
        return;
    }
    
    int inputAnimeCount = m_orderedAnimeIds.size();
    
    LOG(QString("[MyListCardManager] Sorting %1 chains by criteria %2, ascending=%3 (current ordered list has %4 anime)")
        .arg(m_chainList.size()).arg(static_cast<int>(criteria)).arg(ascending).arg(inputAnimeCount));
    
    // Sort chains using comparison function
    std::sort(m_chainList.begin(), m_chainList.end(),
              [this, criteria, ascending](const AnimeChain& a, const AnimeChain& b) {
                  return a.compareWith(b, m_cardCreationDataCache, criteria, ascending) < 0;
              });
    
    // Rebuild flattened anime ID list preserving internal chain order
    m_orderedAnimeIds.clear();
    m_aidToChainIndex.clear();
    
    for (int i = 0; i < m_chainList.size(); ++i) {
        for (int aid : m_chainList[i].getAnimeIds()) {
            m_orderedAnimeIds.append(aid);
            m_aidToChainIndex[aid] = i;
        }
    }
    
    if (m_orderedAnimeIds.size() != inputAnimeCount) {
        LOG(QString("[MyListCardManager] WARNING: Anime count changed during sort! Before: %1, After: %2")
            .arg(inputAnimeCount).arg(m_orderedAnimeIds.size()));
    }
    
    LOG(QString("[MyListCardManager] Rebuilt ordered list: %1 anime in %2 chains")
        .arg(m_orderedAnimeIds.size()).arg(m_chainList.size()));
    
    // Note: We don't call refresh() here because it causes re-entrancy issues
    // when called synchronously during sorting. The caller (window.cpp) will
    // handle refreshing the layout after sorting completes.
}

AnimeChain MyListCardManager::getChainForAnime(int aid) const
{
    QMutexLocker locker(&m_mutex);
    int chainIndex = m_aidToChainIndex.value(aid, -1);
    if (chainIndex >= 0 && chainIndex < m_chainList.size()) {
        return m_chainList[chainIndex];
    }
    return AnimeChain();  // Empty chain
}

int MyListCardManager::getChainIndexForAnime(int aid) const
{
    QMutexLocker locker(&m_mutex);
    return m_aidToChainIndex.value(aid, -1);
}

AnimeCard* MyListCardManager::createCardForIndex(int index)
{
    QMutexLocker locker(&m_mutex);
    
    if (index < 0 || index >= m_orderedAnimeIds.size()) {
        LOG(QString("[MyListCardManager] createCardForIndex: index %1 out of range (size=%2)")
            .arg(index).arg(m_orderedAnimeIds.size()));
        return nullptr;
    }
    
    int aid = m_orderedAnimeIds[index];
    locker.unlock();
    
    LOG(QString("[MyListCardManager] createCardForIndex: creating card for index=%1, aid=%2").arg(index).arg(aid));
    
    // Create the card using the existing createCard method
    AnimeCard* card = createCard(aid);
    if (!card) {
        LOG(QString("[MyListCardManager] createCardForIndex: createCard returned null for aid=%1").arg(aid));
    }
    return card;
}

void MyListCardManager::clearAllCards()
{
    QMutexLocker locker(&m_mutex);
    
    // Clear virtual layout if used
    if (m_virtualLayout) {
        m_virtualLayout->clear();
    }
    
    if (m_layout) {
        // Remove cards from layout and delete them
        const auto cardValues = m_cards.values();
        for (AnimeCard* const card : cardValues) {
            m_layout->removeWidget(card);
            delete card;
        }
    }
    
    m_cards.clear();
    m_orderedAnimeIds.clear();
    m_cardCreationDataCache.clear();  // Clear the comprehensive card creation data cache
    m_episodesNeedingData.clear();
    m_animeNeedingMetadata.clear();
    m_animeNeedingPoster.clear();
    m_animePicnames.clear();
    // Note: NOT clearing m_animeMetadataRequested to prevent re-requesting
}

AnimeCard* MyListCardManager::getCard(int aid)
{
    QMutexLocker locker(&m_mutex);
    return m_cards.value(aid, nullptr);
}

bool MyListCardManager::hasCard(int aid) const
{
    QMutexLocker locker(&m_mutex);
    return m_cards.contains(aid);
}

QList<AnimeCard*> MyListCardManager::getAllCards() const
{
    QMutexLocker locker(&m_mutex);
    return m_cards.values();
}

MyListCardManager::CachedAnimeData MyListCardManager::getCachedAnimeData(int aid) const
{
    QMutexLocker locker(&m_mutex);
    CachedAnimeData result;
    
    if (!m_cardCreationDataCache.contains(aid)) {
        return result;  // Return empty data with hasData = false
    }
    
    const CardCreationData& data = m_cardCreationDataCache[aid];
    
    // Determine the anime name using the same logic as createCard()
    QString animeName;
    if (!data.nameRomaji.isEmpty()) {
        animeName = data.nameRomaji;
    } else if (!data.nameEnglish.isEmpty()) {
        animeName = data.nameEnglish;
    } else if (!data.animeTitle.isEmpty()) {
        animeName = data.animeTitle;
    } else {
        animeName = QString("Anime %1").arg(aid);
    }
    
    result.setAnimeName(animeName);
    result.setTypeName(data.typeName);
    result.setStartDate(data.startDate);
    result.setEndDate(data.endDate);
    result.setIsHidden(data.isHidden);
    result.setIs18Restricted(data.is18Restricted);
    result.setEptotal(data.eptotal);
    result.setStats(data.stats);
    
    // Calculate lastPlayed from episodes
    qint64 maxLastPlayed = 0;
    for (const EpisodeCacheEntry& episode : data.episodes) {
        if (episode.lastPlayed > maxLastPlayed) {
            maxLastPlayed = episode.lastPlayed;
        }
    }
    result.setLastPlayed(maxLastPlayed);
    
    result.setHasData(data.hasData);
    
    return result;
}

bool MyListCardManager::hasCachedData(int aid) const
{
    QMutexLocker locker(&m_mutex);
    return m_cardCreationDataCache.contains(aid) && m_cardCreationDataCache[aid].hasData;
}

void MyListCardManager::updateCardAnimeInfo(int aid)
{
    QMutexLocker locker(&m_mutex);
    
    // Add to pending updates for batch processing
    m_pendingCardUpdates.insert(aid);
    
    // Start timer if not already running
    if (!m_batchUpdateTimer->isActive()) {
        m_batchUpdateTimer->start();
    }
}

void MyListCardManager::updateCardEpisode(int aid, int eid)
{
    // Defer to full card update for now
    // Could be optimized to only update specific episode in the future
    updateCardAnimeInfo(aid);
}

void MyListCardManager::updateCardStatistics(int aid)
{
    updateCardAnimeInfo(aid);
}

void MyListCardManager::refreshAllCards()
{
    LOG("[MyListCardManager] Refreshing all cards to update file markings");
    
    // Get all current aids and recreate cards to pick up new file marks
    QList<int> aids = m_orderedAnimeIds;
    
    // Clear and recreate all cards
    for (int aid : aids) {
        if (m_cards.contains(aid)) {
            // Remove the old card
            AnimeCard* oldCard = m_cards.take(aid);
            if (oldCard) {
                oldCard->deleteLater();
            }
        }
    }
    
    // Recreate cards with updated file marks
    for (int aid : aids) {
        createCard(aid);
    }
    
    // Notify the virtual layout to refresh its widget references
    // This is critical because the old widgets are now scheduled for deletion
    // and the layout must fetch the new widgets from the factory
    if (m_virtualLayout) {
        LOG("[MyListCardManager] Refreshing virtual layout after all cards refresh");
        m_virtualLayout->refresh();
    }
    
    LOG(QString("[MyListCardManager] Refreshed %1 cards").arg(aids.size()));
    emit allCardsLoaded(aids.size());
}

void MyListCardManager::refreshCardsForLids(const QSet<int>& lids)
{
    if (lids.isEmpty()) {
        return;
    }
    
    // Find the unique anime IDs that contain these lids
    QSet<int> aidsToRefresh;
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open in refreshCardsForLids");
        return;
    }
    
    // Query the anime IDs for the given lids using a single IN clause query
    // Note: lids come from internal tracking (not user input) and are converted
    // to strings via QString::number(), ensuring they contain only numeric characters.
    QStringList lidStrings;
    lidStrings.reserve(lids.size());
    for (int lid : lids) {
        lidStrings.append(QString::number(lid));
    }
    QString lidsList = lidStrings.join(",");
    
    QSqlQuery q(db);
    QString queryStr = QString("SELECT DISTINCT aid FROM mylist WHERE lid IN (%1)").arg(lidsList);
    if (q.exec(queryStr)) {
        while (q.next()) {
            aidsToRefresh.insert(q.value(0).toInt());
        }
    } else {
        LOG(QString("[MyListCardManager] Failed to query aids for lids: %1").arg(q.lastError().text()));
    }
    
    if (aidsToRefresh.isEmpty()) {
        LOG(QString("[MyListCardManager] No cards to refresh for %1 lids").arg(lids.size()));
        return;
    }
    
    LOG(QString("[MyListCardManager] Refreshing %1 cards for %2 updated lids")
        .arg(aidsToRefresh.size()).arg(lids.size()));
    
    // Refresh only the affected cards
    for (int aid : aidsToRefresh) {
        if (m_cards.contains(aid)) {
            // Remove the old card
            AnimeCard* oldCard = m_cards.take(aid);
            if (oldCard) {
                oldCard->deleteLater();
            }
            // Recreate with updated data
            createCard(aid);
        }
    }
    
    // Notify the virtual layout to refresh its widget references
    // This is critical because the old widgets are now scheduled for deletion
    // and the layout must fetch the new widgets from the factory
    if (m_virtualLayout) {
        LOG("[MyListCardManager] Refreshing virtual layout after card updates");
        m_virtualLayout->refresh();
    }
    
    LOG(QString("[MyListCardManager] Refreshed %1 cards").arg(aidsToRefresh.size()));
}

void MyListCardManager::updateCardPoster(int aid, const QString &picname)
{
    if (picname.isEmpty()) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    m_animePicnames[aid] = picname;
    m_animeNeedingPoster.insert(aid);
    
    // Release lock before calling download
    locker.unlock();
    
    downloadPoster(aid, picname);
}

void MyListCardManager::updateMultipleCards(const QSet<int> &aids)
{
    QMutexLocker locker(&m_mutex);
    
    m_pendingCardUpdates.unite(aids);
    
    if (!m_batchUpdateTimer->isActive()) {
        m_batchUpdateTimer->start();
    }
}

void MyListCardManager::updateOrAddMylistEntry(int lid)
{
    // Query the database for this mylist entry
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT aid FROM mylist WHERE lid = ?");
    q.addBindValue(lid);
    
    if (!q.exec() || !q.next()) {
        LOG(QString("[MyListCardManager] Error querying mylist entry lid=%1: %2")
            .arg(lid).arg(q.lastError().text()));
        return;
    }
    
    int aid = q.value(0).toInt();
    
    // Check if card exists
    QMutexLocker locker(&m_mutex);
    bool isNewAnime = !m_cards.contains(aid);
    locker.unlock();
    
    if (isNewAnime) {
        // Card doesn't exist, create it
        // First, preload the card creation data to avoid race conditions
        QList<int> aidList{aid};
        preloadCardCreationData(aidList);
        
        AnimeCard *card = createCard(aid);
        if (!card) {
            LOG(QString("[MyListCardManager] Failed to create card for aid=%1").arg(aid));
        } else if (m_initialLoadComplete) {
            // This is a BRAND NEW anime added after initial load - notify for session auto-start
            LOG(QString("[MyListCardManager] New anime aid=%1 added after initial load").arg(aid));
            emit newAnimeAdded(aid);
        }
    } else {
        // Card exists, update it
        updateCardAnimeInfo(aid);
    }
}

void MyListCardManager::onEpisodeUpdated(int eid, int aid)
{
    // Schedule card update
    updateCardEpisode(aid, eid);
    
    // Remove from tracking
    QMutexLocker locker(&m_mutex);
    m_episodesNeedingData.remove(eid);
}

void MyListCardManager::onAnimeUpdated(int aid)
{
    // Schedule card update
    updateCardAnimeInfo(aid);
    
    // Remove from tracking
    QMutexLocker locker(&m_mutex);
    m_animeNeedingMetadata.remove(aid);
    AnimeCard *card = m_cards.value(aid, nullptr);
    
    // Hide warning only if both metadata and poster are no longer needed
    bool stillNeedsData = m_animeNeedingPoster.contains(aid);
    locker.unlock();
    
    if (card && !stillNeedsData) {
        card->setNeedsFetch(false);
    }
}

void MyListCardManager::onFetchDataRequested(int aid)
{
    LOG(QString("[MyListCardManager] Fetch data requested for anime %1").arg(aid));
    
    QMutexLocker locker(&m_mutex);
    
    // Check what data is missing
    bool needsMetadata = m_animeNeedingMetadata.contains(aid);
    bool needsPoster = m_animeNeedingPoster.contains(aid);
    bool hasEpisodesNeedingData = false;
    QSet<int> episodesNeedingData;
    
    // Check if any episodes need data
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        QSqlQuery q(db);
        // Check for episodes that either:
        // 1. Don't exist in episode table at all (e.eid IS NULL)
        // 2. Exist but have missing/empty name or epno fields
        q.prepare("SELECT DISTINCT m.eid FROM mylist m "
                 "LEFT JOIN episode e ON m.eid = e.eid "
                 "WHERE m.aid = ? AND m.eid > 0 AND (e.eid IS NULL OR e.name IS NULL OR e.name = '' OR e.epno IS NULL OR e.epno = '')");
        q.addBindValue(aid);
        if (q.exec()) {
            LOG(QString("[MyListCardManager] Checking episodes for aid=%1").arg(aid));
            while (q.next()) {
                int eid = q.value(0).toInt();
                LOG(QString("[MyListCardManager]   Found episode needing data: eid=%1").arg(eid));
                if (eid > 0) {
                    hasEpisodesNeedingData = true;
                    episodesNeedingData.insert(eid);
                }
            }
        } else {
            LOG(QString("[MyListCardManager] Failed to query episodes needing data: %1").arg(q.lastError().text()));
        }
    }
    
    LOG(QString("[MyListCardManager] Data check for aid=%1: needsMetadata=%2, needsPoster=%3, hasEpisodesNeedingData=%4 (count=%5), alreadyRequested=%6")
        .arg(aid).arg(needsMetadata).arg(needsPoster).arg(hasEpisodesNeedingData).arg(episodesNeedingData.size()).arg(m_animeMetadataRequested.contains(aid)));
    
    bool requestedAnything = false;
    
    // Request metadata if needed and not already requested
    if (!m_animeMetadataRequested.contains(aid)) {
        m_animeMetadataRequested.insert(aid);
        needsMetadata = true;
        requestedAnything = true;
        LOG(QString("[MyListCardManager] Will request anime metadata for aid=%1").arg(aid));
    }
    
    // Request poster if needed and not already downloaded
    if (m_animeNeedingPoster.contains(aid) && m_animePicnames.contains(aid)) {
        QString picname = m_animePicnames[aid];
        needsPoster = true;
        requestedAnything = true;
        LOG(QString("[MyListCardManager] Will download poster for aid=%1, picname=%2").arg(aid).arg(picname));
        locker.unlock();
        
        if (needsMetadata) {
            requestAnimeMetadata(aid);
        }
        if (needsPoster) {
            downloadPoster(aid, picname);
        }
    } else {
        locker.unlock();
        if (needsMetadata) {
            requestAnimeMetadata(aid);
        }
    }
    
    // Request episode data for episodes that need it
    if (hasEpisodesNeedingData) {
        LOG(QString("[MyListCardManager] Requesting episode data for %1 episodes of aid=%2").arg(episodesNeedingData.size()).arg(aid));
        for (int eid : episodesNeedingData) {
            LOG(QString("[MyListCardManager] Emitting episodeDataRequested signal for eid=%1").arg(eid));
            emit episodeDataRequested(eid);
        }
        requestedAnything = true;
    }
    
    if (!requestedAnything) {
        LOG(QString("[MyListCardManager] No data needs to be fetched for aid=%1 (already complete or requested)").arg(aid));
    }
}

void MyListCardManager::onPosterDownloadFinished(QNetworkReply *reply)
{
    // Get the anime ID for this download
    QMutexLocker locker(&m_mutex);
    
    if (!m_posterDownloadRequests.contains(reply)) {
        reply->deleteLater();
        return;
    }
    
    int aid = m_posterDownloadRequests.take(reply);
    AnimeCard *card = m_cards.value(aid, nullptr);
    
    locker.unlock();
    
    if (!card) {
        LOG(QString("[MyListCardManager] Card not found for poster download aid=%1").arg(aid));
        reply->deleteLater();
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        LOG(QString("[MyListCardManager] Poster download error for aid=%1: %2")
            .arg(aid).arg(reply->errorString()));
        reply->deleteLater();
        return;
    }
    
    QByteArray imageData = reply->readAll();
    reply->deleteLater();
    
    if (imageData.isEmpty()) {
        LOG(QString("[MyListCardManager] Empty poster data for aid=%1").arg(aid));
        return;
    }
    
    // Load and set poster
    QPixmap poster;
    if (poster.loadFromData(imageData)) {
        card->setPoster(poster);
        
        // Remove from tracking
        QMutexLocker locker(&m_mutex);
        m_animeNeedingPoster.remove(aid);
        
        // Hide warning if metadata is also no longer needed
        bool stillNeedsData = m_animeNeedingMetadata.contains(aid);
        locker.unlock();
        
        if (!stillNeedsData) {
            card->setNeedsFetch(false);
        }
        
        // Store in database for future use
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("UPDATE anime SET poster_image = ? WHERE aid = ?");
            q.addBindValue(imageData);
            q.addBindValue(aid);
            if (!q.exec()) {
                LOG(QString("[MyListCardManager] Failed to store poster for aid=%1: %2")
                    .arg(aid).arg(q.lastError().text()));
            }
        }
        
        emit cardUpdated(aid);
        LOG(QString("[MyListCardManager] Updated poster for aid=%1").arg(aid));
    } else {
        LOG(QString("[MyListCardManager] Failed to load poster image for aid=%1").arg(aid));
    }
}

void MyListCardManager::processBatchedUpdates()
{
    QSet<int> toUpdate;
    
    {
        QMutexLocker locker(&m_mutex);
        toUpdate = m_pendingCardUpdates;
        m_pendingCardUpdates.clear();
    }
    
    if (toUpdate.isEmpty()) {
        return;
    }
    
    LOG(QString("[MyListCardManager] Processing %1 batched card updates").arg(toUpdate.size()));
    
    for (const int aid : std::as_const(toUpdate)) {
        updateCardFromDatabase(aid);
    }
}

// Helper function to parse tag lists into TagInfo objects
static QList<AnimeCard::TagInfo> parseTags(const QString& tagNames, const QString& tagIds, const QString& tagWeights)
{
    QList<AnimeCard::TagInfo> tags;
    
    if (tagNames.isEmpty() || tagIds.isEmpty() || tagWeights.isEmpty()) {
        return tags;
    }
    
    QStringList names = tagNames.split(',');
    QStringList ids = tagIds.split(',');
    QStringList weights = tagWeights.split(',');
    
    // All three lists should have the same size
    int count = qMin(qMin(names.size(), ids.size()), weights.size());
    
    for (int i = 0; i < count; ++i) {
        AnimeCard::TagInfo tag(names[i].trimmed(), 
                               ids[i].trimmed().toInt(), 
                               weights[i].trimmed().toInt());
        tags.append(tag);
    }
    
    // Sort by weight (highest first)
    std::sort(tags.begin(), tags.end());
    
    return tags;
}

QString MyListCardManager::determineAnimeName(const QString& nameRomaji, const QString& nameEnglish, const QString& animeTitle, int aid)
{
    return AnimeUtils::determineAnimeName(nameRomaji, nameEnglish, animeTitle, aid);
}

QList<AnimeCard::TagInfo> MyListCardManager::getTagsOrCategoryFallback(const QString& tagNames, const QString& tagIds, const QString& tagWeights, const QString& category)
{
    QList<AnimeCard::TagInfo> tags = parseTags(tagNames, tagIds, tagWeights);
    
    if (!tags.isEmpty()) {
        return tags;
    }
    
    // Fallback to category if no tag data
    if (category.isEmpty()) {
        return tags;  // Empty list
    }
    
    QList<AnimeCard::TagInfo> categoryTags;
    QStringList categoryNames = category.split(',');
    int weight = 1000;  // Arbitrary high weight for category fallback
    
    for (const QString& catName : std::as_const(categoryNames)) {
        AnimeCard::TagInfo tag(catName.trimmed(), 0, weight--);
        categoryTags.append(tag);
    }
    
    return categoryTags;
}

void MyListCardManager::updateCardAiredDates(AnimeCard* card, const QString& startDate, const QString& endDate)
{
    if (!startDate.isEmpty()) {
        aired airedDates(startDate, endDate);
        card->setAired(airedDates);
    } else {
        card->setAiredText("Unknown");
        
        // Track that this anime needs metadata if we have access to aid
        int aid = card->getAnimeId();
        if (aid > 0) {
            QMutexLocker locker(&m_mutex);
            m_animeNeedingMetadata.insert(aid);
        }
    }
}

AnimeCard* MyListCardManager::createCard(int aid)
{
    // Check if card already exists - prevent duplicates
    if (hasCard(aid)) {
        LOG(QString("[MyListCardManager] Card already exists for aid=%1, skipping duplicate creation").arg(aid));
        return m_cards[aid];
    }
    
    // Get data from comprehensive cache - NO SQL QUERIES HERE
    if (!m_cardCreationDataCache.contains(aid)) {
        LOG(QString("[MyListCardManager] ERROR: No card creation data for aid=%1 - data must be preloaded first!").arg(aid));
        return nullptr;
    }
    
    const CardCreationData& data = m_cardCreationDataCache[aid];
    
    // Determine anime name
    QString animeName = determineAnimeName(data.nameRomaji, data.nameEnglish, data.animeTitle, aid);
    if (animeName.isEmpty()) {
        animeName = QString("Anime %1").arg(aid);
    }
    
    // Create card
    AnimeCard *card = new AnimeCard(nullptr);
    card->setAnimeId(aid);
    card->setAnimeTitle(animeName);
    card->setHidden(data.isHidden);
    card->setIs18Restricted(data.is18Restricted);
    
    // Set type
    if (!data.typeName.isEmpty()) {
        card->setAnimeType(data.typeName);
    } else {
        card->setAnimeType("Unknown");
        m_animeNeedingMetadata.insert(aid);
    }
    
    // Set aired dates using helper
    updateCardAiredDates(card, data.startDate, data.endDate);
    
    // Set tags using helper (handles category fallback)
    QList<AnimeCard::TagInfo> tags = getTagsOrCategoryFallback(data.tagNameList, data.tagIdList, data.tagWeightList, data.category);
    if (!tags.isEmpty()) {
        card->setTags(tags);
    }
    
    // Set rating
    if (!data.rating.isEmpty()) {
        card->setRating(data.rating);
    }
    
    // Load poster asynchronously
    if (!data.posterData.isEmpty()) {
        // Defer poster loading to avoid blocking
        QByteArray posterDataCopy = data.posterData; // Copy for lambda capture
        QMetaObject::invokeMethod(this, [this, card, posterDataCopy]() {
            QPixmap poster;
            if (poster.loadFromData(posterDataCopy)) {
                card->setPoster(poster);
            }
        }, Qt::QueuedConnection);
    } else if (!data.picname.isEmpty()) {
        m_animePicnames[aid] = data.picname;
        m_animeNeedingPoster.insert(aid);
        // Disabled auto-download - user can request via context menu
        // downloadPoster(aid, picname);
    } else {
        m_animeNeedingPoster.insert(aid);
        m_animeNeedingMetadata.insert(aid);
    }
    
    // Load episodes from cache (no SQL queries)
    if (!data.episodes.isEmpty()) {
        loadEpisodesForCardFromCache(card, aid, data.episodes);
    }
    
    // Set statistics from cache
    int totalNormalEpisodes = data.eptotal;
    if (totalNormalEpisodes <= 0) {
        totalNormalEpisodes = data.stats.normalEpisodes();
    }
    card->setStatistics(data.stats.normalEpisodes(), totalNormalEpisodes, 
                       data.stats.normalViewed(), data.stats.otherEpisodes(), data.stats.otherViewed());
    
    // Add to cache first (before layout to avoid triggering layout updates prematurely)
    QMutexLocker locker(&m_mutex);
    m_cards[aid] = card;
    
    // Add to layout only if not using virtual scrolling
    // In virtual scrolling mode, the VirtualFlowLayout handles widget positioning
    if (m_layout && !m_virtualLayout) {
        m_layout->addWidget(card);
    }
    locker.unlock();
    
    // Connect fetch data request signal from card
    connect(card, &AnimeCard::fetchDataRequested, this, &MyListCardManager::onFetchDataRequested);
    
    // Connect hide card request signal
    connect(card, &AnimeCard::hideCardRequested, this, &MyListCardManager::onHideCardRequested);
    
    // Connect mark episode watched signal
    connect(card, &AnimeCard::markEpisodeWatchedRequested, this, &MyListCardManager::onMarkEpisodeWatchedRequested);
    
    // Connect mark file watched signal
    connect(card, &AnimeCard::markFileWatchedRequested, this, &MyListCardManager::onMarkFileWatchedRequested);
    
    // Show warning indicator if metadata or poster is missing (instead of auto-fetching)
    if (m_animeNeedingMetadata.contains(aid) || m_animeNeedingPoster.contains(aid)) {
        card->setNeedsFetch(true);
    }
    
    emit cardCreated(aid, card);
    
    return card;
}

void MyListCardManager::updateCardFromDatabase(int aid)
{
    QMutexLocker locker(&m_mutex);
    AnimeCard *card = m_cards.value(aid, nullptr);
    locker.unlock();
    
    if (!card) {
        LOG(QString("[MyListCardManager] Card not found for update aid=%1").arg(aid));
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Query updated anime data
    QSqlQuery q(db);
    q.prepare("SELECT a.nameromaji, a.nameenglish, "
              "at.title as anime_title, "
              "a.eps, a.typename, a.startdate, a.enddate, a.picname, a.poster_image, a.category, "
              "a.rating, a.tag_name_list, a.tag_id_list, a.tag_weight_list "
              "FROM anime a "
              "LEFT JOIN anime_titles at ON a.aid = at.aid AND at.type = 1 "
              "WHERE a.aid = ?");
    q.addBindValue(aid);
    
    if (!q.exec() || !q.next()) {
        LOG(QString("[MyListCardManager] Failed to query anime for update aid=%1: %2")
            .arg(aid).arg(q.lastError().text()));
        return;
    }
    
    QString animeName = q.value(0).toString();
    QString animeNameEnglish = q.value(1).toString();
    QString animeTitle = q.value(2).toString();
    int eps = q.value(3).toInt();
    QString typeName = q.value(4).toString();
    QString startDate = q.value(5).toString();
    QString endDate = q.value(6).toString();
    QString picname = q.value(7).toString();
    QByteArray posterData = q.value(8).toByteArray();
    QString category = q.value(9).toString();
    QString rating = q.value(10).toString();
    QString tagNameList = q.value(11).toString();
    QString tagIdList = q.value(12).toString();
    QString tagWeightList = q.value(13).toString();
    
    // Update anime name using helper
    animeName = determineAnimeName(animeName, animeNameEnglish, animeTitle, aid);
    if (!animeName.isEmpty()) {
        card->setAnimeTitle(animeName);
    }
    
    // Update type
    if (!typeName.isEmpty()) {
        card->setAnimeType(typeName);
    }
    
    // Update aired dates using helper
    if (!startDate.isEmpty()) {
        aired airedDates(startDate, endDate);
        card->setAired(airedDates);
    }
    
    // Update tags using helper (handles category fallback)
    QList<AnimeCard::TagInfo> tags = getTagsOrCategoryFallback(tagNameList, tagIdList, tagWeightList, category);
    if (!tags.isEmpty()) {
        card->setTags(tags);
    }
    
    // Update rating
    if (!rating.isEmpty()) {
        card->setRating(rating);
    }
    
    // Update poster if available
    if (!posterData.isEmpty()) {
        QPixmap poster;
        if (poster.loadFromData(posterData)) {
            card->setPoster(poster);
        }
    } else if (!picname.isEmpty() && !m_animePicnames.contains(aid)) {
        m_animePicnames[aid] = picname;
        downloadPoster(aid, picname);
    }
    
    // Reload episodes
    card->clearEpisodes();
    loadEpisodesForCard(card, aid);
    
    // Recalculate and update statistics
    AnimeStats stats = calculateStatistics(aid);
    int totalNormalEpisodes = (eps > 0) ? eps : stats.normalEpisodes();
    card->setStatistics(stats.normalEpisodes(), totalNormalEpisodes,
                       stats.normalViewed(), stats.otherEpisodes(), stats.otherViewed());
    
    emit cardUpdated(aid);
    emit cardNeedsSorting(aid);
}

void MyListCardManager::loadEpisodesForCard(AnimeCard *card, int aid)
{
    // Query episode data directly from the database
    // This is used by updateCardFromDatabase() to reload episodes after metadata updates
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG(QString("[MyListCardManager] Database not open in loadEpisodesForCard for aid=%1").arg(aid));
        return;
    }
    
    QList<EpisodeCacheEntry> episodes;
    
    QSqlQuery q(db);
    q.prepare("SELECT m.lid, m.eid, m.fid, m.state, m.viewed, m.storage, "
              "e.name as episode_name, e.epno, "
              "f.filename, m.last_played, "
              "lf.path as local_file_path, "
              "f.resolution, f.quality, "
              "g.name as group_name, "
              "m.local_watched "
              "FROM mylist m "
              "LEFT JOIN episode e ON m.eid = e.eid "
              "LEFT JOIN file f ON m.fid = f.fid "
              "LEFT JOIN local_files lf ON m.local_file = lf.id "
              "LEFT JOIN `group` g ON m.gid = g.gid "
              "WHERE m.aid = ? "
              "ORDER BY e.epno, m.lid");
    q.addBindValue(aid);
    
    if (q.exec()) {
        while (q.next()) {
            EpisodeCacheEntry entry;
            entry.lid = q.value(0).toInt();
            entry.eid = q.value(1).toInt();
            entry.fid = q.value(2).toInt();
            entry.state = q.value(3).toInt();
            entry.viewed = q.value(4).toInt();
            entry.storage = q.value(5).toString();
            entry.episodeName = q.value(6).toString();
            entry.epno = q.value(7).toString();
            entry.filename = q.value(8).toString();
            entry.lastPlayed = q.value(9).toLongLong();
            entry.localFilePath = q.value(10).toString();
            entry.resolution = q.value(11).toString();
            entry.quality = q.value(12).toString();
            entry.groupName = q.value(13).toString();
            entry.localWatched = q.value(14).toInt();
            
            episodes.append(entry);
        }
    } else {
        LOG(QString("[MyListCardManager] Failed to query episodes for aid=%1: %2")
            .arg(aid).arg(q.lastError().text()));
        return;
    }
    
    // Use the shared helper function to load episodes into the card
    loadEpisodesForCardFromCache(card, aid, episodes);
    
    LOG(QString("[MyListCardManager] Loaded %1 episode entries for aid=%2").arg(episodes.size()).arg(aid));
}

void MyListCardManager::loadEpisodesForCardFromCache(AnimeCard *card, int aid, const QList<EpisodeCacheEntry>& episodes)
{
    // Load episodes from the provided cache data - NO SQL QUERIES
    // Group files by episode
    QMap<int, AnimeCard::EpisodeInfo> episodeMap;
    QMap<int, int> episodeFileCount;
    
    for (const EpisodeCacheEntry& entry : episodes) {
        int eid = entry.eid;
        
        // Get or create episode entry
        if (!episodeMap.contains(eid)) {
            AnimeCard::EpisodeInfo episodeInfo;
            episodeInfo.setEid(eid);
            
            if (!entry.epno.isEmpty()) {
                episodeInfo.setEpisodeNumber(::epno(entry.epno));
            }
            
            episodeInfo.setEpisodeTitle(entry.episodeName.isEmpty() ? "Episode" : entry.episodeName);
            
            if (entry.episodeName.isEmpty()) {
                m_episodesNeedingData.insert(eid);
            }
            
            episodeMap[eid] = episodeInfo;
            episodeFileCount[eid] = 0;
        }
        
        // Create file info
        AnimeCard::FileInfo fileInfo;
        fileInfo.setLid(entry.lid);
        fileInfo.setFid(entry.fid);
        fileInfo.setFileName(entry.filename.isEmpty() ? QString("FID:%1").arg(entry.fid) : entry.filename);
        
        // State string
        QString stateStr;
        switch(entry.state) {
            case 0: stateStr = "Unknown"; break;
            case 1: stateStr = "HDD"; break;
            case 2: stateStr = "CD/DVD"; break;
            case 3: stateStr = "Deleted"; break;
            default: stateStr = QString::number(entry.state); break;
        }
        fileInfo.setState(stateStr);
        
        fileInfo.setViewed(entry.viewed != 0);
        fileInfo.setLocalWatched(entry.localWatched != 0);
        fileInfo.setStorage(!entry.localFilePath.isEmpty() ? entry.localFilePath : entry.storage);
        fileInfo.setLocalFilePath(entry.localFilePath);  // Store local file path for existence check
        fileInfo.setLastPlayed(entry.lastPlayed);
        fileInfo.setResolution(entry.resolution);
        fileInfo.setQuality(entry.quality);
        fileInfo.setGroupName(entry.groupName);
        
        episodeFileCount[eid]++;
        fileInfo.setVersion(episodeFileCount[eid]);
        
        // Add file to episode
        episodeMap[eid].files().append(fileInfo);
    }
    
    // Add all episodes to card in sorted order
    QList<AnimeCard::EpisodeInfo> episodeList = episodeMap.values();
    std::sort(episodeList.begin(), episodeList.end(), 
              [](const AnimeCard::EpisodeInfo& a, const AnimeCard::EpisodeInfo& b) {
        if (a.episodeNumber().isValid() && b.episodeNumber().isValid()) {
            return a.episodeNumber() < b.episodeNumber();
        }
        if (!a.episodeNumber().isValid()) {
            return false;
        }
        if (!b.episodeNumber().isValid()) {
            return true;
        }
        return false;
    });
    
    for (const AnimeCard::EpisodeInfo& episodeInfo : std::as_const(episodeList)) {
        card->addEpisode(episodeInfo);
    }
}

void MyListCardManager::requestAnimeMetadata(int aid)
{
    if (adbapi) {
        LOG(QString("[MyListCardManager] Requesting metadata for anime %1").arg(aid));
        adbapi->Anime(aid);
    }
}

void MyListCardManager::downloadPoster(int aid, const QString &picname)
{
    if (picname.isEmpty()) {
        return;
    }
    
    // AniDB CDN URL for anime posters
    QString url = QString("http://img7.anidb.net/pics/anime/%1").arg(picname);
    
    LOG(QString("[MyListCardManager] Downloading poster for anime %1 from %2").arg(aid).arg(url));
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Usagi/1");
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    QMutexLocker locker(&m_mutex);
    m_posterDownloadRequests[reply] = aid;
}

MyListCardManager::AnimeStats MyListCardManager::calculateStatistics(int aid)
{
    AnimeStats stats{};  // Explicit value initialization
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return stats;
    }
    
    // Query to calculate statistics
    QSqlQuery q(db);
    q.prepare("SELECT e.epno, m.viewed "
              "FROM mylist m "
              "LEFT JOIN episode e ON m.eid = e.eid "
              "WHERE m.aid = ? "
              "GROUP BY m.eid");
    q.addBindValue(aid);
    
    QSet<int> normalEpisodes;
    QSet<int> otherEpisodes;
    QSet<int> viewedNormalEpisodes;
    QSet<int> viewedOtherEpisodes;
    
    if (q.exec()) {
        while (q.next()) {
            QString epnoStr = q.value(0).toString();
            int viewed = q.value(1).toInt();
            int eid = q.value(0).toInt(); // Using first column as eid placeholder
            
            if (!epnoStr.isEmpty()) {
                ::epno episodeNumber(epnoStr);
                int epType = episodeNumber.type();
                
                if (epType == 1) {
                    normalEpisodes.insert(eid);
                    if (viewed) {
                        viewedNormalEpisodes.insert(eid);
                    }
                } else if (epType > 1) {
                    otherEpisodes.insert(eid);
                    if (viewed) {
                        viewedOtherEpisodes.insert(eid);
                    }
                }
            } else {
                normalEpisodes.insert(eid);
                if (viewed) {
                    viewedNormalEpisodes.insert(eid);
                }
            }
        }
    }
    
    stats.setNormalEpisodes(normalEpisodes.size());
    stats.setOtherEpisodes(otherEpisodes.size());
    stats.setNormalViewed(viewedNormalEpisodes.size());
    stats.setOtherViewed(viewedOtherEpisodes.size());
    
    return stats;
}

void MyListCardManager::preloadAnimeDataCache(const QList<int>& aids)
{
    // Deprecated: This function now calls the comprehensive preloadCardCreationData()
    // Kept for backward compatibility
    preloadCardCreationData(aids);
}

void MyListCardManager::preloadEpisodesCache(const QList<int>& aids)
{
    // Deprecated: This function now calls the comprehensive preloadCardCreationData()
    // Kept for backward compatibility
    preloadCardCreationData(aids);
}

void MyListCardManager::preloadCardCreationData(const QList<int>& aids)
{
    if (aids.isEmpty()) {
        return;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    LOG(QString("[MyListCardManager] Starting comprehensive preload for %1 anime").arg(aids.size()));
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Clear the cache before repopulating
    m_cardCreationDataCache.clear();
    
    // Build IN clause for bulk queries
    // Note: aids come from internal database queries (not user input) and are converted
    // to strings via QString::number(), ensuring they contain only numeric characters.
    // This makes SQL injection impossible as no user-controlled data enters the query.
    QStringList aidStrings;
    aidStrings.reserve(aids.size());
    for (int aid : aids) {
        aidStrings.append(QString::number(aid));
    }
    QString aidsList = aidStrings.join(",");
    
    // Step 1: Load anime basic data and titles
    qint64 step1Start = timer.elapsed();
    QString animeQuery = QString("SELECT a.aid, a.nameromaji, a.nameenglish, a.eptotal, "
                                "at.title as anime_title, "
                                "a.typename, a.startdate, a.enddate, a.picname, a.poster_image, a.category, "
                                "a.rating, a.tag_name_list, a.tag_id_list, a.tag_weight_list, a.hidden, a.is_18_restricted, "
                                "a.relaidlist, a.relaidtype "
                                "FROM anime a "
                                "LEFT JOIN anime_titles at ON a.aid = at.aid AND at.type = 1 "
                                "WHERE a.aid IN (%1)").arg(aidsList);
    
    QSqlQuery q(db);
    if (q.exec(animeQuery)) {
        while (q.next()) {
            int aid = q.value(0).toInt();
            CardCreationData& data = m_cardCreationDataCache[aid];
            
            data.nameRomaji = q.value(1).toString();
            data.nameEnglish = q.value(2).toString();
            data.eptotal = q.value(3).toInt();
            data.animeTitle = q.value(4).toString();
            data.typeName = q.value(5).toString();
            data.startDate = q.value(6).toString();
            data.endDate = q.value(7).toString();
            data.picname = q.value(8).toString();
            data.posterData = q.value(9).toByteArray();
            data.category = q.value(10).toString();
            data.rating = q.value(11).toString();
            data.tagNameList = q.value(12).toString();
            data.tagIdList = q.value(13).toString();
            data.tagWeightList = q.value(14).toString();
            data.isHidden = q.value(15).toInt() == 1;
            data.is18Restricted = q.value(16).toInt() == 1;
            data.relaidlist = q.value(17).toString();
            data.relaidtype = q.value(18).toString();
            data.hasData = true;
        }
    }
    qint64 step1Elapsed = timer.elapsed() - step1Start;
    LOG(QString("[MyListCardManager] Step 1: Loaded anime data for %1 anime in %2 ms")
        .arg(m_cardCreationDataCache.size()).arg(step1Elapsed));
    
    // Step 2: Load anime titles for anime without anime table data
    qint64 step2Start = timer.elapsed();
    QString titlesQuery = QString("SELECT aid, title FROM anime_titles "
                                 "WHERE aid IN (%1) AND type = 1 AND language = 'x-jat'").arg(aidsList);
    QSqlQuery tq(db);
    if (tq.exec(titlesQuery)) {
        while (tq.next()) {
            int aid = tq.value(0).toInt();
            QString title = tq.value(1).toString();
            
            // Only set if not already in cache or if it doesn't have data yet
            if (!m_cardCreationDataCache.contains(aid) || !m_cardCreationDataCache[aid].hasData) {
                CardCreationData& data = m_cardCreationDataCache[aid];
                data.animeTitle = title;
                // Mark as having at least minimal data so filtering can work
                data.hasData = true;
            }
        }
    }
    qint64 step2Elapsed = timer.elapsed() - step2Start;
    LOG(QString("[MyListCardManager] Step 2: Loaded anime titles in %1 ms").arg(step2Elapsed));
    
    // Step 3: Load statistics
    qint64 step3Start = timer.elapsed();
    QString statsQuery = QString("SELECT m.aid, e.epno, m.viewed, m.eid "
                                 "FROM mylist m "
                                 "LEFT JOIN episode e ON m.eid = e.eid "
                                 "WHERE m.aid IN (%1) "
                                 "ORDER BY m.aid").arg(aidsList);
    
    QSqlQuery statsQ(db);
    if (statsQ.exec(statsQuery)) {
        QMap<int, QSet<int>> normalEpisodesMap;
        QMap<int, QSet<int>> otherEpisodesMap;
        QMap<int, QSet<int>> viewedNormalEpisodesMap;
        QMap<int, QSet<int>> viewedOtherEpisodesMap;
        
        while (statsQ.next()) {
            int aid = statsQ.value(0).toInt();
            QString epnoStr = statsQ.value(1).toString();
            int viewed = statsQ.value(2).toInt();
            int eid = statsQ.value(3).toInt();
            
            if (!epnoStr.isEmpty()) {
                ::epno episodeNumber(epnoStr);
                int epType = episodeNumber.type();
                
                if (epType == 1) {
                    normalEpisodesMap[aid].insert(eid);
                    if (viewed) {
                        viewedNormalEpisodesMap[aid].insert(eid);
                    }
                } else if (epType > 1) {
                    otherEpisodesMap[aid].insert(eid);
                    if (viewed) {
                        viewedOtherEpisodesMap[aid].insert(eid);
                    }
                }
            } else {
                // If epno is empty, treat as normal episode
                normalEpisodesMap[aid].insert(eid);
                if (viewed) {
                    viewedNormalEpisodesMap[aid].insert(eid);
                }
            }
        }
        
        // Populate stats in card creation data
        QSet<int> aidsWithStats;
        for (auto it = normalEpisodesMap.constBegin(); it != normalEpisodesMap.constEnd(); ++it) {
            aidsWithStats.insert(it.key());
        }
        for (auto it = otherEpisodesMap.constBegin(); it != otherEpisodesMap.constEnd(); ++it) {
            aidsWithStats.insert(it.key());
        }
        
        for (int aid : aidsWithStats) {
            if (m_cardCreationDataCache.contains(aid)) {
                CardCreationData& data = m_cardCreationDataCache[aid];
                data.stats.setNormalEpisodes(normalEpisodesMap[aid].size());
                data.stats.setNormalViewed(viewedNormalEpisodesMap[aid].size());
                data.stats.setOtherEpisodes(otherEpisodesMap[aid].size());
                data.stats.setOtherViewed(viewedOtherEpisodesMap[aid].size());
                data.stats.setTotalNormalEpisodes(0); // Will be set from eptotal
            }
        }
    }
    qint64 step3Elapsed = timer.elapsed() - step3Start;
    LOG(QString("[MyListCardManager] Step 3: Loaded statistics in %1 ms").arg(step3Elapsed));
    
    // Step 4: Load episode details
    qint64 step4Start = timer.elapsed();
    QString episodesQuery = QString("SELECT m.aid, m.lid, m.eid, m.fid, m.state, m.viewed, m.storage, "
                                   "e.name as episode_name, e.epno, "
                                   "f.filename, m.last_played, "
                                   "lf.path as local_file_path, "
                                   "f.resolution, f.quality, "
                                   "g.name as group_name, "
                                   "m.local_watched "
                                   "FROM mylist m "
                                   "LEFT JOIN episode e ON m.eid = e.eid "
                                   "LEFT JOIN file f ON m.fid = f.fid "
                                   "LEFT JOIN local_files lf ON m.local_file = lf.id "
                                   "LEFT JOIN `group` g ON m.gid = g.gid "
                                   "WHERE m.aid IN (%1) "
                                   "ORDER BY m.aid, e.epno, m.lid").arg(aidsList);
    
    QSqlQuery episodesQ(db);
    if (episodesQ.exec(episodesQuery)) {
        while (episodesQ.next()) {
            int aid = episodesQ.value(0).toInt();
            
            if (m_cardCreationDataCache.contains(aid)) {
                EpisodeCacheEntry entry;
                entry.lid = episodesQ.value(1).toInt();
                entry.eid = episodesQ.value(2).toInt();
                entry.fid = episodesQ.value(3).toInt();
                entry.state = episodesQ.value(4).toInt();
                entry.viewed = episodesQ.value(5).toInt();
                entry.storage = episodesQ.value(6).toString();
                entry.episodeName = episodesQ.value(7).toString();
                entry.epno = episodesQ.value(8).toString();
                entry.filename = episodesQ.value(9).toString();
                entry.lastPlayed = episodesQ.value(10).toLongLong();
                entry.localFilePath = episodesQ.value(11).toString();
                entry.resolution = episodesQ.value(12).toString();
                entry.quality = episodesQ.value(13).toString();
                entry.groupName = episodesQ.value(14).toString();
                entry.localWatched = episodesQ.value(15).toInt();
                
                m_cardCreationDataCache[aid].episodes.append(entry);
            }
        }
    }
    qint64 step4Elapsed = timer.elapsed() - step4Start;
    int totalEpisodes = 0;
    for (const CardCreationData& data : std::as_const(m_cardCreationDataCache)) {
        totalEpisodes += data.episodes.size();
    }
    LOG(QString("[MyListCardManager] Step 4: Loaded %1 episodes in %2 ms").arg(totalEpisodes).arg(step4Elapsed));
    
    qint64 totalElapsed = timer.elapsed();
    LOG(QString("[MyListCardManager] Comprehensive preload complete: %1 anime with full data in %2 ms")
        .arg(m_cardCreationDataCache.size()).arg(totalElapsed));
}

void MyListCardManager::onHideCardRequested(int aid)
{
    LOG(QString("[MyListCardManager] Hide card requested for anime %1").arg(aid));
    
    QMutexLocker locker(&m_mutex);
    AnimeCard *card = m_cards.value(aid, nullptr);
    
    if (!card) {
        LOG(QString("[MyListCardManager] Card not found for hide request aid=%1").arg(aid));
        return;
    }
    
    // Toggle hidden state
    bool isHidden = card->isHidden();
    card->setHidden(!isHidden);
    
    // Update database to persist hidden state
    locker.unlock();
    
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        QSqlQuery q(db);
        
        // Update hidden state
        q.prepare("UPDATE anime SET hidden = ? WHERE aid = ?");
        q.addBindValue(!isHidden ? 1 : 0);
        q.addBindValue(aid);
        if (!q.exec()) {
            LOG(QString("[MyListCardManager] Failed to update hidden state for aid=%1: %2")
                .arg(aid).arg(q.lastError().text()));
        } else {
            LOG(QString("[MyListCardManager] Updated hidden state for aid=%1 to %2").arg(aid).arg(!isHidden));
            // Trigger card sorting/repositioning
            emit cardNeedsSorting(aid);
        }
    }
}

void MyListCardManager::onMarkEpisodeWatchedRequested(int eid)
{
    LOG(QString("[MyListCardManager] Mark episode watched requested for eid=%1").arg(eid));
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Get current timestamp for viewdate
    qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
    
    // Update all files for this episode to viewed=1 and local_watched=1 with viewdate
    QSqlQuery q(db);
    q.prepare("UPDATE mylist SET viewed = 1, local_watched = 1, viewdate = ? WHERE eid = ?");
    q.addBindValue(currentTimestamp);
    q.addBindValue(eid);
    
    if (!q.exec()) {
        LOG(QString("[MyListCardManager] Failed to mark episode watched eid=%1: %2")
            .arg(eid).arg(q.lastError().text()));
        return;
    }
    
    int rowsAffected = q.numRowsAffected();
    LOG(QString("[MyListCardManager] Marked %1 file(s) as watched for episode eid=%2").arg(rowsAffected).arg(eid));
    
    // Get all files for this episode to update API
    q.prepare("SELECT m.lid, f.size, f.ed2k, m.aid FROM mylist m "
              "INNER JOIN file f ON m.fid = f.fid "
              "WHERE m.eid = ?");
    q.addBindValue(eid);
    
    if (!q.exec()) {
        LOG(QString("[MyListCardManager] Failed to query files for episode eid=%1: %2")
            .arg(eid).arg(q.lastError().text()));
        return;
    }
    
    int aid = 0;
    // Emit API update requests for each file
    while (q.next()) {
        int lid = q.value(0).toInt();
        int size = q.value(1).toInt();
        QString ed2k = q.value(2).toString();
        aid = q.value(3).toInt();
        
        // Emit signal for API update (will be handled by Window to call anidb->UpdateFile)
        emit fileNeedsApiUpdate(lid, size, ed2k, 1);
    }
    
    if (aid > 0) {
        LOG(QString("[MyListCardManager] Requesting play button update for aid=%1").arg(aid));
        // Emit signal to update play buttons in tree view
        emit playButtonsNeedUpdate(aid);
        
        // Update the card immediately to reflect the change
        updateCardFromDatabase(aid);
    }
}

void MyListCardManager::onMarkFileWatchedRequested(int lid)
{
    LOG(QString("[MyListCardManager] Mark file watched requested for lid=%1").arg(lid));
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Get current timestamp for viewdate
    qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
    
    // Update this specific file to viewed=1 and local_watched=1 with viewdate
    QSqlQuery q(db);
    q.prepare("UPDATE mylist SET viewed = 1, local_watched = 1, viewdate = ? WHERE lid = ?");
    q.addBindValue(currentTimestamp);
    q.addBindValue(lid);
    
    if (!q.exec()) {
        LOG(QString("[MyListCardManager] Failed to mark file watched lid=%1: %2")
            .arg(lid).arg(q.lastError().text()));
        return;
    }
    
    LOG(QString("[MyListCardManager] Marked file lid=%1 as watched").arg(lid));
    
    // Get file info for API update
    q.prepare("SELECT m.aid, f.size, f.ed2k FROM mylist m "
              "INNER JOIN file f ON m.fid = f.fid "
              "WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (!q.exec() || !q.next()) {
        LOG(QString("[MyListCardManager] Failed to find file info for lid=%1").arg(lid));
        return;
    }
    
    int aid = q.value(0).toInt();
    int size = q.value(1).toInt();
    QString ed2k = q.value(2).toString();
    
    LOG(QString("[MyListCardManager] Marked file lid=%1 as watched, updating card for aid=%2").arg(lid).arg(aid));
    
    // Emit signal for API update (will be handled by Window to call anidb->UpdateFile)
    emit fileNeedsApiUpdate(lid, size, ed2k, 1);
    
    // Emit signal to update play buttons in tree view
    emit playButtonsNeedUpdate(aid);
    
    // Update the card immediately to reflect the change
    updateCardFromDatabase(aid);
}
