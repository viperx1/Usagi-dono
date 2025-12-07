# Anime Data Architecture: Filtering and Display System

## Overview

This document provides a comprehensive explanation of how anime data flows through the system from database to display, covering all data structures, caching layers, and processing steps involved in filtering and card display.

---

## Core Data Structures

### 1. Database Layer (SQLite)

#### Primary Tables:
- **`anime`**: Stores anime metadata (titles, type, dates, ratings, tags, etc.)
- **`mylist`**: Stores user's mylist entries (files they own) with episode info
- **`mylist_anime`**: Links mylist entries to anime IDs
- **`episode`**: Stores episode information from AniDB
- **`file`**: Stores file metadata from AniDB
- **`anime_relation`**: Stores relationships between anime (sequels, prequels, etc.)

#### Cache Tables:
- **`anime_title_search`**: Alternative titles cache for search functionality
  - Schema: `(aid INTEGER, title TEXT COLLATE NOCASE)`
  - Populated during mylist load
  - Updated incrementally when anime metadata changes

---

### 2. Application Layer Data Structures

#### Window Class (Main Controller)

**Core Filtering Lists:**
```cpp
QList<int> allAnimeIdsList;      // Master list of all anime IDs (never filtered)
QSet<int> mylistAnimeIdSet;      // Set of anime IDs in user's mylist
QList<int> filteredAnimeIds;     // Currently filtered/displayed anime IDs
```

**Alternative Titles Cache:**
```cpp
QStringList allAnimeTitles;                  // All alternative titles for search
QMap<QString, int> alternativeTitleToAid;    // Title -> anime ID mapping
```

**Series Chain Cache:**
```cpp
QMap<int, QList<int>> chainCache;  // aid -> ordered chain of related anime IDs
```

**Thread Safety:**
```cpp
QMutex filterOperationsMutex;      // Protects filter/sort operations
QMutex backgroundLoadingMutex;     // Protects background loading operations
```

---

#### MyListCardManager Class (Card Lifecycle Manager)

**Card Cache:**
```cpp
QMap<int, AnimeCard*> m_cards;     // aid -> card widget mapping
```

**Card Creation Data Cache:**
```cpp
QMap<int, CardCreationData> m_cardCreationDataCache;
```

**CardCreationData Structure:**
```cpp
struct CardCreationData {
    // Anime metadata
    QString nameRomaji;
    QString nameEnglish;
    QString animeTitle;
    QString typeName;
    QString startDate;
    QString endDate;
    QString picname;
    QByteArray posterData;
    QString category;
    QString rating;
    QString tagNameList;
    bool isHidden;
    bool is18Restricted;
    int eptotal;
    
    // Statistics
    AnimeStats stats;               // Episode counts and states
    
    // Episodes
    QList<EpisodeCacheEntry> episodes;  // All episodes for this anime
    
    // Validity flag
    bool hasData;
};
```

**EpisodeCacheEntry Structure:**
```cpp
struct EpisodeCacheEntry {
    int lid;              // MyList ID
    int eid;              // Episode ID
    int fid;              // File ID
    int state;            // AniDB state (watched/unwatched)
    int viewed;           // AniDB viewed flag
    QString storage;      // Storage location
    QString episodeName;  // Episode title
    QString epno;         // Episode number (e.g., "1", "S1")
    QString filename;     // Original filename
    qint64 lastPlayed;    // Last playback timestamp
    QString localFilePath;  // Local file path
    QString resolution;   // Video resolution
    QString quality;      // Quality info
    QString groupName;    // Release group
    int localWatched;     // Local watched status
};
```

**Ordered Display List:**
```cpp
QList<int> m_orderedAnimeIds;  // anime IDs in display order (for virtual scrolling)
```

---

#### CachedAnimeData Class (Lightweight Data for Filtering)

Used for filtering/sorting WITHOUT creating card widgets (essential for virtual scrolling):

```cpp
class CachedAnimeData {
private:
    QString m_animeName;        // Display name
    QString m_typeName;         // Anime type (TV, Movie, OVA)
    QString m_startDate;        // Start air date
    QString m_endDate;          // End air date
    bool m_isHidden;            // Hidden flag
    bool m_is18Restricted;      // 18+ flag
    int m_eptotal;              // Total episode count
    AnimeStats m_stats;         // Episode statistics
    qint64 m_lastPlayed;        // Most recent playback
    bool m_hasData;             // Validity flag
};
```

---

#### AnimeStats Class (Episode Statistics)

Tracks episode counts and viewing status:

