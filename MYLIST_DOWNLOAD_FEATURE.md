# MyList Export Download Feature

## Overview
Added functionality to streamline the MyList export download process from AniDB, with template selection support.

## New Feature: "Download MyList from AniDB" Button

### What It Does
1. Shows a dialog allowing users to select their preferred export template format
2. Opens AniDB's export page in the default browser with the selected template pre-selected
3. Guides users to download and then import the file

### Supported Templates
- **csv-adborg** (Recommended) - Optimized format with aid,eid,gid,lid,status,viewdate structure
- **xml** - Full structured data in XML format
- **csv** - Standard comma-separated values format
- **json** - JSON format for modern applications
- **anidb** - AniDB's default template format

### User Workflow
1. Click "Download MyList from AniDB" button
2. Select desired template format from dropdown (defaults to csv-adborg)
3. Browser opens to AniDB export page with template pre-selected
4. Log in to AniDB (if not already logged in)
5. Click "Export" to download the file
6. Return to Usagi-dono and click "Import MyList from File"
7. Select the downloaded file
8. Data is imported automatically

### Technical Implementation

#### UI Changes
Added new button between "Fetch MyList Stats from API" and "Import MyList from File":
```cpp
QPushButton *mylistDownloadButton = new QPushButton("Download MyList from AniDB");
```

#### Function: downloadMylistExport()
```cpp
void Window::downloadMylistExport()
{
    // 1. Show template selection dialog
    // 2. Build URL with selected template
    // 3. Open URL in default browser
    // 4. Log instructions for user
}
```

#### URL Format
```
https://anidb.net/user/export?template={template_name}&list=mylist
```

Where `{template_name}` is one of: csv-adborg, xml, csv, json, anidb

### Advantages
- **User-friendly**: Single click to access export page with proper template
- **No authentication needed**: Uses browser's existing AniDB session
- **Template selection**: User can choose their preferred format
- **Seamless integration**: Works with existing import functionality
- **No additional dependencies**: Uses standard Qt classes (QDesktopServices, QInputDialog)

### Code Changes
**Modified Files**:
- `usagi/src/window.h` - Added function declaration and necessary includes
- `usagi/src/window.cpp` - Added button, function implementation, and dialog

**New Includes**:
- `<QDesktopServices>` - For opening URLs in browser
- `<QInputDialog>` - For template selection dialog
- `<QUrl>` - For URL handling

### Benefits Over Manual Process
**Before**:
1. User manually navigates to https://anidb.net/user/export
2. User selects template from many options (may not know which is best)
3. User exports and downloads
4. User clicks "Import MyList from File"
5. User selects downloaded file

**After**:
1. User clicks "Download MyList from AniDB"
2. User selects recommended template (csv-adborg by default)
3. Browser opens to export page with template pre-selected
4. User exports and downloads
5. User clicks "Import MyList from File"
6. User selects downloaded file

**Improvement**: One less navigation step, recommended template highlighted, proper template pre-selected.

### Future Enhancements (Optional)
1. **API-based template list**: Query AniDB to get current list of available templates
2. **Direct download with authentication**: Implement session-based download (complex, requires credential management)
3. **Remember last selected template**: Save user preference for future exports
4. **Auto-import**: Automatically import after download completes (requires browser extension or native messaging)

### Security Considerations
- No credentials are stored or transmitted by Usagi-dono
- Uses browser's existing authentication (cookies/session)
- URL is HTTPS (secure connection)
- No risk of credential exposure

### Compatibility
- Works on all platforms (Windows, Linux, macOS)
- Requires default browser to be configured
- No additional software needed

## Testing
Manual testing required (Qt environment needed):
1. Click "Download MyList from AniDB" button
2. Verify dialog appears with template options
3. Select "csv-adborg (Recommended)" and click OK
4. Verify browser opens to https://anidb.net/user/export?template=csv-adborg&list=mylist
5. Verify log output shows appropriate messages
6. Export data from AniDB
7. Import using "Import MyList from File" button
8. Verify data imports correctly

## Documentation Updates
- Updated user guides to mention new download feature
- Added workflow diagrams showing the new process
- Documented all supported template formats
