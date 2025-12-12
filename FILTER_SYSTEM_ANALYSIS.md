# Filter System Analysis

## Overview
The filtering system in Usagi-dono allows users to filter anime displayed in the MyList tab based on various criteria. All filtering happens in-memory on already-loaded data - no database operations are performed during filtering.

## Filter Types

### 1. Search Filter (Text Search)
**Location:** `MyListFilterSidebar::m_searchField` (QLineEdit)  
**Purpose:** Search anime by title or alternative titles  
**Implementation:** `Window::matchesSearchFilter()`  
**Features:**
- Case-insensitive partial string matching
- Searches main title (from card/cached data)
- Searches alternative titles from `animeAlternativeTitlesCache` (AnimeMetadataCache)
- 300ms debounced input to avoid filtering on every keystroke
- Empty search matches all anime

### 2. Type Filter (Anime Type)
**Location:** `MyListFilterSidebar::m_typeFilter` (QComboBox)  
**Purpose:** Filter by anime type  
**Options:**
- All Types (empty = no filter)
- TV Series
- Movie
- OVA
- TV Special
- Web
- Music Video
- Other

**Implementation:** Direct string comparison with card's type or cached data's typeName

### 3. Completion Filter (Watch Status)
**Location:** `MyListFilterSidebar::m_completionFilter` (QComboBox)  
**Purpose:** Filter by watching completion status  
**Options:**
- All (no filter)
- Completed: All normal episodes viewed
- Watching: Some episodes viewed but not all
- Not Started: No episodes viewed

**Implementation:** Compares `normalViewed` vs `totalEpisodes`:
- Completed: `normalViewed >= totalEpisodes` (or if total unknown: `normalViewed >= normalEpisodes` and `normalEpisodes > 0`)
- Watching: `normalViewed > 0 && normalViewed < totalEpisodes` (or if total unknown: `normalViewed > 0 && normalViewed < normalEpisodes`)
- Not Started: `normalViewed == 0`

### 4. Unwatched Episodes Filter
**Location:** `MyListFilterSidebar::m_showOnlyUnwatchedCheckbox` (QCheckBox)  
**Purpose:** Show only anime that have unwatched episodes  
**Implementation:** Shows anime where:
- `normalEpisodes > normalViewed` OR
- `otherEpisodes > otherViewed`

### 5. In MyList Filter
**Location:** `MyListFilterSidebar::m_inMyListCheckbox` (QCheckBox)  
**Purpose:** Filter to show only anime in user's mylist vs all anime in database  
**Default:** Checked (show only mylist anime)  
**Implementation:** 
- Uses `mylistAnimeIdSet` for quick O(1) lookup
- When unchecked for the first time, loads ALL anime from database
- Triggers preloading of all anime data on first use

### 6. Adult Content Filter (18+ Filter)
**Location:** `MyListFilterSidebar::m_adultContentFilter` (QComboBox)  
**Purpose:** Filter adult/18+ restricted content  
**Options:**
- Ignore: No filtering based on 18+ status
- Hide 18+: Hide adult content (default)
- Show only 18+: Show only adult content

**Implementation:** Uses `card->is18Restricted()` or `cachedData.is18Restricted()`

### 7. Series Chain Display
**Location:** `MyListFilterSidebar::m_showSeriesChainCheckbox` (QCheckBox)  
**Purpose:** Enable/disable series chain mode (show prequels/sequels together)  
**Implementation:** 
- Passed to `cardManager->setAnimeIdList(filteredAnimeIds, showSeriesChain)`
- Builds anime chains and expands them to include related anime
- Handled by `MyListCardManager` and `AnimeChain` classes

## Filter Application Process

### Main Entry Point: `Window::applyMylistFilters()`

**Execution Flow:**
1. **Mutex Lock:** Acquires `filterOperationsMutex` to prevent concurrent filter operations
2. **Get Base Data:** Retrieves full unfiltered list from `allAnimeIdsList`
3. **Handle MyList State Change:** If "In MyList" filter toggled:
   - First uncheck loads ALL anime from database via query
   - Preloads card creation data for all anime
   - Updates `allAnimeIdsList` with full anime list
4. **Get Filter Values:** Retrieves all filter settings from sidebar
5. **Apply Filters Sequentially:** For each anime in `allAnimeIds`:
   - Skip if not in mylist (when `inMyListOnly` is true) - **EARLY EXIT**
   - Apply search filter
   - Apply type filter
   - Apply completion filter
   - Apply unwatched filter
   - Apply adult content filter
   - Add to `filteredAnimeIds` if all filters pass
6. **Build Chains:** Call `cardManager->setAnimeIdList(filteredAnimeIds, showSeriesChain)`
   - If series chain enabled, builds and expands chains
   - Returns final display list (may include additional anime from chain expansion)
7. **Final Validation:** Checks all anime in display list have cached data, preloads if missing
8. **Update UI:** 
   - Updates chain connections via `cardManager->updateSeriesChainConnections()`
   - Refreshes virtual layout
   - Updates status label

**Thread Safety:** Uses `filterOperationsMutex` to prevent race conditions

### Filter Triggers

Filters are applied when:
1. User changes any filter setting → `MyListFilterSidebar::filterChanged` signal
2. User changes sort settings → `MyListFilterSidebar::sortChanged` signal (triggers re-sort after filter)
3. MyList data loads → `Window::onMylistLoadingFinished()`

