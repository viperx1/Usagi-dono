# Anime Chain Feature

## Overview

The anime chain feature allows users to view related anime (prequels/sequels) as grouped sequences with specialized sorting behavior. This feature is already implemented and functional in the codebase.

## How It Works

### 1. Relation Data Storage

Relation data is stored in the `anime` table in the database:
- `relaidlist`: List of related anime IDs (apostrophe-separated)
- `relaidtype`: List of relation types (apostrophe-separated, numeric codes)

Relation type codes:
- `1` = Sequel
- `2` = Prequel
- Other types exist but are not used for chain grouping

### 2. Relation Data Loading

The `WatchSessionManager` class handles relation data:
- `loadAnimeRelations(int aid)`: Loads relation data from database into `m_relationsCache`
- `getSeriesChain(int aid)`: Returns ordered list of anime IDs from prequel to sequel
- `getOriginalPrequel(int aid)`: Finds the first anime in the chain

The loading is lazy and cached:
- First call loads data from database
- Subsequent calls return cached data
- No separate preload needed - efficient as-is

### 3. Chain Display Mode

Users enable chain mode via the "Display series chain" checkbox in the filter sidebar.

When enabled, the display behavior changes:

**Sorting (`sortMylistCards()` in window.cpp, lines 4152-4355)**:
1. Groups anime into chains using `getSeriesChain()`
2. External sorting: Chains are sorted by selected criteria (title, type, aired date, etc.) using the first anime in each chain as the representative
3. Internal sorting: Within each chain, the prequel→sequel order is preserved
4. Result: Chains stay together and maintain their sequential order

**Visual Representation**:
- Individual anime cards are displayed in sequence
- Arrows connect related anime via the `ArrowOverlay` class in `virtualflowlayout.cpp`
- Connection points are calculated from card edges (`getLeftConnectionPoint()`, `getRightConnectionPoint()`)

### 4. Chain Grouping Logic

The chain grouping algorithm (in `sortMylistCards()`):

```
For each anime in the filtered list:
  1. Get its full series chain
  2. Filter chain to only include anime in the current view
  3. Use first anime as the chain key
  4. Store filtered chain under that key

Sort chain keys by selected criteria
Flatten chains back to ordered list
```

### 5. Example

Consider these anime:
- A (2020) - original
- B (2021) - sequel to A  
- C (2022) - sequel to B
- D (2019) - unrelated anime

**Without chain mode** (sorted by year ascending):
```
D → A → B → C
```

**With chain mode** (sorted by year ascending):
- Chains: [D], [A→B→C]
- External sort by year: [D(2019)], [A→B→C (2020)]
- Result: D, then A→B→C in sequence
```
D → A → B → C
```

But if sorting by title, chains would be sorted by their first anime's title.

## Implementation Classes

### WatchSessionManager
- Manages relation data cache
- Provides `getSeriesChain()` and `getOriginalPrequel()` methods
- Located in: `usagi/src/watchsessionmanager.{h,cpp}`

### Window::sortMylistCards()
- Implements chain grouping and nested sorting
- Located in: `usagi/src/window.cpp`, lines 4095-4670

### ArrowOverlay
- Draws arrows connecting related anime
- Located in: `usagi/src/virtualflowlayout.cpp`, lines 14-118

### AnimeCard
- Stores prequel/sequel AIDs for arrow drawing
- Provides connection point methods
- Located in: `usagi/src/animecard.{h,cpp}`

### AnimeChain (New)
- Widget class for potential future enhanced chain display
- Currently not used but available for expansion
- Located in: `usagi/src/animechain.{h,cpp}`

## Current Status

The anime chain feature is **fully implemented and functional**:
- ✓ Relation data is stored in database
- ✓ Relation data is loaded on-demand and cached
- ✓ Chain grouping works correctly
- ✓ External sorting (chains vs chains) works
- ✓ Internal sorting (prequel→sequel order) is preserved
- ✓ Visual arrows connect related anime

## Future Enhancements

Possible improvements:
1. Use `AnimeChain` widget to provide more prominent visual grouping (background boxes around chains)
2. Add chain statistics (e.g., "Chain of 3 anime")
3. Collapse/expand chains to hide/show members
4. Different arrow styles or colors for different relation types
5. Show relation type labels (prequel/sequel) on arrows
