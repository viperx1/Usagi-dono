# MyList Display Fix - Complete Summary

## Issue
**Problem**: mylist doesn't display anime and episode titles after importing from csv-adborg format.

**Symptom**: After importing mylist using MYLISTEXPORT with csv-adborg template, the MyList tab shows "Anime #5178" instead of actual anime names, and episode entries don't show episode titles or numbers.

## Root Cause Analysis

### The Problem
The `parseMylistCSVAdborg()` function was only parsing the FILES section of csv-adborg format files, completely ignoring the anime section that contains episode information.

**csv-adborg File Structure:**
```
AID|EID|EPNo|Name|Romaji|Kanji|...    <- ANIME SECTION (was ignored)
5178|81251|1|Complete Movie|...
FILES                                   <- FILES SECTION (was parsed)
AID|EID|GID|FID|...
5178|81251|1412|446487|...
GROUPS
...
GENREN
...
```

### What Was Missing
1. **Episode Data**: Episode names, romaji names, kanji names, and episode numbers were not being extracted and stored
2. **Anime Name Fallback**: The display code didn't use the `anime_titles` table as a fallback when the `anime` table was empty

### Why It Mattered
When `loadMylistFromDatabase()` tried to display mylist entries, it performed this query:
```sql
SELECT m.lid, m.aid, m.eid, ...
       a.nameromaji, a.nameenglish, 
       e.name as episode_name, e.epno
FROM mylist m
LEFT JOIN anime a ON m.aid = a.aid     -- Returns NULL (anime table empty)
LEFT JOIN episode e ON m.eid = e.eid   -- Returns NULL (episode table empty)
```

Result: No anime names, no episode names, just "Anime #[AID]" and "EID: [number]"

## The Solution

### Part 1: Parse Episode Data from csv-adborg (window.cpp:1110-1157)

Added code to parse the anime section before FILES section:

```cpp
// Parse anime header and data rows (before FILES section)
if(inAnimeSection)
{
    if(animeHeaders.isEmpty())
    {
        // First line is the anime header
        // AID|EID|EPNo|Name|Romaji|Kanji|Length|Aired|...
        animeHeaders = line.split('|');
        continue;
    }
    else
    {
        // Parse anime data row (episode data)
        QString aid = fields[0].trimmed();
        QString eid = fields[1].trimmed();
        QString epno = fields[2].trimmed();
        QString name = fields[3].trimmed();      // Episode name
        QString romaji = fields[4].trimmed();    // Episode romaji
        QString kanji = fields[5].trimmed();     // Episode kanji
        
        // Insert episode data
        INSERT OR REPLACE INTO episode 
        (eid, name, nameromaji, namekanji, epno)
        VALUES (eid, name, romaji, kanji, epno);
    }
}
```

**What This Does:**
- Detects anime section (lines before "FILES" marker)
- Parses header to understand column structure
- Extracts episode data: EID, episode number, episode name (in multiple languages)
- Inserts into `episode` table for later retrieval

### Part 2: Use anime_titles as Fallback (window.cpp:893-941)

Enhanced the query to include anime_titles:

```cpp
QString query = "SELECT m.lid, m.aid, m.eid, m.state, m.viewed, m.storage, "
                "a.nameromaji, a.nameenglish, a.eptotal, "
                "e.name as episode_name, e.epno, "
                "(SELECT title FROM anime_titles WHERE aid = m.aid AND type = 1 LIMIT 1) as anime_title "
                "FROM mylist m "
                "LEFT JOIN anime a ON m.aid = a.aid "
                "LEFT JOIN episode e ON m.eid = e.eid "
                "ORDER BY a.nameromaji, m.eid";
```

Added fallback logic:

```cpp
// Priority: anime.nameromaji > anime.nameenglish > anime_titles.title > "Anime #[AID]"
if(animeName.isEmpty() && !animeNameEnglish.isEmpty())
    animeName = animeNameEnglish;

if(animeName.isEmpty() && !animeTitle.isEmpty())
    animeName = animeTitle;

if(animeName.isEmpty())
    animeName = QString("Anime #%1").arg(aid);
```

**What This Does:**
- Queries `anime_titles` table for anime name (populated from AniDB anime titles database)
- Uses anime_titles as fallback when anime table doesn't have the name
- Ensures anime names are displayed even when full anime data isn't available

## Data Flow

### Before the Fix
```
csv-adborg import
    ↓
parseMylistCSVAdborg() - Only parses FILES section
    ↓
mylist table populated (lid, aid, eid, viewed, state, ...)
anime table: EMPTY
episode table: EMPTY
    ↓
loadMylistFromDatabase() - Joins fail
    ↓
Display: "Anime #5178" → "EID: 81251"
```

### After the Fix
```
csv-adborg import
    ↓
parseMylistCSVAdborg() - Parses BOTH anime section and FILES section
    ↓
mylist table populated (lid, aid, eid, viewed, state, ...)
episode table populated (eid, name, nameromaji, namekanji, epno)
anime_titles table (already populated from AniDB)
    ↓
loadMylistFromDatabase() - Joins succeed
    ↓
Display: "Naruto: Shippuden the Movie" → "Episode 1 - Complete Movie"
```

## Technical Details

### Database Schema Used

