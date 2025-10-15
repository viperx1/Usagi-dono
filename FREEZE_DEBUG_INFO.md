# Debug Information Added for App Freeze Issue

## Issue
App freezes after the last commit. Added comprehensive debug logging to diagnose where the freeze occurs.

## Root Cause Analysis

Based on code analysis, identified three potential sources of UI freezing:

1. **Synchronous DNS Lookup (Line 12 in constructor)**
   - `QHostInfo::fromName("api.anidb.net")` performs a blocking DNS resolution
   - Can take several seconds if DNS is slow or unavailable
   - Blocks the thread during constructor initialization

2. **Anime Titles Download and Processing**
   - Downloads anime titles database on first run or after 24 hours
   - Decompresses gzip data (can be several MB)
   - Processes potentially thousands of anime titles in a single database transaction
   - All happens during constructor or shortly after

3. **Large Database Transaction**
   - `parseAndStoreAnimeTitles()` processes all anime titles in one transaction
   - Can insert thousands of records without yielding control
   - Blocks UI thread during processing

## Solution

Added comprehensive debug logging at all critical points to help identify exactly where the freeze occurs:

### 1. Constructor Initialization Tracking
- Log when constructor starts
- Log before and after DNS lookup
- Log database connection setup
- Log database transaction start/commit
- Log settings reading
- Log network manager initialization
- Log packet sender timer setup
- Log anime titles update check
- Log constructor completion

### 2. Anime Titles Processing Tracking
- Log when checking if update is needed
- Log last update timestamp and decision
- Log download callback trigger
- Log decompression start/completion
- Log parsing start with data size
- Log database transaction operations
- **Log progress every 1000 titles** to show it's not frozen
- Log transaction commit
- Log final completion

### 3. Debug Message Categories
All debug messages use clear category prefixes:
- `[AniDB Init]` - Constructor initialization steps
- `[AniDB Anime Titles]` - Anime titles download and processing
- `[AniDB Error]` - Error conditions

## Changes Made

### File: usagi/src/anidbapi.cpp

**Constructor (lines 4-114):**
- Added 15 debug log statements tracking each initialization stage
- Specifically logs DNS lookup timing (before/after)
- Logs database operations with transaction boundaries
- Logs anime titles update decision

**shouldUpdateAnimeTitles() (lines 914-935):**
- Added debug logging showing whether update is needed
- Shows seconds since last update for troubleshooting

**onAnimeTitlesDownloaded() (lines 947-1055):**
- Added debug logging for download callback
- Logs decompression operations
- Logs when parsing begins and completes

**parseAndStoreAnimeTitles() (lines 1057-1117):**
- Added debug logging for parsing start with data size
- Logs database transaction operations
- **Critical: Logs progress every 1000 titles** to show the app is not frozen
- Logs transaction commit timing

## Expected Output

When the app starts, the log will show:

```
[AniDB Init] Constructor started
[AniDB Init] Starting DNS lookup for api.anidb.net (this may block)
[AniDB Init] DNS lookup completed
[AniDB Init] DNS resolved successfully to 89.163.218.20
[AniDB Init] Setting up database connection
[AniDB Init] Opening database
[AniDB Init] Database opened, starting transaction
[AniDB Init] Committing database transaction
[AniDB Init] Database transaction committed
[AniDB Init] Reading settings from database
[AniDB Init] Initializing network manager
[AniDB Init] Setting up packet sender timer
[AniDB Init] Checking if anime titles need update
[AniDB Anime Titles] Checking if anime titles need update
[AniDB Anime Titles] No previous download found, download needed
[AniDB Init] Starting anime titles download
[AniDB Init] Constructor completed successfully
```

If anime titles are being downloaded:
```
[AniDB Anime Titles] Download callback triggered
Downloaded 1234567 bytes of compressed anime titles data
[AniDB Anime Titles] Detected gzip format, using zlib decompression
[AniDB Anime Titles] Starting inflate operation (this may take a moment)
[AniDB Anime Titles] Decompression completed successfully
Decompressed to 12345678 bytes
[AniDB Anime Titles] Starting to parse and store titles
[AniDB Anime Titles] Starting to parse anime titles data (12345678 bytes)
[AniDB Anime Titles] Starting database transaction for 234567 lines
[AniDB Anime Titles] Clearing old anime titles from database
[AniDB Anime Titles] Processing progress: 1000 titles inserted
[AniDB Anime Titles] Processing progress: 2000 titles inserted
...
[AniDB Anime Titles] Committing database transaction with 50000 titles
[AniDB Anime Titles] Parsed and stored 50000 anime titles successfully
[AniDB Anime Titles] Finished parsing and storing titles
Anime titles updated successfully at ...
```

## Diagnosis Instructions

With these debug logs, we can now identify exactly where the freeze occurs:

1. **If logs stop at "Starting DNS lookup":**
   - DNS resolution is hanging
   - Network connectivity issue
   - DNS server not responding

2. **If logs stop during "Opening database" or transaction:**
   - Database file locked or corrupted
   - Filesystem issue

3. **If logs stop after "Starting anime titles download":**
   - Network download hanging
   - Server not responding

4. **If logs show "Processing progress" messages but UI is frozen:**
   - Database transaction is taking too long
   - Too many titles being processed
   - Need to process in smaller batches or background thread

5. **If logs stop at a specific progress count:**
   - Indicates problematic data at that point
   - Helps narrow down the issue

## Testing

Since Qt6 is not available in the current environment, the changes have been:
- Reviewed for syntax correctness
- Verified to use existing Debug() infrastructure
- Checked to follow existing code patterns
- Confirmed to use proper C++ string concatenation

The changes are purely additive (adding debug logging) and do not modify any business logic, making them very low risk.

## Impact

- **Type**: Debug logging enhancement
- **Risk**: Minimal - only adds logging, no logic changes
- **Performance**: Negligible - debug messages already exist elsewhere
- **Benefit**: Will identify exact location of freeze for targeted fix
