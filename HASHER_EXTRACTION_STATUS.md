# HasherCoordinator Extraction Status

**Date:** 2025-12-08  
**Status:** Foundation Complete - Core Logic Extraction in Progress

## What Has Been Completed

### 1. HasherCoordinator Class Structure ✅
- Created hashercoordinator.h (161 lines)
- Created hashercoordinator.cpp (425 lines)
- Total: 586 lines of new code

### 2. UI Extraction ✅
All hasher UI widgets moved to HasherCoordinator:
- Buttons: Add Files, Add Directories, Add Last Directory, Start, Stop, Clear
- Table: hashes_ (file list with columns)
- Progress bars: Per-thread progress + total progress
- Settings: File state combo, checkboxes (add to mylist, mark watched, move to, rename to)
- Text output area
- All layout management

### 3. File Selection Logic ✅
**Fully implemented methods:**
- `addFiles()` - Select and add individual files
- `addDirectories()` - Select and add directories with multi-selection
- `addLastDirectory()` - Add files from last used directory
- `addFilesFromDirectory()` - Recursively walk directory and add files
- `shouldFilterFile()` - Check if file matches filter masks
- `updateFilterCache()` - Update compiled regex filter cache
- `hashesInsertRow()` - Add file row to hash table

### 4. Helper Methods ✅
- `calculateTotalHashParts()` - Calculate total parts for progress tracking
- `getFilesNeedingHash()` - Get list of files that need hashing
- `createUI()` - Build entire hasher UI
- `setupConnections()` - Wire all signals/slots

## What Remains To Be Done

### 1. Hasher Control Methods (Priority: HIGH)

**startHashing()** - ~100 lines
- Currently: Stub with LOG statement
- Needs: Extract from Window::ButtonHasherStartClick (lines 1476-1569)
- Logic:
  - Iterate through hash table to identify files needing hashing
  - Queue files with existing hashes for deferred processing
  - Start timer for batch processing
  - Setup progress tracking
  - Enable/disable buttons
  - Start hasher thread pool

**stopHashing()** - ~40 lines  
- Currently: Stub with LOG statement
- Needs: Extract from Window::ButtonHasherStopClick (lines 1571-1609)
- Logic:
  - Enable/disable buttons
  - Reset progress bars
  - Reset progress of partially processed files
  - Broadcast stop signal to hasher threads
  - Signal thread pool to stop

**clearHasher()** - ✅ DONE
- Simply clears the hash table

### 2. File Hashing Coordination (Priority: HIGH)

**onFileHashed()** - ~150 lines
- Currently: Stub with LOG statement
- Needs: Extract from Window::getNotifyFileHashed (lines 1720-1869)
- Logic:
  - Find file in hash table
  - Update UI with hash result
  - Update hash column
  - Call AniDB API methods (LocalIdentify, File, MylistAdd)
  - Update database
  - Link local file to mylist
  - Handle error cases

**Challenge:** This method needs access to:
- AniDB API instance (m_adbapi)
- Checkbox states (addtomylist, markwatched, hasherFileState, storage)
- Window's getNotifyLogAppend method

**provideNextFileToHash()** - ~30 lines
- Currently: Stub with LOG statement
- Needs: Extract from Window::provideNextFileToHash (lines 1611-1637)
- Logic:
  - Thread-safe file assignment (uses mutex)
  - Find next file needing hashing
  - Mark file as assigned
  - Add to hasher thread pool
  - Signal completion when no more files

### 3. Progress Tracking (Priority: MEDIUM)

**setupHashingProgress()** - ~20 lines
- Currently: Stub
- Needs: Extract from Window::setupHashingProgress (lines 1445-1474)
- Logic:
  - Calculate total hash parts
  - Initialize progress tracker
  - Reset progress bars

**onProgressUpdate()** - PARTIAL ✅
- Currently: Updates thread progress bars
- Working for basic progress display

**onHashingFinished()** - ~30 lines
- Currently: Stub that emits hashingFinished signal
- Needs: Extract from Window::hasherFinished (lines 2272-2290)
- Logic:
  - Re-enable buttons
  - Clear progress bars
  - Log completion
  - Emit signal

### 4. Batch Processing (Priority: MEDIUM)

**processPendingHashedFiles()** - ~50 lines
- Currently: Stub
- Needs: Extract from Window::processPendingHashedFiles (lines 2233-2289)
- Logic:
  - Process pre-hashed files in batches
  - Update database
  - Call AniDB APIs
  - Stop timer when queue empty

### 5. Settings Handlers (Priority: LOW)

**onMarkWatchedStateChanged()** - ~10 lines
- Currently: Stub
- Needs: Extract from Window::markwatchedStateChanged (lines 1722-1738)
- Simple checkbox state handler

## Design Challenges

### Challenge 1: AniDB API Access
HasherCoordinator needs to call AniDB API methods like:
- `LocalIdentify(size, hash)`
- `File(size, hash)`
- `MylistAdd(size, hash, watched, state, storage)`
- `UpdateLocalFileStatus(path, status)`
- `LinkLocalFileToMylist(size, hash, path)`

**Current Solution:** HasherCoordinator has reference to AniDBApi via m_adbapi

### Challenge 2: Checkbox/Setting Access
Methods like onFileHashed() need to read checkbox states:
- addtomylist->checkState()
- markwatched->checkState()
- hasherFileState->currentIndex()
- storage->text()

**Options:**
1. Store references to these widgets in HasherCoordinator
2. Cache values when Start is clicked
3. Have Window pass values as parameters
4. Emit signals and let Window handle API calls

**Recommended:** Option 2 - Cache settings when startHashing() is called

### Challenge 3: Log Output
Currently uses Window's getNotifyLogAppend() method.

**Solution:** Emit logMessage() signal, Window connects to it

### Challenge 4: Database Updates
Needs to update local database via AniDBApi methods.

**Solution:** Keep AniDBApi reference, it's a core dependency

## Integration Plan

### Phase A: Complete Core Methods (2-4 hours)
1. Implement startHashing() with setting caching
2. Implement stopHashing()
3. Implement provideNextFileToHash()
4. Implement setupHashingProgress()

### Phase B: File Processing (3-5 hours)
1. Implement onFileHashed() with API calls
2. Implement processPendingHashedFiles()
3. Handle all edge cases

### Phase C: Window Integration (2-3 hours)
1. Create HasherCoordinator instance in Window constructor
2. Replace hasher tab with coordinator's widget
3. Connect signals from coordinator to Window
4. Remove old hasher code from Window
5. Test all functionality

### Phase D: Cleanup (1-2 hours)
1. Remove unused methods from Window
2. Remove unused member variables
3. Update includes
4. Run linter/formatter
5. Final testing

## Estimated Total Remaining Work: 8-14 hours

## Current Files

```
usagi/src/hashercoordinator.h    161 lines
usagi/src/hashercoordinator.cpp  425 lines
usagi/src/window.h               510 lines (will reduce by ~60)
usagi/src/window.cpp            6046 lines (will reduce by ~800)
```

## Expected Final State

```
usagi/src/hashercoordinator.h    170 lines
usagi/src/hashercoordinator.cpp  850 lines  
usagi/src/window.h               450 lines (-60)
usagi/src/window.cpp            5200 lines (-846)
```

## Notes

- This is a large, complex extraction
- HasherCoordinator is mostly self-contained but needs AniDB API access
- Signal/slot pattern used for coordination with Window
- Foundation is solid, remaining work is methodical extraction
- All changes compile successfully
- No functionality has been broken yet (stubs are safe)

