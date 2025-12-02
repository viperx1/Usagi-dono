# File Version Extraction Fix

## Issue
The `getHigherVersionFileCount()` function and related file version extraction code incorrectly interpreted the AniDB API state field bit structure, leading to incorrect file version detection.

## Root Cause
The original implementation used `state & 0x07` to extract file version, which incorrectly assumed version was stored in bits 0-2. According to the [AniDB UDP API Definition](https://wiki.anidb.net/UDP_API_Definition), the state field uses a different bit encoding.

## AniDB API State Field Specification

### Bit Structure
```
Bit / Value   Meaning
-----------   -------
0 / 1         FILE_CRCOK: file matched official CRC
1 / 2         FILE_CRCERR: file DID NOT match official CRC
2 / 4         FILE_ISV2: file is version 2
3 / 8         FILE_ISV3: file is version 3
4 / 16        FILE_ISV4: file is version 4
5 / 32        FILE_ISV5: file is version 5
6 / 64        FILE_UNC: file is uncensored
7 / 128       FILE_CEN: file is censored
```

### Examples from API Documentation
- `state = 0` → version 1 (not CRC checked)
- `state = 1` → version 1 (CRC OK)
- `state = 8` → version 3 (not CRC checked)
- `state = 9` → version 3 (CRC OK + ISV3)
- `state = 34` → version 5 (CRC ERR + ISV5)

## Fix Implementation

### Version Extraction Logic
The fix implements proper priority-based version detection:

```cpp
int version = 1;  // Default to version 1
if (state & 32) {      // Bit 5: FILE_ISV5
    version = 5;
} else if (state & 16) { // Bit 4: FILE_ISV4
    version = 4;
} else if (state & 8) {  // Bit 3: FILE_ISV3
    version = 3;
} else if (state & 4) {  // Bit 2: FILE_ISV2
    version = 2;
}
// else version = 1 (no version bits set)
```

### SQL Query Update
Updated the SQL queries in `getHigherVersionFileCount()` to use a CASE statement:

```sql
CASE 
  WHEN (f.state & 32) THEN 5  -- Bit 5: v5
  WHEN (f.state & 16) THEN 4  -- Bit 4: v4
  WHEN (f.state & 8) THEN 3   -- Bit 3: v3
  WHEN (f.state & 4) THEN 2   -- Bit 2: v2
  ELSE 1                       -- No version bits: v1
END
```

## Changes Made

### Modified Files
1. **usagi/src/watchsessionmanager.cpp**
   - Fixed `getFileVersion()` with correct bit extraction
   - Updated `getHigherVersionFileCount()` with CASE-based SQL query
   - Added `getFileCountForEpisode()` debug logging
   - All functions now include comprehensive debug logging

2. **usagi/src/watchsessionmanager.h**
   - Updated comment to reflect correct bit structure

3. **usagi/src/anidbapi.cpp**
   - Enhanced `storeFileData()` with version extraction and CRC status logging
   - Added detailed state field decoding in debug output

4. **tests/test_watchsessionmanager.cpp**
   - Updated test to use correct state values (state=4 for v2, not state=2)
   - Added comprehensive API spec comments

## Debug Logging

### Added Logging Features
All file version functions now log:
- Raw state value (decimal and hexadecimal)
- Extracted version number
- File IDs (fid, lid)
- CRC status (in storeFileData)
- Detailed information about files with higher versions

### Example Debug Output
```
[WatchSessionManager::getFileVersion] lid=1001, fid=5001, raw_state=1 (0x1), extracted_version=1
[WatchSessionManager::getHigherVersionFileCount] lid=1001, my_version=1, higher_version_count=1
[WatchSessionManager::getHigherVersionFileCount]   Higher version file: lid=1100, fid=5100, state=4 (0x4), version=2, path=/test/anime1/episode1_v2.mkv
[AniDB storeFileData] fid=5001, raw_state=1 (0x1), extracted_version=1, crc_status=CRC OK
```

## Verification

### Database Schema
✅ The `file` table correctly includes `state INTEGER` field
✅ State field is properly populated from AniDB FILE responses

### API Mask
✅ FILE command includes `fSTATE = 0x01000000` in all invocations
✅ State field is requested from AniDB API in all FILE queries

### Test Results
✅ All 20 WatchSessionManager tests pass
✅ Test validates v1 file gets lower score than v2 file
✅ Debug logging confirms correct version extraction
✅ No new clazy warnings introduced

## Impact

### Affected Features
This fix impacts the following features:
1. **Duplicate File Detection**: Correctly identifies older file versions
2. **Automatic File Management**: Properly prioritizes newer versions for retention
3. **Watch Session Scoring**: Accurately calculates deletion eligibility based on file version

### User-Visible Changes
- Files with higher version numbers (v2, v3, v4, v5) are now correctly recognized
- Automatic deletion marking now properly favors keeping newer versions
- Debug logs provide clear insight into version detection for troubleshooting

## Testing

### Test Coverage
- Unit test validates version-based scoring with v1 and v2 files
- Debug logging provides runtime verification
- Test uses correct state values per API specification

### Manual Testing Recommendations
When testing with real AniDB data:
1. Check debug logs for state values from FILE responses
2. Verify version extraction matches expected versions
3. Confirm duplicate detection correctly identifies file versions
4. Test with files having different state values (CRC OK/ERR combinations)

## References
- [AniDB UDP API Definition](https://wiki.anidb.net/UDP_API_Definition)
- Issue: "verify that this and other functions correctly read file version"
- Code: `usagi/src/watchsessionmanager.cpp`, `usagi/src/anidbapi.cpp`
