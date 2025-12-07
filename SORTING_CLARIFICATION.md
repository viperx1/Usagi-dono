# Internal vs External Sorting Clarification

## The Two-Level Sorting Problem

### User Example
If a chain consists of anime: **V, S, D** (in prequel→sequel order), alphabetically sorting the anime list results in:

```
A...C, [D, S, V], E..R, T, U, W...Z
     ↑____________↑
     Chain stays together,
     internal order preserved (D→S→V)
```

**NOT:**
```
A...C, D, E..R, S, T, U, V, W...Z
       ↑       ↑     ↑
       Chain broken apart
```

---

## Two-Level Sorting Design

### Level 1: External Sorting (Chain-to-Chain)
**What:** Sort chains relative to each other  
**Criteria:** Based on representative anime (first in chain)  
**Example:** If sorting by title:
- Chain A (representative: "Attack on Titan") comes before
- Chain B (representative: "Bleach")

### Level 2: Internal Sorting (Within Chain)
**What:** Maintain order within each chain  
**Criteria:** Always prequel → sequel (fixed, never changes)  
**Example:** Chain always shows: Season 1 → Season 2 → Season 3

---

## Implementation Details

### In AnimeChain Class

```cpp
class AnimeChain
{
public:
    // Internal order: NEVER changes, always prequel→sequel
    QList<int> getAnimeIds() const { return m_animeIds; }  // Already ordered
    
    // External comparison: Used for sorting chains against each other
    int compareWith(const AnimeChain& other,
                    const QMap<int, CardCreationData>& dataCache,
                    SortCriteria criteria,
                    bool ascending) const;
    
    // Representative anime: Used for external comparisons
    int getRepresentativeAnimeId() const {
        return m_animeIds.isEmpty() ? 0 : m_animeIds.first();
    }

private:
    // Internal order is IMMUTABLE once chain is built
    // This list is ALWAYS in prequel→sequel order
    QList<int> m_animeIds;
};
```

### In MyListCardManager

```cpp
void MyListCardManager::sortChains(SortCriteria criteria, bool ascending)
{
    // EXTERNAL SORTING: Sort chains relative to each other
    std::sort(m_chainList.begin(), m_chainList.end(),
              [this, criteria, ascending](const AnimeChain& a, const AnimeChain& b) {
                  return a.compareWith(b, m_cardCreationDataCache, criteria, ascending) < 0;
              });
    
    // After external sort, flatten to anime ID list
    // Each chain maintains its INTERNAL order (prequel→sequel)
    m_orderedAnimeIds.clear();
    for (const AnimeChain& chain : m_chainList) {
        // Chain's internal order is preserved in getAnimeIds()
        m_orderedAnimeIds.append(chain.getAnimeIds());
    }
}
```

### Example: Sorting by Title (Alphabetical)

**Input:** 3 chains in database
```
Chain 1: [Tokyo Ghoul, Tokyo Ghoul √A, Tokyo Ghoul:re]  // Representative: Tokyo Ghoul
Chain 2: [Attack on Titan, Attack on Titan S2, Attack on Titan S3]  // Representative: Attack on Titan
Chain 3: [Death Note]  // Representative: Death Note (single anime)
```

**After External Sort (alphabetical by representative title):**
```
Position 0: Chain 2 [Attack on Titan, Attack on Titan S2, Attack on Titan S3]
Position 1: Chain 3 [Death Note]
Position 2: Chain 1 [Tokyo Ghoul, Tokyo Ghoul √A, Tokyo Ghoul:re]
```

**Flattened anime ID list passed to view:**
```
[Attack on Titan, Attack on Titan S2, Attack on Titan S3, Death Note, Tokyo Ghoul, Tokyo Ghoul √A, Tokyo Ghoul:re]
 ↑___________________________________________________↑  ↑________↑  ↑_________________________________________↑
                 Chain 2 (internal order)              Chain 3              Chain 1 (internal order)
```

**Visual result:**
```
[Card: Attack on Titan] → [Card: Attack on Titan S2] → [Card: Attack on Titan S3]
[Card: Death Note]
[Card: Tokyo Ghoul] → [Card: Tokyo Ghoul √A] → [Card: Tokyo Ghoul:re]
```

---

## Key Invariants

### 1. Internal Order NEVER Changes
```cpp
// When chain is built, order is set once
QList<int> chainAids = buildChainFromAid(startAid, availableAids);
AnimeChain chain(chainAids);  // Order is now immutable

// Later, even after sorting:
chain.getAnimeIds();  // ALWAYS returns same order (prequel→sequel)
```

### 2. External Sort Changes Chain Position Only
```cpp
// Before external sort:
m_chainList = [ChainA, ChainB, ChainC]

// After external sort (by title):
m_chainList = [ChainB, ChainA, ChainC]
              ↑ Order changed

// But each chain's internal order unchanged:
ChainA.getAnimeIds() == [S1, S2, S3]  // Still prequel→sequel
ChainB.getAnimeIds() == [Movie1, Movie2]  // Still prequel→sequel
```