```cpp
class AnimeStats {
private:
    int m_normalEpisodes;       // Count of normal episodes
    int m_normalWatched;        // Count of normal episodes watched
    int m_normalFiles;          // Count of files for normal episodes
    int m_specialEpisodes;      // Count of special episodes
    int m_specialWatched;       // Count of special episodes watched
    int m_specialFiles;         // Count of files for special episodes
    int m_totalFiles;           // Total file count
    int m_totalWatched;         // Total watched count
};
```

---

## Data Flow: From Database to Display

### Phase 1: Initial Load (Background Thread)

**Triggered by:** Application startup or manual mylist refresh

```
1. startBackgroundLoading()
   └─> MylistLoaderWorker::doWork()
       └─> SQL: "SELECT DISTINCT aid FROM mylist_anime ORDER BY aid"
       └─> Returns: QList<int> aids (all anime IDs in mylist)
       
2. onMylistLoadingFinished(QList<int> aids)
   ├─> allAnimeIdsList = aids              // Store master list
   ├─> mylistAnimeIdSet = aids.toSet()     // Create fast lookup set
   └─> loadAnimeAlternativeTitlesForFiltering()
       └─> AnimeTitlesLoaderWorker::doWork()
           ├─> Populate anime_title_search table with all alternative titles
           │   SQL: INSERT INTO anime_title_search VALUES for each title type
           └─> Returns: QStringList titles, QMap<QString, int> titleToAid
```

**Alternative Titles Loaded:**
- Romaji name
- English name
- Official title
- Synonyms (comma-separated, split into individual titles)
- Short names
- All stored case-insensitive for search

---

### Phase 2: Filtering (Main Thread)

**Triggered by:** Search text change, filter checkbox change, sort change, series chain toggle

```
applyMylistFilters()  [Protected by filterOperationsMutex]
│
├─> Start with allAnimeIdsList (master list, never modified)
│
├─> Apply Search Filter (if search text present)
│   └─> For each anime in allAnimeIdsList:
│       └─> Check if any alternative title contains search text (case-insensitive)
│       └─> If match, add to filteredAnimeIds
│
├─> Apply MyList Filter (if "Only in MyList" checked)
│   └─> Filter filteredAnimeIds to only include anime in mylistAnimeIdSet
│
├─> Apply Type Filter (if type selected)
│   └─> For each anime in filteredAnimeIds:
│       └─> Load CachedAnimeData and check typeName matches
│
├─> Apply Completion Filter (if completion status selected)
│   └─> For each anime in filteredAnimeIds:
│       └─> Load CachedAnimeData and check AnimeStats completion
│
├─> Apply Unwatched Filter (if "Show Unwatched Only" checked)
│   └─> Filter to anime with unwatched episodes (stats.normalWatched < stats.normalEpisodes)
│
├─> Apply Adult Content Filter (based on user preference)
│   └─> Filter to hide/show 18+ anime (CachedAnimeData.is18Restricted)
│
└─> Apply Series Chain Display (if enabled)
    │
    ├─> Expand Chains (add related anime)
    │   └─> For each anime in filteredAnimeIds:
    │       ├─> Get series chain: getSeriesChain(aid)
    │       │   └─> SQL: "SELECT * FROM anime_relation WHERE aid=? ORDER BY..."
    │       │   └─> Returns: [prequel, anime1, anime2, sequel, ...] (ordered chain)
    │       └─> Add ALL anime in chain to animeToProcess (ignore mylist filter)
    │
    ├─> Build Chain Groups
    │   └─> Group anime by their shared chain
    │   └─> Cache chains for fast lookup
    │   └─> Handle edge cases (empty chains, anime not in own chain)
    │
    ├─> Rebuild filteredAnimeIds
    │   └─> Clear and rebuild from chain groups in proper order
    │
    └─> Apply Sorting Within Chains
        └─> Sort chain groups themselves
        └─> Preserve episode order within each chain
```

---

### Phase 3: Data Preloading (Before Card Creation)

**Critical Step:** Ensures NO SQL queries happen during card creation

```
Before cardManager->setAnimeIdList(filteredAnimeIds):

1. Validate Final List
   └─> For each aid in filteredAnimeIds:
       └─> Check if cardManager->hasCachedData(aid)
       └─> Collect anime without cached data

2. Preload Missing Data
   └─> cardManager->preloadCardCreationData(missingAids)
       │
       ├─> preloadAnimeDataCache(aids)
       │   └─> SQL: "SELECT * FROM anime WHERE aid IN (...)"
       │   └─> For each anime:
       │       ├─> Store in m_cardCreationDataCache[aid]
       │       ├─> Load poster from database
       │       └─> Calculate AnimeStats
       │
       └─> preloadEpisodesCache(aids)
           └─> SQL: "SELECT * FROM mylist WHERE aid IN (...)"
           └─> Build EpisodeCacheEntry list for each anime
           └─> Store in m_cardCreationDataCache[aid].episodes

Result: m_cardCreationDataCache contains EVERYTHING needed for instant card creation
```

