# Episode Number Type (epno) Implementation

## Overview

This document describes the implementation of the `epno` type, a custom C++ class that properly handles episode number sorting by type and numeric value, with automatic leading zero removal.

## Problem Statement

The original implementation sorted episode numbers as text strings, which caused:
- Incorrect numeric sorting: "1", "10", "2" instead of "1", "2", "10"
- No type-based grouping: Regular episodes mixed with specials, credits, etc.
- Leading zeros displayed: "01", "02" instead of "1", "2"

## Solution: The epno Class

Created a custom type `epno` that:
1. Encapsulates episode type (1-6) and number
2. Overloads comparison operators for proper sorting
3. Provides display formatting with automatic leading zero removal
4. Integrates with Qt's tree widget for sorting

### Episode Types

```cpp
1 = regular episode (no prefix)
2 = special ("S" prefix)
3 = credit ("C" prefix)
4 = trailer ("T" prefix)
5 = parody ("P" prefix)
6 = other ("O" prefix)
```

## Implementation Details

### Class Definition (epno.h)

```cpp
class epno
{
public:
    // Constructors
    epno();                              // Default constructor
    epno(const QString& epnoString);     // Parse from string (e.g., "01", "S01")
    epno(int type, int number);          // Construct from type and number
    
    // Getters
    int type() const;                    // Get episode type (1-6)
    int number() const;                  // Get numeric value (no leading zeros)
    QString toString() const;            // Get original string
    QString toDisplayString() const;     // Get formatted display string
    
    // Comparison operators (for sorting)
    bool operator<(const epno& other) const;
    bool operator>(const epno& other) const;
    bool operator==(const epno& other) const;
    bool operator!=(const epno& other) const;
    bool operator<=(const epno& other) const;
    bool operator>=(const epno& other) const;
    
    // Validation
    bool isValid() const;
    
private:
    int m_type;          // Episode type (1-6)
    int m_number;        // Numeric value (without prefix or leading zeros)
    QString m_rawString; // Original string from database/API
    
    void parse(const QString& str);  // Parse episode string
};
```

### Key Methods

#### Constructor from String

Parses episode strings like "01", "S01", "C01" and extracts:
- Type based on prefix (or default to type 1 for regular episodes)
- Numeric value (automatically removes leading zeros via `toInt()`)

```cpp
epno ep1("01");    // type=1, number=1
epno ep2("S01");   // type=2, number=1
epno ep3("010");   // type=1, number=10
```

#### toDisplayString()

Returns formatted display string:
- Regular episodes: Just the number ("1", "2", "10")
- Special episodes: "Special 1", "Special 2"
- Credit episodes: "Credit 1", "Credit 2"
- etc.

Leading zeros are automatically removed.

#### Comparison Operators

Implements proper sorting logic:

```cpp
bool epno::operator<(const epno& other) const
{
    // Sort by type first, then by number
    if(m_type != other.m_type)
        return m_type < other.m_type;
    return m_number < other.m_number;
}
```

This ensures:
1. Regular episodes (type 1) come before specials (type 2)
2. Specials come before credits (type 3)
3. Within each type, episodes sort numerically

### Custom Tree Widget Item

Created `EpisodeTreeWidgetItem` that extends `QTreeWidgetItem`:

```cpp
class EpisodeTreeWidgetItem : public QTreeWidgetItem
{
public:
    void setEpno(const epno& ep);    // Store epno object
    epno getEpno() const;            // Retrieve epno object
    
    // Override comparison for sorting
    bool operator<(const QTreeWidgetItem &other) const override;
    
private:
    epno m_epno;  // Episode number object
};
```

The overridden `operator<` uses the `epno` comparison when sorting by episode number column:

```cpp
bool operator<(const QTreeWidgetItem &other) const override
{
    int column = treeWidget()->sortColumn();
    
    if(column == 1)  // Episode number column
    {
        const EpisodeTreeWidgetItem *otherEpisode = 
            dynamic_cast<const EpisodeTreeWidgetItem*>(&other);
        if(otherEpisode && m_epno.isValid() && otherEpisode->m_epno.isValid())
        {
            return m_epno < otherEpisode->m_epno;  // Use epno comparison
        }
    }
    
    return QTreeWidgetItem::operator<(other);  // Default comparison
}
```

## Usage in Window.cpp

### Loading Episodes

