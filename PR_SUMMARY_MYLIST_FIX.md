# PR Summary: Fix Mylist Database Bug

## Issue
The mylist database table had duplicate values in the `lid` (mylist ID) and `fid` (file ID) columns. When examining the database in an external editor, both columns contained the same value, which is incorrect as they should represent different IDs.

## Root Cause
The 221 MYLIST response handler in `usagi/src/anidbapi.cpp` incorrectly assumed the AniDB API response format included the mylist ID (lid) as the first field. 

**Incorrect assumption:**
```
lid|fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
```

**Actual AniDB API format:**
```
fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
```

When querying MYLIST by lid, the response does NOT include the lid because it's already known from the query parameter. The handler was mapping the first response field (fid) to the lid database column, causing both lid and fid to appear to have the "same" value (because lid was storing the fid value).

## Changes Made

### 1. Fixed 221 MYLIST Response Handler (`usagi/src/anidbapi.cpp`, lines 462-520)
- Extract the lid from the original MYLIST command stored in the packets table
- Correctly parse the response starting with fid at index 0
- Added success/error debug logging
- Updated comment to reflect correct API response format

**Key changes:**
- Before: `lid = token2.at(0)` (WRONG - this is actually fid)
- After: Extract lid from packets table, then `fid = token2.at(0)` (CORRECT)

### 2. Fixed 220 FILE Response Handler (`usagi/src/anidbapi.cpp`, line 383)
Discovered and fixed a separate bug where the filename field was using the wrong index:
- Before: Used `token2.at(26)` twice for both airdate and filename
- After: Uses `token2.at(26)` for airdate and `token2.at(27)` for filename

### 3. Added Test (`tests/test_mylist_221_fix.cpp`)
Created comprehensive test demonstrating:
- Correct parsing of 221 MYLIST response
- Verification that lid and fid have different values
- Correct lid extraction from MYLIST command

### 4. Added Documentation
- **MYLIST_LID_FID_BUG_FIX.md**: Detailed technical documentation
- **MYLIST_BUG_FIX_VISUAL.md**: Visual examples showing before/after

## Example

### Before Fix ❌
```
Command: MYLIST lid=123456
Response: 789012|297776|18795|...

Database Result:
  lid    fid     eid
  ──────────────────
  789012 297776  18795  ← ALL WRONG!
```

### After Fix ✅
```
Command: MYLIST lid=123456
Response: 789012|297776|18795|...

Database Result:
  lid     fid     eid
  ──────────────────
  123456  789012  297776  ← ALL CORRECT!
```

## Files Modified
- `usagi/src/anidbapi.cpp`: Fixed 221 MYLIST and 220 FILE response handlers
- `tests/test_mylist_221_fix.cpp`: Added test (NEW)
- `MYLIST_LID_FID_BUG_FIX.md`: Technical documentation (NEW)
- `MYLIST_BUG_FIX_VISUAL.md`: Visual documentation (NEW)

## Testing
To run the new test:
```bash
cmake --build build
./build/tests/test_mylist_221_fix
```

## Impact on Users

### Positive Impact
- All new MYLIST queries will be parsed correctly
- No more duplicate/incorrect lid and fid values
- Database integrity is maintained

### Required Action
Users with existing databases should:
1. Clear the mylist table: `DELETE FROM mylist;`
2. Re-import mylist data using MYLISTEXPORT feature
3. Or query each entry again with MYLIST command

The old data from previous 221 MYLIST responses is incorrect due to the field mapping bug.

## Implementation Pattern
This fix follows the same pattern used by other response handlers in the codebase:
- **210 MYLIST ENTRY ADDED** (lines 234-308)
- **311 MYLIST ENTRY EDITED** (lines 558-630)

Both of these already correctly extract information from the original command stored in the packets table when the response doesn't include all parameters.

## Code Quality
- Minimal, surgical changes to fix the specific issue
- Added appropriate error handling and debug logging
- Follows existing code patterns and conventions
- Added test coverage
- Comprehensive documentation

## Related Issues
This fix resolves the duplicate entry issue mentioned in the issue where "file id" and "mylist id" were mixed up, causing incorrect database entries.
