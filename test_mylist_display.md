# Manual Test Plan for MyList Display Fix

## Issue
MyList doesn't display anime and episode titles after importing from csv-adborg format.

## Root Cause
The `parseMylistCSVAdborg` function only parsed the FILES section and didn't parse episode data from the anime section. Additionally, `loadMylistFromDatabase` didn't use the `anime_titles` table as a fallback for anime names.

## Changes Made

### 1. parseMylistCSVAdborg Enhancement
- Now parses the anime section header: `AID|EID|EPNo|Name|Romaji|Kanji|...`
- Extracts episode data: episode ID, episode number, episode name (in multiple languages)
- Inserts episode data into the `episode` table

### 2. loadMylistFromDatabase Enhancement
- Modified SQL query to include anime title from `anime_titles` table
- Uses anime_titles as fallback when anime table doesn't have the name
- Priority: anime.nameromaji > anime.nameenglish > anime_titles.title > "Anime #[AID]"

## Test Procedure

### Prerequisites
1. Usagi-dono application built and running
2. AniDB account with some anime in mylist
3. Access to export mylist using csv-adborg template

### Test Steps

#### Test 1: Fresh Import from csv-adborg
1. Clear the existing database (or use a test database)
2. Export mylist from AniDB using "csv-adborg" template
3. Download the .tar.gz file
4. In Usagi-dono, go to MyList tab
5. Import the downloaded file
6. **Expected Result**: 
   - Episode titles should be displayed (from episode table)
   - Anime titles should be displayed (from anime_titles table)
   - Tree structure should show anime as parents, episodes as children

#### Test 2: Verify Episode Display
1. After import, check the MyList tree widget
2. Expand any anime entry
3. **Expected Result**: 
   - Episode should show: "Episode [number] - [episode name]"
   - Example: "Episode 1 - Complete Movie"
   - Not: "EID: 81251"

#### Test 3: Verify Anime Display
1. Check the anime parent items in the tree
2. **Expected Result**: 
   - Anime name should be displayed (from anime_titles)
   - Not: "Anime #5178"
   - Note: If anime_titles table is not populated, it will show "Anime #[AID]"

#### Test 4: anime_titles Population
1. Check if anime_titles table is populated
2. Run SQL query: `SELECT COUNT(*) FROM anime_titles;`
3. **Expected Result**: 
   - Should have entries (updated from AniDB anime titles database)
   - If zero, run the anime titles update process

#### Test 5: Database Verification
Run these SQL queries to verify data was inserted:

```sql
-- Check episode table has data
SELECT COUNT(*) FROM episode;

-- Check specific episode data
SELECT eid, name, epno FROM episode WHERE eid = 81251;

-- Check mylist entries
SELECT COUNT(*) FROM mylist;

-- Check the join query works
SELECT m.lid, m.aid, m.eid,
       a.nameromaji as anime_from_table,
       (SELECT title FROM anime_titles WHERE aid = m.aid AND type = 1 LIMIT 1) as anime_from_titles,
       e.name as episode_name, 
       e.epno
FROM mylist m
LEFT JOIN anime a ON m.aid = a.aid
LEFT JOIN episode e ON m.eid = e.eid
LIMIT 5;
```

### Expected SQL Results
```
episode table: Should have entries for each episode
mylist table: Should have entries for each file
Join query: Should show episode names and anime titles
```

## Troubleshooting

### Issue: Anime titles still show "Anime #[AID]"
**Cause**: anime_titles table is empty
**Solution**: 
1. Wait for automatic anime titles update (happens on startup if > 7 days old)
2. Or manually trigger update via AniDB API

### Issue: Episode titles not showing
**Cause**: Episode data not parsed correctly
**Solution**: 
1. Check csv-adborg file format matches expected format
2. Verify episode section comes before FILES section
3. Check logs for parsing errors

### Issue: Import shows 0 entries
**Cause**: tar.gz extraction failed or wrong file format
**Solution**: 
1. Check file is valid tar.gz
2. Check it contains anime/*.csv files
3. Verify CSV files have correct format

## Test Data Example

### Valid csv-adborg anime/a5178.csv:
```
AID|EID|EPNo|Name|Romaji|Kanji|Length|Aired|Rating|Votes|State|myState
5178|81251|1|Complete Movie|||100|04.08.2007 00:00|7.18|6|0|2
FILES
AID|EID|GID|FID|size|ed2k|md5|sha1|crc|Length|Type|Quality|Resolution|vCodec|aCodec|sub|dub|isWatched|State|myState|FileType
5178|81251|1412|446487|734003200|1cd4d283e5e6077d657511909d0f6a44|...
GROUPS
...
GENREN
...
```

### Expected Database Entries After Import:
```sql
-- Episode table
eid: 81251, name: "Complete Movie", epno: "1"

-- MyList table  
lid: 446487, aid: 5178, eid: 81251, viewed: 1, state: 2

-- Display should show:
Anime: [Title from anime_titles for aid=5178]
  ├─ Episode 1 - Complete Movie
```

## Success Criteria
- ✅ Episode titles are displayed correctly
- ✅ Anime titles are displayed (when anime_titles is populated)
- ✅ Tree structure shows anime > episodes hierarchy
- ✅ No crashes during import
- ✅ Database queries complete successfully

## Regression Testing
- Ensure existing mylist entries (from FILE command) still display correctly
- Ensure mylist add via file hashing still works
- Ensure UI remains responsive during import
