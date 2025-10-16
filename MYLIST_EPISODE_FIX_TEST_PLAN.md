# MyList Episode Display Fix - Test Plan

## Issue
Mylist episode column shows eid instead of episode number, and episode title column is empty.

## Changes Summary
- Added comprehensive error logging for database operations
- Fixed SQL string escaping to prevent corruption
- Added warning messages when episode data is missing

## How to Test

### Step 1: Build and Run
```bash
# Build the project
cd /path/to/Usagi-dono
mkdir build && cd build
cmake ..
make

# Run the application
./usagi
```

### Step 2: Import MyList Data
1. Request a mylist export from AniDB:
   - In Usagi-dono, go to Settings tab
   - Click "Request MyList Export" button
   - Wait for AniDB to generate the export (may take several minutes)
   - Check AniDB notifications for the export URL
   
2. Download and import the export:
   - Download the .tar.gz file from AniDB
   - In Usagi-dono, the file should be automatically detected and imported
   - OR manually trigger import if needed

### Step 3: Check Log Output
During and after import, check the log window for messages:

**Success indicators:**
- "Successfully imported X mylist entries"
- "Loaded X mylist entries for Y anime"
- No error messages

**Problem indicators:**
- `Error inserting episode X: ...` - Episode INSERT failed
  - Indicates a database problem or SQL syntax error
  - Share the full error message for diagnosis
  
- `Error inserting mylist entry (fid=X): ...` - MyList INSERT failed
  - Indicates a problem with FILES section parsing
  - Check if CSV format is as expected
  
- `Warning: No episode number found for EID X (AID Y)` - Episode displayed with eid fallback
  - Episode table doesn't have data for this episode
  - Could mean CSV had empty EPNo field, or episode wasn't parsed

- `Warning: No episode name found for EID X (AID Y)` - Episode title is empty
  - Episode table doesn't have name for this episode
  - Could mean CSV had empty Name field

### Step 4: Verify MyList Display
1. Go to MyList tab
2. Expand some anime entries to see episodes
3. Check if episode numbers and titles are displayed correctly

**Expected Result:**
```
├─ Anime Title
│  ├─ Episode 1 - Episode Title Here
│  ├─ Episode 2 - Another Episode Title
│  └─ Episode 3 - Yet Another Title
```

**If issue persists:**
```
├─ Anime Title
│  ├─ 123456 - 
│  ├─ 123457 - 
│  └─ 123458 - 
```
(Numbers are episode IDs, titles are empty)

### Step 5: Database Verification (Optional)
If you have database access, you can verify the data:

```sql
-- Check if episode table has data
SELECT COUNT(*) FROM episode;
SELECT eid, name, epno FROM episode LIMIT 10;

-- Check if mylist entries have matching episodes
SELECT 
  m.lid, m.eid, m.aid,
  e.epno, e.name
FROM mylist m
LEFT JOIN episode e ON m.eid = e.eid
LIMIT 10;
```

**Expected:**
- episode table should have rows
- LEFT JOIN should show matching epno and name values (not NULL)

**If issue persists:**
- episode table may be empty
- LEFT JOIN shows NULL for e.epno and e.name

## Troubleshooting

### Issue: Episode data not showing
**Possible Causes:**
1. CSV file from AniDB has empty episode fields
2. Episode INSERT queries are failing
3. Old mylist data (from before episode parsing was implemented)

**Solutions:**
1. Check log for error/warning messages
2. Re-import mylist data to ensure episode parsing runs
3. If using old database, consider clearing and re-importing

### Issue: No error messages but still broken
**Possible Causes:**
1. Episode data is in database but JOIN isn't working
2. Data type mismatch between mylist.eid and episode.eid

**Solutions:**
1. Run database verification SQL queries above
2. Check if episode.eid matches mylist.eid
3. Share database query results for further diagnosis

### Issue: Import shows 0 entries
**Possible Causes:**
1. tar.gz extraction failed
2. CSV files not in expected location
3. CSV format is different from expected

**Solutions:**
1. Verify .tar.gz file is valid
2. Check if it contains anime/*.csv files
3. Manually inspect CSV file format

## Reporting Issues

If problems persist after testing, please provide:
1. **Complete log output** from the import process
2. **Error messages** if any appear
3. **Sample CSV data** (if possible, anonymized is fine)
4. **Database query results** from verification step
5. **Screenshots** of the MyList display showing the issue

This information will help identify the root cause and provide a targeted fix.
