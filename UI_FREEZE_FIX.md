# UI Freeze and Immediate File Identification Fix

## Problem Statement

The application experienced two main issues:

1. **UI freezes during hashing**: The user interface would become completely unresponsive while hashing files
2. **Delayed file identification**: File identification (both local and API) was batched and only happened after ALL files were hashed, causing significant delays

## Root Causes

### UI Freezes
The hashing process emits `notifyPartsDone` signal for every 102KB chunk read. For large files, this results in thousands of signals being sent from the HasherThread to the main UI thread. The `getNotifyPartsDone()` function was updating progress bars and labels on every single signal, causing excessive UI repaints and making the interface feel frozen.

### Delayed Identification
The `getNotifyFileHashed()` function was accumulating all hashed files into `pendingHashedFiles` list. File identification only happened in `hasherFinished()` after ALL files were hashed, using batch `LocalIdentify`. This meant users had to wait for the entire batch to hash before any identification happened.

## Solution

### 1. Throttled UI Updates (Fix for UI Freezes)

Modified `getNotifyPartsDone()` in `window.cpp` to throttle UI updates to at most once every 100ms:

```cpp
// Throttle UI updates to prevent freezing - update at most every 100ms
static QElapsedTimer uiUpdateTimer;
if (uiUpdateTimer.elapsed() >= 100 || isLastPart) {
    // Update progress bars and labels
    progressFile->setValue(done);
    progressTotal->setValue(completedHashParts);
    // ... etc
    uiUpdateTimer.restart();
}
```

**Impact**: Reduces UI updates from potentially thousands per second to ~10 per second, making the UI responsive while still showing smooth progress.

### 2. Immediate File Identification (Fix for Delayed Identification)

Modified `getNotifyFileHashed()` in `window.cpp` to process file identification immediately:

```cpp
void Window::getNotifyFileHashed(ed2k::ed2kfilestruct data)
{
    // ... UI updates ...
    
    // Process file identification immediately instead of batching
    if (addtomylist->checkState() > 0)
    {
        // Update hash in database immediately
        adbapi->updateLocalFileHash(filePath, data.hexdigest, 1);
        
        // Perform LocalIdentify immediately
        std::bitset<2> li = adbapi->LocalIdentify(data.size, data.hexdigest);
        
        // Make API calls immediately if needed
        if(li[AniDBApi::LI_FILE_IN_DB] == 0)
        {
            tag = adbapi->File(data.size, data.hexdigest);
            hashes->item(i, 5)->setText(tag);
        }
        
        if(li[AniDBApi::LI_FILE_IN_MYLIST] == 0)
        {
            tag = adbapi->MylistAdd(data.size, data.hexdigest, ...);
            hashes->item(i, 6)->setText(tag);
        }
    }
}
```

**Impact**: File identification happens immediately as soon as each file is hashed, not waiting for the entire batch. Users see results faster and can take action sooner.

### 3. Simplified hasherFinished()

Removed batch identification processing from `hasherFinished()`:

```cpp
void Window::hasherFinished()
{
    // Only handle remaining hash database updates
    if (!pendingHashUpdates.isEmpty())
    {
        adbapi->batchUpdateLocalFileHashes(pendingHashUpdates, 1);
        pendingHashUpdates.clear();
    }
    
    // Note: File identification now happens immediately in getNotifyFileHashed()
    
    buttonstart->setEnabled(1);
    buttonclear->setEnabled(1);
}
```

**Impact**: Simplified code, removed `pendingHashedFiles` list and associated batch processing logic.

## Performance Considerations

### Batch vs. Immediate Processing

The original design used batch processing with the assumption that batching would be more efficient. However:

1. **LocalIdentify is already fast**: It's just a database query, so batching provides minimal benefit
2. **API calls are rate-limited**: AniDB API has rate limits, so batch or sequential doesn't matter much
3. **User experience**: Immediate feedback is much more important than marginal efficiency gains

### Why Keep Some Batching?

Hash database updates (`pendingHashUpdates`) are still batched because:
1. Database writes can benefit from batching
2. These are background operations that don't affect user-visible progress
3. They don't block file identification

## Testing

Added `test_immediate_identification.cpp` to verify:
1. LocalIdentify can be called immediately after obtaining a hash
2. Multiple files can be identified sequentially without batching
3. The workflow supports immediate processing

## Migration Notes

- Removed `pendingHashedFiles` list and `HashedFileData` struct from `window.h`
- `batchLocalIdentify()` function is still available but no longer used in the main hashing workflow
- Pre-hashed files (those with existing hashes) still process immediately as before

## Related Issue

This fix addresses the issue reported in GitHub issue about "file identification and ui freezes":
> "during hashing ui freezes completly. also file identification (both local and api) should be initiated immediately after file hash is obtained. batch processing should only happen if hashing is faster than identification."