### 3. Flattening Preserves Internal Order
```cpp
QList<int> flatList;
for (const AnimeChain& chain : sortedChains) {
    // Append entire chain in its internal order
    flatList.append(chain.getAnimeIds());
}
// Result: Chains in external sorted order, each maintaining internal order
```

---

## Potential Clarifications Needed

### 1. What if user wants to sort WITHIN chains?
**Answer:** Not supported in this design. Internal order is always prequel→sequel.
**Rationale:** Chain definition is based on story progression, which has natural order.
**Alternative:** Disable chain mode if user wants different ordering.

### 2. What if a chain has no clear prequel?
**Example:** Multiple movies with no sequel relationship
**Answer:** They form separate single-item chains.
**Rationale:** Chain building only groups anime with explicit sequel/prequel relations.

```cpp
// Movie A, Movie B (no relation) → 2 separate chains
Chain 1: [Movie A]
Chain 2: [Movie B]
```

### 3. What if chains overlap or have cycles?
**Example:** A→B, B→C, C→A (cycle)
**Answer:** Cycle detection in chain building prevents infinite loops.

```cpp
QList<int> MyListCardManager::buildChainFromAid(int startAid, ...) const
{
    QSet<int> visited;  // Prevents revisiting same anime
    while (currentAid > 0 && !visited.contains(currentAid)) {
        visited.insert(currentAid);  // Mark as visited
        // ... follow prequel/sequel links
    }
    // If cycle detected, loop terminates
}
```

### 4. How are sort criteria applied?
**Answer:** Only representative (first) anime's data is used for external comparison.

```cpp
int AnimeChain::compareWith(..., SortCriteria criteria, ...) const
{
    int myAid = getRepresentativeAnimeId();        // First anime
    int otherAid = other.getRepresentativeAnimeId();  // First anime
    
    // Compare using representative anime's data
    const CardCreationData& myData = dataCache[myAid];
    const CardCreationData& otherData = dataCache[otherAid];
    
    // Only representative anime's title/date/type is compared
    return myData.animeTitle.compare(otherData.animeTitle);
}
```

**Implication:** Chain "Attack on Titan, AoT S2, AoT S3" is sorted based on "Attack on Titan" only.

### 5. What if representative anime is missing data?
**Answer:** Fallback to anime ID comparison (numeric).

```cpp
if (!dataCache.contains(myAid) || !dataCache.contains(otherAid)) {
    result = myAid - otherAid;  // Numeric ID comparison
}
```

### 6. Can chains be filtered?
**Answer:** Yes, chains are rebuilt after filtering.

**Flow:**
```
User applies filter (e.g., "only TV series")
  ↓
Filter anime IDs
  ↓
Build chains from filtered IDs
  ↓
Some chains may be broken/incomplete if filtered members removed
```

**Example:**
```
Original chain: [Movie, TV S1, TV S2, OVA]
Filter: "TV only"
Filtered IDs: [TV S1, TV S2]
Resulting chain: [TV S1, TV S2]  // Movie and OVA filtered out
```

### 7. Visual indicators for chains?
**Answer:** Phase 4 enhancement (future).

**Possible implementations:**
- Background color grouping
- Chain position badges ("1/3", "2/3", "3/3")
- Connecting lines/arrows (already implemented)
- Border styling (first card: left border, last card: right border)

---

## Design Validation Questions

### Q1: Is the two-level sorting clear in the code?
**A:** Yes, explicit separation:
- External: `sortChains()` in MyListCardManager
- Internal: `getAnimeIds()` in AnimeChain (immutable)

### Q2: Can external sorting be changed without affecting internal order?
**A:** Yes, `sortChains()` reorders `m_chainList` without modifying individual chains.

### Q3: Is performance acceptable?
**A:** Yes:
- Chain building: O(n) - one pass through anime IDs
- External sorting: O(k log k) where k = number of chains (k < n)
- Total: O(n + k log k) which is faster than O(n log n) individual sorting

### Q4: Is the design extensible?
**A:** Yes:
- New sort criteria: Add enum value + case in `compareWith()`
- New chain types: Extend relation type checking in `buildChainFromAid()`
- New visual styles: Add to Phase 4 without changing core logic

### Q5: Does this handle edge cases?
**A:** Yes:
- Single-anime chains: Treated same as multi-anime chains
- Missing data: Fallback to ID comparison
- Cycles: Detected and prevented
- Filtering: Chains rebuilt from filtered set

---

## Summary

**The design correctly implements two-level sorting:**

1. **External (Chain-to-Chain):** Sorts chains by representative anime's criteria
2. **Internal (Within-Chain):** Maintains immutable prequel→sequel order

**Key points:**
- Internal order is set once during chain building and never changes
- External sorting only reorders chains, not anime within chains
- Flattening preserves both levels of ordering
- Performance is optimized by sorting fewer chains rather than many anime
- Design is SOLID-compliant and extensible

**No ambiguities identified** - the design document clearly separates internal and external sorting responsibilities across classes.
