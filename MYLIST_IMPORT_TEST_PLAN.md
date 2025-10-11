# MyList Import Feature - Test Plan

## Overview
This document describes how to test the new mylist import functionality added to address the issue "load mylist returns 0 results".

## Prerequisites
1. AniDB account with a populated mylist
2. Usagi-dono application built and running
3. Test export files (provided below)

## Test Case 1: Fetch MyList Stats from API

**Objective:** Verify MYLISTSTATS command works correctly

**Steps:**
1. Open Usagi-dono
2. Navigate to the "MyList" tab
3. Click "Fetch MyList Stats from API" button
4. Check the log output

**Expected Results:**
- Message "Fetching mylist statistics from API..." appears in log
- MYLISTSTATS command is sent to AniDB UDP API
- Informational message about using import feature appears
- Response code 223 should be received with statistics

**Pass Criteria:**
- No crashes or errors
- Log shows command was sent
- Instructions for import appear

## Test Case 2: Import MyList from XML File

**Objective:** Verify XML format parsing works correctly

**Steps:**
1. Create test XML file with content from `/tmp/test_mylist_export.xml`
2. Open Usagi-dono
3. Navigate to the "MyList" tab
4. Click "Import MyList from File" button
5. Select the XML file
6. Check log output
7. Click "Load MyList from Database"

**Expected Results:**
- File dialog opens with XML filter
- After selection, log shows "Successfully imported 3 mylist entries"
- Tree view auto-refreshes and shows imported entries
- Entries have correct data:
  - lid: 123456, 123457, 123458
  - fid values match
  - aid values match (901234, 901234, 901235)
  - Viewed status is correct (based on viewdate)

**Pass Criteria:**
- All 3 entries imported successfully
- Data matches test file
- No SQL errors in log
- Tree view displays entries correctly

## Test Case 3: Import MyList from CSV File

**Objective:** Verify CSV/TXT format parsing works correctly

**Steps:**
1. Create test CSV file with content from `/tmp/test_mylist_export.csv`
2. Open Usagi-dono
3. Navigate to the "MyList" tab
4. Click "Import MyList from File" button
5. Select the CSV file
6. Check log output
7. Click "Load MyList from Database"

**Expected Results:**
- File dialog opens with CSV filter
- Header line is skipped during parsing
- After selection, log shows "Successfully imported 3 mylist entries"
- Tree view shows imported entries
- Entry data matches test file

**Pass Criteria:**
- All 3 entries imported (header line skipped)
- Data matches test file
- No SQL errors in log
- Viewed status calculated correctly

## Test Case 4: Auto-detect Format

**Objective:** Verify format auto-detection works

**Steps:**
1. Rename test XML file to have .txt extension
2. Import the file
3. Verify it's parsed as XML

**Expected Results:**
- File is correctly detected as XML despite .txt extension
- Parsing succeeds

**Pass Criteria:**
- Correct format detected
- All entries imported

## Test Case 5: SQL Injection Prevention

**Objective:** Verify special characters in storage field don't break SQL

**Steps:**
1. Create XML file with storage field containing single quotes:
   ```xml
   <mylist lid="999" fid="888" eid="777" aid="666" gid="555" 
           state="1" viewdate="1609459200" storage="/anime/test's file.mkv"/>
   ```
2. Import the file
3. Check log for SQL errors
4. Verify entry in database

**Expected Results:**
- No SQL errors
- Entry imported successfully
- Storage field contains single quote correctly

**Pass Criteria:**
- No SQL errors in log
- Entry exists in database with correct storage value
- Single quotes properly escaped

## Test Case 6: Empty/Missing Fields

**Objective:** Verify handling of missing or empty fields

**Steps:**
1. Create test file with:
   - Empty viewdate (should set viewed=0)
   - viewdate="0" (should set viewed=0)
   - viewdate="1609459200" (should set viewed=1)
   - Missing optional fields
2. Import and verify

**Expected Results:**
- Empty/zero viewdate → viewed=0
- Non-zero viewdate → viewed=1
- Missing fields use default values (0 or empty string)

**Pass Criteria:**
- All entries imported
- Viewed status calculated correctly
- Default values applied appropriately

## Test Case 7: Large File Import

**Objective:** Verify performance with realistic mylist size

**Steps:**
1. Export actual mylist from AniDB (could be 100+ entries)
2. Import the file
3. Measure time and check for issues

**Expected Results:**
- Import completes in reasonable time (<5 seconds for 1000 entries)
- All entries imported
- No memory issues

**Pass Criteria:**
- Performance acceptable
- All entries imported successfully
- No crashes or memory leaks

## Test Case 8: Re-import (Update Existing)

**Objective:** Verify INSERT OR REPLACE works correctly

**Steps:**
1. Import test file
2. Modify storage field in one entry
3. Re-import the file
4. Verify entry is updated, not duplicated

**Expected Results:**
- Entry is updated, not duplicated
- Modified storage field is updated
- Entry count remains same

**Pass Criteria:**
- No duplicate entries
- Updates applied correctly
- Database integrity maintained

## Test Case 9: Invalid File Format

**Objective:** Verify error handling for invalid files

**Steps:**
1. Try to import a non-mylist file (e.g., random text)
2. Check error handling

**Expected Results:**
- Log shows "No entries imported. Please check the file format."
- No crashes
- Database unchanged

**Pass Criteria:**
- Graceful error handling
- Helpful error message
- No database corruption

## Test Case 10: Cancel File Dialog

**Objective:** Verify cancel handling

**Steps:**
1. Click "Import MyList from File"
2. Click "Cancel" in file dialog

**Expected Results:**
- Dialog closes
- No errors
- Operation cancelled cleanly

**Pass Criteria:**
- No crashes or errors
- UI remains responsive

## Integration Test: Complete Workflow

**Objective:** Test complete user workflow

**Steps:**
1. Start with empty database
2. Click "Load MyList from Database" → should show 0 entries
3. Click "Fetch MyList Stats from API" → stats sent
4. Export mylist from AniDB website
5. Click "Import MyList from File" → select export
6. Verify import success message
7. Click "Load MyList from Database" → should show all entries
8. Verify tree structure (anime → episodes)
9. Verify all columns show correct data

**Expected Results:**
- Complete workflow executes without errors
- User can successfully populate their mylist
- Data displays correctly in tree view

**Pass Criteria:**
- All steps complete successfully
- Data accurately reflects AniDB mylist
- UI is intuitive and responsive

## Manual Testing Notes

Since Qt is not available in the build environment, these tests must be performed manually:

1. Build the application with Qt 5.4.1 or later
2. Run through each test case
3. Document results
4. Report any issues found

## Known Limitations

1. XML parser uses simple regex, not full XML parser
   - Handles standard AniDB export format correctly
   - May not handle all edge cases of XML syntax

2. CSV parser expects specific field order
   - Based on AniDB export format
   - May need adjustment if format changes

3. HTTP API mylist export requires manual download
   - No automated HTTP API integration yet
   - Future enhancement could add direct HTTP API call

## Success Criteria Summary

The implementation is considered successful if:
- ✓ Users can fetch mylist stats via UDP API
- ✓ Users can import complete mylist from XML export
- ✓ Users can import complete mylist from CSV export
- ✓ Format is auto-detected correctly
- ✓ SQL injection is prevented
- ✓ Empty databases are properly populated
- ✓ The original issue "load mylist returns 0 results" is resolved
