# MyList Display Fix - Visual Flow Diagram

## Problem Flow (Before Fix)

```
┌─────────────────────────────────────────────────────────────────┐
│                    csv-adborg Import File                       │
│  (archive.tar.gz containing anime/*.csv files)                  │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│               parseMylistCSVAdborg()                            │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ Anime Section (IGNORED ❌)                                │ │
│  │ AID|EID|EPNo|Name|Romaji|Kanji|...                        │ │
│  │ 5178|81251|1|Complete Movie|||...                         │ │
│  └───────────────────────────────────────────────────────────┘ │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ FILES Section (PARSED ✅)                                 │ │
│  │ AID|EID|GID|FID|size|ed2k|...                             │ │
│  │ 5178|81251|1412|446487|734003200|...                      │ │
│  └───────────────────────────────────────────────────────────┘ │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Database State                               │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ mylist table: ✅                                          │ │
│  │  lid=446487, aid=5178, eid=81251, viewed=1               │ │
│  └───────────────────────────────────────────────────────────┘ │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ episode table: ❌ EMPTY                                   │ │
│  └───────────────────────────────────────────────────────────┘ │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ anime table: ❌ EMPTY                                     │ │
│  └───────────────────────────────────────────────────────────┘ │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│              loadMylistFromDatabase()                           │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ Query:                                                     │ │
│  │   SELECT m.lid, m.aid, m.eid,                             │ │
│  │          a.nameromaji, a.nameenglish,  ← Returns NULL    │ │
│  │          e.name, e.epno                ← Returns NULL    │ │
│  │   FROM mylist m                                            │ │
│  │   LEFT JOIN anime a ON m.aid = a.aid                      │ │
│  │   LEFT JOIN episode e ON m.eid = e.eid                    │ │
│  └───────────────────────────────────────────────────────────┘ │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                    UI Display (BAD ❌)                          │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ MyList Tab:                                                │ │
│  │ ├─ Anime #5178                ← No anime name!           │ │
│  │ │  └─ EID: 81251              ← No episode name!         │ │
│  │ │     State: CD/DVD                                       │ │
│  │ │     Viewed: Yes                                         │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## Solution Flow (After Fix)

```
┌─────────────────────────────────────────────────────────────────┐
│                    csv-adborg Import File                       │
│  (archive.tar.gz containing anime/*.csv files)                  │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│               parseMylistCSVAdborg() [FIXED]                    │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ Anime Section (NOW PARSED ✅)                             │ │
│  │ AID|EID|EPNo|Name|Romaji|Kanji|...                        │ │
│  │ 5178|81251|1|Complete Movie|||...                         │ │
│  │                                                             │ │
│  │ Extracted:                                                  │ │
│  │   eid = 81251                                              │ │
│  │   epno = "1"                                               │ │
│  │   name = "Complete Movie"                                  │ │
│  │   romaji = ""                                              │ │
│  │   kanji = ""                                               │ │
│  │                                                             │ │
│  │ INSERT INTO episode VALUES (...)  ← NEW! ✅              │ │
│  └───────────────────────────────────────────────────────────┘ │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ FILES Section (STILL PARSED ✅)                           │ │
│  │ AID|EID|GID|FID|size|ed2k|...                             │ │
│  │ 5178|81251|1412|446487|734003200|...                      │ │
│  │                                                             │ │
│  │ INSERT INTO mylist VALUES (...)                            │ │
│  └───────────────────────────────────────────────────────────┘ │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Database State                               │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ mylist table: ✅                                          │ │
│  │  lid=446487, aid=5178, eid=81251, viewed=1               │ │
│  └───────────────────────────────────────────────────────────┘ │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ episode table: ✅ NOW POPULATED                           │ │
│  │  eid=81251, name="Complete Movie", epno="1"              │ │
│  └───────────────────────────────────────────────────────────┘ │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ anime_titles table: ✅ (pre-existing from AniDB)         │ │
│  │  aid=5178, title="Naruto: Shippuden the Movie"           │ │
│  └───────────────────────────────────────────────────────────┘ │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│              loadMylistFromDatabase() [ENHANCED]                │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ Query:                                                     │ │
│  │   SELECT m.lid, m.aid, m.eid,                             │ │
│  │          a.nameromaji, a.nameenglish,  ← Returns NULL    │ │
│  │          e.name, e.epno,               ← Returns data ✅ │ │
│  │          (SELECT title FROM            ← NEW! Fallback ✅│ │
│  │           anime_titles                                     │ │
│  │           WHERE aid = m.aid) as anime_title                │ │
│  │   FROM mylist m                                            │ │
│  │   LEFT JOIN anime a ON m.aid = a.aid                      │ │
│  │   LEFT JOIN episode e ON m.eid = e.eid                    │ │
│  │                                                             │ │
│  │ Fallback Logic:                                            │ │
│  │   if (a.nameromaji is not empty)                           │ │
│  │       use a.nameromaji                                     │ │
│  │   else if (a.nameenglish is not empty)                     │ │
│  │       use a.nameenglish                                    │ │
│  │   else if (anime_title is not empty)  ← NEW! ✅          │ │
│  │       use anime_title                                      │ │
│  │   else                                                      │ │
│  │       use "Anime #[aid]"                                   │ │
│  └───────────────────────────────────────────────────────────┘ │
└────────────────────┬────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                    UI Display (GOOD ✅)                         │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ MyList Tab:                                                │ │
│  │ ├─ Naruto: Shippuden the Movie  ← From anime_titles! ✅ │ │
│  │ │  └─ Episode 1 - Complete Movie ← From episode table! ✅││
│  │ │     State: CD/DVD                                       │ │
│  │ │     Viewed: Yes                                         │ │
│  │ │     Storage:                                            │ │
│  │ │     Mylist ID: 446487                                   │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## Key Changes Summary

### Change 1: Parse Episode Data
```
Location: parseMylistCSVAdborg() - lines 1110-1157
Action:   Added parsing of anime section (episode data)
Input:    AID|EID|EPNo|Name|Romaji|Kanji|...
Output:   INSERT INTO episode (eid, name, nameromaji, namekanji, epno)
Effect:   episode table now populated after import
```

### Change 2: Use anime_titles Fallback
```
Location: loadMylistFromDatabase() - lines 893-941
Action:   Enhanced query to include anime_titles
Input:    SELECT ... (SELECT title FROM anime_titles ...) ...
Output:   Anime name from anime_titles when anime table is empty
Effect:   Anime names now displayed correctly
```

## Data Dependencies

```
┌──────────────────────────────────────────────────────────┐
│                    Data Sources                          │
└──────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────────┐
    │ csv-adborg│    │  AniDB   │    │    AniDB     │
    │  import  │    │   FILE   │    │ Anime Titles │
    │   .tgz   │    │ command  │    │   Database   │
    └─────┬────┘    └─────┬────┘    └──────┬───────┘
          │               │                 │
          ▼               ▼                 ▼
    ┌──────────┐    ┌──────────┐    ┌──────────────┐
    │ episode  │    │  anime   │    │anime_titles  │
    │  table   │    │  table   │    │    table     │
    │  (NEW!)  │    │(optional)│    │  (fallback)  │
    └─────┬────┘    └─────┬────┘    └──────┬───────┘
          │               │                 │
          └───────────────┼─────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │ loadMylistFromDatabase│
              │       (display)       │
              └───────────────────────┘
```

## State Transitions

### Episode Display
```
Before:
  Input:  eid = 81251
  Lookup: episode table (empty)
  Output: "EID: 81251" ❌

After:
  Input:  eid = 81251
  Lookup: episode table → name="Complete Movie", epno="1"
  Output: "Episode 1 - Complete Movie" ✅
```

### Anime Display
```
Before:
  Input:  aid = 5178
  Lookup: anime table (empty)
  Output: "Anime #5178" ❌

After:
  Input:  aid = 5178
  Lookup: anime table (empty) → anime_titles table
  Output: "Naruto: Shippuden the Movie" ✅
```

## Code Flow Comparison

### Before (Simplified)
```cpp
// In parseMylistCSVAdborg
for each line in CSV:
    if line == "FILES":
        inFilesSection = true
    if inFilesSection:
        parse file data
        INSERT INTO mylist

// In loadMylistFromDatabase
SELECT m.lid, a.nameromaji, e.name
FROM mylist m
LEFT JOIN anime a        // Returns NULL
LEFT JOIN episode e      // Returns NULL
```

### After (Simplified)
```cpp
// In parseMylistCSVAdborg
for each line in CSV:
    if line == "FILES":
        inAnimeSection = false
        inFilesSection = true
    if inAnimeSection:        // NEW!
        parse episode data    // NEW!
        INSERT INTO episode   // NEW!
    if inFilesSection:
        parse file data
        INSERT INTO mylist

// In loadMylistFromDatabase
SELECT m.lid, 
       a.nameromaji, 
       e.name,                                    // Returns data!
       (SELECT title FROM anime_titles           // NEW! Fallback
        WHERE aid = m.aid) as anime_title
FROM mylist m
LEFT JOIN anime a        // Returns NULL
LEFT JOIN episode e      // Returns data!
```
