# Episode Sorting Implementation

## Overview

This document describes the implementation of improved episode sorting in the mylist feature. The changes address the issue of properly sorting episodes by type (regular, special, credit, trailer, parody, other) and by numeric episode number, while also removing leading zeros from the display.

## Problem Statement

Previously, the mylist was sorted using text-based comparison of the `epno` field (e.g., "01", "02", "10", "S01"). This caused several issues:

1. **Incorrect numeric sorting**: Text-based sorting would produce "1", "10", "2" instead of "1", "2", "10"
2. **Mixed episode types**: Regular episodes, specials, credits, etc. were not grouped properly
3. **Leading zeros displayed**: Episode numbers showed as "01", "02" instead of "1", "2"
4. **Redundant type information**: The API returns both `epno` (string with prefix) and `type` (integer 1-6)

## Solution

### Database Schema Changes

Added a new `type` column to the `episode` table:

```sql
CREATE TABLE IF NOT EXISTS `episode`(
    `eid` INTEGER PRIMARY KEY,
    `name` TEXT,
    `nameromaji` TEXT,
    `namekanji` TEXT,
    `rating` INTEGER,
    `votecount` INTEGER,
    `epno` TEXT,
    `type` INTEGER  -- NEW: Episode type
);
```

Episode type values:
- `1`: Regular episode (no prefix)
- `2`: Special ("S" prefix)
- `3`: Credit ("C" prefix)
- `4`: Trailer ("T" prefix)
- `5`: Parody ("P" prefix)
- `6`: Other ("O" prefix)

### API Response Parsing

#### EPISODE Command (Code 240)

Updated parsing to extract the `type` field from the API response:

```
Response format: eid|aid|length|rating|votes|epno|eng|romaji|kanji|aired|type
```

The `type` field at index 10 is now stored in the database.

#### FILE Command (Code 220)

When file data includes episode information, the type is derived from the `epno` prefix if not provided separately:

```cpp
QString eptype = "1";  // Default to regular episode
if(!epno.isEmpty())
{
    QString epnoUpper = epno.toUpper();
    if(epnoUpper.startsWith("S"))
        eptype = "2";  // Special
    else if(epnoUpper.startsWith("C"))
        eptype = "3";  // Credit
    // ... etc
}
```

### Display Changes

#### Mylist Tree Widget

The `loadMylistFromDatabase()` function now:

1. **Queries the type field** from the database:
   ```sql
   SELECT m.lid, m.aid, m.eid, m.state, m.viewed, m.storage,
          a.nameromaji, a.nameenglish, a.eptotal,
          e.name as episode_name, e.epno, e.type, ...
   ```

2. **Sorts by type and numeric value**:
   ```sql
   ORDER BY a.nameromaji, e.type, 
            CAST(REPLACE(REPLACE(... e.epno ...) AS INTEGER)
   ```

3. **Removes leading zeros** from display:
   ```cpp
   bool ok;
   int epNum = numericPart.toInt(&ok);
   if(ok)
   {
       numericPart = QString::number(epNum);  // Removes leading zeros
   }
   ```

4. **Formats display string** based on type:
   - Type 1 (Regular): "1", "2", "10"
   - Type 2 (Special): "Special 1", "Special 2"
   - Type 3 (Credit): "Credit 1", "Credit 2"
   - Type 4 (Trailer): "Trailer 1", "Trailer 2"
   - Type 5 (Parody): "Parody 1", "Parody 2"
   - Type 6 (Other): "Other 1", "Other 2"

5. **Stores sort data** in item's UserRole:
   ```cpp
   episodeItem->setData(1, Qt::UserRole, eptype);      // For type-based sorting
   episodeItem->setData(1, Qt::UserRole + 1, epNum);   // For numeric sorting
   ```

#### Episode Update Function

The `updateEpisodeInTree()` function was updated to:
- Query the `type` field from the database
- Use the same formatting logic as `loadMylistFromDatabase()`
- Ensure consistency between initial load and dynamic updates

## Sorting Behavior

Episodes are now sorted in the following order:

1. **By anime name** (ascending)
2. **By episode type** (ascending):
   - Regular episodes (type 1)
   - Special episodes (type 2)
   - Credit episodes (type 3)
   - Trailer episodes (type 4)
   - Parody episodes (type 5)
   - Other episodes (type 6)
3. **By episode number** (numeric, ascending)

### Example Sorting

Given these episodes for an anime:
- Episode 1
- Episode 2
- Episode 10
- Special 1
- Special 2
- Credit 1

They will be sorted and displayed as:
1. Episode 1
2. Episode 2
3. Episode 10
4. Special 1
5. Special 2
6. Credit 1

## Testing

A comprehensive test suite was added in `tests/test_episode_sorting.cpp` to verify:

1. **Leading zero removal**: "01" → "1", "001" → "1", "10" → "10"
2. **Episode type formatting**: Correct prefixes for each type
3. **Numeric sorting**: Episodes sort numerically, not alphabetically
4. **Type grouping**: Episodes are grouped by type, then sorted numerically within each group

## Migration

The changes are backward compatible with existing databases:

1. The `ALTER TABLE` commands will add the `type` column if it doesn't exist
2. Existing episodes without a `type` value will be treated as regular episodes (type 1)
3. The `epno` field is still stored for reference and compatibility
4. Type information can be derived from `epno` prefix if not explicitly set

## Files Modified

1. **usagi/src/anidbapi.cpp**:
   - Added `type` column to episode table schema
   - Updated EPISODE response parsing (code 240)
   - Updated FILE response parsing (code 220)
   
2. **usagi/src/window.cpp**:
   - Updated `loadMylistFromDatabase()` to query and use type field
   - Updated episode display logic to remove leading zeros
   - Updated `updateEpisodeInTree()` to use type field
   - Modified SQL query to sort by type and numeric value

3. **tests/test_episode_sorting.cpp**:
   - Added comprehensive test suite for episode sorting logic
   - Tests cover leading zero removal, type formatting, and sort order

4. **tests/CMakeLists.txt**:
   - Added test_episode_sorting executable to build configuration

## API Reference

From AniDB UDP API Definition:

```
EPISODE: Retrieve Episode Data

Response:
240 EPISODE
{int eid}|{int aid}|{int4 length}|{int4 rating}|{int votes}|{str epno}|{str eng}|{str romaji}|{str kanji}|{int aired}|{int type}

Info:
- Returned 'epno' includes special character (only if special) and padding (only if normal).
  Special characters are S(special), C(credits), T(trailer), P(parody), O(other).
- The type is the raw episode type, used to indicate numerically what the special character will be
  1: regular episode (no prefix)
  2: special ("S")
  3: credit ("C")
  4: trailer ("T")
  5: parody ("P")
  6: other ("O")
```

## Benefits

1. **Correct sorting**: Episodes now sort numerically within their type groups
2. **Better organization**: Episodes are grouped by type (regular, special, credit, etc.)
3. **Cleaner display**: No leading zeros in episode numbers
4. **Type awareness**: The application can now distinguish between episode types programmatically
5. **Future extensibility**: The type field enables future features like filtering by episode type

## Future Enhancements

Possible future improvements based on this implementation:

1. **Filter by episode type**: Add UI controls to show/hide specific episode types
2. **Custom sorting**: Allow users to choose sort order (by type, by number, etc.)
3. **Batch operations**: Select all episodes of a specific type
4. **Statistics**: Show count of each episode type per anime
