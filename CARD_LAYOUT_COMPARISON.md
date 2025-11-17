# Anime Card Layout Comparison

This document provides a detailed visual comparison of the before and after states of the anime card layout changes.

## Card Size Comparison

### Before
```
Card: 500×350 pixels
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  ┌──────┐  Anime Title (Bold, 12pt)                        │
│  │      │  Type: TV Series                                 │
│  │ 160  │  Aired: 2020-01-01 to 2020-03-31               │
│  │  ×   │  Tags: Action, Fantasy, Adventure               │
│  │ 220  │  Episodes: 12/13+2 | Viewed: 10/12+1           │
│  │      │                                                  │
│  └──────┘                                                   │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ [▶]  Ep 1: First Episode Title                        │ │
│  │       \File: 1080p [GroupName]                        │ │
│  │ [▶]  Ep 2: Second Episode Title                       │ │
│  │       \File: 720p [GroupName]                         │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### After
```
Card: 600×450 pixels
┌────────────────────────────────────────────────────────────────────┐
│                                                                    │
│  ┌──────────┐  Anime Title (Bold, 12pt)                          │
│  │          │  Type: TV Series                                   │
│  │          │  Aired: 2020-01-01 to 2020-03-31                  │
│  │   240    │  Tags: Action, Fantasy, Adventure                 │
│  │    ×     │  Episodes: 12/13+2 | Viewed: 10/12+1             │
│  │   330    │                                                    │
│  │          │                                                    │
│  │          │                                                    │
│  └──────────┘                                                     │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │ [ ] [▶]  Ep 1: First Episode Title                          │ │
│  │          \File: 1080p [GroupName]                           │ │
│  │ [ ] [▶]  Ep 2: Second Episode Title                         │ │
│  │          \File: 720p [GroupName]                            │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

## Tree Widget Column Structure

### Before (2 Columns)
```
Column Layout:
┌─────────┬──────────────────────────────────┐
│ Col 0   │ Col 1                            │
│ (50px)  │ (auto)                           │
├─────────┼──────────────────────────────────┤
│   ▶     │ Ep 1: Episode Title              │
├─────────┼──────────────────────────────────┤
│   ▶     │   \File: 1080p [Group]          │
├─────────┼──────────────────────────────────┤
│   ✗     │   \File: Missing                │
└─────────┴──────────────────────────────────┘

Play button and data are in adjacent columns.
Expand button overlaps with play button area.
```

### After (3 Columns)
```
Column Layout:
┌────┬─────────┬──────────────────────────────────┐
│C0  │ Col 1   │ Col 2                            │
│(30)│ (50px)  │ (auto)                           │
├────┼─────────┼──────────────────────────────────┤
│    │   ▶     │ Ep 1: Episode Title              │
├────┼─────────┼──────────────────────────────────┤
│    │   ▶     │   \File: 1080p [Group]          │
├────┼─────────┼──────────────────────────────────┤
│    │   ✗     │   \File: Missing                │
└────┴─────────┴──────────────────────────────────┘

Expand button has dedicated column (handled by Qt).
Play button has its own column.
Data is clearly separated in column 2.
```

## Poster Size Increase

### Dimension Comparison
```
Before: 160×220 pixels
┌──────┐
│      │
│      │
│      │
│      │
│      │
│      │
│      │
└──────┘

After: 240×330 pixels (50% increase in each dimension)
┌──────────┐
│          │
│          │
│          │
│          │
│          │
│          │
│          │
│          │
│          │
│          │
│          │
└──────────┘

Area increase: 2.25× (160×220 = 35,200 → 240×330 = 79,200 pixels)
```

## Interaction Areas

### Before
```
Click Areas:
┌────────────────────────────────┐
│ [Poster Area - Click for Info]│ ← Card click (not on tree)
├────────────────────────────────┤
│ [▶│Episode Info]               │ ← Play button + episode
│    └─────────────────────────┘ │    (overlapping areas)
└────────────────────────────────┘
```

### After
```
Click Areas:
┌────────────────────────────────┐
│ [Poster Area - Click for Info]│ ← Card click (not on tree)
├────────────────────────────────┤
│ [│▶│Episode Info]              │ ← Expand | Play | Episode
│  │ │ └──────────────────────┘ │    (distinct areas)
└──┴─┴────────────────────────────┘
```

## Column Width Allocation

### Before (500px card width)
```
Total tree width: ~490px (with margins)

┌──────┬────────────────────────────────┐
│ 50px │ ~440px                         │
│      │                                │
│ Play │ Episode/File Data              │
└──────┴────────────────────────────────┘

Play button: 10.2% of width
Data area: 89.8% of width
```

### After (600px card width)
```
Total tree width: ~590px (with margins)

┌────┬──────┬──────────────────────────────┐
│30px│ 50px │ ~510px                       │
│    │      │                              │
│Exp │ Play │ Episode/File Data            │
└────┴──────┴──────────────────────────────┘

Expand area: 5.1% of width
Play button: 8.5% of width
Data area: 86.4% of width
```

## Benefits Summary

| Aspect                 | Before    | After     | Improvement        |
|------------------------|-----------|-----------|-------------------|
| Card Size              | 500×350   | 600×450   | +20% width, +29% height |
| Poster Size            | 160×220   | 240×330   | +50% each dimension |
| Poster Area            | 35,200 px²| 79,200 px²| +125% (2.25×)     |
| Tree Columns           | 2         | 3         | Better organization |
| Expand Button Column   | Shared    | Dedicated | No overlap        |
| Play Button Column     | 50px      | 50px      | Maintained        |
| Data Column Width      | ~440px    | ~510px    | +16%              |

## Implementation Details

### Code Changes
1. **animecard.h**: Updated `getCardSize()` return value
2. **animecard.cpp**: 
   - Updated `m_posterLabel->setFixedSize()`
   - Changed `setColumnCount(2)` to `setColumnCount(3)`
   - Added `setColumnWidth(0, 30)` for expand column
   - Updated all column indices in `addEpisode()` function
   - Moved play button delegate to column 1
   - Moved data storage/retrieval to column 2

### Backward Compatibility
- ✅ No database schema changes
- ✅ No data migration required
- ✅ Existing functionality preserved
- ✅ All signals/slots still work

## Testing Notes

When testing in Qt6 environment, verify:
1. Card displays at 600×450 pixels
2. Poster displays at 240×330 pixels
3. Poster scales maintaining aspect ratio
4. Tree has 3 visible columns
5. Expand button (▶/▼) appears in leftmost narrow column
6. Play button (▶/✗) appears in middle column
7. Episode/file text appears in rightmost column
8. Clicking play button triggers episodeClicked signal
9. Expand/collapse works correctly
10. Tooltips appear on episode/file text (column 2)
