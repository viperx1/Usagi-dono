# Directory Watcher Feature

## Overview
The Directory Watcher feature automatically monitors a specified directory for new video files, hashes them, and adds them to MyList with the "HDD unwatched" state.

## Features

### Automatic File Detection
- Monitors a directory and its subdirectories for new video files
- Detects files with common video extensions (mkv, mp4, avi, mov, wmv, flv, webm, m4v, mpg, mpeg, ogv, 3gp, ts, m2ts)
- Uses a debounce timer to avoid processing files that are still being written

### Automatic Hashing and MyList Addition
- When a new file is detected, it's automatically added to the hasher table
- The file is automatically hashed by the directory watcher
- After hashing, if the user is logged in, the file is automatically added to MyList with:
  - State: Internal (HDD)
  - Viewed: No (unwatched)
- If the user is not logged in, files are hashed but not added to MyList (can be added manually later)

### Configuration Options
Available in the Settings tab:

1. **Enable Directory Watcher**: Toggle to start/stop monitoring
2. **Watch Directory**: Path to the directory to monitor
3. **Browse Button**: Select directory via file dialog
4. **Auto-start on application launch**: Start directory watcher when the application starts

### Persistent Settings
All directory watcher settings are saved to `settings.dat` and persist across application restarts.

### Processed Files Tracking
The directory watcher keeps track of files it has already hashed to avoid duplicate processing:
- Hashed file paths are stored in the database (`local_files` table with status != 0)
- Files that have been hashed (status 1 = in AniDB, status 2 = not in AniDB) are skipped on subsequent scans
- Files with status 0 (not yet hashed) will be re-detected and processed
- This ensures only new, unhashed files trigger the auto-hash functionality

## Usage

### Setup
1. Open the application and navigate to the **Settings** tab
2. Locate the "Directory Watcher" section
3. Click the **Browse...** button to select the directory you want to monitor
4. Check the **Enable Directory Watcher** checkbox to start monitoring
5. (Optional) Check **Auto-start on application launch** to automatically start monitoring when the app starts
6. Click **Save** to save your settings

### Monitoring
- When the directory watcher is enabled, it will display "Status: Watching [directory path]"
- New video files added to the watched directory will automatically appear in the Log tab
- Files will be added to the Hasher tab and automatically hashed
- If you're logged in, hashed files will be automatically added to MyList

### Viewing Results
- Check the **Log** tab to see when new files are detected
- Check the **Hasher** tab to see the hashing progress
- Check the **MyList** tab to see the files after they're added to your list

## Implementation Details

### Class: DirectoryWatcher

**Location**: `usagi/src/directorywatcher.h`, `usagi/src/directorywatcher.cpp`

**Key Methods**:
- `startWatching(const QString &directory)`: Start monitoring a directory
- `stopWatching()`: Stop monitoring
- `isWatching()`: Check if currently monitoring
- `watchedDirectory()`: Get the currently monitored directory

**Signals**:
- `newFileDetected(const QString &filePath)`: Emitted when a new video file is detected

**Private Methods**:
- `scanDirectory()`: Scan the watched directory for new files
- `loadProcessedFiles()`: Load previously hashed files from database (status != 0)
- `saveProcessedFile(const QString &filePath)`: Save a file to database with status=0

### Integration with Window

The directory watcher is integrated into the main Window class:

**Settings UI Components**:
- `watcherEnabled`: QCheckBox to enable/disable monitoring
- `watcherDirectory`: QLineEdit to display/edit the monitored directory
- `watcherBrowseButton`: QPushButton to browse for a directory
- `watcherAutoStart`: QCheckBox to enable auto-start
- `watcherStatusLabel`: QLabel to display current status

**Slots**:
- `onWatcherEnabledChanged(int state)`: Handle enable/disable toggle
- `onWatcherBrowseClicked()`: Handle browse button click
- `onWatcherNewFileDetected(const QString &filePath)`: Handle new file detection

### File Processing Flow

