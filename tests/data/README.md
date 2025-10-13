# MyList Import Test Data

This directory contains sample CSV files for testing the mylist import functionality.

## Test Files

### test_csv_adborg.csv
Tests the csv-adborg template format from AniDB MYLISTEXPORT.
- Format: aid,eid,gid,lid,status,viewdate,anime_name,episode_name
- Contains 5 entries with various states
- Tests: Header detection, column mapping, status field parsing

### test_standard_csv.csv
Tests the standard MYLISTEXPORT CSV format (backward compatibility).
- Format: lid,fid,eid,aid,gid,date,state,viewdate,storage,...
- Contains 5 entries matching test_csv_adborg.csv data
- Tests: Position-based parsing, standard format support

### test_mixed_case.csv
Tests case-insensitive header matching.
- Format: Mixed case headers (AID, EID, LID, STATUS)
- Contains 2 entries
- Tests: Case-insensitive column name matching

### test_minimal_fields.csv
Tests minimal required fields.
- Format: Only aid,lid,status,viewdate
- Contains 2 entries
- Tests: Optional field defaults, minimal valid format

## Expected Import Results

All test files should import successfully with these characteristics:

**test_csv_adborg.csv**: 5 entries
- lids: 123456-123460
- aids: 901234, 901234, 901235, 901235, 901236
- fids: All 0 (default, not in csv-adborg)
- states: 1, 1, 1, 2, 3
- viewed: 1, 1, 0, 1, 0

**test_standard_csv.csv**: 5 entries
- lids: 123456-123460
- aids: 901234, 901234, 901235, 901235, 901236
- fids: 789012-789016
- states: 1, 1, 1, 2, 3
- viewed: 1, 1, 0, 1, 0

**test_mixed_case.csv**: 2 entries
- lids: 123456, 123457
- aids: 901234, 901234
- All fields parsed correctly despite mixed case

**test_minimal_fields.csv**: 2 entries
- lids: 123456, 123457
- aids: 901234, 901235
- Missing fields default to 0

## Usage

1. Import each file via the application's "Import MyList from File" button
2. Check log output for import count
3. Click "Load MyList from Database" to verify entries
4. Verify lid, aid, state, and viewed status match expected results

## Notes

- These files use comma separators
- All files have header rows
- test_minimal_fields.csv and test_mixed_case.csv start with "#" comment
- Real AniDB exports may have additional fields which are safely ignored
