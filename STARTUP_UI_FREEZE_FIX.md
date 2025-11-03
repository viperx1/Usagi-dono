# Startup UI Freeze Fix (Directory Watcher)

## Problem Statement

On startup, when the directory watcher discovers files, the application begins hashing and the UI becomes completely unresponsive. This issue is different from the previous UI freeze fix (which addressed freezes during hashing progress updates).

## Root Cause

The issue occurs in `window.cpp:onWatcherNewFilesDetected()` function (lines 2185-2242 before the fix). When the directory watcher detects files on startup:

1. Files are added to the hasher table (fast operation)
2. The function then loops through **ALL files** with existing hashes in the hasher table
3. For each file with an existing hash, it performs:
   - Synchronous file I/O (`QFileInfo fileInfo(filePath); fileInfo.size()`)
   - Database update (`adbapi->updateLocalFileHash()`)
   - Database query (`adbapi->LocalIdentify()`)
   - Potentially multiple API calls (`adbapi->File()`, `adbapi->MylistAdd()`)
   - Multiple database status updates (`adbapi->UpdateLocalFileStatus()`)

4. With hundreds or thousands of files in the database, this **entire loop runs synchronously in the UI thread**, blocking it completely

### Why This Was a Problem

- **Synchronous I/O**: Reading file size from disk for each file
- **Synchronous DB Operations**: Multiple database queries and updates per file
- **Blocking API Calls**: API call queuing operations for each file
- **UI Thread Blocking**: All of this happens in the main thread without yielding to the event loop
- **Scale**: With 1000 files, this could mean 1000+ file I/O operations and 5000+ database operations all at once

## Solution

Implemented **deferred batch processing** using Qt's timer mechanism:

### Changes Made

#### 1. Added Data Structures (`window.h`)

```cpp
// Deferred processing for already-hashed files to prevent UI freeze
struct HashedFileInfo {
    int rowIndex;
    QString filePath;
    QString filename;
    QString hexdigest;
    qint64 fileSize;
};
QList<HashedFileInfo> pendingHashedFilesQueue;
QTimer *hashedFilesProcessingTimer;
void processPendingHashedFiles();
```

#### 2. Initialized Timer (`window.cpp` constructor)

```cpp
// Initialize timer for deferred processing of already-hashed files
hashedFilesProcessingTimer = new QTimer(this);
hashedFilesProcessingTimer->setSingleShot(false);
hashedFilesProcessingTimer->setInterval(10); // Process in small chunks every 10ms
connect(hashedFilesProcessingTimer, &QTimer::timeout, 
        this, &Window::processPendingHashedFiles);
```

**Key parameters:**
- `setSingleShot(false)`: Timer repeats automatically
- `setInterval(10)`: Fires every 10 milliseconds
- This allows ~100 timer ticks per second

#### 3. Modified `onWatcherNewFilesDetected()` to Queue Instead of Process

**Before (blocking):**
```cpp
// Process files with existing hashes immediately
for (const auto& pair : filesWithHashes) {
    // ... 50+ lines of synchronous processing per file ...
}
```

**After (non-blocking):**
```cpp
// Queue files with existing hashes for deferred processing
for (const auto& pair : filesWithHashes) {
    // Just collect file info - no I/O or database operations yet
    HashedFileInfo info;
    info.rowIndex = rowIndex;
    info.filePath = filePath;
    info.filename = filename;
    info.hexdigest = hexdigest;
    info.fileSize = fileInfo.size(); // Only I/O operation here
    pendingHashedFilesQueue.append(info);
}

// Start timer to process queued files in batches
if (!filesWithHashes.isEmpty()) {
    Logger::log(QString("Queued %1 already-hashed file(s) for deferred processing")
                .arg(filesWithHashes.size()));
    hashedFilesProcessingTimer->start();
}
```

#### 4. Implemented `processPendingHashedFiles()` Method

Processes files in small batches to keep UI responsive:

```cpp
void Window::processPendingHashedFiles()
{
    // Process files in small batches to keep UI responsive
    const int batchSize = 5; // Process 5 files per timer tick
    int processed = 0;
    
    while (!pendingHashedFilesQueue.isEmpty() && processed < batchSize) {
        HashedFileInfo info = pendingHashedFilesQueue.takeFirst();
        processed++;
        
        // [Process the file - same logic as before, just deferred]
        // - Mark as hashed in UI
        // - Update database
        // - Perform LocalIdentify
        // - Queue API calls if needed
    }
    
    // Stop timer if queue is empty
    if (pendingHashedFilesQueue.isEmpty()) {
        hashedFilesProcessingTimer->stop();
        Logger::log("Finished processing all already-hashed files");
    }
}
```

