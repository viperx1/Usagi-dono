# Anime Card Tweaks Summary

This document summarizes the changes made to implement the anime card tweaks requested in the issue.

## Changes Implemented

### 1. Poster Size Increase (50%)

**Before:**
- Poster size: 160×220 pixels
- Card size: 500×350 pixels

**After:**
- Poster size: 240×330 pixels (50% increase)
- Card size: 600×450 pixels (scaled up to accommodate)

The poster continues to use `setScaledContents(true)` to automatically scale images while maintaining aspect ratio within the fixed size bounds.

### 2. Tree Widget Column Restructure

**Before (2 columns):**
```
Column 0 (50px): Play button (▶ or ✗)
Column 1 (auto):  Episode/File data
```

**After (3 columns):**
```
Column 0 (30px):  Empty - expand button only (handled by Qt)
Column 1 (50px):  Play button (▶ or ✗)
Column 2 (auto):  Episode/File data
```

## Visual Comparison

### Card Layout
```
BEFORE (500×350):                      AFTER (600×450):
┌────────────────────────────┐        ┌──────────────────────────────┐
│ ┌────┐  Title             │        │ ┌──────┐  Title              │
│ │    │  Type              │        │ │      │  Type               │
│ │160 │  Aired             │        │ │      │  Aired              │
│ │ ×  │  Tags              │        │ │ 240  │  Tags               │
│ │220 │  Episodes          │        │ │  ×   │  Episodes           │
│ └────┘                     │        │ │ 330  │                     │
│                            │        │ │      │                     │
│ [▶] Ep 1: Title           │        │ └──────┘                     │
│  \File details...         │        │                              │
│ [▶] Ep 2: Title           │        │ [ ] [▶] Ep 1: Title         │
└────────────────────────────┘        │      \File details...       │
                                       │ [ ] [▶] Ep 2: Title         │
                                       └──────────────────────────────┘
```

### Tree Widget Column Layout
```
BEFORE (2 columns):
┌──────┬────────────────────────────┐
│  ▶   │ Ep 1: Episode Title       │
│      │  \File details...         │
└──────┴────────────────────────────┘

AFTER (3 columns):
┌────┬──────┬────────────────────────┐
│    │  ▶   │ Ep 1: Episode Title   │ <-- Column 0: expand button area
│    │      │  \File details...     │     Column 1: play button
└────┴──────┴────────────────────────┘     Column 2: data
```

## Technical Details

### File Changes
- **usagi/src/animecard.h**
  - Updated `getCardSize()` from 500×350 to 600×450

- **usagi/src/animecard.cpp**
  - Updated poster label size from 160×220 to 240×330
  - Changed tree widget from 2 to 3 columns
  - Set column widths: 30px (expand), 50px (play button), auto (data)
  - Updated `addEpisode()` to populate all 3 columns correctly:
    - Column 0: Empty (expand button handled by Qt)
    - Column 1: Play button (▶ or ✗)
    - Column 2: Episode/file information
  - Updated data storage to use column 2 for UserRole data (lid, eid, fid)
  - Updated signal handler to read from column 2

### Backward Compatibility
- No database changes required
- No data migration needed
- Existing cards will automatically use the new layout
- Play button functionality preserved

## Benefits

1. **Larger Posters**: 2.25× larger area (50% increase in each dimension) for better visual recognition
2. **Clearer Layout**: Separate columns for expand button, play button, and data
3. **Better UX**: Expand button area is now distinct from the play button area
4. **Consistent Spacing**: Column widths optimized for their purpose

## Testing Checklist

- [ ] Verify card displays at correct size (600×450)
- [ ] Verify poster displays at correct size (240×330)
- [ ] Verify poster scales while maintaining aspect ratio
- [ ] Verify expand button appears in column 0
- [ ] Verify play button appears in column 1
- [ ] Verify episode/file data appears in column 2
- [ ] Verify play button click still works correctly
- [ ] Verify file items display correctly under episodes
- [ ] Verify expand/collapse functionality works
- [ ] Verify tooltips still appear on column 2
