# Anime Card Tweaks Implementation

This document describes the implementation of the 7 anime card tweaks requested in the issue.

## Features Implemented

### 1. Hide/Unhide Option
**Status:** ✅ Implemented

**Changes:**
- Added "Hide"/"Unhide" option to card context menu (right-click)
- Hidden state persists to database via new `hidden` column in `anime` table
- Hidden cards display as compact title-only view (600x40px) with grayed styling
- All card elements (poster, stats, episodes) are hidden for hidden cards
- `hideCardRequested(int aid)` signal emitted to MyListCardManager
- MyListCardManager handles database updates via `onHideCardRequested()` slot

**Files Modified:**
- `animecard.h`: Added `m_isHidden`, `setHidden()`, `isHidden()`, `hideCardRequested` signal
- `animecard.cpp`: Context menu, visual state changes, compact display logic
- `mylistcardmanager.h`: Added `onHideCardRequested()` slot
- `mylistcardmanager.cpp`: Database persistence, hidden state loading in `createCard()`

**Known Limitation:**
- Hidden cards are not automatically sorted to bottom of list
- Would require window.cpp modifications to handle `cardNeedsSorting` signal
- Deferred to avoid over-modification per instructions

### 2. Auto-Fetch Missing Data
**Status:** ✅ Implemented

**Changes:**
- Card click now checks if warning indicator is visible
- If visible, automatically emits `fetchDataRequested(int aid)` signal
- No need for separate context menu action (but kept for explicit fetching)

**Files Modified:**
- `animecard.cpp`: `mousePressEvent()` now checks `m_warningLabel->isVisible()`

### 3. Mark Episode as Watched
**Status:** ✅ Implemented

**Changes:**
- Added custom context menu to episode tree widget items
- Right-click on episode (parent item, not file) shows "Mark as watched" option
- Updates all files for the episode to `viewed=1` in database
- Emits `markEpisodeWatchedRequested(int eid)` signal
- MyListCardManager updates card to reflect changes

**Files Modified:**
- `animecard.h`: Added `markEpisodeWatchedRequested` signal
- `animecard.cpp`: Connected `customContextMenuRequested` signal in `setupUI()`
- `mylistcardmanager.h`: Added `onMarkEpisodeWatchedRequested()` slot
- `mylistcardmanager.cpp`: Database update and card refresh logic

### 4. Poster Aspect Ratio
**Status:** ✅ Verified and Fixed

**Changes:**
- Changed `setScaledContents(true)` to `setScaledContents(false)`
- Manual scaling using `QPixmap::scaled()` with `Qt::KeepAspectRatio`
- Added `Qt::SmoothTransformation` for better quality
- Poster label set to center alignment to handle aspect ratio properly

**Files Modified:**
- `animecard.cpp`: `setupUI()` and `setPoster()` methods

### 5. Full-Size Poster Overlay
**Status:** ✅ Implemented

**Changes:**
- Click on poster shows full-size poster overlay
- Overlay positioned over original poster location
- Dark semi-transparent background (rgba(0, 0, 0, 200))
- White border for emphasis
- Automatically hides when mouse leaves card
- Original poster stored in `m_originalPoster` for overlay use

**Files Modified:**
- `animecard.h`: Added `m_posterOverlay`, `m_originalPoster`, `showPosterOverlay()`, `hidePosterOverlay()`, `enterEvent()`, `leaveEvent()`
- `animecard.cpp`: Implemented overlay creation and management, mouse event handling

### 6. Stop Session Button
**Status:** ✅ Already Implemented

**Note:**
- "Reset Session" button already exists in card UI
- Connected to `resetWatchSessionRequested(int aid)` signal
- Clears local watch status for all episodes

**No changes needed.**

### 7. Visual Indicator for Unwatched Episodes
**Status:** ✅ Implemented

**Changes:**
- Cards with unwatched episodes show light green background (#e8f5e9)
- `updateCardBackgroundForUnwatchedEpisodes()` checks episode tree for unwatched files
- Called automatically when episodes are added via `addEpisode()`
- Checks for play button symbol (▶) in file items to determine unwatched status

**Files Modified:**
- `animecard.h`: Added `updateCardBackgroundForUnwatchedEpisodes()` method
- `animecard.cpp`: Implemented background color logic, called from `addEpisode()`

## Technical Details

### Database Changes
- Added `hidden` column to `anime` table (INTEGER, default 0)
- Column auto-created with `ALTER TABLE anime ADD COLUMN hidden INTEGER DEFAULT 0`
- Safe to run multiple times (SQLite ignores if column exists)

### Signal Flow
```
AnimeCard User Actions:
├─ Right-click card → hideCardRequested(aid) → MyListCardManager::onHideCardRequested()
├─ Right-click episode → markEpisodeWatchedRequested(eid) → MyListCardManager::onMarkEpisodeWatchedRequested()
├─ Click card with warning → fetchDataRequested(aid) → MyListCardManager::onFetchDataRequested()
└─ Click poster → showPosterOverlay() (internal, no signal)

MyListCardManager Actions:
├─ onHideCardRequested() → Update DB → cardNeedsSorting(aid)
└─ onMarkEpisodeWatchedRequested() → Update DB → updateCardAnimeInfo(aid)
```

### Memory Management
- All dynamically allocated widgets use Qt parent-child ownership
- `m_posterOverlay` created with `this` as parent → auto-deleted with card
- No manual cleanup needed in destructor

### Qt6 Compatibility
- Uses `QEnterEvent` (Qt6 API) instead of `QEvent` for `enterEvent()`
- All code compatible with Qt6.9.2 as specified in project

## Testing Recommendations

### Manual Testing
1. **Hide/Unhide:**
   - Right-click card → "Hide"
   - Verify card shows only title and is compact
   - Right-click hidden card → "Unhide"
   - Verify card returns to normal size with all elements

2. **Auto-Fetch:**
   - Find card with warning indicator (⚠)
   - Click anywhere on card (not on tree)
   - Verify fetch request is triggered (check logs)

3. **Mark Watched:**
   - Right-click on episode item (not file) in tree
   - Select "Mark as watched"
   - Verify all files update to viewed status
   - Verify card background changes if last unwatched episode

4. **Poster Overlay:**
   - Click on a card's poster image
   - Verify full-size overlay appears
   - Move mouse out of card area
   - Verify overlay disappears

5. **Unwatched Indicator:**
   - Find card with unwatched episodes
   - Verify light green background
   - Mark all episodes as watched
   - Verify background returns to normal

### Build Testing
```bash
mkdir build && cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
cmake --build .
# Run clazy on modified files (per repository instructions)
```

## Future Enhancements (Out of Scope)

1. **Automatic Sorting:**
   - Implement window.cpp handler for `cardNeedsSorting` signal
   - Re-order cards in FlowLayout to move hidden cards to bottom
   - May require sorting algorithm in MyListCardManager

2. **Hidden Card Filtering:**
   - Add toggle to show/hide hidden cards completely
   - Would require filter logic in MyListCardManager or window

3. **Batch Operations:**
   - Multi-select cards for batch hide/unhide
   - Would require selection tracking in window

## Code Quality Notes

- All changes follow existing code style and patterns
- Minimal modifications to existing functionality
- Added comprehensive logging for debugging
- Thread-safe operations using QMutexLocker where needed
- Proper Qt signal/slot connections with lambda captures
