# Plan A Revised: Chain Management in MyListCardManager (SOLID Principles)

## SOLID Principles Analysis

### Current Architecture Issues

**Single Responsibility Principle (SRP) Violation:**
- `Window` class currently handles:
  - UI management
  - Chain building logic
  - Sorting logic
  - Filtering logic
- This violates SRP - Window should focus on UI coordination, not data transformation

**Correct Separation of Concerns:**
- `Window`: UI coordination, user input handling
- `MyListCardManager`: Card/chain data management, sorting, filtering
- `WatchSessionManager`: Session state, relation data queries
- `AnimeChain`: Pure data structure with comparison operators

### Proposed Architecture (SOLID-Compliant)

```
Window (UI Layer)
  ↓ (requests sorted/filtered data)
MyListCardManager (Data Management Layer)
  ↓ (uses for chain building)
CardCreationDataCache (relation data preloaded)
  ↓ (uses for ordering)
AnimeChain (Data Structure with operators)
```

---

## Revised Implementation Plan

### Phase 1: Add Operator Overloading to AnimeChain

Make AnimeChain sortable by overloading comparison operators:

```cpp
// animechain.h
class AnimeChain
{
public:
    // ... existing methods ...
    
    // Comparison operators for sorting
    // These delegate to a comparison function that can use different criteria
    bool operator<(const AnimeChain& other) const;
    bool operator>(const AnimeChain& other) const;
    bool operator==(const AnimeChain& other) const;
    
    // Set the sort criteria for comparisons
    enum class SortCriteria {
        ByRepresentativeTitle,
        ByRepresentativeDate,
        ByRepresentativeType,
        ByChainLength,
        ByRepresentativeId
    };
    
    // Static sort criteria (thread-local or per-comparison context)
    static void setSortCriteria(SortCriteria criteria, bool ascending = true);
    static SortCriteria getCurrentSortCriteria();
    
    // Comparison using cached data
    int compareWith(const AnimeChain& other, 
                    const QMap<int, CardCreationData>& dataCache,
                    SortCriteria criteria,
                    bool ascending) const;

private:
    QList<int> m_animeIds;
    
    // Cache representative data for sorting performance
    mutable int m_cachedRepresentativeAid;
    mutable bool m_cacheValid;
};
```

**Implementation:**
```cpp
// animechain.cpp
bool AnimeChain::operator<(const AnimeChain& other) const
{
    // Default comparison: by representative anime ID
    return getRepresentativeAnimeId() < other.getRepresentativeAnimeId();
}

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
        result = myAid - otherAid;  // Fallback to ID comparison
    } else {
        const CardCreationData& myData = dataCache[myAid];
        const CardCreationData& otherData = dataCache[otherAid];
        
        switch (criteria) {
            case SortCriteria::ByRepresentativeTitle:
                result = myData.animeTitle.compare(otherData.animeTitle);
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
```

---

### Phase 2: Move Chain Building to MyListCardManager

**Current Problem:** Window builds chains using WatchSessionManager
**Solution:** MyListCardManager builds chains using cached relation data

```cpp
// mylistcardmanager.h
class MyListCardManager : public QObject
{
    Q_OBJECT
    
public:
    // ... existing methods ...
    
    // NEW: Set anime IDs and automatically build chains if enabled
    void setAnimeIdList(const QList<int>& aids, bool chainModeEnabled);
    
    // NEW: Build chains from anime IDs using cached relation data
    QList<AnimeChain> buildChainsFromAnimeIds(const QList<int>& aids) const;
    
    // NEW: Sort chains using specified criteria
    void sortChains(AnimeChain::SortCriteria criteria, bool ascending);
    
    // NEW: Get chains
    QList<AnimeChain> getChains() const { return m_chainList; }
    
private:
    // Chain storage
    QList<AnimeChain> m_chainList;
    QMap<int, int> m_aidToChainIndex;
    bool m_chainModeEnabled;
    
    // NEW: Build a single chain from starting anime ID
    QList<int> buildChainFromAid(int startAid, 
                                   const QSet<int>& availableAids) const;
    
    // NEW: Find prequel in cached data
    int findPrequelAid(int aid) const;
    
    // NEW: Find sequel in cached data
    int findSequelAid(int aid) const;
};
```

