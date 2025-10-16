# Export Check Interval Implementation

## Overview
This document describes the implementation of the dynamic export check interval feature for AniDB MyList exports.

## Requirements
- Set periodic check for export to 1 minute initially
- Increase the check interval by 1 minute every time there is no new notification
- Reset the interval to 1 minute on success (when export notification is found)

## Implementation Details

### Changes Made

#### 1. Added New Member Variable (`anidbapi.h`)
- Added `int notifyCheckIntervalMs` to track the current check interval in milliseconds

#### 2. Initialization (`anidbapi.cpp`)
- Initialize `notifyCheckIntervalMs` to 60000 ms (1 minute) in constructor
- Set initial timer interval to 1 minute when export is queued

#### 3. Dynamic Interval Adjustment (`checkForNotifications()`)
- After each check without finding an export notification:
  - Increment `notifyCheckIntervalMs` by 60000 ms (1 minute)
  - Update the timer with the new interval
  - Log the next check interval
- Enhanced debug logging to show current interval and next interval

#### 4. Reset on Success
- When export notification is detected (contains .tgz link):
  - Stop the timer
  - Reset `notifyCheckIntervalMs` to 60000 ms (1 minute)
  - This ensures the next export starts with a 1-minute interval

### Behavior

#### Check Interval Progression
When an export is queued, the check intervals progress as follows:
1. First check: 1 minute after queue
2. Second check: 2 minutes after first check
3. Third check: 3 minutes after second check
4. ...
5. 20th check: 20 minutes after 19th check
6. After 20 attempts: Stop checking

**Total time**: 1 + 2 + 3 + ... + 20 = 210 minutes (3.5 hours)

This is significantly longer than the previous implementation (10 minutes with 30-second intervals), giving exports more time to complete while reducing server load.

#### Reset Scenarios
The interval is reset to 1 minute in the following cases:
- When an export notification is successfully found
- After 20 unsuccessful attempts (cleanup)

### Code Changes Summary

**Files Modified:**
- `usagi/src/anidbapi.h`: Added `notifyCheckIntervalMs` member variable
- `usagi/src/anidbapi.cpp`: 
  - Initialize `notifyCheckIntervalMs` in constructor
  - Set initial interval to 1 minute when export is queued
  - Increase interval by 1 minute after each unsuccessful check
  - Reset interval to 1 minute when export is found
  - Enhanced debug logging

### Benefits
1. **Reduced server load**: Checks become less frequent over time
2. **Better resource management**: Gradually backing off reduces unnecessary API calls
3. **Extended monitoring period**: Total monitoring time increased from 10 minutes to 3.5 hours
4. **Improved user experience**: Export notifications are still caught promptly for quick exports, while slow exports get more time

### Testing Considerations
When testing this feature:
1. Request a MyList export
2. Observe the debug logs showing increasing intervals
3. Verify that finding an export notification stops checking and resets the interval
4. Verify that requesting another export starts with a 1-minute interval again
5. Verify that after 20 attempts without finding export, checking stops

## Related Code Locations
- Timer initialization: `anidbapi.cpp:104-107`
- Export queue handling: `anidbapi.cpp:309-316`
- Export notification detection (PUSH): `anidbapi.cpp:668-674`
- Export notification detection (FETCHED): `anidbapi.cpp:845-851`
- Periodic check function: `anidbapi.cpp:1579-1620`