## Sorting (Post-Filter)

After filtering, `Window::sortMylistCards()` is called to sort the filtered results.

**Sort Options:**
- Anime Title
- Type
- Aired Date (default)
- Episodes (Count)
- Completion %
- Last Played

**Sort Direction:** Ascending/Descending toggle

**Special Handling:**
- When series chain enabled, sorts chains by representative (first) anime
- Last Played sorting places unplayed items at end regardless of direction

## Data Sources for Filtering

### With Virtual Scrolling (Current Implementation)
- **Cached Data:** `MyListCardManager::getCachedAnimeData()` provides pre-loaded metadata
- **Card Widgets:** Only created on-demand for visible items
- **Fallback:** If card exists, uses card data; otherwise uses cached data

### Cache Architecture
- **AnimeMetadataCache:** Stores alternative titles for search filtering
- **MyListCardManager Cache:** Stores all anime metadata (type, episodes, completion, etc.)
- **Preloading:** Data preloaded when anime list changes to support virtual scrolling

## Code Structure Analysis

### Strengths
1. **Clear Separation:** Filters defined in `MyListFilterSidebar`, logic in `Window::applyMylistFilters()`
2. **Debouncing:** Search input properly debounced (300ms) to avoid excessive filtering
3. **Virtual Scrolling Support:** Filters work with cached data, no need to create all cards
4. **Thread Safety:** Mutex prevents concurrent filter operations
5. **Early Exit Optimization:** "In MyList" filter checked first for quick rejection

### Areas for Improvement

#### 1. Filter Logic Duplication
**Issue:** Filter application has repeated card/cache data retrieval pattern:
```cpp
// Repeated pattern for each filter:
if (card) {
    value = card->getSomeValue();
} else {
    value = cachedData.someValue();
}
```

**Improvement Opportunity:** Create a unified data accessor that abstracts card vs cached data

#### 2. Filter Logic Not Encapsulated
**Issue:** All filter logic is in one large function (`applyMylistFilters`)  
**Improvement Opportunity:** Extract each filter into separate methods or filter objects

#### 3. Filter Combination is Sequential, Not Composable
**Issue:** Filters are applied with sequential `&&` operations  
**Improvement Opportunity:** Use filter objects that can be composed and tested independently

#### 4. No Filter Unit Tests
**Issue:** No comprehensive tests for filter combinations  
**Improvement Opportunity:** Add tests for all filter types and combinations

#### 5. Complex Completion Filter Logic
**Issue:** Completion filter has complex conditional logic for unknown episode counts  
**Improvement Opportunity:** Extract to helper method with clear documentation

## Recommendations

### 1. Create Filter Classes (SOLID Principles)
```cpp
class AnimeFilter {
public:
    virtual bool matches(const AnimeData& data) const = 0;
};

class SearchFilter : public AnimeFilter {
    QString searchText;
    const AnimeMetadataCache* cache;
public:
    bool matches(const AnimeData& data) const override;
};

// Composite filter
class CompositeFilter : public AnimeFilter {
    QList<AnimeFilter*> filters;
public:
    bool matches(const AnimeData& data) const override {
        for (auto* filter : filters) {
            if (!filter->matches(data)) return false;
        }
        return true;
    }
};
```

### 2. Create Unified Data Accessor
```cpp
class AnimeDataAccessor {
    AnimeCard* card;
    MyListCardManager::CachedAnimeData cachedData;
public:
    QString getTitle() const { return card ? card->getAnimeTitle() : cachedData.animeName(); }
    QString getType() const { return card ? card->getAnimeType() : cachedData.typeName(); }
    // ... other accessors
};
```

### 3. Add Comprehensive Tests
- Test each filter type individually
- Test all filter combinations
- Test edge cases (empty lists, missing data, etc.)
- Test filter application order doesn't matter (except "In MyList")

### 4. Simplify Completion Filter
```cpp
enum class CompletionStatus {
    NotStarted,
    Watching,
    Completed
};

CompletionStatus getCompletionStatus(int normalViewed, int normalEpisodes, int totalEpisodes) {
    if (normalViewed == 0) return CompletionStatus::NotStarted;
    
    int total = (totalEpisodes > 0) ? totalEpisodes : normalEpisodes;
    if (normalViewed >= total && total > 0) return CompletionStatus::Completed;
    
    return CompletionStatus::Watching;
}
```

## Current Test Coverage

**Existing:** `test_card_filtering.cpp`
- Tests `AnimeMetadataCache` for search filtering
- Tests alternative title matching
- Tests case-insensitive search
- Tests partial matching
- Tests missing anime detection

**Missing:**
- No tests for type filter
- No tests for completion filter
- No tests for unwatched filter
- No tests for adult content filter
- No tests for filter combinations
- No tests for filter application order

## Conclusion

The current filter system is **functional and well-structured** with:
- ✅ All major filter types implemented
- ✅ Proper thread safety
- ✅ Good performance optimization (early exit, caching)
- ✅ Virtual scrolling support

However, there are opportunities for improvement:
- ⚠️ Filter logic could be more modular (SOLID principles)
- ⚠️ Code duplication in data access pattern
- ⚠️ Limited test coverage
- ⚠️ Complex completion filter logic

The filters **are applied all at once** (not spread across multiple locations) in `Window::applyMylistFilters()`, which is good for maintainability. The code is not overly complicated, but could benefit from better encapsulation for testing and reusability.