1. User enables directory watcher and selects a directory
2. DirectoryWatcher starts monitoring the directory
3. When a new video file is detected:
   - `newFilesDetected` signal is emitted
   - `onWatcherNewFilesDetected` slot is called
   - File is added to the hasher table via `hashesinsertrow()`
   - The hasher is automatically started:
     - If user is logged in:
       - "Add to MyList" is enabled
       - "Mark watched" is disabled (unwatched)
       - File state is set to "Internal (HDD)"
     - File is hashed via `hashFiles()` signal
4. After hashing completes:
   - File information is retrieved from AniDB
   - If user is logged in, file is added to MyList with the configured state
   - MyList is automatically refreshed to show the new entry

## Testing

A comprehensive test suite is available in `tests/test_directorywatcher.cpp`:

**Test Cases**:
- `testInitialization`: Verify proper initialization
- `testStartWatching`: Test starting to watch a directory
- `testStopWatching`: Test stopping the watcher
- `testNewFileDetection`: Verify new files are detected
- `testVideoFileValidation`: Ensure only video files are detected
- `testInvalidDirectory`: Handle invalid directory paths
- `testProcessedFilesTracking`: Verify files aren't processed twice

Run tests with:
```bash
cd build
ctest -R test_directorywatcher -V
```

## Technical Notes

### QFileSystemWatcher
- Uses Qt's `QFileSystemWatcher` for directory monitoring
- Monitors the directory and emits signals when changes occur
- Platform-specific implementation (inotify on Linux, FSEvents on macOS, etc.)

### Debounce Timer
- 2-second debounce timer prevents processing incomplete file copies
- Timer restarts on each directory change
- Only processes files after the directory has been stable for 2 seconds

### Video File Extensions
Supported video file extensions:
- mkv, mp4, avi, mov, wmv
- flv, webm, m4v, mpg, mpeg
- ogv, 3gp, ts, m2ts

### Settings Storage
Settings are stored in `settings.dat` using Qt's QSettings (INI format):
- `watcherEnabled`: boolean
- `watcherDirectory`: string (directory path)
- `watcherAutoStart`: boolean
- `watchedFiles`: array of processed file paths

## Future Enhancements

Potential improvements for future versions:
1. **Custom file extension filters**: Allow users to specify which file types to monitor
2. **Multiple watch directories**: Monitor multiple directories simultaneously
3. **File size threshold**: Ignore files below a certain size
4. **Notification system**: Desktop notifications when new files are detected
5. **Batch processing**: Queue multiple files and process them together
6. **Storage location override**: Allow setting a custom storage location per watched directory
7. **File stability check**: Wait for file size to stabilize before processing
8. **Ignore patterns**: Allow users to specify patterns to ignore (e.g., `*sample*`)

## Troubleshooting

### Files not being detected
- Ensure the directory path is correct and exists
- Check that the "Enable Directory Watcher" checkbox is checked
- Verify the file has a supported video extension
- Check the Log tab for error messages

### Files detected but not hashed
- Files detected by the directory watcher are automatically hashed
- If the hasher is busy, files will be queued and processed after the current batch completes
- Check the Hasher tab to see the queue status

### Files not appearing in MyList
- Ensure you are logged in to AniDB (files are only added to MyList when logged in)
- Check that the file was successfully hashed (no errors in Log)
- Verify the file exists in AniDB (may need manual FILE command)
- Try refreshing MyList manually

## Related Files

- `usagi/src/directorywatcher.h` - DirectoryWatcher header
- `usagi/src/directorywatcher.cpp` - DirectoryWatcher implementation
- `usagi/src/window.h` - Main window header (includes UI components)
- `usagi/src/window.cpp` - Main window implementation (includes slots)
- `tests/test_directorywatcher.cpp` - Unit tests
- `usagi/CMakeLists.txt` - Build configuration
- `tests/CMakeLists.txt` - Test build configuration

## Commit History

- Initial implementation: Added DirectoryWatcher class and UI integration
- Test suite: Added comprehensive unit tests for directory watcher functionality
- Documentation: Created this documentation file
