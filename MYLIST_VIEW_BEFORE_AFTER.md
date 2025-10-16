# MyList View: Before and After Comparison

## Issue Requirements
The issue requested the following improvements:
1. Episode column should show episode number or episode type (not EID)
2. Add episode title as a separate column
3. Keep anime entries collapsed by default

## Before Changes

### Column Layout (6 columns)
```
┌──────────────────────┬──────────────────────────┬────────┬────────┬──────────┬───────────┐
│ Anime                │ Episode                  │ State  │ Viewed │ Storage  │ Mylist ID │
├──────────────────────┼──────────────────────────┼────────┼────────┼──────────┼───────────┤
│ ▼ Cowboy Bebop       │                          │        │        │          │           │
│   │                  │ Episode 1 - Asteroid ... │ HDD    │ Yes    │ /anime.. │ 12345     │
│   │                  │ Episode 2 - Stray Dog... │ HDD    │ Yes    │ /anime.. │ 12346     │
│ ▼ Ghost in Shell SAC │                          │        │        │          │           │
│   │                  │ Episode 1 - Public Sec..│ HDD    │ No     │ /anime.. │ 12347     │
│   │                  │ Episode S1 - Tachikoma..│ HDD    │ No     │ /anime.. │ 12348     │
└──────────────────────┴──────────────────────────┴────────┴────────┴──────────┴───────────┘
```

### Problems
1. ❌ Episode column combined number and title - hard to scan
2. ❌ Episode title was truncated due to space constraints
3. ❌ All anime entries expanded by default - visual clutter
4. ❌ Special episodes showed "Episode S1" instead of "Special 1"

## After Changes

### Column Layout (7 columns)
```
┌──────────────────────┬─────────┬──────────────────────┬────────┬────────┬──────────┬───────────┐
│ Anime                │ Episode │ Episode Title        │ State  │ Viewed │ Storage  │ Mylist ID │
├──────────────────────┼─────────┼──────────────────────┼────────┼────────┼──────────┼───────────┤
│ ▶ Cowboy Bebop       │         │                      │        │        │          │           │
│ ▶ Ghost in Shell SAC │         │                      │        │        │          │           │
│ ▼ Neon Genesis Evan. │         │                      │        │        │          │           │
│   │                  │ 1       │ Angel Attack         │ HDD    │ Yes    │ /anime.. │ 12345     │
│   │                  │ 2       │ The Beast            │ HDD    │ Yes    │ /anime.. │ 12346     │
│   │                  │ 3       │ A Transfer           │ HDD    │ No     │ /anime.. │ 12347     │
│   │                  │ Special 1│ Director's Cut EP 21│ HDD    │ No     │ /anime.. │ 12348     │
│   │                  │ Credit 1│ Cruel Angel's Thesis│ HDD    │ Yes    │ /anime.. │ 12349     │
│ ▶ Steins;Gate        │         │                      │        │        │          │           │
└──────────────────────┴─────────┴──────────────────────┴────────┴────────┴──────────┴───────────┘
```

### Improvements
1. ✅ Episode number/type in dedicated, narrow column
2. ✅ Full episode title visible in wider dedicated column
3. ✅ Anime entries collapsed by default - cleaner view
4. ✅ Special episodes clearly marked: "Special 1", "Credit 1", "Trailer 1", etc.
5. ✅ Easy to identify episode types at a glance
6. ✅ Better use of screen space

## Episode Type Detection

The system now intelligently detects and formats different episode types:

| AniDB Format | Old Display      | New Display  | Description           |
|--------------|------------------|--------------|-----------------------|
| `1`, `2`, `3`| Episode 1        | 1            | Regular episodes      |
| `S1`, `S2`   | Episode S1       | Special 1    | Special episodes      |
| `C1`, `C2`   | Episode C1       | Credit 1     | Credits/OP/ED         |
| `T1`, `T2`   | Episode T1       | Trailer 1    | Trailers/PVs          |
| `P1`, `P2`   | Episode P1       | Parody 1     | Parody episodes       |
| `O1`, `O2`   | Episode O1       | Other 1      | Other special content |
| (no epno)    | EID: 123456      | 123456       | Fallback to episode ID|

## User Benefits

### Visual Clarity
- **Before**: Combined episode info required reading full text
- **After**: Episode type visible at a glance in narrow column

### Screen Space
- **Before**: Long combined strings got truncated
- **After**: Full episode titles visible, better column width distribution

### Navigation
- **Before**: All anime expanded = long scrolling required
- **After**: Collapsed by default = see more anime at once

### Sorting
- **Before**: Sorting by episode meant sorting by the full string "Episode 1 - Title"
- **After**: Can sort by episode number independently of title

## Technical Implementation

### Column Width Distribution
```
Before (total ~910px):
  Anime:      300px (32.9%)
  Episode:    150px (16.5%) - wasted space
  State:      100px (10.9%)
  Viewed:      80px (8.8%)
  Storage:    200px (22.0%)
  Mylist ID:   80px (8.8%)

After (total ~1010px):
  Anime:      300px (29.7%)
  Episode:     80px (7.9%)  - optimized
  Ep. Title:  250px (24.8%) - new column
  State:      100px (9.9%)
  Viewed:      80px (7.9%)
  Storage:    200px (19.8%)
  Mylist ID:   80px (7.9%)
```

### Code Changes
- Changed column count from 6 to 7
- Added episode type detection logic (S→Special, C→Credit, etc.)
- Separated episode number from title display
- Removed automatic expansion of anime entries
- Updated all column indices in setText() calls

## Compatibility
- ✅ No database schema changes required
- ✅ Works with existing mylist data
- ✅ Backward compatible with csv-adborg imports
- ✅ No changes to data storage, only display

## Future Enhancements
Potential improvements based on user feedback:
- Column visibility preferences
- Save/restore expand/collapse state per anime
- Filter by episode type
- Custom sorting options
- Episode type color coding
