# MyList Card View - Visual Mockup

## Overview
This document provides a visual representation of the new card-based layout for the MyList tab.

## Card View Layout

```
╔═══════════════════════════════════════════════════════════════════════════════════════════════════╗
║                                    USAGI - MyList Tab (Card View)                                 ║
╠═══════════════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                                   ║
║  [ Switch to Tree View ]  Sort by: [ Anime Title ▼ ]                                            ║
║                                                                                                   ║
║  ┌───────────────────────┐ ┌───────────────────────┐ ┌───────────────────────┐                 ║
║  │ ┌───┬─────────────────┤ │ ┌───┬─────────────────┤ │ ┌───┬─────────────────┤                 ║
║  │ │   │ Cowboy Bebop    │ │ │   │ Death Note      │ │ │   │ Attack on Titan │                 ║
║  │ │ ? │ Type: TV Series │ │ │ ? │ Type: TV Series │ │ │ ? │ Type: TV Series │                 ║
║  │ │   │ Aired: 1998-1999│ │ │   │ Aired: 2006-2007│ │ │   │ Aired: 2013     │                 ║
║  │ └───┤ Episodes: 5/26  │ │ └───┤ Episodes: 37/37 │ │ └───┤ Episodes: 12/25 │                 ║
║  │     │ Viewed: 3/5     │ │     │ Viewed: 37/37   │ │     │ Viewed: 8/12    │                 ║
║  │     ├─────────────────┤ │     ├─────────────────┤ │     ├─────────────────┤                 ║
║  │     │ Ep 1: Asteroid  │ │     │ Ep 1: Rebirth ✓ │ │     │ Ep 1: To You... ✓│                ║
║  │     │   Blues ✓       │ │     │ Ep 2: Confronta │ │     │ Ep 2: That Day  ✓│                ║
║  │     │ Ep 2: Stray Dog │ │     │   -tion ✓       │ │     │ ...              │                 ║
║  │     │   Strut ✓       │ │     │ ...              │ │     │ Ep 12: Scream   │                 ║
║  │     │ Ep 5: Ballad of │ │     │ Ep 37: New      │ │     │   [HDD]          │                 ║
║  │     │   Fallen... ✓   │ │     │   World ✓       │ │     │                  │                 ║
║  │     │ Ep 10: Ganymede │ │     │                  │ │     │                  │                 ║
║  │     │   Elegy [HDD]   │ │     │                  │ │     │                  │                 ║
║  │     │ Ep 13: Jupiter  │ │     │                  │ │     │                  │                 ║
║  │     │   Jazz (P1)     │ │     │                  │ │     │                  │                 ║
║  └─────┴─────────────────┘ └─────┴─────────────────┘ └─────┴─────────────────┘                 ║
║                                                                                                   ║
║  ┌───────────────────────┐ ┌───────────────────────┐                                            ║
║  │ ┌───┬─────────────────┤ │ ┌───┬─────────────────┤                                            ║
║  │ │   │ Steins;Gate     │ │ │   │ Fullmetal       │                                            ║
║  │ │ ? │ Type: TV Series │ │ │ ? │   Alchemist     │                                            ║
║  │ │   │ Aired: 2011     │ │ │   │ Type: TV Series │                                            ║
║  │ └───┤ Episodes: 24/24 │ │ └───┤ Aired: 2009-2010│                                            ║
║  │     │ Viewed: 24/24   │ │     │ Episodes: 64/64 │                                            ║
║  │     ├─────────────────┤ │     │ Viewed: 50/64   │                                            ║
║  │     │ Ep 1: Turning   │ │     ├─────────────────┤                                            ║
║  │     │   Point ✓       │ │     │ Ep 1: Fullmetal │                                            ║
║  │     │ Ep 2: Time      │ │     │   Alchemist ✓   │                                            ║
║  │     │   Travel ✓      │ │     │ Ep 2: The First │                                            ║
║  │     │ ...              │ │     │   Day ✓         │                                            ║
║  │     │ Ep 24: The      │ │     │ ...              │                                            ║
║  │     │   Endless ✓     │ │     │ Ep 50: Death ✓  │                                            ║
║  │     │                  │ │     │ Ep 51: Laws and │                                            ║
║  │     │                  │ │     │   Promises      │                                            ║
║  └─────┴─────────────────┘ └─────┴─────────────────┘                                            ║
║                                                                                                   ║
║  MyList Status: Loaded 5 anime                                                                  ║
║                                                                                                   ║
╚═══════════════════════════════════════════════════════════════════════════════════════════════════╝
```

## Card Structure Detail

### Individual Card (400x300 pixels)

```
┌────────────────────────────────┐
│ ┌────┬──────────────────────┐  │
│ │    │ Anime Title          │  │  ← Title (bold, 12pt)
│ │ ?  │ Type: TV Series      │  │  ← Type (9pt, gray)
│ │    │ Aired: 2013-2014     │  │  ← Aired dates (9pt, gray)
│ └────┤ Episodes: 12/24      │  │  ← Episodes in mylist/total (9pt)
│      │ Viewed: 8/12         │  │  ← Viewed/episodes in mylist (9pt)
│      ├──────────────────────┤  │
│      │ Ep 1: Episode Title ✓│  │  ← Episode list (8pt)
│      │ Ep 2: Another Ep ✓   │  │     Green = viewed
│      │ Ep 3: More [HDD]     │  │     [HDD] = file state
│      │ Ep 4: Episode Four   │  │     Clickable for playback
│      │ Ep 5: Five ✓         │  │
│      │ Ep 6: Six            │  │
│      │ ...                  │  │  ← Scrollable if many episodes
│      │                      │  │
│      │                      │  │
└──────┴──────────────────────┘  │
        Card dimensions:          
        Width: 400px              
        Height: 300px             
```

