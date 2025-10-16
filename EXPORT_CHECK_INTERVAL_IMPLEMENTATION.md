# Export Check Interval Implementation

## Overview
This document describes the implementation of the dynamic export check interval feature for AniDB MyList exports with 48-hour monitoring and restart persistence.

## Requirements
- Set periodic check for export to 1 minute initially
- Increase the check interval by 1 minute every time there is no new notification
- Cap the interval at 60 minutes to avoid very long waits
- Continue checking for up to 48 hours (exports can take up to 24 hours per AniDB API)
- Persist export queue state across application restarts
- On startup, check for existing export notification before making a new request
- Reset the interval to 1 minute on success (when export notification is found)

## Implementation Details

### Changes Made

#### 1. Added New Member Variables (`anidbapi.h`)
- Added `int notifyCheckIntervalMs` to track the current check interval in milliseconds
- Added `qint64 exportQueuedTimestamp` to track when the export was queued
- Added `void saveExportQueueState()` to persist state to database
- Added `void loadExportQueueState()` to restore state from database
- Added `void checkForExistingExport()` to check for existing export on startup

#### 2. Initialization (`anidbapi.cpp`)
- Initialize `notifyCheckIntervalMs` to 60000 ms (1 minute) in constructor
- Initialize `exportQueuedTimestamp` to 0
- Call `loadExportQueueState()` to restore any pending export state from previous session
- Set initial timer interval to 1 minute when export is queued

#### 3. State Persistence
- Export queue state is saved to the database after each check and when export is found
- State includes: `isExportQueued`, `notifyCheckAttempts`, `notifyCheckIntervalMs`, `exportQueuedTimestamp`
- State is loaded on startup and export checking resumes if a pending export is found
- On startup with pending export, check for existing notification first before requesting a new one

#### 4. Dynamic Interval Adjustment (`checkForNotifications()`)
- After each check without finding an export notification:
  - Increment `notifyCheckIntervalMs` by 60000 ms (1 minute)
  - Cap at 3600000 ms (60 minutes) to avoid very long waits
  - Update the timer with the new interval
  - Save state to database
- Check if 48 hours have elapsed since export was queued
  - If yes, stop checking and clear state
- Enhanced debug logging to show current interval, attempt number, and elapsed time

#### 5. Reset on Success
- When export notification is detected (contains .tgz link):
  - Stop the timer
  - Reset `notifyCheckIntervalMs` to 60000 ms (1 minute)
  - Reset `notifyCheckAttempts` to 0
  - Clear `exportQueuedTimestamp`
  - Save cleared state to database
  - This ensures the next export starts with a 1-minute interval

### Behavior

#### Check Interval Progression
When an export is queued, the check intervals progress as follows:
1. First check: 1 minute after queue
2. Second check: 2 minutes after first check
3. Third check: 3 minutes after second check
4. ...
5. 60th check: 60 minutes after 59th check
6. Subsequent checks: Every 60 minutes (capped)
7. Continues until 48 hours elapse or export is found

**Total monitoring time**: Up to 48 hours (172800 seconds)

With intervals of 1, 2, 3, ... 60, 60, 60... minutes, the export can take up to 24 hours (per API definition) and we monitor for 48 hours to be safe.

#### Restart Behavior
When the application is restarted with a pending export:
1. State is loaded from database on initialization
2. After 5 seconds (allowing time for login), `checkForExistingExport()` is called
3. Checks if 48 hours have elapsed - if yes, clears state
4. If still valid, requests NOTIFYLIST to check for existing export notification
5. Resumes periodic checking with the saved interval
6. If export notification is found in existing notifications, stops checking
7. Otherwise, continues periodic checks as normal

This avoids requesting a new export unnecessarily when one is already in progress.

#### Reset Scenarios
The interval and state are reset in the following cases:
- When an export notification is successfully found
- After 48 hours have elapsed since export was queued
- State is persisted to survive application restarts

### Code Changes Summary

**Files Modified:**
- `usagi/src/anidbapi.h`: 
  - Added `notifyCheckIntervalMs` member variable
  - Added `exportQueuedTimestamp` member variable
  - Added `saveExportQueueState()`, `loadExportQueueState()`, and `checkForExistingExport()` methods
- `usagi/src/anidbapi.cpp`: 
  - Initialize `notifyCheckIntervalMs` and `exportQueuedTimestamp` in constructor
  - Load persisted state on initialization
  - Save timestamp when export is queued
  - Increase interval by 1 minute after each unsuccessful check (capped at 60 minutes)
  - Check for 48-hour timeout
  - Reset interval and clear state when export is found
  - Save state after each check and when export is found
  - Implemented state persistence methods
  - Enhanced debug logging with elapsed time

### Benefits
1. **Extended monitoring**: Supports exports that take up to 24 hours (monitors for 48 hours)
2. **Reduced server load**: Checks become less frequent over time, capped at 60 minutes
3. **Better resource management**: Gradually backing off reduces unnecessary API calls
4. **Improved user experience**: Export notifications are still caught promptly for quick exports
5. **Restart resilience**: Export monitoring continues across application restarts
6. **Avoids duplicate requests**: Checks for existing export before requesting new one on startup
7. **Smart timeout**: Automatically cleans up after 48 hours

### Testing Considerations
When testing this feature:
1. Request a MyList export
2. Observe the debug logs showing increasing intervals (capped at 60 minutes)
3. Verify that finding an export notification stops checking and resets the interval
4. Verify that requesting another export starts with a 1-minute interval again
5. **Restart testing**: 
   - Request an export
   - Restart the application
   - Verify state is loaded and checking resumes
   - Verify it checks for existing export first
6. Verify that after 48 hours without finding export, checking stops and state is cleared

## Related Code Locations
- Timer initialization: `anidbapi.cpp:103-111`
- Export queue handling: `anidbapi.cpp:314-326`
- Export notification detection (PUSH): `anidbapi.cpp:677-687`
- Export notification detection (FETCHED): `anidbapi.cpp:858-868`
- Periodic check function: `anidbapi.cpp:1593-1649`
- State persistence: `anidbapi.cpp:1651-1687`
- State loading: `anidbapi.cpp:1689-1730`
- Existing export check: `anidbapi.cpp:1732-1764`