---

### Phase 4: Card Display (Virtual Scrolling)

**Lazy Loading:** Only cards in viewport are created

```
1. cardManager->setAnimeIdList(filteredAnimeIds)
   └─> m_orderedAnimeIds = filteredAnimeIds
   └─> Notify virtual layout of new count

2. VirtualFlowLayout determines visible range
   └─> For each visible index:
       └─> cardManager->createCardForIndex(index)
           │
           ├─> Get aid = m_orderedAnimeIds[index]
           │
           ├─> Check if card exists: m_cards.contains(aid)
           │   └─> If yes: return existing card (fast path)
           │   └─> If no: create new card
           │
           └─> createCard(aid)  [Data MUST be preloaded]
               │
               ├─> Get data: m_cardCreationDataCache[aid]
               │   └─> ERROR if not found: "No card creation data"
               │
               ├─> Create AnimeCard widget
               │
               ├─> Set all data from cache (no SQL!)
               │   ├─> Set title, type, dates
               │   ├─> Set poster image
               │   ├─> Set tags/category
               │   └─> Set statistics
               │
               ├─> Load episodes from cache
               │   └─> loadEpisodesForCardFromCache(card, aid, data.episodes)
               │       └─> For each episode:
               │           ├─> Add to card widget
               │           ├─> Set watched state
               │           └─> Set file info
               │
               ├─> Set series chain info
               │   └─> setSeriesChainInfo(card, aid, filteredAnimeIds)
               │       └─> Determine if anime has prequel/sequel
               │       └─> Draw arrows on card
               │
               └─> Store in cache: m_cards[aid] = card

3. Cards rendered in viewport
   └─> User scrolls -> new cards created for new visible range
   └─> Old cards remain in cache for instant reuse
```

---

## Key Design Patterns

### 1. **Two-Phase Data Loading**
- **Phase 1:** Load anime IDs only (lightweight)
- **Phase 2:** Preload full data for filtered subset (batch operation)

### 2. **Lazy Card Creation**
- Cards created only when visible (virtual scrolling)
- Cards cached permanently (created once, reused forever)
- Filtering changes card visibility, not card state

### 3. **Multi-Level Caching**
```
Database
  ↓
CardCreationDataCache (full data for card creation)
  ↓
CachedAnimeData (lightweight for filtering)
  ↓
Card Widgets (full UI components)
```

### 4. **Separation of Concerns**
- **Window:** Manages filtering logic and anime ID lists
- **MyListCardManager:** Manages card lifecycle and caching
- **AnimeCard:** UI widget with display logic only
- **VirtualFlowLayout:** Manages viewport and lazy loading

---

## Critical Invariants

### Data Consistency Rules:

1. **allAnimeIdsList NEVER changes** after initial load (until manual refresh)
   - Master source of truth
   - All filtering starts from this list

2. **filteredAnimeIds rebuilds completely** on each filter change
   - Never modified in-place
   - Always reconstructed from allAnimeIdsList

3. **Card creation data MUST be preloaded** before setAnimeIdList()
   - Validation check before card manager receives IDs
   - Prevents "No card creation data" errors

4. **Cards are immutable once created**
   - Updates happen via signals/slots
   - Card state synchronized with database changes

5. **Series chain expansion ignores mylist filter**
   - Shows complete chains including anime not in mylist
   - User sees full series context

---

## Performance Optimizations

### 1. **Batch SQL Queries**
- Single query for all anime: `WHERE aid IN (...)`
- Single query for all episodes: `WHERE aid IN (...)`
- Eliminates N+1 query problem

### 2. **Virtual Scrolling**
- Only 20-30 cards in DOM at once
- Handles 10,000+ anime without performance issues
- Card widgets recycled efficiently

### 3. **Alternative Titles Cache**
- Pre-populated in database table
- Case-insensitive search via COLLATE NOCASE
- Updated incrementally (not rebuilt on every change)

### 4. **Thread Safety with Minimal Locking**
- filterOperationsMutex only held during filtering
- Background loading in separate threads
- Cards created on main thread (Qt UI requirement)

---

## Common Issues and Solutions

### Issue 1: "No card creation data for aid=X"

**Cause:** Card creation attempted before data preload

**Solution:** 
```cpp
// Before setAnimeIdList(), validate:
QList<int> missingData;
for (int aid : filteredAnimeIds) {
    if (!cardManager->hasCachedData(aid)) {
        missingData.append(aid);
    }
}
if (!missingData.isEmpty()) {
    cardManager->preloadCardCreationData(missingData);
}
```

### Issue 2: Cards disappear when enabling series chain

**Causes:**
1. getSeriesChain() returns chain not including queried anime
2. Chain grouping loses anime with empty chains
3. Sorting filters out anime not in filtered chain

