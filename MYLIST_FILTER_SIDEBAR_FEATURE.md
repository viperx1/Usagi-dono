# MyList Filter Sidebar Feature

## Overview

A filter sidebar has been added to the MyList tab to enable searching and filtering anime in your collection. This feature works entirely with data already loaded in memory - no database operations are performed during filtering, making it fast and responsive.

## Features

### 1. Search by Title
- Search field with support for:
  - Main anime title (romaji)
  - English title
  - All alternative titles from the anime_titles table
- Case-insensitive search
- Debounced input (300ms delay) for smooth performance

### 2. Filter by Type
Filter anime by their type:
- All Types (no filter)
- TV Series
- Movie
- OVA
- ONA
- TV Special
- Web
- Music Video
- Other

### 3. Filter by Completion Status
Filter based on viewing progress:
- **All**: Show all anime
- **Completed**: Show only anime where all normal episodes have been viewed
- **Watching**: Show anime with some episodes viewed but not completed
- **Not Started**: Show anime with no episodes viewed

### 4. Show Only Unwatched
- Checkbox to show only anime that have unwatched episodes
- Works for both normal and special episodes

### 5. Reset Filters
- Button to quickly reset all filters to their default state

## Usage

1. Open the MyList tab
2. The filter sidebar appears on the left side of the screen
3. Use any combination of filters:
   - Type text in the search field
   - Select a type from the dropdown
   - Select a completion status
   - Check the "Show only with unwatched episodes" box
4. Cards update automatically as you change filters
5. The status label at the bottom shows how many anime match your filters

## Technical Implementation

### Architecture
- **MyListFilterSidebar**: Qt widget containing all filter controls
- **Window::applyMylistFilters()**: Applies filters by showing/hiding cards
- **Window::matchesSearchFilter()**: Checks if anime matches search text
- **Window::loadAnimeAlternativeTitlesForFiltering()**: Pre-loads alternative titles for efficient searching

### Performance
- Alternative titles are cached on initial load
- No database queries during filtering
- Cards are simply shown/hidden using `setVisible()`
- Debounced search input prevents excessive updates

### Data Sources
- Main titles from `anime` table (nameromaji, nameenglish)
- Alternative titles from `anime_titles` table
- Episode viewing data from existing card statistics

## Code Changes

### New Files
- `usagi/src/mylistfiltersidebar.h`
- `usagi/src/mylistfiltersidebar.cpp`

### Modified Files
- `usagi/src/window.h` - Added sidebar widget and helper methods
- `usagi/src/window.cpp` - Integrated sidebar and implemented filtering
- `usagi/CMakeLists.txt` - Added new files to build

## Future Enhancements

Potential improvements for future versions:
1. Filter by tags
2. Filter by rating
3. Filter by aired date range
4. Save/load filter presets
5. More advanced search operators (AND/OR/NOT)
6. Search history

## Compatibility

- Works with existing MyList card view
- No changes to database schema required
- Compatible with existing sort functionality
- All tests pass (except those requiring display/X server in CI)