**Implementation:**
```cpp
// mylistcardmanager.cpp

void MyListCardManager::setAnimeIdList(const QList<int>& aids, bool chainModeEnabled)
{
    {
        QMutexLocker locker(&m_mutex);
        m_chainModeEnabled = chainModeEnabled;
        
        if (chainModeEnabled) {
            // Build chains BEFORE sorting
            LOG("[MyListCardManager] Building chains from anime IDs");
            m_chainList = buildChainsFromAnimeIds(aids);
            
            // Build aid -> chain index map
            m_aidToChainIndex.clear();
            for (int i = 0; i < m_chainList.size(); ++i) {
                for (int aid : m_chainList[i].getAnimeIds()) {
                    m_aidToChainIndex[aid] = i;
                }
            }
            
            LOG(QString("[MyListCardManager] Built %1 chains from %2 anime")
                .arg(m_chainList.size()).arg(aids.size()));
        }
        
        m_orderedAnimeIds = aids;
    }
    
    if (m_virtualLayout) {
        m_virtualLayout->setItemCount(aids.size());
    }
}

QList<AnimeChain> MyListCardManager::buildChainsFromAnimeIds(const QList<int>& aids) const
{
    QList<AnimeChain> chains;
    QSet<int> availableAids = QSet<int>(aids.begin(), aids.end());
    QSet<int> processedAids;
    
    for (int aid : aids) {
        if (processedAids.contains(aid)) {
            continue;
        }
        
        // Build chain starting from this anime
        QList<int> chainAids = buildChainFromAid(aid, availableAids);
        
        // Mark all anime in this chain as processed
        for (int chainAid : chainAids) {
            processedAids.insert(chainAid);
        }
        
        // Create chain object
        chains.append(AnimeChain(chainAids));
    }
    
    return chains;
}

QList<int> MyListCardManager::buildChainFromAid(
    int startAid, 
    const QSet<int>& availableAids) const
{
    QList<int> chain;
    QSet<int> visited;
    
    // Find the original prequel (first in chain)
    int currentAid = startAid;
    while (true) {
        if (visited.contains(currentAid)) {
            break;  // Cycle detected
        }
        visited.insert(currentAid);
        
        int prequelAid = findPrequelAid(currentAid);
        if (prequelAid == 0 || !availableAids.contains(prequelAid)) {
            break;  // No prequel or not in our list
        }
        currentAid = prequelAid;
    }
    
    // Now build chain forward from the prequel
    visited.clear();
    currentAid = currentAid;  // Start from the prequel we found
    
    while (currentAid > 0 && !visited.contains(currentAid)) {
        if (availableAids.contains(currentAid)) {
            chain.append(currentAid);
        }
        visited.insert(currentAid);
        
        int sequelAid = findSequelAid(currentAid);
        if (sequelAid == 0) {
            break;
        }
        currentAid = sequelAid;
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

void MyListCardManager::sortChains(AnimeChain::SortCriteria criteria, bool ascending)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_chainModeEnabled || m_chainList.isEmpty()) {
        return;
    }
    
    LOG(QString("[MyListCardManager] Sorting %1 chains").arg(m_chainList.size()));
    
    // Sort chains using comparison function
    std::sort(m_chainList.begin(), m_chainList.end(),
              [this, criteria, ascending](const AnimeChain& a, const AnimeChain& b) {
                  return a.compareWith(b, m_cardCreationDataCache, criteria, ascending) < 0;
              });
    
    // Rebuild flattened anime ID list
    m_orderedAnimeIds.clear();
    m_aidToChainIndex.clear();
    
    for (int i = 0; i < m_chainList.size(); ++i) {
        for (int aid : m_chainList[i].getAnimeIds()) {
            m_orderedAnimeIds.append(aid);
            m_aidToChainIndex[aid] = i;
        }
    }
    
    LOG(QString("[MyListCardManager] Rebuilt ordered list: %1 anime")
        .arg(m_orderedAnimeIds.size()));
    
    // Update virtual layout
    if (m_virtualLayout) {
        m_virtualLayout->setItemCount(m_orderedAnimeIds.size());
        m_virtualLayout->refresh();
    }
}
```

---

### Phase 3: Simplify Window Class

Window now delegates to MyListCardManager:

