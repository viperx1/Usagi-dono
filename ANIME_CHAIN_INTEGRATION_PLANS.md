# AnimeChain Integration Plans

## Current Architecture Overview

**Card Creation Flow:**
1. `Window::loadMylistAsCards()` loads anime IDs from database
2. `MyListCardManager::preloadCardCreationData()` preloads all data (now includes relation data)
3. `MyListCardManager::setAnimeIdList()` sets ordered list of anime IDs
4. `VirtualFlowLayout` uses item factory to create cards on-demand via `MyListCardManager::createCardForIndex()`

**Current Chain Handling:**
- `Window::sortMylistCards()` groups anime into chains
- Chains are sorted externally (by first anime criteria)
- Internal order (prequel→sequel) is preserved
- Result is flattened back to `QList<int>` of anime IDs
- Cards displayed individually with arrows connecting them

## Integration Approaches

### **Plan A: Chain-Aware Item Factory (Recommended)**

**Concept:** Keep individual cards but make the layout chain-aware by passing chain metadata alongside the anime ID list.

**Implementation:**
1. Add chain grouping information to MyListCardManager:
   ```cpp
   class MyListCardManager {
       QList<int> m_animeIdList;              // Flattened list (current)
       QList<AnimeChain> m_chainList;         // NEW: List of chains
       QMap<int, int> m_aidToChainIndex;      // NEW: aid -> chain index mapping
   };
   ```

2. Modify `setAnimeIdList()` to also accept chain data:
   ```cpp
   void setAnimeIdList(const QList<int>& aids, const QList<AnimeChain>& chains);
   ```

3. Cards can query their chain context:
   ```cpp
   AnimeCard* card = createCardForIndex(index);
   int aid = card->getAnimeId();
   int chainIndex = m_aidToChainIndex[aid];
   AnimeChain chain = m_chainList[chainIndex];
   // Card knows: position in chain, siblings, etc.
   ```

4. Visual enhancements:
   - Cards can render differently based on position (first/middle/last in chain)
   - Background color/border to group chain members
   - Chain statistics (e.g., "3/5 in series")

**Pros:**
- ✅ Minimal changes to existing architecture
- ✅ Works with virtual scrolling
- ✅ Cards can be individually interactive
- ✅ Easy to implement incrementally
- ✅ No breaking changes to VirtualFlowLayout

**Cons:**
- ❌ Still creates individual widgets (not a "chain widget")
- ❌ Visual grouping is cosmetic, not structural

---

### **Plan B: Dual-Mode Layout (Chain Widget vs Card Widget)**

**Concept:** Create a separate widget type for chains, and use a factory that returns either AnimeCard or AnimeChainWidget based on mode.

**Implementation:**
1. Create `AnimeChainWidget` that contains multiple AnimeCards:
   ```cpp
   class AnimeChainWidget : public QWidget {
       QHBoxLayout *m_layout;
       QList<AnimeCard*> m_cards;
       
       void addCard(AnimeCard *card);  // Add card to chain
       QSize sizeHint() const;         // Sum of card widths + spacing
   };
   ```

2. Modify item factory to be mode-aware:
   ```cpp
   QWidget* MyListCardManager::createWidgetForIndex(int index) {
       if (chainModeEnabled && isChainIndex(index)) {
           return createChainWidgetForIndex(index);
       } else {
           return createCardForIndex(index);
       }
   }
   ```

3. VirtualFlowLayout needs to know chain vs card:
   - In card mode: 1 item = 1 anime = 1 card widget
   - In chain mode: 1 item = 1 chain = 1 chain widget (containing N cards)

**Pros:**
- ✅ True "chain as a unit" representation
- ✅ Can apply operations to entire chain
- ✅ Clear visual grouping (frame around chain)

**Cons:**
- ❌ Complex changes to VirtualFlowLayout
- ❌ Variable item sizes (chains have different widths)
- ❌ Virtual scrolling assumes fixed item sizes
- ❌ Memory overhead (creates all cards in visible chains)
- ❌ Significant refactoring required

---

### **Plan C: List-Based Chain View (Alternative Layout)**

**Concept:** Instead of modifying the flow layout, create an alternative vertical list view where each item is a chain.

