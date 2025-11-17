# Unknown Files Feature

## Overview
This feature adds support for handling files that are not found in the AniDB database (320 NO SUCH FILE error). When a file's hash is not recognized by AniDB, it is added to an "Unknown Files" widget where users can manually bind it to an anime and episode.

## Implementation

### User Interface
- **Location**: Hasher tab, below the main hashes table
- **Visibility**: Hidden by default, shown only when unknown files are detected
- **Columns**:
  1. **Filename**: Displays the filename with full path in tooltip
  2. **Anime**: Search field with autocomplete for anime titles
  3. **Episode**: Dropdown populated based on selected anime
  4. **Action**: "Bind" button to create mylist entry

### Workflow

1. **File Hashing**: User adds files to hasher and starts hashing
2. **API Query**: Each file's hash is queried against AniDB
3. **320 Error Handling**: If AniDB returns "320 NO SUCH FILE":
   - File is marked red in hashes table (existing behavior)
   - File is added to unknown files widget (new behavior)
   - Unknown files section becomes visible
4. **Manual Binding**:
   - User searches for anime using autocomplete (populated from anime_titles table)
   - User selects episode from dropdown (populated from file/episode tables)
   - User clicks "Bind" button
5. **Mylist Entry Creation**:
   - System calls MylistAdd API with file size and hash
   - Entry is created in user's mylist
   - File is removed from unknown files widget
   - Widget is hidden if no more unknown files remain

### Database Integration
- Uses existing `anime_titles` table for autocomplete
- Uses existing `episode` and `file` tables for episode selection
- Uses existing `MylistAdd` API for creating entries
- Updates `local_files` table status appropriately

### Code Components

#### New Classes
- `unknown_files_`: QTableWidget subclass for displaying unknown files
  - Custom event handling (delete key disabled to prevent data inconsistency)
  - Fixed height to prevent UI dominance
  - 4 columns with appropriate widths

#### New Methods (Window class)
- `unknownFilesInsertRow()`: Adds a file to the unknown files widget
  - Creates row with filename, anime search, episode dropdown, bind button
  - Sets up QCompleter with anime titles from database
  - Connects signals for anime selection → episode population
  - Connects bind button to create mylist entry
  
- `onUnknownFileBindClicked()`: Handles bind button click
  - Validates anime and episode selection
  - Retrieves file details from episode
  - Calls MylistAdd API
  - Updates local file status
  - Removes file from widget
  - Hides widget if empty

#### Modified Methods
- `getNotifyMylistAdd()`: Enhanced to handle 320 errors
  - Adds unknown file to widget when 320 error occurs
  - Shows unknown files section
  - Logs the event

### Widget Lifecycle
- **Created**: During Window construction
- **Hidden initially**: Both widget and label are hidden
- **Shown**: When first unknown file is added
- **Hidden again**: When last unknown file is bound or removed

## Usage

### For Users
1. Add files to hasher as normal
2. Start hashing
3. If files are not in AniDB:
   - They appear in "Unknown Files" section
   - Search for the correct anime title
   - Select the correct episode
   - Click "Bind" to add to mylist

### For Developers
To add an unknown file programmatically:
```cpp
window->unknownFilesInsertRow(filename, filepath, hash, size);
```

To check if unknown files widget is visible:
```cpp
bool hasUnknownFiles = window->unknownFiles->isVisible();
```

## Technical Notes

### Why Manual Binding?
Files may not be in AniDB for several reasons:
- Very new releases not yet added to database
- Fansub releases that haven't been submitted
- Modified or custom files
- Corrupted or incomplete files

Manual binding allows users to still track these files in their mylist while waiting for proper database entries.

### Limitations
1. Only shows episodes that already have file entries in the database
2. Requires anime titles to be downloaded first (via anime_titles feature)
3. Cannot create new anime or episode entries
4. Delete key is disabled in unknown files widget to prevent data inconsistency

### Future Enhancements
Possible improvements for future versions:
- Support for creating new anime/episode entries
- Batch binding of multiple files
- Remember user's anime selections for similar filenames
- Export unknown files list for external processing
- Integration with alternative anime databases

## Testing

### Manual Testing Checklist
- [ ] Unknown files widget is hidden on startup
- [ ] Widget appears when 320 error occurs
- [ ] Anime search shows autocomplete results
- [ ] Selecting anime populates episode dropdown
- [ ] Bind button is disabled until both anime and episode are selected
- [ ] Clicking bind creates mylist entry
- [ ] File is removed from widget after successful bind
- [ ] Widget hides when last file is bound
- [ ] Multiple unknown files can be handled simultaneously

### Edge Cases to Test
- Empty anime_titles database (autocomplete should be empty)
- Anime with no episodes in database
- Multiple files from same anime
- User not logged in (bind should show error)
- Network errors during bind operation

## API Compatibility
This feature uses existing AniDB APIs:
- MYLISTADD: For creating mylist entries with size+ed2k hash
- No new API endpoints required

## Database Schema
No database schema changes required. Uses existing tables:
- `anime_titles`: For autocomplete
- `episode`: For episode information
- `file`: For linking episodes to anime
- `local_files`: For tracking file status
- `mylist`: Updated via API

## Security Considerations
- File paths are stored temporarily in memory only
- No user input is passed directly to SQL (uses parameterized queries)
- File binding requires user to be logged in
- Manual binding respects user's mylist settings (state, storage, etc.)