**Key features:**
- Processes **only 5 files per timer tick**
- Returns control to the event loop after each batch
- Automatically stops when queue is empty

## Performance Characteristics

### Processing Rate
- Batch size: 5 files
- Timer interval: 10ms
- Theoretical max: ~500 files/second (5 × 100 ticks/sec)
- Actual rate depends on I/O and database performance

### UI Responsiveness
- Event loop gets control every 10ms
- UI remains responsive even with 1000s of files
- User can interact with the application immediately on startup

### Comparison

| Scenario | Before (Blocking) | After (Deferred) |
|----------|------------------|------------------|
| 100 files | UI frozen ~5-10 seconds | UI responsive, completes in ~200ms |
| 1000 files | UI frozen ~50-100 seconds | UI responsive, completes in ~2 seconds |
| 10000 files | UI frozen ~500-1000 seconds | UI responsive, completes in ~20 seconds |

*Times are estimates based on typical I/O and database performance*

## Benefits

1. **Immediate UI Responsiveness**: User can interact with the application immediately
2. **Background Processing**: Files are still processed, just not all at once
3. **Smooth User Experience**: No apparent freeze or hang
4. **Scalable**: Works with any number of files in database
5. **Visible Progress**: Users can see files being processed gradually
6. **Cancellable**: If needed, timer can be stopped programmatically

## Related Issues and Fixes

This fix complements the previous UI freeze fix documented in `UI_FREEZE_FIX.md`:
- **Previous fix**: Throttled progress bar updates during hashing (from 1000s/sec to ~10/sec)
- **This fix**: Deferred processing of already-hashed files at startup (from blocking to batched)

Together, these fixes ensure the UI remains responsive both during:
1. Initial file discovery and processing (this fix)
2. Active file hashing operations (previous fix)

## Testing

### Manual Testing
1. Add 100+ files to a directory being watched
2. Restart the application
3. Observe:
   - UI should be immediately responsive
   - Files should appear in the hasher table
   - Files should be gradually processed (visible in Log tab)
   - No freezing or hanging behavior

### Expected Behavior
- Application window opens immediately
- All UI controls are responsive
- Files are added to hasher table quickly
- Processing happens in background
- Log messages show progress: "Queued N already-hashed file(s) for deferred processing"
- Log messages eventually show: "Finished processing all already-hashed files"

## Implementation Notes

### Why 5 Files per Batch?
- Small enough to complete quickly (< 10ms typically)
- Large enough to make progress at reasonable rate
- Tunable via `batchSize` constant if needed

### Why 10ms Timer Interval?
- Fast enough for smooth progress (~100 updates/second)
- Slow enough to not overwhelm event loop
- Standard interval for responsive UI updates
- Allows ~50-100ms for user input response time

### Thread Safety
- All processing happens in main UI thread (no additional threads)
- Qt's event loop handles scheduling naturally
- No mutex or locking needed

### Memory Usage
- Queue holds file info structs (small - ~100 bytes per file)
- 1000 files = ~100KB of memory (negligible)
- Queue is cleared as files are processed

### Design Tradeoff: File Size Reading

The implementation reads file size (`QFileInfo::size()`) during the queuing phase, not in deferred processing:

**Why This Is Acceptable:**
- `QFileInfo::size()` reads file **metadata** (not file content) - typically < 1ms per file
- Worst case: 1000 files = ~1 second total for metadata reads
- Original problem: 50-100 seconds of UI freeze from database queries and API calls
- The file size is required for subsequent API calls (LocalIdentify, File, MylistAdd)

**Alternative Considered:**
- Deferring file size reading to batch processing
- **Rejected because**: 
  - Adds complexity: files could be deleted between queue and process
  - Minimal benefit: metadata reads are 50-100x faster than database operations
  - Not the bottleneck: the freeze was caused by DB/API operations, not file I/O

**Verdict**: The current implementation strikes the right balance between simplicity and performance. The file metadata reads are negligible (1-2% of original problem) and don't justify the added complexity of deferring them.

## Future Enhancements

Potential improvements if needed:
1. **Adaptive Batch Size**: Adjust based on processing time
2. **Priority Queue**: Process visible files first
3. **Pause/Resume**: Allow user to pause background processing
4. **Progress Indicator**: Show progress bar for background processing
5. **Move to Worker Thread**: For true parallel processing (more complex)

## Files Modified

- `usagi/src/window.h` - Added data structures and method declaration
- `usagi/src/window.cpp` - Implemented deferred processing logic

## Commit

- Commit: `e5041e8` - "Fix UI freeze on startup by deferring already-hashed file processing"
- Files changed: 2
- Lines added: 98
- Lines removed: 51
- Net change: +47 lines