## Toolbar Controls

```
┌─────────────────────────────────────────────────────────────────┐
│ [ Switch to Tree View ]  Sort by: [ Anime Title ▼ ]            │
│                                                                  │
│ ↑                        ↑                                       │
│ Toggle Button            Sort Dropdown                           │
│ (150px width)            Options:                                │
│                          - Anime Title                           │
│                          - Type                                  │
│                          - Aired Date                            │
│                          - Episodes (Count)                      │
│                          - Completion %                          │
└─────────────────────────────────────────────────────────────────┘
```

## Visual Indicators

### Episode Status
- `✓` = Episode has been viewed (displayed in green)
- `[HDD]` = File is on HDD
- `[CD/DVD]` = File is on CD/DVD
- `[Deleted]` = File has been deleted
- `✗` = File not found (displayed in red)

### Card Layout
- **Poster Area**: 80x110 pixels (left side)
  - Shows "?" placeholder when no image
  - Ready for actual anime poster images
  
- **Info Area**: Right side of top section
  - Title: Bold, 12pt font, max 40 chars (word wrapped)
  - Type: Gray text, 9pt font
  - Aired: Gray text, 9pt font
  - Statistics: Normal text, 9pt font

- **Episode List**: Bottom section
  - Scrollable list (8pt font)
  - Alternating row colors
  - Click to play
  - Hover for tooltip with details

## Responsive Layout

### Wide Window (1400px+)
```
Card Card Card Card
Card Card Card Card
Card Card
```
(3-4 cards per row)

### Medium Window (1000px)
```
Card Card Card
Card Card Card
Card
```
(2-3 cards per row)

### Narrow Window (800px)
```
Card Card
Card Card
Card
```
(2 cards per row)

## Color Scheme

- **Card Background**: White (#FFFFFF)
- **Card Border**: Raised frame effect (Qt default)
- **Poster Placeholder**: Light gray (#F0F0F0) with gray text (#999999)
- **Title Text**: Black, bold
- **Info Text**: Dark gray (#666666)
- **Stats Text**: Dark gray (#333333)
- **Viewed Episodes**: Green (#009600)
- **Episode List**: Alternating row colors (default Qt style)
- **Status Bar**: Light gray background (#F0F0F0)

## Hover/Click Effects

### Card Hover
- Slight shadow/highlight effect (Qt default for QFrame)
- Cursor remains default (pointer shows for episodes)

### Episode Hover
- Background highlight (light blue)
- Cursor changes to pointer
- Tooltip appears with full details

### Episode Click
- Starts playback for that episode
- Visual feedback (brief highlight)

## Sorting Examples

### By Anime Title (Alphabetical)
```
Attack on Titan → Cowboy Bebop → Death Note → Fullmetal Alchemist → Steins;Gate
```

### By Aired Date (Chronological)
```
Cowboy Bebop (1998) → Death Note (2006) → Fullmetal Alchemist (2009) → Steins;Gate (2011) → Attack on Titan (2013)
```

### By Episodes Count (Ascending)
```
Cowboy Bebop (5) → Attack on Titan (12) → Steins;Gate (24) → Death Note (37) → Fullmetal Alchemist (64)
```

### By Completion % (Ascending)
```
Attack on Titan (66%) → Fullmetal Alchemist (78%) → Cowboy Bebop (60%) → Steins;Gate (100%) → Death Note (100%)
```

## Scrolling Behavior

- **Vertical Scroll**: Enabled when cards exceed viewport height
- **Horizontal Scroll**: Disabled (cards wrap to next row)
- **Smooth Scrolling**: Qt default scroll behavior
- **Mouse Wheel**: Scrolls vertically

## Comparison: Tree View vs Card View

### Tree View (Classic)
```
▼ Cowboy Bebop                           [columns: Episode, State, Viewed, etc.]
  │ Episode 1: Asteroid Blues             1        HDD      Yes
  │ Episode 2: Stray Dog Strut            2        HDD      Yes
  │ Episode 5: Ballad of Fallen Angels    5        HDD      Yes
  │ Episode 10: Ganymede Elegy            10       HDD      No
  │ Episode 13: Jupiter Jazz (Part 1)     13       HDD      No
```

### Card View (New)
```
┌───────────────────────┐
│ ┌───┬───────────────┐ │
│ │ ? │ Cowboy Bebop  │ │
│ │   │ Type: TV      │ │
│ └───┤ Episodes: 5/26│ │
│     │ Viewed: 3/5   │ │
│     ├───────────────┤ │
│     │ Ep 1: ... ✓   │ │
│     │ Ep 2: ... ✓   │ │
│     │ Ep 5: ... ✓   │ │
│     │ Ep 10: ...    │ │
│     │ Ep 13: ...    │ │
└─────┴───────────────┘ │
```

## Summary

The card view provides:
- **Visual**: More appealing, poster-based layout
- **Efficient**: See multiple anime at once
- **Organized**: Clear separation between anime
- **Interactive**: Click episodes directly from cards
- **Flexible**: Multiple sorting options
- **Compatible**: Toggle to tree view anytime
