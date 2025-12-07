# App Freeze Fix Summary

## Issue Description

The application was freezing during startup after loading the mylist data. The last log message before the freeze was:
```
[23:30:02.269] [mylistcardmanager.cpp:484] [MyListCardManager] Rebuilt ordered list: 621 anime in 546 chains
```

## Root Cause Analysis (UPDATED after issue #846)

The freeze was caused by calling `refresh()` from **within** `sortChains()` creating a re-entrancy issue.

### Call Sequence Analysis

**Initial understanding (issue #844):**
The freeze appeared to happen right after sortChains completed, suggesting a redundant refresh call.

**Detailed analysis (issues #845 and #846 with comprehensive logging):**
1. First `refresh()` completes successfully before sorting
2. `window.cpp:4185` - Calls `cardManager->sortChains()`
3. Inside `sortChains()` at `mylistcardmanager.cpp:492` - Calls `m_virtualLayout->refresh()`
4. Inside `refresh()` - Calls `updateVisibleItems()` 
5. Inside `updateVisibleItems()` - Calls `createOrReuseWidget()` for index 0
6. Inside `createOrReuseWidget()` at line 680 - Calls `m_itemFactory(index)` ❌ **FREEZE HERE**

The factory callback (`createCardForIndex`) never returns when called from within the `sortChains()` context, indicating a **re-entrancy issue**.

### Why Re-entrancy Causes the Freeze

When `refresh()` is called from within `sortChains()`:
- The sorting operation holds certain state/locks
- `refresh()` triggers widget operations that may process Qt events
- These events may try to access state that's being modified by sortChains
- This creates a deadlock or infinite wait situation

### The Solution

**Remove the `refresh()` call from inside `sortChains()`** and call it AFTER `sortChains()` returns in `window.cpp`.

This ensures:
- sortChains completes its reordering operation fully
- The refresh happens in a clean context without re-entrancy
- No simultaneous state modification and widget updates

### Changes Made

**Files Modified:**
- `usagi/src/mylistcardmanager.cpp` - Removed `refresh()` call from `sortChains()`
- `usagi/src/window.cpp` - Added `refresh()` call AFTER `sortChains()` returns

**Before (commit 5a1bd57):**
```cpp
// In mylistcardmanager.cpp sortChains():
m_virtualLayout->setItemCount(m_orderedAnimeIds.size());
m_virtualLayout->refresh();  // ❌ Called from within sortChains - causes re-entrancy freeze

// In window.cpp:
cardManager->sortChains(criteria, sortAscending);
animeIds = cardManager->getAnimeIdList();
// No refresh here because we thought sortChains handled it
```

**After (commit b5fd99d):**
```cpp
// In mylistcardmanager.cpp sortChains():
// No refresh call - just complete the sorting operation

// In window.cpp:
cardManager->sortChains(criteria, sortAscending);
animeIds = cardManager->getAnimeIdList();
if (mylistVirtualLayout) {
    mylistVirtualLayout->refresh();  // ✅ Called AFTER sortChains returns - no re-entrancy
}
```

## Debug Logging Journey

The fix was identified through systematic debug logging across multiple iterations:

### Issue #844 (Original Report)
- User running old code without debug logs
- Freeze at line 484: "Rebuilt ordered list: 621 anime in 546 chains"

### Commits b7968ab, 15bd088, 86053dd
- Added comprehensive logging to sortChains(), refresh(), calculateLayout(), updateVisibleItems()
- Replaced qDebug() with LOG() macro for consistency

### Issue #845 (First Test with Debug Logs)
- Confirmed user running updated code
- Freeze identified in widget creation loop at line 600-604

### Commit ab61ec4
- Added detailed per-iteration logging in updateVisibleItems loop
- Added step-by-step logging in createOrReuseWidget (factory, parent, geometry, show, signal)

### Issue #846 (Pinpoint Exact Freeze)
- Logs showed first refresh() completes successfully
- sortChains() called and rebuilds list
- sortChains() calls refresh() internally
- Second refresh() freezes when calling m_itemFactory(0)
- Factory callback never returns - **re-entrancy issue identified**

### Commit b5fd99d (The Fix)
- Removed refresh() from inside sortChains()
- Added refresh() in window.cpp AFTER sortChains() returns
- Eliminates re-entrancy by calling refresh in clean context

## Testing Recommendations

1. **Basic test**: Start the app and verify it loads without freezing
2. **Chain sorting test**: Toggle series chain mode on and verify sorting works
3. **Multiple sorts**: Try changing sort criteria multiple times
4. **Performance test**: Check that the app remains responsive after loading

## Related Files Modified
```

**After:**
```cpp
cardManager->sortChains(criteria, sortAscending);
animeIds = cardManager->getAnimeIdList();

// Note: No need to refresh the virtual layout here because sortChains() already did it
```

## Debug Logging Added

To help identify the exact freeze location and assist with future debugging, comprehensive debug logging was added to:

1. **mylistcardmanager.cpp**: `sortChains()` method
   - Log before/after `setItemCount()`
   - Log before/after `refresh()`
   - Log completion

2. **virtualflowlayout.cpp**: Layout update methods
   - `refresh()`: Entry, each step, completion
   - `calculateLayout()`: Entry, key calculations, completion
   - `updateVisibleItems()`: Entry, visible range, widget count, completion

3. **window.cpp**: Sorting and loading methods
   - Log before/after `sortChains()` call
   - Log before/after `sortMylistCards()` call
   - Log all steps in `onMylistLoadingFinished()`

All logs use the `LOG()` macro (custom logger to file) for consistency across the application.

## Expected Behavior After Fix

The application should:
1. Complete the chain sorting without freezing
2. Display all logs showing successful completion of each step
3. Show the mylist with all anime cards properly sorted and displayed

## Testing Recommendations

1. **Basic test**: Start the app and verify it loads without freezing
2. **Chain sorting test**: Toggle series chain mode on and verify sorting works
3. **Performance test**: Check that the app is responsive after loading
4. **Log verification**: Confirm all debug logs appear in sequence without gaps

## Related Files Modified

- `usagi/src/mylistcardmanager.cpp` - Added debug logging, no logic changes
- `usagi/src/virtualflowlayout.cpp` - Added debug logging using LOG() macro, removed QDebug header
- `usagi/src/window.cpp` - Removed redundant refresh() call, added debug logging

**Note**: All debug logging uses the `LOG()` macro for consistency. The initial implementation used `qDebug()` for low-level layout operations, but this was changed to `LOG()` to ensure all debug output goes to the log file rather than just Qt debug output.

## Technical Notes

### Why setItemCount() Doesn't Prevent the Issue

The `setItemCount()` method checks if the count changed (line 175-177 in virtualflowlayout.cpp):
```cpp
if (m_itemCount == count) {
    return;
}
```

During sorting, the item count (621 anime) remains the same, so `setItemCount(621)` returns early without doing any layout work. This means the `refresh()` call at line 492 is necessary and correct. The issue was the **second** `refresh()` call from window.cpp.

### Virtual Layout Architecture

- **VirtualFlowLayout**: Manages visible widgets and layout calculations
- **MyListCardManager**: Owns the card widgets and provides them via factory callback
- **Window**: Coordinates between card manager and virtual layout

The card manager holds a reference to the virtual layout and can update it directly through `sortChains()`. The window should not call `refresh()` again after operations that already refresh the layout internally.
