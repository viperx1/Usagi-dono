# Episode Format Change Implementation

## Overview

This document describes the implementation of the new episode format "A/B+C" for the mylist anime row display.

## Issue Reference

Issue: "mylist" - Change the format in "episode" column in "anime" row

## Changes Made

### New Format Specification

**Episode Column Format: "A/B+C"**
- A = amount of "normal" episodes (type 1) saved in mylist
- B = total amount of "normal" episodes (from anime.eptotal)
- C = sum of all other types (types 2-6: special, credit, trailer, parody, other) in mylist

**Viewed Column Format: "A/B+C"**
- A = normal episodes (type 1) viewed
- B = total normal episodes (type 1) in mylist (not from eptotal, but from your actual mylist)
- C = other type episodes (types 2-6) viewed

### Code Changes

Modified `Window::loadMylistFromDatabase()` in `/usagi/src/window.cpp`:

1. **Added new tracking maps** (lines 1109-1112):
   ```cpp
   QMap<int, int> animeNormalEpisodeCount;    // aid -> count of normal episodes (type 1) in mylist
   QMap<int, int> animeOtherEpisodeCount;     // aid -> count of other type episodes (types 2-6) in mylist
   QMap<int, int> animeNormalViewedCount;     // aid -> count of normal episodes viewed
   QMap<int, int> animeOtherViewedCount;      // aid -> count of other type episodes viewed
   ```

2. **Extract episode type from epno** (lines 1177-1197):
   - Parse the epno string to get the episode type
   - Default to type 1 (normal) if epno is not available

3. **Track statistics by episode type** (lines 1199-1223):
   - Increment normal or other episode counters based on episode type
   - Track viewed episodes separately for normal and other types

4. **Format anime row display** (lines 1270-1309):
   - Episode column: Shows "A/B+C" or "A/B" if no other types exist
   - Viewed column: Shows "A/B+C" or "A/B" if no other types exist

### Episode Type Reference

From `epno.h` and `epno.cpp`:

```cpp
1 = regular episode (no prefix)
2 = special ("S" prefix)
3 = credit ("C" prefix)
4 = trailer ("T" prefix)
5 = parody ("P" prefix)
6 = other ("O" prefix)
```

## Test Scenarios

### Scenario 1: Anime with only normal episodes
**Input:**
- 5 normal episodes in mylist (types: 1,1,1,1,1)
- Total episodes: 12
- 3 episodes viewed

**Expected Output:**
- Episode column: "5/12"
- Viewed column: "3/5"

### Scenario 2: Anime with normal and special episodes
**Input:**
- 10 normal episodes in mylist (type 1)
- 2 special episodes in mylist (type 2)
- Total episodes: 12
- 8 normal episodes viewed
- 1 special episode viewed

**Expected Output:**
- Episode column: "10/12+2"
- Viewed column: "8/10+1"

### Scenario 3: Anime with mixed episode types
**Input:**
- 24 normal episodes in mylist (type 1)
- 3 special episodes (type 2)
- 2 credit episodes (type 3)
- 1 trailer (type 4)
- Total episodes: 24
- 20 normal episodes viewed
- 2 special episodes viewed
- 1 credit viewed
- 0 trailers viewed

**Expected Output:**
- Episode column: "24/24+6" (3+2+1 = 6 other types)
- Viewed column: "20/24+3" (2+1+0 = 3 other types viewed)

### Scenario 4: Anime without eptotal data
**Input:**
- 8 normal episodes in mylist
- 1 special episode
- eptotal: 0 (not available)
- 5 normal viewed
- 1 special viewed

**Expected Output:**
- Episode column: "8+1"
- Viewed column: "5/8+1"

### Scenario 5: All episodes viewed
**Input:**
- 12 normal episodes in mylist
- 2 special episodes
- Total episodes: 12
- All episodes viewed

**Expected Output:**
- Episode column: "12/12+2"
- Viewed column: "12/12+2"

## Implementation Notes

1. **Type Detection**: Episode type is determined by parsing the epno string using the `epno` class's `type()` method.

2. **Default Behavior**: If epno data is not available when initially loading the mylist, the episode defaults to type 1 (normal). This is corrected when episode data is loaded via the EPISODE API.

3. **Backward Compatibility**: The format gracefully degrades to simpler formats when:
   - No other episode types exist: Shows "A/B" instead of "A/B+0"
   - No eptotal available: Shows "A+C" or just "A" if no other types

4. **Display Only Change**: This change only affects the display format in the anime parent rows. Individual episode child rows remain unchanged.

## Files Modified

- `/usagi/src/window.cpp`: Updated `Window::loadMylistFromDatabase()` function

## Testing

To test this implementation:

1. Build the application with CMake
2. Log in to AniDB
3. Load the mylist
4. Verify that anime rows show the new format
5. Test with anime that have:
   - Only normal episodes
   - Mixed episode types (normal + specials/credits/etc.)
   - No eptotal data
   - Various viewing states

## Future Enhancements

Potential improvements for future versions:

1. **Dynamic Updates**: Update anime row statistics when episode data is lazy-loaded via EPISODE API
2. **Tooltip Details**: Add tooltips showing breakdown of episode types (e.g., "2 specials, 1 credit")
3. **Sorting**: Consider how the new format affects column sorting
4. **Export**: Ensure mylist export features handle the display format correctly

## Known Edge Cases

1. **Initial Load Without Episode Data**: If epno data is missing on initial load, episodes default to type 1. The display corrects itself after EPISODE API data arrives or on next mylist reload.

2. **Empty Mylist**: If an anime has no episodes in mylist (shouldn't normally happen), counts show as "0/X" or "0".
