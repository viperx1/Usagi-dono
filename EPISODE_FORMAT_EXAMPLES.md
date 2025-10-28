# Episode Format Examples

This document provides visual examples of how the new episode format appears in the mylist.

## Before vs After

### Old Format
```
Anime Name                  | Episode | Episode Title | State | Viewed  | Storage
-----------------------------------------------------------------------------------
Death Note                  | 37/37   |               |       | 35/37   |
├─ 1                        |         | Rebirth       | HDD   | Yes     | /anime/death-note/
├─ 2                        |         | Confrontation | HDD   | Yes     | /anime/death-note/
├─ ...                      |         | ...           | ...   | ...     | ...
├─ 37                       |         | New World     | HDD   | No      | /anime/death-note/
└─ Special 1                |         | Relight       | HDD   | No      | /anime/death-note/
```

### New Format
```
Anime Name                  | Episode   | Episode Title | State | Viewed    | Storage
---------------------------------------------------------------------------------------
Death Note                  | 37/37+1   |               |       | 35/37+0   |
├─ 1                        |           | Rebirth       | HDD   | Yes       | /anime/death-note/
├─ 2                        |           | Confrontation | HDD   | Yes       | /anime/death-note/
├─ ...                      |           | ...           | ...   | ...       | ...
├─ 37                       |           | New World     | HDD   | No        | /anime/death-note/
└─ Special 1                |           | Relight       | HDD   | No        | /anime/death-note/
```

**Interpretation:**
- Episode: "37/37+1" = 37 normal episodes out of 37 total normal episodes + 1 special
- Viewed: "35/37+0" = 35 normal episodes viewed out of 37 normal episodes + 0 specials viewed

## Example Scenarios

### Scenario 1: Anime with Only Normal Episodes
```
One Piece                   | 1050/1100+0  |        | 980/1050+0
```
- Has 1050 normal episodes in mylist out of 1100 total
- No special episodes
- Viewed 980 out of 1050 normal episodes

Note: The "+0" could be omitted to show as "1050/1100" and "980/1050" for cleaner display.

### Scenario 2: Anime with Mixed Episode Types
```
Naruto Shippuden            | 500/500+15   |        | 480/500+12
```
- Has all 500 normal episodes in mylist
- Has 15 other type episodes (specials, OVAs, etc.)
- Viewed 480 out of 500 normal episodes
- Viewed 12 out of 15 other type episodes

### Scenario 3: Anime with Many Specials
```
Pokemon                     | 276/276+52   |        | 200/276+30
```
- Has all 276 normal episodes
- Has 52 other type episodes (movies, specials, OVAs)
- Viewed 200 normal episodes
- Viewed 30 other type episodes

### Scenario 4: Incomplete Series
```
Hunter x Hunter (2011)      | 100/148+2    |        | 85/100+1
```
- Has 100 out of 148 normal episodes in mylist
- Has 2 special episodes
- Viewed 85 out of 100 normal episodes in mylist
- Viewed 1 out of 2 special episodes

### Scenario 5: Anime Without Episode Total (eps = 0)
```
Ongoing Series              | 50/?+5       |        | 40/50+4
```
- Has 50 normal episodes in mylist
- Has 5 other type episodes
- Total episode count not available (ongoing or unknown)
- Episode column uses "?" to indicate unknown total: "50/?+5"
- Viewed 40 out of 50 normal episodes
- Viewed 4 out of 5 other type episodes

### Scenario 6: Complete Anime, All Viewed
```
Cowboy Bebop                | 26/26+1      |        | 26/26+1
```
- Has all 26 normal episodes
- Has 1 special episode (movie)
- All episodes viewed

### Scenario 7: Short Anime with OVAs
```
FLCL                        | 6/6+3        |        | 6/6+2
```
- Has all 6 normal episodes
- Has 3 OVA episodes
- All normal episodes viewed
- 2 out of 3 OVAs viewed

## Episode Type Breakdown

The "+C" component includes episodes of these types:
- Type 2: Specials (S prefix)
- Type 3: Credits (C prefix)
- Type 4: Trailers (T prefix)
- Type 5: Parodies (P prefix)
- Type 6: Other (O prefix)

Example with multiple types:
```
Anime Name                  | Episode   | Episode Title      | Viewed
---------------------------------------------------------------------------
My Anime                    | 12/12+7   |                    | 12/12+5
├─ 1-12                     |           | (normal episodes)  | Yes
├─ Special 1                |           | Beach Episode      | Yes
├─ Special 2                |           | Hot Springs        | Yes
├─ Credit 1                 |           | Opening Credits    | Yes
├─ Credit 2                 |           | Ending Credits     | No
├─ Trailer 1                |           | PV1                | Yes
├─ Trailer 2                |           | PV2                | No
└─ Other 1                  |           | Behind the Scenes  | Yes
```

The +7 includes: 2 specials + 2 credits + 2 trailers + 1 other = 7 total
Viewed +5 includes: 2 specials + 1 credit + 1 trailer + 1 other = 5 viewed

## UI Column Layout

```
Column 0: Anime Name (parent) / Empty (child)
Column 1: Episode Format (parent) / Episode Number (child)
Column 2: Empty (parent) / Episode Title (child)
Column 3: Empty (parent) / State (child)
Column 4: Viewed Format (parent) / Yes/No (child)
Column 5: Empty (parent) / Storage (child)
Column 6: Empty (parent) / Mylist ID (child)
```

## Benefits of New Format

1. **Better Information Density**: See at a glance how many specials/OVAs you have
2. **Clearer Progress Tracking**: Separate normal episode progress from bonus content
3. **Consistent Format**: Both Episode and Viewed columns use the same A/B+C format
4. **Handles Edge Cases**: Gracefully handles missing data or ongoing series

## Technical Implementation

The format is generated in `Window::loadMylistFromDatabase()` using these variables:
- `normalEpisodes`: Count of type 1 episodes in mylist
- `totalEpisodes`: From anime.eptotal (total normal episodes that exist)
- `otherEpisodes`: Count of types 2-6 episodes in mylist
- `normalViewed`: Count of viewed type 1 episodes
- `otherViewed`: Count of viewed types 2-6 episodes

Logic:
```cpp
// Episode column
if(totalEpisodes > 0) {
    if(otherEpisodes > 0)
        text = QString("%1/%2+%3").arg(normalEpisodes, totalEpisodes, otherEpisodes);
    else
        text = QString("%1/%2").arg(normalEpisodes, totalEpisodes);
}

// Viewed column
if(otherEpisodes > 0)
    text = QString("%1/%2+%3").arg(normalViewed, normalEpisodes, otherViewed);
else
    text = QString("%1/%2").arg(normalViewed, normalEpisodes);
```