```cpp
void Window::loadMylistFromDatabase()
{
    // ... query database ...
    
    QString epnoStr = q.value(10).toString();
    
    // Create custom tree widget item
    EpisodeTreeWidgetItem *episodeItem = new EpisodeTreeWidgetItem(animeItem);
    
    if(!epnoStr.isEmpty())
    {
        // Create epno object from string
        epno episodeNumber(epnoStr);
        
        // Store epno object for sorting
        episodeItem->setEpno(episodeNumber);
        
        // Display formatted string (leading zeros removed)
        episodeItem->setText(1, episodeNumber.toDisplayString());
    }
}
```

### Updating Episodes

```cpp
void Window::updateEpisodeInTree(int eid, int aid)
{
    QString epnoStr = q.value(0).toString();
    
    if(!epnoStr.isEmpty())
    {
        epno episodeNumber(epnoStr);
        
        // Store epno object if this is an EpisodeTreeWidgetItem
        EpisodeTreeWidgetItem *epItem = 
            dynamic_cast<EpisodeTreeWidgetItem*>(episodeItem);
        if(epItem)
        {
            epItem->setEpno(episodeNumber);
        }
        
        // Display formatted episode number
        episodeItem->setText(1, episodeNumber.toDisplayString());
    }
}
```

## Sorting Behavior

When user clicks on the episode number column header, Qt calls the comparison operator:

1. `QTreeWidget` calls `EpisodeTreeWidgetItem::operator<()`
2. Custom operator checks if sorting by episode number column
3. If yes, uses `epno::operator<()` for comparison
4. Episodes sort by type first, then numerically

### Example Sort Order

Given these episodes:
```
Input (unsorted): 02, 010, S01, 01, 100, S02, C01
```

After sorting:
```
Output (sorted):
1         (type=1, num=1)
2         (type=1, num=2)
10        (type=1, num=10)
100       (type=1, num=100)
Special 1 (type=2, num=1)
Special 2 (type=2, num=2)
Credit 1  (type=3, num=1)
```

## Testing

Comprehensive test suite in `tests/test_epno.cpp` verifies:

1. **Constructor parsing**: Correct type and number extraction
2. **Leading zero removal**: "01" → 1, "001" → 1
3. **Type formatting**: "S01" → "Special 1"
4. **Comparison operators**: Proper sorting logic
5. **Sort order**: Episodes sort by type then numerically
6. **Validation**: `isValid()` works correctly

Example test:

```cpp
void TestEpno::testSorting()
{
    QList<epno> episodes;
    episodes << epno("2") << epno("010") << epno("S01") << epno("01") 
             << epno("100") << epno("S02") << epno("C01");
    
    std::sort(episodes.begin(), episodes.end());
    
    // Verify correct order
    QCOMPARE(episodes[0].toDisplayString(), QString("1"));
    QCOMPARE(episodes[1].toDisplayString(), QString("2"));
    QCOMPARE(episodes[2].toDisplayString(), QString("10"));
    QCOMPARE(episodes[3].toDisplayString(), QString("100"));
    QCOMPARE(episodes[4].toDisplayString(), QString("Special 1"));
    QCOMPARE(episodes[5].toDisplayString(), QString("Special 2"));
    QCOMPARE(episodes[6].toDisplayString(), QString("Credit 1"));
}
```

## Files Modified/Created

### New Files
- `usagi/src/epno.h` - Class definition
- `usagi/src/epno.cpp` - Implementation
- `tests/test_epno.cpp` - Test suite

### Modified Files
- `usagi/CMakeLists.txt` - Added epno.cpp and epno.h to build
- `usagi/src/window.h` - Added epno include, EpisodeTreeWidgetItem class
- `usagi/src/window.cpp` - Uses epno type for episode numbers
- `tests/CMakeLists.txt` - Added test_epno executable

## Benefits

1. ✅ **Object-oriented design**: Episode numbers are first-class objects
2. ✅ **Proper sorting**: Comparison operators ensure correct sort order
3. ✅ **Type-safe**: Episode type and number are encapsulated
4. ✅ **Reusable**: epno class can be used anywhere episode numbers are needed
5. ✅ **Maintainable**: Logic is centralized in one class
6. ✅ **Testable**: Comprehensive test suite validates behavior
7. ✅ **Clean display**: Automatic leading zero removal

## Future Enhancements

- Add episode range support (e.g., "1-3", "S1-S5")
- Support for decimal episodes (e.g., "1.5", "S1.5")
- Localization of type names (Special → 特別編 in Japanese)
- Serialization methods for database storage
