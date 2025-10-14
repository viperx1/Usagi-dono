# MyList UI Simplification - Auto-load Implementation

## Overview
Simplified the MyList tab interface by removing manual buttons and implementing automatic loading on application startup.

## Changes Made

### UI Simplification
**Removed**:
- "Load MyList from Database" button
- "Fetch MyList Stats from API" button
- "Download MyList from AniDB" button
- "Import MyList from File" button

**Added**:
- Status label showing current mylist state
- Automatic loading on startup (1 second delay)

### Code Cleanup
**Removed Functions**:
- `fetchMylistStatsFromAPI()` - Manual stats fetch
- `downloadMylistExport()` - Manual download dialog
- `importMylistFromFile()` - Manual file import

**Removed Includes**:
- `QDesktopServices` - No longer needed for browser opening
- `QInputDialog` - No longer needed for template selection
- `QUrl` - No longer needed for URL handling

### New Functionality

**Auto-load on Startup**:
```cpp
// In Window constructor
QTimer::singleShot(1000, this, [this]() {
    mylistStatusLabel->setText("MyList Status: Loading from database...");
    loadMylistFromDatabase();
    mylistStatusLabel->setText("MyList Status: Ready");
});
```

**Status Updates**:
```cpp
// In loadMylistFromDatabase()
mylistStatusLabel->setText(QString("MyList Status: %1 entries loaded").arg(totalEntries));
```

## User Experience

### Before
- User had to click "Load MyList from Database" to see data
- 4 buttons cluttering the interface
- Manual workflow required

### After
- MyList loads automatically on startup
- Clean interface with just status label
- No user interaction needed
- Status shows current state

## Status Label States

1. **"MyList Status: Ready"** - Initial state
2. **"MyList Status: Loading from database..."** - During load
3. **"MyList Status: X entries loaded"** - After successful load

## Technical Details

### Automatic Loading
- Triggered 1 second after application starts
- Uses QTimer::singleShot() for delayed execution
- Ensures UI is fully initialized before loading

### Status Label
- QLabel widget with centered alignment
- Light gray background (#f0f0f0)
- 5px padding for visual spacing
- Updates dynamically based on state

## Benefits

1. **Simplified UI**: No button clutter
2. **Automatic**: No manual steps required
3. **Informative**: Status always visible
4. **Clean**: Minimal, focused interface

## Future Enhancements (Not Implemented)

### Automatic HTTP Download
To implement automatic download from AniDB would require:

1. **Web Authentication**:
   - Session cookie management
   - CSRF token handling
   - Login form submission

2. **Export Download**:
   - Navigate to export page
   - Select csv-adborg template
   - Trigger export
   - Download file

3. **Auto-import**:
   - Parse downloaded data
   - Import to database
   - Update UI

This would require significant implementation including:
- QNetworkCookieJar for session management
- HTML parsing for CSRF tokens
- Form submission handling
- File download and temporary storage
- Integration with existing parser

## Commit
- Hash: 30399b2
- Files changed: 2 (window.h, window.cpp)
- Lines changed: +14, -136

## Testing
Manual testing required (Qt environment):
- [ ] Application starts without errors
- [ ] Status label appears in MyList tab
- [ ] MyList loads automatically after 1 second
- [ ] Status updates to show entry count
- [ ] No buttons visible in MyList tab