**Solutions:**
1. Always add queried anime to its own chain before caching
2. Create single-anime chain for empty chain results
3. Treat anime as single-anime chain if not in filtered chain

### Issue 3: Search doesn't find anime after update

**Cause:** Alternative titles cache not updated when metadata changes

**Solution:**
```cpp
// In getNotifyAnimeUpdated():
updateAnimeAlternativeTitlesInCache(aid);
```

---

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                        SQLite Database                       │
│  ┌─────────┐  ┌─────────┐  ┌──────────┐  ┌───────────────┐│
│  │  anime  │  │ mylist  │  │ episode  │  │ anime_title_  ││
│  │         │  │         │  │          │  │   search      ││
│  └────┬────┘  └────┬────┘  └────┬─────┘  └───────┬───────┘│
└───────┼────────────┼────────────┼─────────────────┼────────┘
        │            │            │                 │
        ▼            ▼            ▼                 ▼
┌────────────────────────────────────────────────────────────┐
│              Background Loading (Separate Thread)          │
│  - Load all anime IDs                                      │
│  - Populate alternative titles cache                       │
│  - Return lightweight lists to main thread                 │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│                   Window (Main Thread)                     │
│  ┌──────────────────┐  ┌──────────────────────────────┐  │
│  │ allAnimeIdsList  │  │  alternativeTitleToAid       │  │
│  │ (never filtered) │  │  (for search)                │  │
│  └────────┬─────────┘  └──────────────────────────────┘  │
│           │                                                │
│           ▼                                                │
│  ┌──────────────────────────────────────────────────┐    │
│  │         applyMylistFilters()                     │    │
│  │  1. Start with allAnimeIdsList                   │    │
│  │  2. Apply search via alternativeTitleToAid       │    │
│  │  3. Apply type/completion/unwatched filters      │    │
│  │  4. Expand series chains (add related anime)    │    │
│  │  5. Group and sort chains                        │    │
│  └──────────────────┬───────────────────────────────┘    │
│                     │                                      │
│                     ▼                                      │
│           ┌──────────────────┐                            │
│           │ filteredAnimeIds │                            │
│           └─────────┬────────┘                            │
└─────────────────────┼─────────────────────────────────────┘
                      │
                      ▼
┌────────────────────────────────────────────────────────────┐
│              Data Preloading (Batch SQL)                   │
│  For each aid in filteredAnimeIds:                         │
│  1. Load anime metadata                                    │
│  2. Load all episodes                                      │
│  3. Calculate statistics                                   │
│  4. Store in m_cardCreationDataCache                       │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│           MyListCardManager (Card Factory)                 │
│  ┌─────────────────────────────────────────────────────┐  │
│  │ m_orderedAnimeIds = filteredAnimeIds                │  │
│  │ Notify VirtualFlowLayout of count                   │  │
│  └─────────────────────────────────────────────────────┘  │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│            VirtualFlowLayout (Viewport Manager)            │
│  - Calculate visible range based on scroll position       │
│  - For each visible index:                                 │
│    └─> cardManager->createCardForIndex(index)             │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│               Card Creation (On Demand)                    │
│  ┌──────────────────────────────────────────────────────┐ │
│  │ Check cache: m_cards[aid] exists?                    │ │
│  │   YES: Return existing card (fast)                   │ │
│  │   NO:  Create new card using m_cardCreationDataCache │ │
│  │         - Set all properties from cache              │ │
│  │         - No SQL queries needed                      │ │
│  │         - Store in m_cards[aid]                      │ │
│  └──────────────────────────────────────────────────────┘ │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│                Display (User sees cards)                   │
│  - Cards rendered in viewport                              │
│  - Scroll triggers new card creation for new visible range│
│  - Old cards remain cached for instant reuse              │
└────────────────────────────────────────────────────────────┘
```

---

## Summary

The anime data system follows a **multi-stage pipeline**:

1. **Load**: Background thread loads anime IDs and titles
2. **Filter**: Main thread applies filters to ID list
3. **Preload**: Batch load full data for filtered anime
4. **Display**: Virtual scrolling creates cards on-demand
5. **Cache**: Cards and data cached for instant reuse

**Key Benefits:**
- ✅ Fast filtering (works with IDs only)
- ✅ Efficient display (virtual scrolling)
- ✅ No N+1 queries (batch loading)
- ✅ Smooth scrolling (card reuse)
- ✅ Scales to thousands of anime
- ✅ Thread-safe with minimal locking

**Design Philosophy:** 
- Separate data concerns (IDs vs full data vs UI widgets)
- Batch operations over individual queries
- Cache aggressively, invalidate intelligently
- Create UI widgets lazily, cache permanently
