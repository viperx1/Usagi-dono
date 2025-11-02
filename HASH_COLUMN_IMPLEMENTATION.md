# Hash Column and Immediate Processing

## Problem
Files with cached hashes (status=1) were being rediscovered on every app launch but never progressing to status>=2, causing an endless rediscovery loop.

## Solution
Added a "Hash" column to the hashes UI table and modified the workflow to check for existing hashes at the start of hashing, processing them immediately instead of deferring to batch processing.

## Implementation

### 1. Added Hash Column (Column 9)
- Extended hashes table from 9 to 10 columns
- New column header: "Hash"
- Column width: 250 pixels
- Displays the ed2k hash if available from database

### 2. Modified `hashesinsertrow()`
When adding files to the hasher table:
```cpp
// Check if file already has a hash in the database
QString existingHash = adbapi->getLocalFileHash(file.absoluteFilePath());
QTableWidgetItem *item10 = new QTableWidgetItem(existingHash.isEmpty() ? QString("") : existingHash);
hashes->setItem(hashes->rowCount()-1, 9, item10);
```

### 3. Modified `ButtonHasherStartClick()`
When user clicks Start:
- Scan rows for files with progress="0" (not yet processed)
- Check column 9 (Hash) for each file
- **Files WITH hash:**
  - Process immediately: `LocalIdentify()` + `File()` + `MylistAdd()` APIs
  - Update UI immediately
  - Skip hasher thread
- **Files WITHOUT hash:**
  - Add to list for hasher thread
  - Process normally

### 4. Modified `onWatcherNewFilesDetected()`
When directory watcher detects files:
- Add files to table (automatically populates hashes)
- Separate into two lists: with hashes vs. without
- **Files WITH hash:**
  - Process immediately if logged in
  - Call APIs directly
- **Files WITHOUT hash:**
  - Start hasher thread as before

## Benefits

1. **Visible hashes:** Users can see which files have cached hashes in UI
2. **Immediate processing:** Already-hashed files bypass hasher thread completely
3. **Status progression:** Files move from status=1 to status=2/3 immediately
4. **No rediscovery loop:** Files are fully processed on first encounter
5. **More efficient:** No need to read and hash files with cached hashes
6. **Clear workflow:** User can see exactly what's happening

## User Experience

**Before:**
- File appears in hasher
- Gets hashed (or retrieves from cache)
- Waits for all files to finish
- Batch processed at end
- May get stuck at status=1

**After:**
- File appears in hasher
- Hash column shows existing hash (if available)
- Click Start:
  - Files with hashes → processed immediately
  - Files without hashes → get hashed normally
- All files progress to final status

## Technical Details

**Hash column population:**
- Happens in `hashesinsertrow()` when file is added
- Uses `getLocalFileHash()` to query database
- Empty string if no hash exists

**Immediate processing:**
- Happens in `ButtonHasherStartClick()` and `onWatcherNewFilesDetected()`
- Only if hash column is not empty
- Calls same APIs as batch processing
- Updates UI in same way

**Compatibility:**
- Existing batch processing unchanged for new files
- Existing tests continue to work
- No breaking changes to API or database
