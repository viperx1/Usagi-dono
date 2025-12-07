# Plan A Implementation Details: Chain-Aware Item Factory

## Overview

Plan A enhances the existing architecture by adding chain metadata alongside the anime ID list. Cards remain individual widgets but gain awareness of their chain context.

## Chain Generation Points

### Option 1: Generate During Sorting (Current Flow, Minimal Changes)

**When:** In `Window::sortMylistCards()` after chain grouping logic (line 4152+)

**Flow:**
```
sortMylistCards() called
  ↓
If chain mode enabled:
  - Build chainGroups map (existing code)
  - Sort chain keys
  - Flatten to anime ID list
  - NEW: Create AnimeChain objects from chainGroups ← GENERATE HERE
  - NEW: Pass chains to MyListCardManager
  ↓
setAnimeIdList() with chain metadata
```

**Pros:**
- ✅ Leverages existing chain grouping logic
- ✅ Chains automatically rebuilt when sorting changes
- ✅ Minimal code duplication

**Cons:**
- ❌ Chains regenerated on every sort
- ❌ Couples chain generation to sorting

**Code location:** `window.cpp` lines 4345-4360

---

### Option 2: Generate Once During Filter Application (Efficient)

**When:** In `Window::applyMylistFilters()` before sorting

**Flow:**
```
applyMylistFilters() called
  ↓
Filter anime IDs
  ↓
If chain mode enabled:
  - Build chains from filtered IDs ← GENERATE HERE
  - Store chains for sorting
  ↓
sortMylistCards() called
  - Use pre-built chains for sorting
  - Pass chains to MyListCardManager
```

**Pros:**
- ✅ Chains built once per filter change
- ✅ More efficient (no rebuild on sort change)
- ✅ Separates chain building from sorting

**Cons:**
- ❌ Need to store chains in Window class
- ❌ More state management

**Code location:** `window.cpp` around line 5640 (after filtering, before sorting)

---

### Option 3: Generate in MyListCardManager (Decoupled)

**When:** When `setAnimeIdList()` is called with chain mode flag

**Flow:**
```
Window calls setAnimeIdList(aids, chainModeEnabled)
  ↓
MyListCardManager::setAnimeIdList()
  ↓
If chainModeEnabled:
  - Build chains using CardCreationDataCache ← GENERATE HERE
  - Store chains in m_chainList
  - Build aid->chain index map
```

**Pros:**
- ✅ Decoupled from Window logic
- ✅ MyListCardManager owns chain data
- ✅ Uses preloaded relation data efficiently

**Cons:**
- ❌ Duplicates chain-building logic
- ❌ Need access to relation data in MyListCardManager

**Code location:** `mylistcardmanager.cpp` in `setAnimeIdList()`

---

## Recommended: **Hybrid Approach (Option 1 + Option 3)**

Build chains in Window (using WatchSessionManager) and pass them to MyListCardManager:

```cpp
// In Window::sortMylistCards() - after line 4345
if (seriesChainEnabled && watchSessionManager) {
    // Build AnimeChain objects from chainGroups
    QList<AnimeChain> chains;
    for (const QList<int>& chainAids : chainGroups.values()) {
        chains.append(AnimeChain(chainAids));
    }
    
    // Pass both anime IDs and chains to card manager
    cardManager->setAnimeIdListWithChains(animeIds, chains);
} else {
    cardManager->setAnimeIdList(animeIds);
}
```

---

## Implementation Steps

### Phase 1: Add Chain Storage to MyListCardManager

```cpp
// mylistcardmanager.h
class MyListCardManager {
private:
    QList<int> m_orderedAnimeIds;           // Existing: flattened list
    QList<AnimeChain> m_chainList;          // NEW: list of chains
    QMap<int, int> m_aidToChainIndex;       // NEW: aid -> chain index
    bool m_chainModeEnabled;                // NEW: is chain mode active

public:
    // NEW: Enhanced method that accepts chain data
    void setAnimeIdListWithChains(const QList<int>& aids, 
                                   const QList<AnimeChain>& chains);
    
    // NEW: Query methods
    AnimeChain getChainForAnime(int aid) const;
    int getChainIndexForAnime(int aid) const;
    bool isChainModeEnabled() const { return m_chainModeEnabled; }
};
```

**Implementation:**
```cpp
// mylistcardmanager.cpp
void MyListCardManager::setAnimeIdListWithChains(
    const QList<int>& aids, 
    const QList<AnimeChain>& chains)
{
    {
        QMutexLocker locker(&m_mutex);
        m_orderedAnimeIds = aids;
        m_chainList = chains;
        m_chainModeEnabled = !chains.isEmpty();
        
        // Build aid -> chain index map
        m_aidToChainIndex.clear();
        for (int i = 0; i < chains.size(); ++i) {
            for (int aid : chains[i].getAnimeIds()) {
                m_aidToChainIndex[aid] = i;
            }
        }
        
        LOG(QString("[MyListCardManager] Set %1 anime IDs with %2 chains")
            .arg(aids.size()).arg(chains.size()));
    }
    
    if (m_virtualLayout) {
        m_virtualLayout->setItemCount(aids.size());
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
```

---

### Phase 2: Modify Window to Pass Chain Data