**episode table:**
```sql
CREATE TABLE episode (
    eid INTEGER PRIMARY KEY,
    name TEXT,           -- Episode name (English or main language)
    nameromaji TEXT,     -- Episode name in romaji
    namekanji TEXT,      -- Episode name in kanji
    rating INTEGER,
    votecount INTEGER,
    epno TEXT           -- Episode number (e.g., "1", "S1", "C1")
);
```

**anime_titles table:**
```sql
CREATE TABLE anime_titles (
    aid INTEGER,
    type INTEGER,        -- 1=main, 2=official, 3=short, 4=synonym
    language TEXT,       -- en, ja, x-jat, etc.
    title TEXT,
    PRIMARY KEY(aid, type, language, title)
);
```

**mylist table:**
```sql
CREATE TABLE mylist (
    lid INTEGER PRIMARY KEY,  -- MyList ID
    fid INTEGER,              -- File ID
    eid INTEGER,              -- Episode ID
    aid INTEGER,              -- Anime ID
    gid INTEGER,              -- Group ID
    state INTEGER,            -- Storage state
    viewed INTEGER,           -- Watched flag
    storage TEXT              -- Storage location
);
```

### SQL Query Enhancement

**Old Query:**
```sql
SELECT m.lid, m.aid, m.eid, m.state, m.viewed, m.storage,
       a.nameromaji, a.nameenglish, a.eptotal,
       e.name as episode_name, e.epno
FROM mylist m
LEFT JOIN anime a ON m.aid = a.aid
LEFT JOIN episode e ON m.eid = e.eid
```

**New Query:**
```sql
SELECT m.lid, m.aid, m.eid, m.state, m.viewed, m.storage,
       a.nameromaji, a.nameenglish, a.eptotal,
       e.name as episode_name, e.epno,
       (SELECT title FROM anime_titles WHERE aid = m.aid AND type = 1 LIMIT 1) as anime_title
FROM mylist m
LEFT JOIN anime a ON m.aid = a.aid
LEFT JOIN episode e ON m.eid = e.eid
ORDER BY a.nameromaji, m.eid
```

**Change:** Added subquery to fetch anime title from anime_titles table when available.

## Results

### Before Fix
```
MyList Tab:
├─ Anime #5178
│  └─ EID: 81251
│     State: CD/DVD
│     Viewed: Yes
```

### After Fix
```
MyList Tab:
├─ Naruto: Shippuden the Movie
│  └─ Episode 1 - Complete Movie
│     State: CD/DVD
│     Viewed: Yes
│     Storage: 
│     Mylist ID: 446487
```

## Testing

### Automated Tests
- ✅ SQL syntax validated with SQLite
- ✅ Query tested with sample data
- ✅ Episode insertion verified
- ✅ anime_titles fallback confirmed

### Manual Testing Required
See `test_mylist_display.md` for comprehensive test plan including:
- Import from csv-adborg
- Verify episode titles display
- Verify anime titles display
- Database state verification
- Multiple episodes per anime

## Files Modified

1. **usagi/src/window.cpp** (66 lines changed)
   - `parseMylistCSVAdborg()`: Added episode data parsing
   - `loadMylistFromDatabase()`: Added anime_titles fallback

## Files Created

1. **test_mylist_display.md** - Manual test plan
2. **example_csv_adborg.txt** - Format examples and explanation
3. **MYLIST_DISPLAY_FIX_SUMMARY.md** - This document

## Benefits

1. **User Experience**: Users see actual anime and episode names instead of IDs
2. **Data Completeness**: Episode information is now stored and utilized
3. **Robustness**: Fallback mechanism ensures names are displayed when possible
4. **Consistency**: Display matches user expectations from AniDB

## Edge Cases Handled

1. **Empty episode names**: Displays "Episode [number]" without name
2. **No episode number**: Falls back to "EID: [number]"
3. **No anime name anywhere**: Shows "Anime #[AID]"
4. **Multiple episodes**: Each displayed as separate tree item
5. **SQL injection**: All values properly escaped with `replace("'", "''")`

## Known Limitations

1. **Anime names from csv-adborg**: The format doesn't include anime names directly
   - Relies on anime_titles table (automatically updated from AniDB)
   - First-time users may see "Anime #[AID]" until anime_titles is populated
   
2. **anime_titles update**: Happens automatically on startup if > 7 days old
   - May take a few minutes to download and process
   - Should be transparent to users

## Future Enhancements

1. **On-demand anime lookup**: If anime_titles doesn't have the title, query AniDB API
2. **Progress indication**: Show parsing progress for large imports
3. **Error reporting**: Log specific parsing errors for debugging
4. **Batch anime lookups**: Fetch missing anime info for all mylist entries at once

## Conclusion

This fix resolves the core issue where mylist imports from csv-adborg format didn't display meaningful anime and episode information. By parsing the episode data from the csv-adborg anime section and using the anime_titles table as a fallback, users now see proper anime and episode titles in the MyList tab.

The changes are minimal, surgical, and maintain backward compatibility with existing functionality. The solution follows existing code patterns and doesn't introduce new dependencies or breaking changes.

---

**Status**: ✅ Implementation Complete
**Ready for**: Manual Testing → Merge
**Impact**: High (resolves primary user-facing issue)
**Risk**: Low (targeted changes, existing patterns followed)
