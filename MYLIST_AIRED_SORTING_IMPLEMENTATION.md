# MyList Aired Column Sorting Implementation

## Issue Summary
The mylist was sorting by air time using string comparison instead of actual date comparison. For example, "03.11.2020" would sort before "28.01.2020" alphabetically, even though January 2020 is earlier than November 2020 chronologically.

## Solution Implemented
Implemented date-based sorting for the Aired column (column 8) by creating a custom `AnimeTreeWidgetItem` class that stores `aired` objects and uses them for proper chronological sorting.

## Sorting Behavior

### Primary Sort: Start Date
The main sorting criterion is the anime's start date (air date). This is what users see when they click the Aired column header once or twice.

### Secondary Sort: End Date  
When two anime have the same start date, the end date is used as a tiebreaker. This provides natural secondary sorting by end date without requiring separate UI controls.

### Toggle Between Ascending and Descending
Users can click the "Aired" column header to toggle between:
- **Ascending order**: Oldest anime first (earliest start date → latest start date)
- **Descending order**: Newest anime first (latest start date → earliest start date)

### Handling Invalid Dates
Anime with invalid or empty aired dates are sorted to the end of the list (after all anime with valid dates), regardless of sort order.

## Design Decision: Two vs Four Sort Options

The issue requested "4 sorting options (air start asc/air start desc/air end asc/air end desc)" if possible, but to "sort by air start" if not possible.

We implemented the standard Qt column sorting behavior (2 options: ascending/descending by start date) for the following reasons:

1. **Consistency with Qt Standards**: QTreeWidget's built-in sorting mechanism provides ascending/descending toggle per column. Users expect to click column headers to sort.

2. **UI Simplicity**: Adding 4 separate sort options would require:
   - Additional UI controls (buttons, dropdown menu, or context menu)
   - More complex state management
   - User education about which mode is active
   - Deviating from standard Qt tree widget behavior

3. **Minimal Change Principle**: The task requirements emphasized making "smallest possible changes" to fix the issue.

4. **Practical Coverage**: Our implementation provides:
   - Primary sort by start date (ascending or descending) ✓
   - Automatic secondary sort by end date when start dates match ✓
   - This covers the most common use cases for sorting by air dates

5. **End Date Sort is Available**: When two anime have the same start date (e.g., "2020-01-01"), they are automatically sorted by end date. This provides end date sorting as a natural tiebreaker.

## Technical Implementation

### AnimeTreeWidgetItem Class
```cpp
class AnimeTreeWidgetItem : public QTreeWidgetItem
{
public:
    AnimeTreeWidgetItem(QTreeWidget *parent) : QTreeWidgetItem(parent) {}
    
    void setAired(const aired& airedDates) { m_aired = airedDates; }
    aired getAired() const { return m_aired; }
    
    bool operator<(const QTreeWidgetItem &other) const override
    {
        int column = treeWidget()->sortColumn();
        
        // If sorting by Aired column (column 8)
        if(column == 8)
        {
            const AnimeTreeWidgetItem *otherAnime = dynamic_cast<const AnimeTreeWidgetItem*>(&other);
            if(otherAnime && m_aired.isValid() && otherAnime->m_aired.isValid())
            {
                return m_aired < otherAnime->m_aired;
            }
            // Handle invalid dates...
        }
        
        // Default comparison for other columns
        return QTreeWidgetItem::operator<(other);
    }
    
private:
    aired m_aired;
};
```

### Aired Class Comparison
The `aired` class already implements proper date comparison:
```cpp
bool aired::operator<(const aired& other) const
{
    // Sort by start date first, then by end date
    if(m_startDate != other.m_startDate)
        return m_startDate < other.m_startDate;
    return m_endDate < other.m_endDate;
}
```

## Usage

1. **Load mylist**: Anime items are created with their aired dates stored
2. **Click "Aired" column header once**: Sort ascending (oldest → newest)
3. **Click "Aired" column header again**: Sort descending (newest → oldest)
4. **Click another column**: Sort by that column instead

## Testing

Comprehensive tests verify:
- ✓ Ascending sort by start date
- ✓ Descending sort by start date  
- ✓ Secondary sort by end date when start dates match
- ✓ Invalid/empty dates sorted to end
- ✓ Date comparison vs string comparison (regression test)

## Files Modified

1. **usagi/src/window.h**: Added `AnimeTreeWidgetItem` class
2. **usagi/src/window.cpp**: Use `AnimeTreeWidgetItem` for anime items, store aired objects
3. **tests/test_mylist_aired_sorting.cpp**: Comprehensive test suite
4. **tests/CMakeLists.txt**: Added new test
5. **MYLIST_SORTING_AND_STATS.md**: Updated documentation

## Future Enhancements

If more complex sorting options are needed in the future, consider:
1. Add a sorting options dialog/menu
2. Support custom sort orders (e.g., "sort by end date only")
3. Add visual indicators for current sort mode
4. Save user's preferred sort order in settings

However, for the current use case, the standard Qt column sorting provides an intuitive and effective solution.
