# MyList Export Format Migration: csv-adborg → xml-plain-cs

## Summary

This document describes the migration from csv-adborg template to xml-plain-cs template for mylist exports, fixing the critical lid/fid inconsistency issue.

## Problem Statement

The csv-adborg template format had a fundamental data integrity issue:

### Issue
- **No lid values**: The csv-adborg format did not contain proper mylist ID (lid) values
- **Placeholder usage**: Parser used fid (file ID) as a placeholder for lid
- **Duplicate entries**: This caused lid = fid in the database, which is incorrect
- **API inconsistency**: AniDB API expects lid and fid to be different values
- **Data corruption**: Duplicate entries could occur when same file appears in multiple mylist entries

### Old Behavior (csv-adborg)
```sql
-- Example entry after csv-adborg import
INSERT INTO mylist (lid, fid, eid, aid, ...) 
VALUES (789012, 789012, 297776, 5178, ...);
       ^^^^^^  ^^^^^^
       Same value! This is wrong!
```

### New Behavior (xml-plain-cs)
```sql
-- Example entry after xml-plain-cs import  
INSERT INTO mylist (lid, fid, eid, aid, ...) 
VALUES (123456, 789012, 297776, 5178, ...);
       ^^^^^^  ^^^^^^
       Different values! This is correct!
```

## Solution

### Changes Made

#### 1. Parser Implementation (window.cpp)
- **Renamed**: `parseMylistCSVAdborg()` → `parseMylistExport()`
- **Complete rewrite**: Replaced CSV parsing logic with XML parsing using QXmlStreamReader
- **Proper lid handling**: Now extracts lid from XML attributes instead of using fid placeholder
- **Simplified code**: Removed complex CSV section parsing (FILES, GROUPS, GENREN)

#### 2. Template Update (All files)
- **Default template**: Changed from "csv-adborg" to "xml-plain-cs"
- Updated in:
  - `window.cpp` (3 locations)
  - `window.h` (1 location)
  - `anidbapi.cpp` (2 locations)
  - `anidbapi.h` (1 location)

#### 3. XML Parsing
Added QXmlStreamReader support:
```cpp
QXmlStreamReader xml(&xmlFile);
while(!xml.atEnd() && !xml.hasError()) {
    if(xml.name() == QString("mylist")) {
        QString lid = attributes.value("lid").toString();
        QString fid = attributes.value("fid").toString();
        // lid and fid are now properly different!
    }
}
```

#### 4. Testing
Created comprehensive test (`test_mylist_xml_parser.cpp`):
- Verifies XML extraction from tar.gz
- Validates XML parsing with QXmlStreamReader
- Confirms lid ≠ fid (key test!)
- Tests viewdate → viewed conversion logic
- All 6 tests pass

#### 5. Documentation
- Updated `MYLIST_IMPORT_GUIDE.md`
- Removed `example_csv_adborg.txt`
- Added `example_xml_plain_cs.txt`
- Documented migration path for existing users

## Format Comparison

### csv-adborg (OLD - Removed)
```
AID|EID|EPNo|Name|Romaji|Kanji|Length|Aired|Rating|Votes|State|myState
5178|81251|1|Complete Movie|||100|04.08.2007 00:00|7.18|6|0|2
FILES
AID|EID|GID|FID|size|ed2k|md5|sha1|crc|Length|Type|Quality|Resolution|vCodec|aCodec|sub|dub|isWatched|State|myState|FileType
5178|81251|1412|446487|734003200|...
```
**Problem**: No lid field! Parser used FID (446487) as both lid and fid.

### xml-plain-cs (NEW - Current)
```xml
<mylistexport>
  <mylist lid="123456" fid="789012" eid="297776" aid="5178" gid="1412" 
          state="1" viewdate="1640995200" storage="HDD"/>
</mylistexport>
```
**Solution**: Proper lid (123456) and fid (789012) - different values!

## Benefits

1. **Data Integrity**: lid and fid are properly different
2. **API Consistency**: Matches AniDB API response format
3. **No Duplicates**: Prevents duplicate entries from fid reuse
4. **Simpler Code**: XML parsing is cleaner than CSV section parsing
5. **Standard Format**: Uses standard XML instead of custom CSV format

## Migration Guide for Users

If you previously imported mylist using csv-adborg:

### Step 1: Clear Old Data (Optional but Recommended)
```sql
DELETE FROM mylist;
```

### Step 2: Request New Export
1. Open Usagi-dono
2. Go to Settings tab
3. Click "Request MyList Export"
4. Wait for notification with download link

### Step 3: Import New File
The application will automatically:
- Download the xml-plain-cs export
- Parse XML with proper lid values
- Import into database with correct lid/fid separation

### Verification
After import, verify in database that lid ≠ fid:
```sql
SELECT COUNT(*) FROM mylist WHERE lid = fid;
-- Should return 0 (no entries where lid equals fid)
```

## Technical Details

### Database Schema (Unchanged)
```sql
CREATE TABLE `mylist`(
    `lid` INTEGER PRIMARY KEY,  -- Now properly unique!
    `fid` INTEGER,               -- Different from lid
    `eid` INTEGER,
    `aid` INTEGER,
    `gid` INTEGER,
    `state` INTEGER,
    `viewed` INTEGER,
    `storage` TEXT
)
```

### Key Code Change
```cpp
// OLD (csv-adborg)
.arg(fid)  // Use fid as lid since we don't have lid
.arg(fid)

// NEW (xml-plain-cs)
.arg(lid)  // Now using proper lid from XML
.arg(fid.isEmpty() ? "0" : fid)
```

## Files Changed

### Code Files
- `usagi/src/window.h` - Function rename, add XML include
- `usagi/src/window.cpp` - Complete parser rewrite, template changes
- `usagi/src/anidbapi.h` - Default template change
- `usagi/src/anidbapi.cpp` - Template references update

### Test Files
- `tests/test_mylist_xml_parser.cpp` - New XML parser test
- `tests/CMakeLists.txt` - Add new test

### Documentation Files
- `MYLIST_IMPORT_GUIDE.md` - Update recommendations
- `MYLIST_XML_MIGRATION.md` - This file (migration guide)
- `example_xml_plain_cs.txt` - New format examples
- `example_csv_adborg.txt` - Removed (deprecated)

## Compatibility

### Breaking Changes
- csv-adborg template is no longer supported
- Old imports with lid=fid need to be re-imported

### Backward Compatibility
- Database schema unchanged
- Existing mylist entries remain valid
- Can safely re-import over old data (INSERT OR REPLACE)

## Testing Results

All tests pass:
```
Test #1: test_hash ........................   Passed
Test #2: test_crashlog ....................   Passed
Test #3: test_anidbapi ....................   Passed
Test #4: test_anime_titles ................   Passed
Test #5: test_url_extraction ..............   Passed
Test #6: test_mylist_xml_parser ...........   Passed

100% tests passed, 0 tests failed out of 6
```

Key test verification in test_mylist_xml_parser:
```cpp
// Verify no entries where lid == fid
QVERIFY(query.exec("SELECT lid, fid FROM mylist WHERE lid = fid"));
QVERIFY(!query.next());  // Should be no results - PASSES ✓
```

## Conclusion

This migration fixes a critical data integrity issue where mylist entries had incorrect lid values (using fid as placeholder). The new xml-plain-cs format provides proper lid values, ensuring:

✓ Data integrity (lid ≠ fid)  
✓ API consistency  
✓ No duplicate entries  
✓ Cleaner, simpler code  
✓ All tests passing  

Users should re-import their mylist using the new xml-plain-cs template to benefit from these fixes.
