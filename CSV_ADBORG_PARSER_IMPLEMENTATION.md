# csv-adborg Parser Implementation Summary

## Overview
Completely rewrote mylist import functionality to support only AniDB's csv-adborg MYLISTEXPORT template format.

## Changes Made

### Removed
1. **parseMylistXML()** - XML format no longer supported
2. **parseMylistCSV()** - Old comma-delimited parser removed
3. All references to other template formats

### Implemented
**parseMylistCSVAdborg(const QString &tarGzPath)**

Handles the actual csv-adborg export format:
- Input: Path to .tgz file containing anime/*.csv files
- Format: Pipe-delimited (|) sections per anime
- Sections: Anime info, FILES, GROUPS, GENREN

## csv-adborg Format Details

### File Structure
```
archive.tgz
└── anime/
    ├── a5178.csv
    ├── a10596.csv
    └── ... (one file per anime)
```

### CSV File Structure
Each anime file contains:

1. **Anime Header Row**:
```
AID|EID|EPNo|Name|Romaji|Kanji|Length|Aired|Rating|Votes|State|myState
```

2. **Anime Data** (one or more episodes)

3. **FILES Section**:
```
FILES
AID|EID|GID|FID|size|ed2k|md2|sha1|crc|Length|Type|Quality|Resolution|vCodec|aCodec|sub|dub|isWatched|State|myState|FileType
```

4. **GROUPS Section**:
```
GROUPS
GID|AGID|Name|ShortName|State
```

5. **GENREN Section**:
```
GENREN
GenID|GenName|isHentai|Weight
```

## Implementation Details

### Extraction Process
1. Creates temporary directory in system temp path
2. Uses QProcess to run: `tar -xzf {tarGzPath}`
3. Timeout: 30 seconds
4. Error handling for extraction failures

### Parsing Logic
```cpp
For each anime/*.csv file:
  1. Read file content
  2. Look for "FILES" section marker
  3. Parse header row after FILES marker
  4. Split data lines by pipe delimiter (|)
  5. Extract fields by position:
     - AID (position 0)
     - EID (position 1)
     - GID (position 2)
     - FID (position 3)
     - isWatched (position 17)
     - State (position 18)
     - myState (position 19)
  6. Insert into database
```

### Database Mapping
| Database Column | Source | Notes |
|----------------|--------|-------|
| lid | FID | csv-adborg doesn't provide lid |
| fid | FID | File ID |
| eid | EID | Episode ID (0 if missing) |
| aid | AID | Anime ID |
| gid | GID | Group ID (0 if missing) |
| state | myState | User's storage state |
| viewed | isWatched | 1 if watched, 0 otherwise |
| storage | "" | Not provided in FILES section |

### SQL Query
```sql
INSERT OR REPLACE INTO `mylist` 
(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) 
VALUES (FID, FID, EID, AID, GID, myState, isWatched, '')
```

### Cleanup
- Automatically removes temporary directory after processing
- Uses `QDir().removeRecursively()`

## Example Data

### Input File: anime/a5178.csv
```
AID|EID|EPNo|Name|Romaji|Kanji|Length|Aired|Rating|Votes|State|myState
5178|81251|1|Complete Movie|||100|04.08.2007 00:00|7.18|6|0|2
FILES
AID|EID|GID|FID|size|ed2k|md5|sha1|crc|Length|Type|Quality|Resolution|vCodec|aCodec|sub|dub|isWatched|State|myState|FileType
5178|81251|1412|446487|734003200|1cd4d283e5e6077d657511909d0f6a44|95f1591739665290d11e3844d01d19bb|530d8d23f63044b5477bdcb1d2a1f5557796e6ba|75f57621|5689|10|very high|768x432|H264/AVC|MP3 CBR|english|japanese|1|1|2|avi
GROUPS
GID|AGID|Name|ShortName|State
1412|33634|Dattebayo|DB|complete
GENREN
GenID|GenName|isHentai|Weight 
4|Action|0|400
```

### Database Entry
```sql
lid=446487, fid=446487, eid=81251, aid=5178, gid=1412, state=2, viewed=1, storage=''
```

## Code Changes

### window.h
- Added includes: QProcess, QDateTime, QDir
- Removed: parseMylistXML() declaration
- Removed: parseMylistCSV() declaration
- Added: parseMylistCSVAdborg() declaration

### window.cpp
- Removed: ~150 lines (old XML and CSV parsers)
- Added: ~140 lines (csv-adborg parser with extraction)

## Testing

### Test Data
- Used provided file: 1760354161-4344-71837.tgz
- Contains: 395 anime CSV files
- Format validation: ✅ Passed
- Pipe delimiter: ✅ Correct
- Section parsing: ✅ FILES section extracted
- Field count: ✅ 20+ fields per line

### Manual Testing Required
1. Extract and parse tar.gz file
2. Verify database entries
3. Check mylist display in UI
4. Confirm auto-load on startup works

## Known Limitations

1. **No lid from csv-adborg**: Using FID as lid since the format doesn't provide it
2. **No storage path**: FILES section doesn't include file path
3. **Single transaction**: All files processed in one transaction (fast but less fault-tolerant)
4. **No retry logic**: Extraction failures not retried
5. **Fixed timeout**: 30-second extraction timeout may not be enough for very large archives

## Future Enhancements

### Notifications API Integration
As mentioned in the comment, the user suggested:
- Request export via API command with template specification
- Receive AniDB message with download link
- Automatically download and import

This would require:
1. Implement notifications API handler
2. Add API command to request export
3. Parse notification message for download link
4. Download file to temp location
5. Call parseMylistCSVAdborg() automatically
6. Update status label with progress

### Improvements
1. **Progress reporting**: Show extraction/parsing progress
2. **Incremental updates**: Check existing entries before insert
3. **Error recovery**: Continue on individual file errors
4. **Validation**: Verify FID/AID exist in AniDB
5. **Storage path**: Try to match files on disk by hash

## Commits
- **a53b5da**: Implement csv-adborg parser, remove XML/CSV parsers
- **f22d954**: Clean up accidentally committed test files

## Files
- Modified: window.h, window.cpp
- Modified: .gitignore (added anime/, *.tgz, etc.)
- Removed: All old parser code (~150 lines)
- Added: csv-adborg parser (~140 lines)

## Status
✅ Implementation complete
✅ Test data validated
⏳ Manual Qt testing required
⏳ Notifications API integration pending
