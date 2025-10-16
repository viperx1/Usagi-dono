# MyList Display Fix - Episode Numbers vs Episode IDs

## Problem
The mylist feature was displaying episode IDs (internal database IDs like 345678) instead of human-readable episode numbers (like 1, 2, 3). This made the mylist UI confusing as users would see "Episode 345678" instead of "Episode 1".

## Root Cause
The `loadMylistFromDatabase()` function in `window.cpp` was directly using the `eid` field from the database as if it were the episode number. However, `eid` is an AniDB internal episode ID, not the episode number that users expect to see.

```cpp
// OLD CODE (incorrect)
QString episodeDisplay = QString("Episode %1").arg(eid); // eid = 345678
```

## Solution
Since the episode number is not stored in the database tables, the fix extracts it from the filename where it's commonly present in various formats.

### Implementation Details

1. **Added file table join**: Modified the SQL query to include the filename from the file table
2. **Regex-based extraction**: Implemented pattern matching to extract episode numbers from filenames
3. **Smart fallback**: If extraction fails, falls back to episode name or clearly labels it as "EID"

### Supported Filename Patterns
The regex supports all common anime filename patterns:
- `E01`, `e01` - Standard episode notation
- `S01E07` - TV show format
- `- 01` - Dash-separated
- `_01_` - Underscore-separated  
- `_e12_` - Lowercase e with underscores
- `[01]` - Bracket notation
- Episode numbers 1-999+

### Display Logic
```
IF episode number found in filename:
    Display: "Episode N" or "Episode N - Title"
ELSE IF episode name exists:
    Display: episode name as-is
ELSE:
    Display: "EID: N" (clearly marking it as an ID)
```

## Files Changed
- `usagi/src/window.cpp` - Updated `loadMylistFromDatabase()` function

## Testing
Tested with 9 different filename patterns:
- ✅ All standard patterns extract correctly
- ✅ Edge cases handled properly
- ✅ Code compiles without errors
- ✅ Existing tests still pass

## Before/After Examples

### Before Fix
```
Anime Name
  └─ Episode 345678     [confusing - looks like episode 345678]
  └─ Episode 345679
  └─ Episode 345680
```

### After Fix
```
Anime Name
  └─ Episode 1          [clear and correct]
  └─ Episode 2
  └─ Episode 3
```

Or if episode name is available:
```
Anime Name
  └─ Episode 1 - Pilot
  └─ Episode 2 - Rising Action
```

## Future Improvements (Optional)
If desired, could add episode number storage by:
1. Adding `epno` column to episode table
2. Parsing episode number from FILE response (amask includes aEPISODE_NUMBER)
3. Storing during FILE/EPISODE response handling

However, the current solution is sufficient and requires no database schema changes.