```cpp
// window.cpp

void Window::sortMylistCards(int sortIndex)
{
    // ... existing filtering/setup code ...
    
    bool seriesChainEnabled = filterSidebar && filterSidebar->getShowSeriesChain();
    
    if (seriesChainEnabled) {
        // Chain mode: Let MyListCardManager handle everything
        LOG("[Window] Chain mode: delegating to MyListCardManager");
        
        // Map sortIndex to AnimeChain::SortCriteria
        AnimeChain::SortCriteria criteria = mapSortIndexToCriteria(sortIndex);
        
        cardManager->sortChains(criteria, sortAscending);
    } else {
        // Normal mode: Sort individual cards
        // ... existing sorting logic ...
        
        cardManager->setAnimeIdList(animeIds, false);  // false = chain mode disabled
    }
}

AnimeChain::SortCriteria Window::mapSortIndexToCriteria(int sortIndex)
{
    switch (sortIndex) {
        case 0: return AnimeChain::SortCriteria::ByRepresentativeTitle;
        case 1: return AnimeChain::SortCriteria::ByRepresentativeDate;
        case 2: return AnimeChain::SortCriteria::ByRepresentativeType;
        default: return AnimeChain::SortCriteria::ByRepresentativeId;
    }
}

void Window::applyMylistFilters()
{
    // ... existing filter logic ...
    
    bool seriesChainEnabled = filterSidebar && filterSidebar->getShowSeriesChain();
    
    // Pass filtered anime IDs with chain mode flag
    // Chains will be built BEFORE any sorting
    cardManager->setAnimeIdList(filteredAnimeIds, seriesChainEnabled);
    
    // Then apply sorting
    sortMylistCards(filterSidebar->getSortIndex());
}
```

---

## Benefits of This Approach

### 1. SOLID Principles Compliance

**Single Responsibility:**
- `Window`: UI coordination only
- `MyListCardManager`: Chain building, sorting, data management
- `AnimeChain`: Data structure with comparison logic

**Open/Closed:**
- New sort criteria can be added without modifying existing code
- Just add new enum value and case in compareWith()

**Dependency Inversion:**
- Window depends on MyListCardManager abstraction
- MyListCardManager uses CardCreationDataCache (injected dependency)

### 2. Performance Benefits

**Chains Built Once:**
- Chains generated in `setAnimeIdList()` BEFORE sorting
- Sorting just reorders chains, doesn't rebuild them
- Filter change rebuilds chains, sort change doesn't

**Efficient Sorting:**
- Comparison operators use cached data
- No database queries during sort
- O(n log n) complexity for chain sorting

**Timeline:**
```
Filter Change:
  ↓
setAnimeIdList(aids, chainMode=true)
  ↓
buildChainsFromAnimeIds() ← BUILD ONCE
  ↓
sortChains(criteria) ← FAST (just reorder)
  ↓
User changes sort ← FAST (chains already built)
```

### 3. Clean Architecture

```
┌─────────────────────────────────────┐
│           Window (UI)               │
│  - User input handling              │
│  - UI coordination                  │
└──────────────┬──────────────────────┘
               │ delegates to
┌──────────────▼──────────────────────┐
│      MyListCardManager              │
│  - Chain building (uses cache)      │
│  - Chain sorting                    │
│  - Data management                  │
└──────────────┬──────────────────────┘
               │ uses
┌──────────────▼──────────────────────┐
│    CardCreationDataCache            │
│  - Preloaded relation data          │
│  - All anime metadata               │
└──────────────┬──────────────────────┘
               │ structures data as
┌──────────────▼──────────────────────┐
│         AnimeChain                  │
│  - Comparison operators             │
│  - Chain data structure             │
└─────────────────────────────────────┘
```

---

## Migration Steps

1. ✅ **Done:** Relation data in CardCreationDataCache
2. ✅ **Done:** AnimeChain data structure
3. 🔄 **Next:** Add comparison operators to AnimeChain
4. 🔄 **Next:** Add chain building methods to MyListCardManager
5. 🔄 **Next:** Simplify Window to delegate to MyListCardManager
6. ⏭️ **Later:** Visual enhancements (badges, borders)

---

## Code Review Checklist

- [ ] Chain building uses only cached data (no DB queries)
- [ ] Chains built once per filter change (not per sort)
- [ ] Window class delegates chain logic to MyListCardManager
- [ ] AnimeChain has comparison operators for sorting
- [ ] SOLID principles: each class has single responsibility
- [ ] Thread-safe: mutex protection where needed
- [ ] Efficient: O(n) chain building, O(n log n) sorting

---

## Performance Comparison

| Scenario | Old Approach | New Approach |
|----------|-------------|--------------|
| Filter change | Build chains in Window | Build chains in CardManager |
| Sort change | Rebuild chains + sort | Just reorder chains (FAST) |
| Data source | WatchSessionManager DB queries | CardCreationDataCache (RAM) |
| Responsibility | Window does everything | Each class has one job |
| Testability | Hard (Window coupled to UI) | Easy (CardManager isolated) |

**Expected improvement:** Sort operations 10-100x faster (no chain rebuild, cached data)
