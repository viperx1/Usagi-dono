# MyList Sorting and Aggregate Statistics

## Overview

This document describes the enhancements made to the MyList feature to improve sorting and display aggregate statistics for anime entries.

## Changes Implemented

### 1. Default Sorting Order Change

**Previous Behavior:**
- MyList tree was sorted by anime name (column 0) in ascending order

**New Behavior:**
- MyList tree is now sorted by episode column (column 1) in ascending order
- This makes it easier to see which anime have fewer/more episodes in the mylist

**Code Change:**
```cpp
// Before:
mylistTreeWidget->sortByColumn(0, Qt::AscendingOrder);

// After:
mylistTreeWidget->sortByColumn(1, Qt::AscendingOrder);
```

### 2. Aggregate Statistics in Anime Rows

Anime rows (parent items in the tree) now display aggregate statistics in two columns:

#### Episode Column (Column 1)
- **Format:** `in_mylist/eptotal`
- **Example:** "12/24" means 12 episodes in mylist out of 24 total primary episodes
- **Fallback:** If `eptotal` is 0 or not available, shows just the count: "12"

#### Viewed Column (Column 4)
- **Format:** `viewed/in_mylist`
- **Example:** "8/12" means 8 episodes viewed out of 12 episodes in mylist
- **Shows:** Progress tracking for each anime

### 3. Implementation Details

#### Statistics Tracking
During the database query loop, three QMap structures track per-anime data:
- `animeEpisodeCount[aid]` - Count of episodes in mylist for each anime
- `animeViewedCount[aid]` - Count of viewed episodes for each anime
- `animeEpTotal[aid]` - Total primary type episodes (eptotal from anime table)

#### Statistics Update
After all episodes are processed, a loop updates all anime rows:
```cpp
for(QMap<int, QTreeWidgetItem*>::iterator it = animeItems.begin(); it != animeItems.end(); ++it)
{
    int aid = it.key();
    QTreeWidgetItem *animeItem = it.value();
    
    int episodesInMylist = animeEpisodeCount[aid];
    int viewedEpisodes = animeViewedCount[aid];
    int totalEpisodes = animeEpTotal[aid];
    
    // Column 1 (Episode): show in_mylist/eptotal
    if(totalEpisodes > 0)
        animeItem->setText(1, QString("%1/%2").arg(episodesInMylist).arg(totalEpisodes));
    else
        animeItem->setText(1, QString::number(episodesInMylist));
    
    // Column 4 (Viewed): show viewed/in_mylist
    animeItem->setText(4, QString("%1/%2").arg(viewedEpisodes).arg(episodesInMylist));
}
```

## Benefits

1. **Better Overview:** Users can quickly see how complete their collection is for each anime
2. **Progress Tracking:** The viewed/in_mylist ratio shows watch progress at a glance
3. **Improved Sorting:** Default episode column sorting helps identify anime with fewer episodes
4. **No Breaking Changes:** Episode items (children) retain their existing display format

## Visual Example

```
MyList Tree Structure:

Anime                          Episode    Episode Title    State    Viewed    Storage    Mylist ID
├─ Attack on Titan             12/25      [Aggregate]      -        8/12      -          -
│  ├─                          1          First Episode    HDD      Yes       /data      12345
│  ├─                          2          Second Episode   HDD      Yes       /data      12346
│  └─                          ...        ...              ...      ...       ...        ...
├─ Death Note                  37/37      [Aggregate]      -        37/37     -          -
│  ├─                          1          Rebirth          HDD      Yes       /data      23456
│  ├─                          2          Confrontation    HDD      Yes       /data      23457
│  └─                          ...        ...              ...      ...       ...        ...
└─ Cowboy Bebop                5/26       [Aggregate]      -        3/5       -          -
   ├─                          1          Asteroid Blues   HDD      Yes       /data      34567
   ├─                          2          Stray Dog Strut  HDD      Yes       /data      34568
   └─                          ...        ...              ...      ...       ...        ...
```

## Technical Notes

### Episode Count
The `in_mylist` count includes all episodes in the mylist regardless of their type (regular, special, credit, etc.).

### eptotal Definition
The `eptotal` field from the anime table represents the total number of **primary type episodes** (type 1 = regular episodes). This does not include specials, OVAs, or other episode types.

### Statistics Initialization
Statistics are initialized when an anime item is first created:
```cpp
if(!animeItems.contains(aid))
{
    // Create new anime item
    animeItem = new QTreeWidgetItem(mylistTreeWidget);
    // ... set text and data ...
    
    // Initialize counters
    animeEpisodeCount[aid] = 0;
    animeViewedCount[aid] = 0;
    animeEpTotal[aid] = epTotal;
}

// Increment counters for each episode
animeEpisodeCount[aid]++;
if(viewed)
    animeViewedCount[aid]++;
```

### Sorting Behavior

The mylist tree supports custom sorting for specific columns:

#### Episode Column (Column 1)
Uses the custom `EpisodeTreeWidgetItem` class for episode items, which properly sorts by episode type and number using the `epno` class. For anime items (parents), text-based comparison is used (e.g., "12/24" vs "5/26").

#### Aired Column (Column 8)
Uses the custom `AnimeTreeWidgetItem` class for anime items, which properly sorts by actual air dates instead of string comparison. The `aired` class provides date-based comparison:
- Primary sort: Start date (chronological order)
- Secondary sort: End date (when start dates are equal)
- Items with invalid/empty dates are sorted to the end

Users can click the "Aired" column header to toggle between ascending (oldest first) and descending (newest first) sort order.

## Files Modified

- `usagi/src/window.cpp` - Modified `loadMylistFromDatabase()` function
- `usagi/src/window.h` - Added `AnimeTreeWidgetItem` class for date-based sorting

## Future Enhancements

Potential improvements for future consideration:
1. Add color coding for completion percentage (e.g., green for 100%, yellow for >50%, red for <50%)
2. Add a column showing completion percentage explicitly
3. Allow users to customize the default sort column in settings
4. Update aggregate statistics when episodes are added/removed dynamically
5. Add separate sorting options for air end date (currently sorts by start date primarily)
