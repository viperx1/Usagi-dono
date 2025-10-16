# MyList Field Update - Testing Guide

## Overview
This guide provides instructions for testing the targeted field update implementation for mylist episode data.

## What Changed
**Before**: When episode data arrived from API, entire mylist tree was cleared and rebuilt  
**After**: Only the specific episode fields are updated, preserving tree state

## Prerequisites
- Usagi application compiled with Qt6
- AniDB account credentials configured
- Mylist CSV-adborg export imported (or some mylist entries without episode data)

## Manual Test Plan

### Test 1: Basic Field Update
**Objective**: Verify single episode field updates without full refresh

1. Launch Usagi application
2. Go to MyList tab
3. Identify an anime with episodes showing "Loading..." for number or name
4. **Expand the anime item** (this triggers API requests)
5. Wait for API responses (check Log tab)
6. **Expected Results**:
   - Episode fields update from "Loading..." to actual values
   - Tree remains expanded (not collapsed)
   - No visual flicker
   - Log shows: "Episode data received for EID X (AID Y), updating field..."
   - Log shows: "Updated episode in tree: EID X, epno: N, name: Title"

### Test 2: Multiple Episodes
**Objective**: Verify multiple episode updates in same anime

1. Find anime with 3+ episodes showing "Loading..."
2. **Expand the anime**
3. Wait for all API responses
4. **Expected Results**:
   - All episodes update individually as data arrives
   - Tree stays expanded throughout
   - Each update logged separately
   - No full tree refresh occurs

### Test 3: Scroll Position Preservation
**Objective**: Verify scroll position is maintained during updates

1. Scroll to middle/bottom of mylist
2. Expand an anime at current scroll position
3. Wait for episode data updates
4. **Expected Results**:
   - Scroll position unchanged
   - No jump to top or bottom
   - Updated episodes visible without scrolling

### Test 4: Selection Preservation
**Objective**: Verify selected items remain selected

1. Select an episode item
2. Expand another anime to trigger updates
3. Wait for updates to complete
4. **Expected Results**:
   - Original selection still highlighted
   - No selection change

### Test 5: Mixed Data States
**Objective**: Verify handling of episodes with partial data

1. Find anime with mix of:
   - Complete data (number + name)
   - Missing number only
   - Missing name only
   - Missing both
2. Expand anime
3. **Expected Results**:
   - Only missing fields get updated
   - Existing data remains unchanged
   - Log shows what was updated

### Test 6: Error Handling
**Objective**: Verify graceful handling of errors

1. Trigger update for non-existent EID (may need to manually test)
2. **Expected Results**:
   - Log shows: "No episode data found for EID X"
   - No crash or error dialog
   - Other episodes still update normally

### Test 7: Performance
**Objective**: Verify improved performance with large mylist

1. Load mylist with 100+ entries
2. Expand multiple anime simultaneously (if possible)
3. Observe update speed and UI responsiveness
4. **Expected Results**:
   - Fast, responsive updates (1-5ms per episode)
   - No UI freeze or lag
   - No visual flicker
   - Tree remains stable

## Log Messages to Verify

### Expected Messages (New Behavior)
```
Requesting episode data for EID 12345 (AID 67890)
Episode data received for EID 12345 (AID 67890), updating field...
Updated episode in tree: EID 12345, epno: 1, name: Episode Title
```

### Old Messages (Should NOT See)
```
Episode data received for EID 12345 (AID 67890), refreshing display...
Loaded 42 mylist entries for 12 anime
```

If you see "refreshing display..." and "Loaded X mylist entries", the old code is still running.

## Visual Indicators

### Good Signs
- ✅ Episode fields change smoothly from "Loading..." to actual values
- ✅ Tree remains expanded
- ✅ Scroll position unchanged
- ✅ No screen flicker
- ✅ Fast, responsive updates

### Bad Signs (Indicates Old Behavior)
- ❌ Tree collapses after update
- ❌ Screen flickers/redraws
- ❌ Scroll jumps to top
- ❌ Noticeable delay (>100ms)
- ❌ Selection lost

## Edge Cases to Test

### Case 1: Rapid Updates
Expand anime with 10+ episodes quickly, verify all update correctly

### Case 2: Anime Not in Tree
If API returns data for episode not currently visible, verify no crash

### Case 3: Empty Episode Data
If API returns empty strings, verify "Unknown" is displayed

### Case 4: Special Episode Types
Verify correct formatting:
- Regular: "1", "2", "3"
- Special: "Special 1" (from "S1")
- Credit: "Credit 1" (from "C1")
- Trailer: "Trailer 1" (from "T1")
- Parody: "Parody 1" (from "P1")
- Other: "Other 1" (from "O1")

## Performance Benchmarks

Expected performance (approximate):

| List Size | Old Refresh Time | New Update Time | Improvement |
|-----------|------------------|-----------------|-------------|
| 10 entries | ~30ms | ~1ms | 30x faster |
| 50 entries | ~80ms | ~2ms | 40x faster |
| 100 entries | ~150ms | ~3ms | 50x faster |
| 500 entries | ~800ms | ~5ms | 160x faster |

## Regression Testing

Ensure existing functionality still works:

1. **Import mylist**: CSV-adborg import should work
2. **Full refresh**: Manual refresh should work if implemented
3. **Add to mylist**: Hash & add files should work
4. **Database**: All database queries should work
5. **Other tabs**: Hasher, Settings, Log tabs should work

## Bug Reports

If you find issues, please report with:

1. **Description**: What went wrong
2. **Steps**: How to reproduce
3. **Logs**: Relevant log messages
4. **Expected**: What should have happened
5. **Actual**: What actually happened
6. **Environment**: Qt version, OS, database state

## Success Criteria

All tests pass if:
- ✅ Episode fields update without full refresh
- ✅ Tree structure and state preserved
- ✅ No visual flicker or lag
- ✅ Log messages show targeted updates
- ✅ Performance improved significantly
- ✅ No crashes or errors
- ✅ All edge cases handled gracefully

## Notes

- Testing requires Qt6 environment (not available in CI)
- Best tested with real AniDB account and mylist data
- Consider testing with different network speeds (API delay)
- Test with both small and large mylists
- Verify behavior with and without expanded anime items

## Related Documentation

- [MYLIST_FIELD_UPDATE_IMPLEMENTATION.md](MYLIST_FIELD_UPDATE_IMPLEMENTATION.md) - Implementation details
- [MYLIST_FIELD_UPDATE_SUMMARY.md](MYLIST_FIELD_UPDATE_SUMMARY.md) - Quick summary
- [MYLIST_LAZY_LOADING_BEFORE_AFTER.md](MYLIST_LAZY_LOADING_BEFORE_AFTER.md) - Before/after comparison
