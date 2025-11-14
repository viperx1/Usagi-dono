# Card View Tweaks - Visual Comparison

This document provides a before/after comparison of the card view changes.

## Card Size & Layout

### BEFORE
```
Card Size: 400×300 pixels
┌─────────────────────────────────────────┐
│ ┌──┐  Title (Bold, 12pt)              │
│ │  │  Type: TV Series                 │
│ │80│  Aired: 2023-01-01 - 2023-03-31 │
│ │× │  Episodes: 12/13 | Viewed: 10/12│ (OLD FORMAT)
│ │110                                   │
│ └──┘                                   │
│                                         │
│ ┌─────────────────────────────────────┐ │
│ │ ▶ Ep 1: Episode Title              │ │
│ │  \File details...                  │ │  PLAY BUTTON
│ │ ▶ Ep 2: Episode Title              │ │  BARELY VISIBLE
│ └─────────────────────────────────────┘ │  (35px column)
└─────────────────────────────────────────┘
```

### AFTER
```
Card Size: 500×350 pixels (LARGER)
┌──────────────────────────────────────────────────┐
│ ┌────┐  Title (Bold, 12pt)                      │
│ │    │  Type: TV Series                         │
│ │    │  Aired: 2023-01-01 - 2023-03-31         │
│ │160 │  Tags: Action, Fantasy, Adventure (NEW) │
│ │ ×  │  Episodes: 12/13+2 | Viewed: 10/12+1    │ (NEW FORMAT)
│ │220 │                                          │
│ │    │                                          │
│ └────┘                                          │
│                                                  │
│ ┌────────────────────────────────────────────┐  │
│ │ [▶] Ep 1: Episode Title                   │  │
│ │      \File details...                     │  │  PLAY BUTTON
│ │ [▶] Ep 2: Episode Title                   │  │  CLEARLY VISIBLE
│ └────────────────────────────────────────────┘  │  (50px column)
└──────────────────────────────────────────────────┘
```

## Statistics Display Format

### BEFORE (Simple counts)
```
Episodes: 12/13 | Viewed: 10/12
         ↓    ↓             ↓    ↓
      in list total      viewed  in list
```

**Problems:**
- Doesn't distinguish normal episodes from specials
- Can't see if you have special episodes
- Inconsistent with tree widget display

### AFTER (Tree widget format)
```
Episodes: 12/13+2 | Viewed: 10/12+1
         ↓    ↓  ↓           ↓    ↓  ↓
      normal total other  normal in other
      in list normal      viewed list viewed
                          normal

When no total known:
Episodes: 12/?+2 | Viewed: 10/12+1
```

**Benefits:**
- Matches tree widget exactly
- Shows special episodes at a glance
- Consistent user experience
- More informative

## Episode Type Breakdown

### Episode Types
- **Type 1**: Normal episodes (1, 2, 3, ...)
- **Type 2**: Special episodes (S1, S2, ...)
- **Type 3**: Credits (C1, C2, ...)
- **Type 4**: Trailers (T1, T2, ...)
- **Type 5**: Parodies (P1, P2, ...)
- **Type 6**: Other (O1, O2, ...)

### Example Statistics
```
Anime with mixed episodes:
- 13 normal episodes (1-13)
- 2 special episodes (S1, S2)
- 1 credit (C1)

Display:
Episodes: 13/13+3  (13 normal of 13 total, plus 3 other)
Viewed: 10/13+2    (10 normal viewed, 2 other viewed)
```

## Play Button Improvements

### BEFORE
```
Column: 35px width
Margins: (2,2,-2,-2)
Size: 40px × auto

Result: Button barely visible
┌─────────┐
│ ▶       │  ← Button pushed into corner
└─────────┘
```

### AFTER
```
Column: 50px width (WIDER)
Margins: (1,1,-1,-1) (LESS AGGRESSIVE)
Size: 48px × 24px (MINIMUM)

Result: Button clearly visible
┌──────────┐
│   [▶]    │  ← Button centered and visible
└──────────┘
```

## Tags Display

### NEW Feature
```
Tags/Categories shown below aired date:
┌────────────────────────────────────┐
│ Type: TV Series                    │
│ Aired: 2023-01-01 - 2023-03-31    │
│ Tags: Action, Fantasy, Adventure   │ ← NEW
│ Episodes: 12/13+2 | Viewed: 10/12+1│
└────────────────────────────────────┘

Styling:
- Font: 8pt italic
- Color: #888 (gray)
- Word wrap: enabled
- Max height: 30px
- Only shown when data available
```

## Poster Size Comparison

### Visual Scale
```
BEFORE (80×110):
┌────┐
│    │
│    │
│    │
└────┘

AFTER (160×220):
┌────────┐
│        │
│        │
│        │
│        │
│        │
│        │
└────────┘
(Exactly 2× in both dimensions)
```

## Card Width Adjustment

### Layout Comparison
```
BEFORE (400px):
┌──┬──────────────────┐
│80│ Info (320px)    │
└──┴──────────────────┘

AFTER (500px):
┌────┬────────────────────┐
│160 │ Info (340px)      │
└────┴────────────────────┘

Net info area increase: 20px
(340 - 320 = 20px more for text)
```

## Practical Examples

### Example 1: Complete Anime
```
BEFORE: Episodes: 13/13 | Viewed: 13/13
AFTER:  Episodes: 13/13 | Viewed: 13/13
(No change when only normal episodes)
```

### Example 2: Anime with Specials
```
BEFORE: Episodes: 15/13 | Viewed: 14/15
                  ↑↑↑↑    (Confusing!)
          Looks like more than total?

AFTER:  Episodes: 13/13+2 | Viewed: 12/13+2
                          (Clear!)
          13 normal + 2 specials
```

### Example 3: Incomplete Data
```
BEFORE: Episodes: 5/0 | Viewed: 3/5
                    ↑ (Shows 0 total)

AFTER:  Episodes: 5/? | Viewed: 3/5
                    ↑ (Shows unknown)
```

### Example 4: With Tags
```
BEFORE:
Type: TV Series
Aired: 2023-01-01 - 2023-03-31
Episodes: 12/13 | Viewed: 10/12

AFTER:
Type: TV Series
Aired: 2023-01-01 - 2023-03-31
Tags: Action, Fantasy, Shounen
Episodes: 12/13+2 | Viewed: 10/12+1
```

## Summary of Visual Changes

| Aspect | Before | After | Improvement |
|--------|--------|-------|-------------|
| Card Size | 400×300 | 500×350 | +25% width, +17% height |
| Poster Size | 80×110 | 160×220 | 2× larger (4× area) |
| Play Button Column | 35px | 50px | +43% wider |
| Play Button Size | 40×auto | 48×24 | Better proportions |
| Episode Format | Simple | A/B+C | More informative |
| Tags | None | Shown | New feature |
| Info Area | 320px | 340px | +6% more space |

## User Experience Benefits

1. **Larger Posters**: Better visual recognition of anime
2. **Tags Display**: Quick understanding of anime genre/themes
3. **Clear Statistics**: Matches tree widget, shows special episodes
4. **Visible Buttons**: Easy to click and interact with
5. **More Space**: Better layout with larger card size
6. **Consistency**: Card view now matches tree widget format

## Migration Notes

- No user action required
- No data migration needed
- Existing cards automatically use new format
- Tree view unaffected
- Can toggle between views anytime
