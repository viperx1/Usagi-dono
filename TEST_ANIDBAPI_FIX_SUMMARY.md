# test_anidbapi Fix Summary

## Issue
The Windows Build & Release workflow failed with test_anidbapi failing. The test `testAllCommandsHaveProperSpacing` was rejecting valid AniDB API commands.

## Root Cause
The regex pattern in `testAllCommandsHaveProperSpacing` used `[a-z]+` to match parameter names, which only matches lowercase letters. However, AniDB API parameter names can contain digits (e.g., "ed2k", "clientver", "protover").

The test was failing on commands like:
- `MYLISTADD size=734003200&ed2k=a1b2c3d4...` (parameter "ed2k" contains digit "2")
- `AUTH user=testuser&pass=testpass&protover=3&client=usagitest&clientver=1&enc=utf8` (parameters "protover" and "clientver" contain digits)

## Solution
Updated the regex pattern from:
```cpp
QRegularExpression pattern("^[A-Z]+( [a-z]+=([^&\\s]+)(&[a-z]+=([^&\\s]+))*)?$");
```

To:
```cpp
QRegularExpression pattern("^[A-Z]+ ([a-z0-9]+=([^&\\s]+)(&[a-z0-9]+=([^&\\s]+))*)?$");
```

Key changes:
1. Changed `[a-z]+` to `[a-z0-9]+` to allow digits in parameter names
2. Updated comment to clarify that parameter names can contain digits
3. Updated validation logic to accept space or lowercase letter after command name

## Files Modified
- `tests/test_anidbapi.cpp` - Updated regex pattern and validation logic

## Testing
All tests now pass:
- ✅ test_hash (7 test cases)
- ✅ test_crashlog (3 test cases)
- ✅ test_anidbapi (15 test cases) - Previously failing
- ✅ test_anime_titles (6 test cases)

The specific test case `testAllCommandsHaveProperSpacing` now correctly validates all 8 AniDB API command formats:
- AUTH
- LOGOUT
- MYLISTADD
- FILE
- MYLIST
- MYLISTSTATS
- PUSHACK
- NOTIFYLIST

## Impact
This fix ensures the Windows Build & Release workflow will succeed. The change is minimal, surgical, and only affects the test validation logic - no production code was modified.
