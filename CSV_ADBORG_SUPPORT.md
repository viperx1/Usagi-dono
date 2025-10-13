# CSV-Adborg Template Support - Test Guide

## Overview
This document describes the enhanced CSV parsing functionality that now supports AniDB's "csv-adborg" template format in addition to the standard MYLISTEXPORT format.

## Feature Description
The parseMylistCSV() function now intelligently detects and parses multiple CSV template formats by:
1. Reading the header row to identify column names
2. Building a column mapping based on header names
3. Extracting data based on column names rather than fixed positions

## Supported Formats

### Standard MYLISTEXPORT Format (Original)
- **Field Order**: lid,fid,eid,aid,gid,date,state,viewdate,storage,...
- **Required Fields**: lid, fid, aid
- **Example**:
```csv
lid,fid,eid,aid,gid,date,state,viewdate,storage,source,other,filestate
123456,789012,345678,901234,567890,1234567890,1,1234567890,/path/to/file,...
```

### csv-adborg Template Format (New)
- **Field Order**: aid,eid,gid,lid,status,viewdate,anime_name,episode_name,...
- **Required Fields**: lid, aid
- **Optional Fields**: fid (defaults to 0 if missing)
- **Example**:
```csv
aid,eid,gid,lid,status,viewdate,anime_name,episode_name
901234,345678,567890,123456,1,1234567890,Test Anime,Episode 1
```

## Key Differences

| Feature | Standard Format | csv-adborg Format |
|---------|----------------|-------------------|
| First Column | lid | aid |
| fid Field | Required | Optional (defaults to 0) |
| State Column Name | "state" | "status" |
| Text Fields | Not included | Includes anime_name, episode_name |
| Column Order | Fixed positions | Header-based |

## Technical Implementation

### Header Detection
The parser detects headers by checking if the first line contains:
- "lid" OR
- "aid" OR
- Starts with "#"

### Column Mapping
When a header is detected:
1. Parse header line by splitting on commas
2. Build a QMap<QString, int> mapping column names to positions
3. Use case-insensitive matching for column names
4. Extract fields by looking up column positions in the map

### Field Extraction
- **lid**: Required, must be present in header
- **fid**: Optional for csv-adborg, defaults to "0"
- **eid**: Optional, defaults to "0"
- **aid**: Required, must be present
- **gid**: Optional, defaults to "0"
- **state/status**: Supports both "state" and "status" column names
- **viewdate**: Optional
- **storage**: Optional

### Validation Rules
1. Skip empty lines
2. Skip header line after parsing it
3. Require either (lid AND fid) OR (lid AND aid) for a row to be imported
4. Continue processing other rows even if some fail validation

## Test Cases

### Test Case 1: csv-adborg Format Import
**File**: test_csv_adborg.csv
```csv
aid,eid,gid,lid,status,viewdate,anime_name,episode_name
901234,345678,567890,123456,1,1234567890,Test Anime,Episode 1
901234,345679,567890,123457,1,1234567891,Test Anime,Episode 2
901235,345680,567891,123458,1,0,Another Anime,Episode 1
```

**Expected Results**:
- 3 entries imported successfully
- lid values: 123456, 123457, 123458
- aid values: 901234, 901234, 901235
- fid values: 0 (default) for all entries
- viewed status: 1, 1, 0 (based on viewdate)

### Test Case 2: Standard Format Import (Backward Compatibility)
**File**: test_standard_csv.csv
```csv
lid,fid,eid,aid,gid,date,state,viewdate,storage,source,other,filestate
123456,789012,345678,901234,567890,1234567890,1,1234567890,/path/to/file,...
```

**Expected Results**:
- Import works exactly as before
- No regression in existing functionality
- All fields parsed correctly

### Test Case 3: Mixed Case Headers
**File**: test_mixed_case.csv
```csv
AID,EID,GID,LID,STATUS,VIEWDATE,Anime_Name,Episode_Name
901234,345678,567890,123456,1,1234567890,Test,Ep1
```

**Expected Results**:
- Case-insensitive matching works
- Entry imported successfully

### Test Case 4: No Header (Position-based Fallback)
**File**: test_no_header.csv
```csv
123456,789012,345678,901234,567890,1234567890,1,1234567890,/path/to/file
```

**Expected Results**:
- Falls back to position-based parsing
- Assumes standard format
- Entry imported successfully

### Test Case 5: Partial Fields
**File**: test_partial_fields.csv
```csv
aid,lid,status,viewdate
901234,123456,1,1234567890
```

**Expected Results**:
- Entry imported with partial data
- Missing fields default to "0"
- fid=0, eid=0, gid=0

## Manual Testing Steps

1. Export your mylist from AniDB using the "csv-adborg" template:
   - Go to https://anidb.net/perl-bin/animedb.pl?show=mylist
   - Click "Export"
   - Select "csv-adborg" template
   - Download the file

2. Import the file in Usagi-dono:
   - Open application
   - Go to MyList tab
   - Click "Import MyList from File"
   - Select the downloaded csv-adborg file
   - Verify success message

3. Verify data:
   - Click "Load MyList from Database"
   - Check that all entries are displayed
   - Verify lid, aid, state, and viewed status are correct

## Known Limitations

1. **No fid in csv-adborg**: The csv-adborg template doesn't include fid, so it defaults to 0. This may affect file matching functionality.

2. **Text fields ignored**: Fields like anime_name and episode_name are not imported (they're stored in separate tables populated by other means).

3. **No validation of column values**: The parser doesn't validate that aid, eid, etc. are valid IDs, just that they're not empty.

## Troubleshooting

### Issue: No entries imported from csv-adborg file
**Solution**: 
- Check that the file has a header row with at least "lid" and "aid"
- Verify the file is using comma separators
- Check log output for specific errors

### Issue: fid shows as 0 for all entries
**Explanation**: This is expected for csv-adborg format which doesn't include fid. It won't affect mylist display but may affect file matching.

### Issue: Some entries skipped
**Solution**: 
- Check that all rows have valid lid and aid values
- Empty or missing required fields will cause rows to be skipped
- Check log for count of successfully imported entries

## Success Criteria

✅ csv-adborg template files import successfully
✅ Standard format still works (backward compatibility)
✅ Header-based column detection works
✅ Case-insensitive column matching works
✅ Missing optional fields default appropriately
✅ Both "state" and "status" column names supported
✅ No SQL errors or crashes
✅ Imported entries display correctly in tree view

## Code Changes Summary

**File**: usagi/src/window.cpp
**Function**: parseMylistCSV()
**Changes**:
- Added header detection and parsing
- Added column mapping using QMap<QString, int>
- Added header-based field extraction
- Added support for "status" as alternative to "state"
- Made fid optional (defaults to "0")
- Added default values for optional fields
- Maintained backward compatibility with position-based parsing

**Lines Changed**: ~70 lines modified/added
**Complexity**: Medium (added logic, maintained compatibility)
**Testing**: Manual testing required (no unit test infrastructure)
