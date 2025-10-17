# PR Summary: Fix MyList Import - Switch to xml-plain-cs Template

## Issue Resolution

**Issue**: MyList import from csv-adborg template was using fid (File ID) as a placeholder for lid (MyList ID), causing:
- Duplicate entries in the database
- Inconsistency with AniDB API which expects distinct lid and fid values
- Data integrity issues

**Solution**: Switch to xml-plain-cs template which provides proper lid values.

## Changes Made

### 1. Parser Implementation (4 files)
- **Renamed function**: `parseMylistCSVAdborg()` → `parseMylistExport()`
- **Complete rewrite**: Replaced CSV parsing with XML parsing using QXmlStreamReader
- **Fixed lid/fid**: Now uses proper lid from XML instead of fid placeholder

**Files Modified:**
- `usagi/src/window.h` - Function declaration, added QXmlStreamReader include
- `usagi/src/window.cpp` - Parser implementation (~200 lines rewritten)
- `usagi/src/anidbapi.h` - Default template parameter
- `usagi/src/anidbapi.cpp` - Comments and template references

### 2. Template Switch (7 locations)
Changed from `csv-adborg` to `xml-plain-cs` in:
- Manual export request
- Automatic export request (first run)
- Default function parameter
- Comments and documentation

### 3. New Test (2 files)
Created comprehensive XML parser test:
- `tests/test_mylist_xml_parser.cpp` - New test file (225 lines)
- `tests/CMakeLists.txt` - Test configuration

**Test validates:**
- ✓ XML extraction from tar.gz
- ✓ XML parsing with QXmlStreamReader
- ✓ Proper lid values (not fid placeholders)
- ✓ lid ≠ fid verification (key test!)
- ✓ viewdate → viewed conversion

### 4. Documentation (4 files)
- `MYLIST_IMPORT_GUIDE.md` - Updated to recommend xml-plain-cs
- `example_csv_adborg.txt` - Removed (deprecated)
- `example_xml_plain_cs.txt` - Added (new format examples)
- `MYLIST_XML_MIGRATION.md` - Comprehensive migration guide

## Key Technical Changes

### Before (csv-adborg)
```cpp
// Problem: No lid in CSV format, using fid as placeholder
QString q = QString("INSERT OR REPLACE INTO `mylist` "
    "(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) "
    "VALUES (%1, %2, %3, %4, %5, %6, %7, '')")
    .arg(fid)  // Use fid as lid - WRONG!
    .arg(fid)
    // ...
```

**Result**: lid = fid = 789012 (both same - incorrect!)

### After (xml-plain-cs)
```cpp
// Solution: Extract proper lid from XML
QString lid = attributes.value("lid").toString();
QString fid = attributes.value("fid").toString();
// ...
QString q = QString("INSERT OR REPLACE INTO `mylist` "
    "(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) "
    "VALUES (%1, %2, %3, %4, %5, %6, %7, '%8')")
    .arg(lid)  // Now using proper lid from XML
    .arg(fid.isEmpty() ? "0" : fid)
    // ...
```

**Result**: lid = 123456, fid = 789012 (different - correct!)

## Test Results

```
Test project /home/runner/work/Usagi-dono/Usagi-dono/build
    Start 1: test_hash ........................   Passed    0.01 sec
    Start 2: test_crashlog ....................   Passed    1.22 sec
    Start 3: test_anidbapi ....................   Passed    0.10 sec
    Start 4: test_anime_titles ................   Passed    0.05 sec
    Start 5: test_url_extraction ..............   Passed    0.01 sec
    Start 6: test_mylist_xml_parser ...........   Passed    0.01 sec

100% tests passed, 0 tests failed out of 6
Total Test time (real) =   1.40 sec
```

### Critical Test Verification
```cpp
// Verify no entries where lid == fid
QVERIFY(query.exec("SELECT lid, fid FROM mylist WHERE lid = fid"));
QVERIFY(!query.next());  // Should be no results

// Result: PASSES ✓ (confirms lid and fid are different)
```

## Benefits

1. **Data Integrity** - lid and fid are properly different values
2. **API Consistency** - Matches AniDB API response format
3. **No Duplicates** - Prevents duplicate entries from fid reuse
4. **Simpler Code** - XML parsing is cleaner than CSV section parsing (-77 lines)
5. **Better Testing** - Comprehensive test coverage for XML parsing

## Breaking Changes

⚠️ **csv-adborg template support removed**

Users with existing csv-adborg imports should:
1. Request new export using xml-plain-cs template
2. Re-import to get proper lid values

## Backward Compatibility

✓ Database schema unchanged  
✓ Existing mylist entries remain valid  
✓ Can safely re-import over old data (INSERT OR REPLACE)

## Files Changed

### Code (4 files)
- usagi/src/window.h
- usagi/src/window.cpp
- usagi/src/anidbapi.h
- usagi/src/anidbapi.cpp

### Tests (2 files)
- tests/test_mylist_xml_parser.cpp (new)
- tests/CMakeLists.txt

### Documentation (4 files)
- MYLIST_IMPORT_GUIDE.md (updated)
- example_csv_adborg.txt (removed)
- example_xml_plain_cs.txt (new)
- MYLIST_XML_MIGRATION.md (new)

## Commits

1. `92da06e` - Switch from csv-adborg to xml-plain-cs template for mylist export
2. `c7772b0` - Add test for XML mylist parser to verify lid/fid separation
3. `86fa33a` - Update documentation to reflect xml-plain-cs template change
4. `bd1f8b3` - Add comprehensive migration guide for xml-plain-cs change

## Verification Steps

To verify the fix works correctly:

1. **Build the project**
   ```bash
   cmake --build build
   ```

2. **Run all tests**
   ```bash
   ctest --test-dir build --output-on-failure
   ```
   Expected: All 6 tests pass

3. **Verify XML parsing**
   ```bash
   ./build/tests/test_mylist_xml_parser -v2
   ```
   Expected: Test passes, confirms lid ≠ fid

4. **Manual verification** (optional)
   - Request MyList export from AniDB
   - Import the file
   - Check database: `SELECT COUNT(*) FROM mylist WHERE lid = fid;`
   - Expected result: 0 (no entries with lid=fid)

## Migration Guide

See `MYLIST_XML_MIGRATION.md` for detailed migration instructions.

Quick summary:
1. Clear old mylist data (optional but recommended)
2. Request new export using xml-plain-cs template
3. Import the new file
4. Verify: lid ≠ fid in database

## Conclusion

This PR successfully resolves the mylist import issue by:
- ✓ Switching to xml-plain-cs template with proper lid values
- ✓ Removing csv-adborg support (deprecated format)
- ✓ Implementing robust XML parser with proper lid/fid separation
- ✓ Adding comprehensive test coverage
- ✓ Updating all documentation
- ✓ All tests passing

The change ensures data integrity and consistency with the AniDB API specification.
