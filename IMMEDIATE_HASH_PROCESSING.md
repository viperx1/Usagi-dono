# Immediate Processing of Already-Hashed Files

## Problem Statement
Previously, when files with cached hashes were discovered (e.g., files with status=1 that were rediscovered on app restart), they would:
1. Be added to the hasher queue
2. Have their hash retrieved from the database
3. Emit the `notifyFileHashed` signal
4. Be accumulated in `pendingHashedFiles`
5. Wait for ALL files to finish hashing
6. Finally be processed in batch during `hasherFinished()`

This caused files to be "rediscovered" on every app launch but never fully processed (never reaching status >= 2) if the batch processing was interrupted or if other conditions weren't met.

## Solution
Modified `Window::getNotifyFileHashed()` to immediately process files that already have a matching hash in the database, rather than deferring them to batch processing.

### Implementation Details

When `getNotifyFileHashed()` is called:

1. **Check if file was already hashed:**
   - Query the database for existing hash using `getLocalFileHash(filePath)`
   - Compare existing hash with the newly computed/retrieved hash
   
2. **If file was already hashed (cache hit with matching hash):**
   - Log: "Processing already-hashed file immediately"
   - Update database with status=1
   - If `addtomylist` is checked:
     - Call `LocalIdentify()` immediately
     - Call `File()` API if not in database
     - Call `MylistAdd()` API if not in mylist
   - Update UI immediately with results
   
3. **If file was newly hashed or hash changed:**
   - Accumulate in `pendingHashedFiles` for batch processing
   - Accumulate in `pendingHashUpdates` for batch database update
   - Process in `hasherFinished()` as before

### Benefits

1. **Immediate feedback:** Already-hashed files are processed and updated in the UI immediately
2. **Status progression:** Files quickly move from status=1 to status=2 or 3, preventing rediscovery
3. **Better user experience:** Users see progress immediately for cached files
4. **Solves the rediscovery issue:** Files that were stuck at status=1 are now fully processed on first encounter

### Edge Cases Handled

- **Hash mismatch:** If file content changed (hash doesn't match), treat as new file
- **Thread safety:** Signal/slot mechanism ensures all operations run in main GUI thread
- **Database state:** Handles cases where file exists in database but with different hash

## Testing

Existing tests in `test_hash_reuse.cpp` verify that:
- Cached hashes are correctly retrieved and signals are emitted
- Progress signals are emitted even for cached hashes

The new behavior maintains compatibility with existing batch processing for freshly-hashed files.
