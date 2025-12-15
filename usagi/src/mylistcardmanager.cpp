#include "mylistcardmanager.h"
#include "virtualflowlayout.h"
#include "animeutils.h"
#include "logger.h"
#include "main.h"
#include "watchsessionmanager.h"
#include "fileconsts.h"
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
    , m_chainsBuilt(false)  // Chains not built yet
    , m_chainBuildInProgress(false)  // No build in progress initially
    , m_dataReady(false)  // Data not ready initially
    , m_lastChainBuildAnimeCount(0)  // No chains built yet
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
    setAnimeIdList(aids, true);  // Default: chain mode enabled (will auto-disable if chains not built)
}

void MyListCardManager::setAnimeIdList(const QList<int>& aids, bool chainModeEnabled)
{
    QList<int> finalAnimeIds;
    
    {
        QMutexLocker locker(&m_mutex);
        
        // Wait for ALL data to be ready (preload + chain building complete)
        while (!m_dataReady) {
            LOG("[MyListCardManager] Waiting for data to be ready (preload + chain building)...");
            m_dataReadyCondition.wait(&m_mutex);
        }
        
        m_chainModeEnabled = chainModeEnabled;
        m_expandedChainAnimeIds.clear();
        
        if (chainModeEnabled) {
            // Use pre-built chains from cache instead of rebuilding
            // Chains should have been built once after preloadCardCreationData completed
            
            if (!m_chainsBuilt || m_chainList.isEmpty()) {
                LOG("[MyListCardManager] ERROR: Chain mode enabled but chains not built!");
                m_chainModeEnabled = false;
                finalAnimeIds = aids;
            } else {
                LOG(QString("[MyListCardManager] Using pre-built chains: %1 chains available")
                    .arg(m_chainList.size()));
                
                // Filter chains to only include those with at least one anime from the input list
                // IMPORTANT: Never modify m_chainList - it's the master list from cache
                QSet<int> inputAidSet(aids.begin(), aids.end());
                
                // Build a map of which chain each input anime belongs to
                // This preserves the order of input anime
                QMap<int, int> inputAidToChainIdx;
                for (int aid : aids) {
                    // Find which chain in m_chainList contains this anime
                    for (int chainIdx = 0; chainIdx < m_chainList.size(); ++chainIdx) {
                        if (m_chainList[chainIdx].contains(aid)) {
                            inputAidToChainIdx[aid] = chainIdx;
                            break;
                        }
                    }
                }
                
                // Collect chains in the order they appear in the input list
                // Use a set to avoid duplicate chains
                QSet<int> includedChainIndices;
                QList<AnimeChain> filteredChains;
                
                // Create relation lookup function for standalone chains
                // NOTE: We're already holding m_mutex here, so we cannot call loadRelationDataForAnime
                // (it would try to acquire the mutex again, causing a deadlock)
                // Instead, directly access the cache which is already populated
                auto relationLookup = [this](int aid) -> QPair<int,int> {
                    // Direct cache access - mutex already held by caller (setAnimeIdList)
                    auto it = m_cardCreationDataCache.find(aid);
                    if (it != m_cardCreationDataCache.end()) {
                        return QPair<int,int>(it->getPrequel(), it->getSequel());
                    }
                    return QPair<int,int>(0, 0);
                };
                
                for (int aid : aids) {
                    if (inputAidToChainIdx.contains(aid)) {
                        // Anime is in an existing chain - add that chain
                        int chainIdx = inputAidToChainIdx[aid];
                        if (!includedChainIndices.contains(chainIdx)) {
                            includedChainIndices.insert(chainIdx);
                            filteredChains.append(m_chainList[chainIdx]);
                        }
                    } else {
                        // Anime is NOT in any chain in m_chainList
                        // Create a standalone single-anime chain for it
                        LOG(QString("[MyListCardManager] Anime %1 not found in any chain, creating standalone chain").arg(aid));
                        AnimeChain standaloneChain(aid, relationLookup);
                        filteredChains.append(standaloneChain);
                    }
                }
                
                // Build aid -> chain index map and collect all anime IDs from filtered chains
                // Use filteredChains (local variable) - never modify m_chainList
                // Store filtered chains (including standalone ones) for use by sortChains
                m_displayedChains = filteredChains;
                m_aidToChainIndex.clear();
                finalAnimeIds.clear();
                for (int i = 0; i < filteredChains.size(); ++i) {
                    for (int aid : filteredChains[i].getAnimeIds()) {
                        m_aidToChainIndex[aid] = i;
                        // Add all anime from chains (including expanded ones not in original aids list)
                        if (!finalAnimeIds.contains(aid)) {
                            finalAnimeIds.append(aid);
                        }
                    }
                }
                
                LOG(QString("[MyListCardManager] Filtered to %1 chains containing %2 anime (preserving original %3 chains)")
                    .arg(filteredChains.size()).arg(finalAnimeIds.size()).arg(m_chainList.size()));
            }
        } else {
            // Normal mode: just use the input list as-is
            // IMPORTANT: Do NOT clear m_chainList - it's the master list from cache and is reused
            m_aidToChainIndex.clear();
            m_displayedChains.clear();  // Clear displayed chains when not in chain mode
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
    QSet<int> availableAids = QSet<int>(aids.begin(), aids.end());
    
    LOG(QString("[MyListCardManager] buildChainsFromAnimeIds: input has %1 anime, %2 unique, expansion=ALWAYS ON")
        .arg(aids.size()).arg(availableAids.size()));
    
    // Create relation lookup function
    auto relationLookup = [this](int aid) -> QPair<int,int> {
        const_cast<MyListCardManager*>(this)->loadRelationDataForAnime(aid);
        return QPair<int,int>(findPrequelAid(aid), findSequelAid(aid));
    };
    
    // Map: anime ID -> chain index (which chain this anime belongs to)
    QMap<int, int> animeToChainIdx;
    QList<AnimeChain> chains;
    
    // Create initial chains - one per anime
    int idx = 0;
    for (int aid : availableAids) {
        AnimeChain chain(aid, relationLookup);
        chains.append(chain);
        animeToChainIdx[aid] = idx;
        idx++;
    }
    
    LOG(QString("[MyListCardManager] Created %1 initial chains").arg(chains.size()));
    
    // Track which chain indices have been merged (deleted)
    QSet<int> deletedChains;
    
    // Process each chain: expand and merge as needed
    for (int i = 0; i < chains.size(); i++) {
        if (deletedChains.contains(i)) {
            continue;  // This chain was already merged into another
        }
        
        // Expand this chain to include all related anime
        // The expand method will add related anime not yet in the chain
        QSet<int> processed;
        bool changed = true;
        int iterations = 0;
        const int MAX_ITERATIONS = 100;
        
        while (changed && iterations < MAX_ITERATIONS) {
            changed = false;
            iterations++;
            
            QList<int> currentAnime = chains[i].getAnimeIds();
            for (int aid : currentAnime) {
                if (processed.contains(aid)) {
                    continue;
                }
                processed.insert(aid);
                
                auto unbound = chains[i].getUnboundRelations(aid);
                
                // Process prequel
                if (unbound.first > 0) {
                    if (animeToChainIdx.contains(unbound.first)) {
                        // Prequel is in another chain - merge that chain into this one
                        int otherIdx = animeToChainIdx[unbound.first];
                        if (otherIdx != i && !deletedChains.contains(otherIdx)) {
                            chains[i].mergeWith(chains[otherIdx], relationLookup);
                            deletedChains.insert(otherIdx);
                            
                            // Rebuild map for all anime in merged chain to ensure consistency
                            for (int aid2 : chains[i].getAnimeIds()) {
                                animeToChainIdx[aid2] = i;
                            }
                            changed = true;
                        }
                    } else {
                        // Prequel not in any chain yet - add it via expanding
                        AnimeChain prequelChain(unbound.first, relationLookup);
                        chains[i].mergeWith(prequelChain, relationLookup);
                        animeToChainIdx[unbound.first] = i;
                        changed = true;
                    }
                }
                
                // Process sequel
                if (unbound.second > 0) {
                    if (animeToChainIdx.contains(unbound.second)) {
                        // Sequel is in another chain - merge that chain into this one
                        int otherIdx = animeToChainIdx[unbound.second];
                        if (otherIdx != i && !deletedChains.contains(otherIdx)) {
                            chains[i].mergeWith(chains[otherIdx], relationLookup);
                            deletedChains.insert(otherIdx);
                            
                            // Rebuild map for all anime in merged chain to ensure consistency
                            for (int aid2 : chains[i].getAnimeIds()) {
                                animeToChainIdx[aid2] = i;
                            }
                            changed = true;
                        }
                    } else {
                        // Sequel not in any chain yet - add it via expanding
                        AnimeChain sequelChain(unbound.second, relationLookup);
                        chains[i].mergeWith(sequelChain, relationLookup);
                        animeToChainIdx[unbound.second] = i;
                        changed = true;
                    }
                }
            }
        }
        
        if (iterations >= MAX_ITERATIONS) {
            LOG(QString("[MyListCardManager] [DEBUG] WARNING: Chain %1 hit MAX_ITERATIONS limit!").arg(i));
        }
    }
    
    // Remove deleted chains
    QList<AnimeChain> finalChains;
    for (int i = 0; i < chains.size(); i++) {
        if (!deletedChains.contains(i)) {
            finalChains.append(chains[i]);
        }
    }
    
    LOG(QString("[MyListCardManager] Final chain count: %1").arg(finalChains.size()));
    
    // Verify total anime count and find duplicates
    int totalAnimeInChains = 0;
    QSet<int> allAnimeInChains;
    QMap<int, int> animeOccurrences;  // Track how many times each anime appears
    
    for (int chainIdx = 0; chainIdx < finalChains.size(); ++chainIdx) {
        const AnimeChain& chain = finalChains[chainIdx];
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
    
    if (totalAnimeInChains != allAnimeInChains.size()) {
        LOG(QString("[MyListCardManager] ERROR: Duplicate anime detected! Total slots: %1, Unique anime: %2")
            .arg(totalAnimeInChains).arg(allAnimeInChains.size()));
    }
    
    // Log expansion results
    QSet<int> expandedAnime = allAnimeInChains - availableAids;
    if (!expandedAnime.isEmpty()) {
        LOG(QString("[MyListCardManager] Chain expansion added %1 anime not in original input")
            .arg(expandedAnime.size()));
    }
    
    // Verify no anime from input was lost
    QSet<int> missingAnime = availableAids - allAnimeInChains;
    if (!missingAnime.isEmpty()) {
        QStringList missingList;
        for (int aid : missingAnime) {
            missingList.append(QString::number(aid));
        }
        LOG(QString("[MyListCardManager] ERROR: Missing anime from chains: %1").arg(missingList.join(", ")));
    }
    
    // Log duplicates
    QStringList duplicates;
    for (QMap<int, int>::const_iterator it = animeOccurrences.constBegin(); it != animeOccurrences.constEnd(); ++it) {
        if (it.value() > 1) {
            duplicates.append(QString("%1(x%2)").arg(it.key()).arg(it.value()));
        }
    }
    if (!duplicates.isEmpty()) {
        LOG(QString("[MyListCardManager] ERROR: Duplicate anime in chains: %1").arg(duplicates.join(", ")));
    }
    
    return finalChains;
}

QList<int> MyListCardManager::buildChainFromAid(int startAid, const QSet<int>& availableAids, bool expandChain) const
{
    QList<int> chain;
    QSet<int> visited;
    QSet<int> backwardTraversedAnime;  // Track anime found during backward traversal
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
        
        // Track all anime found during backward traversal when expanding
        if (expandChain) {
            backwardTraversedAnime.insert(currentAid);
        }
        
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
    
    // Ensure the final anime from backward traversal is also tracked
    if (expandChain && currentAid > 0 && !backwardTraversedAnime.contains(currentAid)) {
        backwardTraversedAnime.insert(currentAid);
    }
    
    // Now build chain forward from the last anime in our filtered set
    // (or from the first prequel if expanding)
    visited.clear();
    int chainStart = expandChain ? currentAid : lastAvailableAid;
    currentAid = chainStart;
    
    while (currentAid > 0 && !visited.contains(currentAid)) {
        // When expanding, include all anime in chain
        // When not expanding, only include anime in availableAids
        if (expandChain || availableAids.contains(currentAid)) {
            chain.append(currentAid);
            visited.insert(currentAid);
            
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
    
    // CRITICAL FIX: Ensure all anime from backward traversal and availableAids are in the chain
    // If we couldn't reach some anime via sequel relationships due to database inconsistencies,
    // append them in the correct order based on their prequel relationships
    if (expandChain || !availableAids.isEmpty()) {
        QSet<int> chainSet;
        for (int aid : chain) {
            chainSet.insert(aid);
        }
        
        // Collect all anime that should be in the chain
        QSet<int> shouldBeInChain;
        
        // Add anime from backward traversal (when expanding)
        if (expandChain) {
            for (int aid : backwardTraversedAnime) {
                shouldBeInChain.insert(aid);
            }
        }
        
        // Add anime from availableAids
        for (int aid : availableAids) {
            shouldBeInChain.insert(aid);
        }
        
        // Find anime that should be in chain but are not
        QList<int> missingAnime;
        for (int aid : shouldBeInChain) {
            if (!chainSet.contains(aid)) {
                missingAnime.append(aid);
            }
        }
        
        // If there are missing anime, try to add them in the correct order
        if (!missingAnime.isEmpty()) {
            LOG(QString("[MyListCardManager] WARNING: %1 anime should be in chain but not reachable from chainStart=%2 via sequel relationships")
                .arg(missingAnime.size()).arg(chainStart));
            
            // Sort missing anime by their position in the chain (using prequel/sequel relationships)
            // For each missing anime, traverse to find where it should be inserted
            for (int missingAid : missingAnime) {
                loadRelationDataForAnime(missingAid);
                
                // Find the position to insert this anime
                // Check both directions: is any anime in chain its sequel, or is missing anime a prequel of any in chain
                int insertAfterIndex = -1;
                int insertBeforeIndex = -1;
                
                for (int i = 0; i < chain.size(); ++i) {
                    // Check if chain[i] is the prequel of missing anime (missing should come after chain[i])
                    if (findSequelAid(chain[i]) == missingAid) {
                        insertAfterIndex = i;
                        break;
                    }
                    // Check if missing anime is the prequel of chain[i] (missing should come before chain[i])
                    if (findSequelAid(missingAid) == chain[i]) {
                        insertBeforeIndex = i;
                        break;
                    }
                }
                
                if (insertAfterIndex >= 0) {
                    // Insert after the prequel
                    chain.insert(insertAfterIndex + 1, missingAid);
                    LOG(QString("[MyListCardManager] Inserted aid=%1 after aid=%2 (sequel relationship)")
                        .arg(missingAid).arg(chain[insertAfterIndex]));
                } else if (insertBeforeIndex >= 0) {
                    // Insert before the sequel
                    chain.insert(insertBeforeIndex, missingAid);
                    LOG(QString("[MyListCardManager] Inserted aid=%1 before aid=%2 (prequel relationship)")
                        .arg(missingAid).arg(chain[insertBeforeIndex]));
                } else {
                    // Couldn't find a good position, append at the end
                    chain.append(missingAid);
                    LOG(QString("[MyListCardManager] Appended aid=%1 at end (no relationship found)")
                        .arg(missingAid));
                }
            }
        }
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
    return data.getPrequel();
}

int MyListCardManager::findSequelAid(int aid) const
{
    // Use cached relation data
    if (!m_cardCreationDataCache.contains(aid)) {
        return 0;
    }
    
    const CardCreationData& data = m_cardCreationDataCache[aid];
    return data.getSequel();
}



void MyListCardManager::loadRelationDataForAnime(int aid) const
{
    // This function now ONLY reads from the preloaded cache
    // All relation data must be preloaded via preloadRelationDataForChainExpansion() before chain building
    // Database calls are NOT permitted here to avoid performance issues during chain expansion
    
    QMutexLocker locker(&m_mutex);
    
    // If already in cache, nothing to do
    if (m_cardCreationDataCache.contains(aid)) {
        return;
    }
    
    // If not in cache, it means preloading didn't include this anime
    // This is expected for anime that are not in the database or were filtered out
}

void MyListCardManager::sortChains(AnimeChain::SortCriteria criteria, bool ascending)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_chainModeEnabled || m_displayedChains.isEmpty()) {
        LOG("[MyListCardManager] sortChains: chain mode not enabled or no displayed chains");
        return;
    }
    
    int inputAnimeCount = m_orderedAnimeIds.size();
    
    LOG(QString("[MyListCardManager] Sorting chains by criteria %2, ascending=%3 (current ordered list has %4 anime)")
        .arg(static_cast<int>(criteria)).arg(ascending).arg(inputAnimeCount));
    
    // Sort the displayed chains (includes both pre-built and standalone chains)
    QList<AnimeChain> displayedChains = m_displayedChains;  // Work with a copy
    
    // Sort the displayed chains
    std::sort(displayedChains.begin(), displayedChains.end(),
              [this, criteria, ascending](const AnimeChain& a, const AnimeChain& b) {
                  return a.compareWith(b, m_cardCreationDataCache, criteria, ascending) < 0;
              });
    
    // Rebuild flattened anime ID list from sorted displayed chains
    m_orderedAnimeIds.clear();
    m_aidToChainIndex.clear();
    
    for (int i = 0; i < displayedChains.size(); ++i) {
        for (int aid : displayedChains[i].getAnimeIds()) {
            m_orderedAnimeIds.append(aid);
            m_aidToChainIndex[aid] = i;
        }
    }
    
    // Update m_displayedChains with the sorted version
    m_displayedChains = displayedChains;
    
    if (m_orderedAnimeIds.size() != inputAnimeCount) {
        LOG(QString("[MyListCardManager] WARNING: Anime count changed during sort! Before: %1, After: %2")
            .arg(inputAnimeCount).arg(m_orderedAnimeIds.size()));
    }
    
    LOG(QString("[MyListCardManager] Rebuilt ordered list: %1 anime in %2 chains (master list unchanged with %3 chains)")
        .arg(m_orderedAnimeIds.size()).arg(displayedChains.size()).arg(m_chainList.size()));
    
    // Note: We don't call refresh() here because it causes re-entrancy issues
    // when called synchronously during sorting. The caller (window.cpp) will
    // handle refreshing the layout after sorting completes.
}

void MyListCardManager::updateSeriesChainConnections(bool chainModeEnabled)
{
    QMutexLocker locker(&m_mutex);
    
    for (AnimeCard* card : m_cards) {
        if (card) {
            card->setSeriesChainInfo(0, 0);
        }
    }
    
    if (!chainModeEnabled || m_chainList.isEmpty()) {
        return;
    }
    
    for (const AnimeChain& chain : std::as_const(m_chainList)) {
        const QList<int> chainAnimeIds = chain.getAnimeIds();
        
        for (int i = 0; i < chainAnimeIds.size(); ++i) {
            const int currentAid = chainAnimeIds[i];
            const int prequelAid = (i > 0) ? chainAnimeIds[i - 1] : 0;
            const int sequelAid = (i < chainAnimeIds.size() - 1) ? chainAnimeIds[i + 1] : 0;
            
            AnimeCard* card = m_cards.value(currentAid, nullptr);
            if (card) {
                card->setSeriesChainInfo(prequelAid, sequelAid);
            }
            // Note: No warning if card not found - with virtual scrolling, cards are created on-demand
            // Chain info will be set in createCard() when the card is actually created
        }
    }
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
    
    // Wait for ALL data to be ready before creating cards
    while (!m_dataReady) {
        LOG("[MyListCardManager] createCardForIndex: Waiting for data to be ready...");
        m_dataReadyCondition.wait(&m_mutex);
    }
    
    if (index < 0 || index >= m_orderedAnimeIds.size()) {
        LOG(QString("[MyListCardManager] createCardForIndex: index %1 out of range (size=%2)")
            .arg(index).arg(m_orderedAnimeIds.size()));
        return nullptr;
    }
    
    int aid = m_orderedAnimeIds[index];
    locker.unlock();
    
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
    
    // Reset data ready flag when clearing
    m_dataReady = false;
    
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
    
    // Set recent episode air date from cached data
    result.setRecentEpisodeAirDate(data.recentEpisodeAirDate);
    
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

void MyListCardManager::updateCardEpisode(int aid, int /*eid*/)
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

int MyListCardManager::extractFileVersion(int fileState)
{
    // Extract file version from AniDB file state bits
    // See fileconsts.h (AniDBFileStateBits namespace) for bit flag documentation
    
    // Check version flags in priority order (v5 > v4 > v3 > v2)
    if (fileState & AniDBFileStateBits::FILE_ISV5) {
        return 5;
    } else if (fileState & AniDBFileStateBits::FILE_ISV4) {
        return 4;
    } else if (fileState & AniDBFileStateBits::FILE_ISV3) {
        return 3;
    } else if (fileState & AniDBFileStateBits::FILE_ISV2) {
        return 2;
    }
    
    // No version bits set means version 1
    return 1;
}

AnimeCard* MyListCardManager::createCard(int aid)
{
    // Wait for ALL data to be ready before creating cards
    {
        QMutexLocker locker(&m_mutex);
        while (!m_dataReady) {
            LOG("[MyListCardManager] createCard: Waiting for data to be ready...");
            m_dataReadyCondition.wait(&m_mutex);
        }
    }
    
    // Check if card already exists - prevent duplicates
    if (hasCard(aid)) {
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
    
    // Set series chain info if chain mode is enabled
    QMutexLocker locker(&m_mutex);
    if (m_chainModeEnabled && !m_chainList.isEmpty()) {
        int chainIndex = m_aidToChainIndex.value(aid, -1);
        if (chainIndex >= 0 && chainIndex < m_chainList.size()) {
            const QList<int> chainAnimeIds = m_chainList[chainIndex].getAnimeIds();
            int aidIndex = chainAnimeIds.indexOf(aid);
            if (aidIndex >= 0) {
                int prequelAid = (aidIndex > 0) ? chainAnimeIds[aidIndex - 1] : 0;
                int sequelAid = (aidIndex < chainAnimeIds.size() - 1) ? chainAnimeIds[aidIndex + 1] : 0;
                card->setSeriesChainInfo(prequelAid, sequelAid);
            }
        }
    }
    
    // Add to cache (mutex already locked above)
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
              "m.local_watched, "
              "CASE WHEN we.eid IS NOT NULL THEN 1 ELSE 0 END as episode_watched, "
              "f.state as file_state "
              "FROM mylist m "
              "LEFT JOIN episode e ON m.eid = e.eid "
              "LEFT JOIN file f ON m.fid = f.fid "
              "LEFT JOIN local_files lf ON m.local_file = lf.id "
              "LEFT JOIN `group` g ON m.gid = g.gid "
              "LEFT JOIN watched_episodes we ON m.eid = we.eid "
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
            entry.episodeWatched = q.value(15).toInt();
            entry.fileState = q.value(16).toInt();
            
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

void MyListCardManager::loadEpisodesForCardFromCache(AnimeCard *card, int /*aid*/, const QList<EpisodeCacheEntry>& episodes)
{
    // Load episodes from the provided cache data - NO SQL QUERIES
    // Group files by episode
    QMap<int, AnimeCard::EpisodeInfo> episodeMap;
    
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
            
            // Set episode-level watched status (persists across file replacements)
            episodeInfo.setEpisodeWatched(entry.episodeWatched != 0);
            
            if (entry.episodeName.isEmpty()) {
                m_episodesNeedingData.insert(eid);
            }
            
            episodeMap[eid] = episodeInfo;
        }
        
        // Create file info
        AnimeCard::FileInfo fileInfo;
        fileInfo.setLid(entry.lid);
        fileInfo.setFid(entry.fid);
        fileInfo.setFileName(entry.filename.isEmpty() ? QString("FID:%1").arg(entry.fid) : entry.filename);
        
        // State string
        QString stateStr;
        switch(entry.state) {
            case 0: stateStr = FileStates::UNKNOWN; break;
            case 1: stateStr = FileStates::HDD; break;
            case 2: stateStr = FileStates::CD_DVD; break;
            case 3: stateStr = FileStates::DELETED; break;
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
        
        // Extract file version from AniDB file state bits (not file order)
        // Previously used episodeFileCount as index, but this didn't reflect actual version
        // Now extracts version from state bits (FILE_ISV2-5) which matches AniDB data
        int version = extractFileVersion(entry.fileState);
        fileInfo.setVersion(version);
        
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
    
    // Mark data as NOT ready at start of preload
    {
        QMutexLocker locker(&m_mutex);
        m_dataReady = false;
    }
    
    emit progressUpdate(QString("Loading data for %1 anime...").arg(aids.size()));
    
    QElapsedTimer timer;
    timer.start();
    
    LOG(QString("[MyListCardManager] Starting comprehensive preload for %1 anime").arg(aids.size()));
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[MyListCardManager] Database not open");
        return;
    }
    
    // Don't clear the cache - just add/update entries for the requested anime
    // This allows incremental preloading without losing previously loaded data
    
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
                                "LEFT JOIN anime_titles at ON a.aid = at.aid AND at.type = 1 AND at.language = 'x-jat' "
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
            data.setRelations(q.value(17).toString(), q.value(18).toString());
            data.hasData = true;
        }
    }
    qint64 step1Elapsed = timer.elapsed() - step1Start;
    LOG(QString("[MyListCardManager] Step 1: Loaded anime data for %1 anime in %2 ms")
        .arg(m_cardCreationDataCache.size()).arg(step1Elapsed));
    emit progressUpdate(QString("Loaded anime data (%1 of 3)...").arg(1));
    
    // Step 2: Load anime titles for anime without anime table data OR with empty animeTitle
    // This fills in titles from anime_titles table when anime table is missing/incomplete
    qint64 step2Start = timer.elapsed();
    QString titlesQuery = QString("SELECT aid, title FROM anime_titles "
                                 "WHERE aid IN (%1) AND type = 1 AND language = 'x-jat'").arg(aidsList);
    QSqlQuery tq(db);
    if (tq.exec(titlesQuery)) {
        while (tq.next()) {
            int aid = tq.value(0).toInt();
            QString title = tq.value(1).toString();
            
            // Set if not already in cache OR if animeTitle is empty (even when hasData=true)
            // This ensures we fill missing titles from anime_titles table
            if (!m_cardCreationDataCache.contains(aid)) {
                CardCreationData& data = m_cardCreationDataCache[aid];
                data.animeTitle = title;
                data.hasData = true;
            } else if (m_cardCreationDataCache[aid].animeTitle.isEmpty()) {
                // Fill empty animeTitle even when other data exists
                m_cardCreationDataCache[aid].animeTitle = title;
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
    emit progressUpdate(QString("Loaded statistics (%1 of 3)...").arg(2));
    
    // Step 4: Load episode details
    qint64 step4Start = timer.elapsed();
    QString episodesQuery = QString("SELECT m.aid, m.lid, m.eid, m.fid, m.state, m.viewed, m.storage, "
                                   "e.name as episode_name, e.epno, "
                                   "f.filename, m.last_played, "
                                   "lf.path as local_file_path, "
                                   "f.resolution, f.quality, "
                                   "g.name as group_name, "
                                   "m.local_watched, "
                                   "CASE WHEN we.eid IS NOT NULL THEN 1 ELSE 0 END as episode_watched, "
                                   "f.airdate, "
                                   "f.state as file_state "
                                   "FROM mylist m "
                                   "LEFT JOIN episode e ON m.eid = e.eid "
                                   "LEFT JOIN file f ON m.fid = f.fid "
                                   "LEFT JOIN local_files lf ON m.local_file = lf.id "
                                   "LEFT JOIN `group` g ON m.gid = g.gid "
                                   "LEFT JOIN watched_episodes we ON m.eid = we.eid "
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
                entry.episodeWatched = episodesQ.value(16).toInt();
                entry.airDate = episodesQ.value(17).toLongLong();
                entry.fileState = episodesQ.value(18).toInt();
                
                m_cardCreationDataCache[aid].episodes.append(entry);
            }
        }
    }
    qint64 step4Elapsed = timer.elapsed() - step4Start;
    int totalEpisodes = 0;
    for (CardCreationData& data : m_cardCreationDataCache) {
        totalEpisodes += data.episodes.size();
        
        // Calculate lastPlayed as the maximum timestamp from all episodes
        qint64 maxLastPlayed = 0;
        if (!data.episodes.isEmpty()) {
            QList<EpisodeCacheEntry>::const_iterator maxIt = std::max_element(data.episodes.begin(), data.episodes.end(),
                [](const EpisodeCacheEntry& a, const EpisodeCacheEntry& b) {
                    return a.lastPlayed < b.lastPlayed;
                });
            maxLastPlayed = maxIt->lastPlayed;
        }
        data.lastPlayed = maxLastPlayed;
        
        // Calculate recentEpisodeAirDate as the maximum air date from all episodes
        qint64 maxAirDate = 0;
        if (!data.episodes.isEmpty()) {
            QList<EpisodeCacheEntry>::const_iterator maxAirIt = std::max_element(data.episodes.begin(), data.episodes.end(),
                [](const EpisodeCacheEntry& a, const EpisodeCacheEntry& b) {
                    return a.airDate < b.airDate;
                });
            maxAirDate = maxAirIt->airDate;
        }
        
        // Failover: If no episode air date available, use anime startDate
        if (maxAirDate == 0 && !data.startDate.isEmpty()) {
            // Parse ISO date format (handles "YYYY-MM-DDZ" or "YYYY-MM-DD")
            QDateTime startDateTime = QDateTime::fromString(data.startDate, Qt::ISODate);
            if (startDateTime.isValid()) {
                maxAirDate = startDateTime.toSecsSinceEpoch();
            }
        }
        
        data.recentEpisodeAirDate = maxAirDate;
    }
    LOG(QString("[MyListCardManager] Step 4: Loaded %1 episodes in %2 ms").arg(totalEpisodes).arg(step4Elapsed));
    emit progressUpdate(QString("Loaded episodes (%1 of 3)...").arg(3));
    
    qint64 totalElapsed = timer.elapsed();
    LOG(QString("[MyListCardManager] Comprehensive preload complete: %1 anime with full data in %2 ms")
        .arg(m_cardCreationDataCache.size()).arg(totalElapsed));
    
    // NOTE: Chain building is now done explicitly by the caller after all data loading is complete
    // Callers use buildChainsWithLogging() (or call buildChainsFromCache() directly) after preloadCardCreationData()
    // (See: onMylistLoadingFinished, loadMylistAsCards, applyMylistFilters in window.cpp)
    // This ensures chains are built from the complete dataset, not incrementally during loading
    
    // SPECIAL CASE: Handle FINAL PRELOAD scenario to prevent deadlock
    // When chains are already built (m_chainsBuilt=true) but data not ready (m_dataReady=false),
    // it means we're in FINAL PRELOAD loading missing anime after chains were already built.
    // In this case, mark data ready without rebuilding chains to prevent createCardForIndex() deadlock.
    {
        QMutexLocker locker(&m_mutex);
        if (m_chainsBuilt && !m_dataReady) {
            LOG("[MyListCardManager] Chains already built, marking data ready after preload");
            m_dataReady = true;
            m_dataReadyCondition.wakeAll();
        }
    }
}

void MyListCardManager::preloadRelationDataForChainExpansion(const QList<int>& baseAids)
{
    // Collect all related anime IDs (prequels/sequels) that might be discovered during chain building
    QSet<int> relatedAids;
    
    {
        QMutexLocker locker(&m_mutex);
        
        for (int aid : baseAids) {
            if (m_cardCreationDataCache.contains(aid)) {
                const CardCreationData& data = m_cardCreationDataCache[aid];
                
                // Get prequel and sequel AIDs
                int prequelAid = data.getPrequel();
                int sequelAid = data.getSequel();
                
                if (prequelAid > 0) {
                    relatedAids.insert(prequelAid);
                }
                if (sequelAid > 0) {
                    relatedAids.insert(sequelAid);
                }
            }
        }
    }
    
    // Filter out AIDs that are already in cache
    QSet<int> aidsToLoad;
    {
        QMutexLocker locker(&m_mutex);
        for (int aid : relatedAids) {
            if (!m_cardCreationDataCache.contains(aid)) {
                aidsToLoad.insert(aid);
            }
        }
    }
    
    if (aidsToLoad.isEmpty()) {
        return;
    }
    
    // Bulk-load relation data for missing AIDs
    QStringList aidStrings;
    for (int aid : aidsToLoad) {
        aidStrings.append(QString::number(aid));
    }
    QString aidsList = aidStrings.join(",");
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    QString query = QString("SELECT aid, relaidlist, relaidtype FROM anime WHERE aid IN (%1)").arg(aidsList);
    QSqlQuery q(db);
    
    if (q.exec(query)) {
        QMutexLocker locker(&m_mutex);
        while (q.next()) {
            int aid = q.value(0).toInt();
            CardCreationData data;
            data.setRelations(q.value(1).toString(), q.value(2).toString());
            data.hasData = false;  // Mark as partial data (only relations loaded)
            m_cardCreationDataCache[aid] = data;
        }
    }
}

void MyListCardManager::buildChainsFromCache()
{
    {
        QMutexLocker locker(&m_mutex);
        
        // Get current cache size to check if rebuild is needed
        int currentCacheSize = m_cardCreationDataCache.size();
        
        // Check if chains are already built and cache size hasn't changed significantly
        // Rebuild if cache grew by more than 10% (indicates new anime were loaded)
        if (m_chainsBuilt && !m_chainList.isEmpty()) {
            int sizeDiff = abs(currentCacheSize - m_lastChainBuildAnimeCount);
            double changePercent = m_lastChainBuildAnimeCount > 0 ? 
                (double)sizeDiff / m_lastChainBuildAnimeCount * 100.0 : 100.0;
            
            if (changePercent < 10.0) {
                LOG(QString("[MyListCardManager] Chains already built from %1 anime, cache has %2 anime (%.1f%% change), skipping rebuild")
                    .arg(m_lastChainBuildAnimeCount).arg(currentCacheSize).arg(changePercent));
                // Ensure data is marked ready even when skipping rebuild
                // (this handles case where preloadCardCreationData was called again after chains were built)
                if (!m_dataReady) {
                    m_dataReady = true;
                    locker.unlock();  // Release mutex before waking threads
                    m_dataReadyCondition.wakeAll();
                }
                return;
            } else {
                LOG(QString("[MyListCardManager] Cache size changed significantly: %1 -> %2 anime (%.1f%% change), rebuilding chains")
                    .arg(m_lastChainBuildAnimeCount).arg(currentCacheSize).arg(changePercent));
                // Reset flags to allow rebuild
                m_chainsBuilt = false;
                m_dataReady = false;
            }
        }
        
        // Check if a build is already in progress (another thread is building)
        if (m_chainBuildInProgress) {
            LOG("[MyListCardManager] Chain building already in progress, waiting for completion");
            // Wait for the other thread to complete the build
            while (m_chainBuildInProgress && !m_dataReady) {
                m_dataReadyCondition.wait(&m_mutex);
            }
            LOG("[MyListCardManager] Chain building complete by another thread");
            return;
        }
        
        // Mark that we're starting the build
        m_chainBuildInProgress = true;
    } // Release mutex before the actual building work
    
    emit progressUpdate("Building anime chains...");
    
    // Get all anime IDs from the cache (need to lock again to access cache)
    QList<int> allCachedAids;
    {
        QMutexLocker locker(&m_mutex);
        allCachedAids = m_cardCreationDataCache.keys();
    }
    
    if (allCachedAids.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        LOG("[MyListCardManager] buildChainsFromCache: No anime in cache, skipping chain building");
        m_chainsBuilt = false;
        m_chainBuildInProgress = false;
        m_dataReady = true;  // Set data ready even if no chains (no anime to display)
        m_dataReadyCondition.wakeAll();
        return;
    }
    
    // PRE-LOAD all relation data for related anime that might be discovered during chain expansion
    // This prevents individual DB queries during chain building (which would cause race conditions)
    preloadRelationDataForChainExpansion(allCachedAids);
    
    // Build chains from ALL cached anime (this is the expensive operation, done without holding mutex)
    QList<AnimeChain> newChains = buildChainsFromAnimeIds(allCachedAids);
    
    emit progressUpdate(QString("Processed %1 chains...").arg(newChains.size()));
    
    // Now lock and update the member variables
    {
        QMutexLocker locker(&m_mutex);
        
        m_chainList = newChains;
        
        // Build aid -> chain index map for quick lookups
        m_aidToChainIndex.clear();
        for (int i = 0; i < m_chainList.size(); ++i) {
            for (int aid : m_chainList[i].getAnimeIds()) {
                m_aidToChainIndex[aid] = i;
            }
        }
        
        m_chainsBuilt = true;
        m_chainBuildInProgress = false;
        m_dataReady = true;  // Mark ALL data as ready (preload + chain build complete)
        m_lastChainBuildAnimeCount = m_cardCreationDataCache.size();  // Track cache size used for this build
        
        LOG(QString("[MyListCardManager] Built %1 chains from complete cache (contains %2 total anime)")
            .arg(m_chainList.size())
            .arg(m_aidToChainIndex.size()));
    } // Release mutex before waking threads
    
    emit progressUpdate("Data ready!");
    
    // Wake up any threads waiting for data to be ready
    // Do this AFTER releasing the mutex so waiting threads don't immediately block again
    m_dataReadyCondition.wakeAll();
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
    
    // Mark episode as watched at episode level (persists across file replacements)
    q.prepare("INSERT OR REPLACE INTO watched_episodes (eid, watched_at) VALUES (?, ?)");
    q.addBindValue(eid);
    q.addBindValue(currentTimestamp);
    
    if (!q.exec()) {
        LOG(QString("[MyListCardManager] Failed to mark episode watched at episode level eid=%1: %2")
            .arg(eid).arg(q.lastError().text()));
    } else {
        LOG(QString("[MyListCardManager] Marked episode eid=%1 as watched at episode level").arg(eid));
    }
    
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
    
    // Get file info for API update and episode ID
    q.prepare("SELECT m.aid, f.size, f.ed2k, m.eid FROM mylist m "
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
    int eid = q.value(3).toInt();
    
    // Mark episode as watched at episode level (persists across file replacements)
    if (eid > 0) {
        q.prepare("INSERT OR REPLACE INTO watched_episodes (eid, watched_at) VALUES (?, ?)");
        q.addBindValue(eid);
        q.addBindValue(currentTimestamp);
        
        if (!q.exec()) {
            LOG(QString("[MyListCardManager] Failed to mark episode watched at episode level eid=%1: %2")
                .arg(eid).arg(q.lastError().text()));
        } else {
            LOG(QString("[MyListCardManager] Marked episode eid=%1 as watched at episode level").arg(eid));
        }
    }
    
    LOG(QString("[MyListCardManager] Marked file lid=%1 as watched, updating card for aid=%2").arg(lid).arg(aid));
    
    // Emit signal for API update (will be handled by Window to call anidb->UpdateFile)
    emit fileNeedsApiUpdate(lid, size, ed2k, 1);
    
    // Update the card immediately to reflect the change
    updateCardFromDatabase(aid);
}
