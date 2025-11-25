# Filter Sidebar Updates

## Changes Made (PR Feedback)

Based on user feedback, the following changes were implemented:

### 1. Removed ONA from Anime Types
- ONA (Original Net Animation) has been removed from the type filter dropdown
- Remaining types: TV Series, Movie, OVA, TV Special, Web, Music Video, Other

### 2. Moved Sorting to Sidebar
- Removed the toolbar with sorting controls
- Sorting controls are now integrated into the sidebar
- Sort options: Anime Title, Type, Aired Date, Episodes (Count), Completion %, Last Played
- Includes asc/desc toggle button

### 3. Default Sort: Aired Date Descending
- Changed default sort from "Anime Title Ascending" to "Aired Date Descending"
- Shows newest anime first by default
- Sidebar initializes with this default on every load

### 4. Adult Content Filter
- Added new "Adult Content" filter group in sidebar
- Three options:
  - **Ignore**: Show all anime regardless of 18+ status
  - **Hide 18+**: Filter out 18+ restricted content (default)
  - **Show only 18+**: Show only 18+ restricted content
- Uses `is_18_restricted` field from anime database table
- AnimeCard class updated to store this field
- MyListCardManager updated to load this field from database

### 5. Fixed Card Alignment Issue
- **Problem**: Hidden cards were using `setVisible(false)` which made them invisible but still took up space in the layout
- **Solution**: Hidden cards are now removed from the FlowLayout entirely
- Process:
  1. Remove all cards from layout
  2. Filter cards based on criteria
  3. Add only visible cards back to layout
- Cards are properly positioned without gaps from hidden items

## Technical Implementation

### Database Changes
- No schema changes required
- Uses existing `is_18_restricted` column in `anime` table
- Field is loaded during card creation in MyListCardManager

### UI Changes
- Sidebar height increased to accommodate new controls
- Sort group added between Search and Type filter
- Adult Content group added after Viewed Status

### Code Changes
- **mylistfiltersidebar.h/cpp**: Added sort controls, adult content filter, related getters
- **window.h**: Removed mylistSortComboBox and mylistSortOrderButton members
- **window.cpp**: 
  - Removed toolbar creation code
  - Updated sortMylistCards to use sidebar's sort settings
  - Updated applyMylistFilters to handle adult content and fix alignment
  - Updated references to use filterSidebar->getSortIndex() and getSortAscending()
- **animecard.h/cpp**: Added m_is18Restricted field and setter
- **mylistcardmanager.cpp**: Updated query to load is_18_restricted

## User Impact

### Positive Changes
- Cleaner UI with all controls in one place (sidebar)
- Default sort shows newest anime first (more useful)
- Adult content filtering for family-friendly viewing
- Fixed card alignment makes the view cleaner

### Breaking Changes
- Old sort preferences are not migrated (fresh start with new defaults)
- Toolbar removed completely (all controls now in sidebar)

## Future Enhancements

Possible improvements:
- Remember adult content filter preference
- Add age rating filter (not just 18+)
- Add genre/tag filtering
- Save/load filter presets