```cpp
// window.cpp - in sortMylistCards() around line 4345
if (seriesChainEnabled && watchSessionManager) {
    // After building and sorting chainGroups...
    
    // Convert chainGroups to AnimeChain objects
    QList<AnimeChain> chains;
    QList<int> sortedChainKeys = chainKeys;  // Already sorted
    
    for (int chainKey : sortedChainKeys) {
        const QList<int>& chainAids = chainGroups[chainKey];
        chains.append(AnimeChain(chainAids));
    }
    
    // Pass chains along with anime IDs
    cardManager->setAnimeIdListWithChains(animeIds, chains);
    
    LOG(QString("[Window] Created %1 chains for %2 anime")
        .arg(chains.size()).arg(animeIds.size()));
} else {
    // Normal mode: no chains
    cardManager->setAnimeIdList(animeIds);
}
```

---

### Phase 3: Enable Cards to Query Chain Context (Optional Enhancement)

```cpp
// mylistcardmanager.cpp - in createCardForIndex()
AnimeCard* MyListCardManager::createCardForIndex(int index)
{
    // ... existing card creation code ...
    
    // NEW: If chain mode is enabled, set chain context
    if (m_chainModeEnabled) {
        int chainIndex = m_aidToChainIndex.value(aid, -1);
        if (chainIndex >= 0) {
            const AnimeChain& chain = m_chainList[chainIndex];
            QList<int> chainAids = chain.getAnimeIds();
            int positionInChain = chainAids.indexOf(aid);
            
            // Set chain context on card (requires new AnimeCard method)
            card->setChainContext(positionInChain, chainAids.size(), chainIndex);
        }
    }
    
    return card;
}
```

```cpp
// animecard.h - add new method
class AnimeCard {
public:
    void setChainContext(int position, int total, int chainIndex);
    
private:
    int m_chainPosition;   // Position in chain (0-based)
    int m_chainTotal;      // Total anime in chain
    int m_chainIndex;      // Index of this chain
};
```

---

### Phase 4: Visual Enhancements (Future)

With chain context available, cards can enhance their display:

1. **Chain badges:** "1/3" or "2 of 3 in series"
2. **Styling:** First card has left border, last has right border
3. **Background:** Same background color for chain members
4. **Tooltips:** "Part of: Series Name → Series Name 2 → ..."

```cpp
// animecard.cpp - in paintEvent() or updateUI()
void AnimeCard::setChainContext(int position, int total, int chainIndex)
{
    m_chainPosition = position;
    m_chainTotal = total;
    m_chainIndex = chainIndex;
    
    // Update card styling
    if (total > 1) {
        // Show chain indicator badge
        QString badge = QString("%1/%2").arg(position + 1).arg(total);
        m_chainBadgeLabel->setText(badge);
        m_chainBadgeLabel->show();
        
        // Apply visual grouping
        QString styleSheet = "QFrame { ";
        if (position == 0) {
            styleSheet += "border-left: 3px solid #0078D7; ";
        } else if (position == total - 1) {
            styleSheet += "border-right: 3px solid #0078D7; ";
        }
        styleSheet += "}";
        setStyleSheet(styleSheet);
    }
}
```

---

## Alternative Implementations

### Alt A: Lazy Chain Lookup

Instead of storing chains, cards query on-demand:

```cpp
// In createCardForIndex()
// Don't store chains, just provide lookup method
card->setCardManager(this);  // Give card access to manager

// In AnimeCard - when needed
AnimeChain chain = m_cardManager->getChainForAnime(m_animeId);
```

**Pros:** Less memory, chains computed on-demand
**Cons:** More CPU, need to maintain reference

---

### Alt B: Chain Data in CardCreationDataCache

Store chain info directly in cached data:

```cpp
// mylistcardmanager.h
struct CardCreationData {
    // ... existing fields ...
    
    // Chain context
    int chainIndex;
    int positionInChain;
    int totalInChain;
};
```

**Pros:** All data in one place, no separate storage
**Cons:** Couples chain logic to cache, harder to update dynamically

---

## Summary of Generation Timing Options

| Option | When Generated | Where | Efficiency | Complexity |
|--------|---------------|-------|------------|------------|
| 1. During Sort | Every sort | Window::sortMylistCards() | ⭐⭐ (rebuilds often) | ⭐⭐⭐ (simple) |
| 2. During Filter | Filter change | Window::applyMylistFilters() | ⭐⭐⭐ (once per filter) | ⭐⭐ (state mgmt) |
| 3. In CardManager | setAnimeIdList() | MyListCardManager | ⭐⭐ (builds relation tree) | ⭐ (duplicates logic) |
| **Hybrid (Rec.)** | **During Sort** | **Window + CardManager** | **⭐⭐⭐** | **⭐⭐** |

**Recommendation:** Use the Hybrid approach (Option 1 + storage in CardManager)
- Generate chains in Window using existing logic
- Pass to CardManager for storage and card context
- Simple, efficient, leverages both systems' strengths

---

## Migration Path

1. ✅ **Done:** Relation data in CardCreationDataCache
2. ✅ **Done:** AnimeChain data structure exists
3. 🔄 **Next:** Add chain storage to MyListCardManager (Phase 1)
4. 🔄 **Next:** Modify Window to pass chains (Phase 2)
5. ⏭️ **Later:** Add chain context to cards (Phase 3)
6. ⏭️ **Later:** Visual enhancements (Phase 4)

Each phase is independent and can be implemented/tested separately.