**Implementation:**
1. Create `AnimeChainListWidget` (alternative to VirtualFlowLayout):
   ```cpp
   class AnimeChainListWidget : public QListWidget {
       void setChainList(const QList<AnimeChain>& chains);
   };
   ```

2. Each list item contains a horizontal layout with cards:
   ```
   [Chain 1: Card A → Card B → Card C]
   [Chain 2: Card D]
   [Chain 3: Card E → Card F]
   ```

3. User toggles between views:
   - Grid view (current): Individual cards in flow layout
   - Chain view (new): Chains in list widget

**Pros:**
- ✅ Clean separation of concerns
- ✅ Doesn't break existing grid view
- ✅ Natural fit for chain representation
- ✅ Easy to implement as separate feature

**Cons:**
- ❌ Completely separate view (duplication)
- ❌ No virtual scrolling benefits
- ❌ User has to choose between views

---

### **Plan D: Enhanced Arrow Overlay with Visual Grouping**

**Concept:** Keep the current architecture but enhance visual grouping through improved rendering.

**Implementation:**
1. Keep flattened anime ID list
2. Pass chain metadata to ArrowOverlay
3. Enhance ArrowOverlay to draw:
   - Background boxes around chain groups
   - Colored borders
   - Chain labels (e.g., "Series: Title → Title 2 → Title 3")

4. Add chain context to cards for styling:
   ```cpp
   card->setChainPosition(positionInChain, totalInChain);
   // Card can style itself: rounded corners on first/last, etc.
   ```

**Pros:**
- ✅ Zero architectural changes
- ✅ Works perfectly with virtual scrolling
- ✅ Quick to implement
- ✅ Backwards compatible

**Cons:**
- ❌ Only visual changes, no functional grouping
- ❌ Chains are not "units" in the data model
- ❌ Can't easily operate on entire chain

---

## Recommendation: **Plan A (Chain-Aware Item Factory)**

### Why Plan A?

1. **Balances functionality and complexity**: Provides chain awareness without breaking existing architecture
2. **Incremental implementation**: Can be done step-by-step
3. **Works with virtual scrolling**: Maintains performance benefits
4. **Flexible**: Can add visual enhancements later (backgrounds, borders, etc.)
5. **Future-proof**: Enables chain-based operations (hide chain, mark all watched, etc.)

### Implementation Steps

**Phase 1: Add Chain Metadata**
```cpp
// In MyListCardManager
private:
    QList<AnimeChain> m_chainList;
    QMap<int, int> m_aidToChainIndex;  // aid -> chain index

public:
    void setChainList(const QList<AnimeChain>& chains);
    AnimeChain getChainForAnime(int aid) const;
    int getChainIndexForAnime(int aid) const;
```

**Phase 2: Update Window Sorting**
```cpp
// In Window::sortMylistCards()
// After building chainGroups:
QList<AnimeChain> chains;
for (const QList<int>& chainAids : chainGroups.values()) {
    chains.append(AnimeChain(chainAids));
}
cardManager->setChainList(chains);
```

**Phase 3: Enhance Card Display (Optional)**
```cpp
// In AnimeCard
void setChainInfo(int positionInChain, int totalInChain, int chainIndex);

// Cards can then:
// - Show "2/3 in series"
// - Apply different styling (first/middle/last)
// - Draw connecting indicators
```

**Phase 4: Add Chain Operations (Future)**
```cpp
// In MyListCardManager
void hideEntireChain(int chainIndex);
void markChainAsWatched(int chainIndex);
QList<int> getChainAnimeIds(int chainIndex) const;
```

### Migration Path

1. ✅ Already done: Relation data in CardCreationDataCache
2. ✅ Already done: AnimeChain data structure exists
3. 🔄 Next: Add chain list to MyListCardManager
4. 🔄 Next: Pass chain data from Window to CardManager
5. ⏭️ Later: Enhance card visual styling
6. ⏭️ Later: Add chain-based operations

## Alternative Quick Win: **Plan D (Enhanced Visual Grouping)**

If Plan A seems too complex, Plan D can be implemented very quickly:

1. Pass chain grouping info to ArrowOverlay
2. Draw semi-transparent background boxes around chain groups
3. Add subtle borders or shadows
4. Result: Clear visual indication of chains without any architectural changes

This gives immediate visual improvement while keeping the door open for Plan A later.
