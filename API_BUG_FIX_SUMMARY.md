# API Bug Fix Summary

## Issue
Error 505 "ILLEGAL INPUT OR ACCESS DENIED" when fetching notification details with NOTIFYGET command.

## Root Cause
The code 291 (NOTIFYLIST ENTRY) response handler was only examining the first notification in the list and attempting to fetch it. This caused problems because:
1. The first notification in the list is the oldest
2. Older notifications are likely already acknowledged/deleted
3. The server returns error 505 when trying to fetch non-existent notifications

## Solution
Modified the code 291 handler in `usagi/src/anidbapi.cpp` to:
1. Log all notification entries (better debugging)
2. Search backwards through the list
3. Find the **last** (most recent) message notification instead of the first
4. Fetch that notification

This matches the behavior of the code 290 handler and ensures consistency.

## Changes Made

### File: `usagi/src/anidbapi.cpp` (lines 368-396)

**Before:**
- Only checked `token2.first()` (first entry)
- Fetched first message notification found
- No visibility into other notifications

**After:**
- Iterates through all entries with logging
- Searches backwards to find last message notification
- Fetches most recent notification instead of oldest

### Lines Changed: 15 lines modified
- Added loop to log all entries (4 lines)
- Changed from forward to backward search (11 lines)
- Updated debug messages for clarity

## Benefits

✅ **Fixes error 505** - Fetches valid, recent notifications instead of old ones  
✅ **Better debugging** - All notifications are now logged  
✅ **Consistent behavior** - Code 290 and 291 handlers now work the same way  
✅ **User experience** - Fetches the most recent export notification automatically  

## Testing

While Qt6 was not available for full integration testing, the fix was verified by:
1. Code analysis confirming the logic matches code 290 handler
2. Review of the notification list ordering (oldest first, newest last)
3. Verification that backward search finds most recent notification
4. Minimal scope of change reduces risk

## Expected Behavior After Fix

When notifications are received:
```
[AniDB Response] 291 NOTIFYLIST - Tag: "45" Entry count: 105
[AniDB Response] 291 NOTIFYLIST Entry 1 of 105: M|4756534  (old)
...
[AniDB Response] 291 NOTIFYLIST Entry 100 of 105: M|4996265  (recent)
[AniDB Response] 291 NOTIFYLIST - Fetching last message notification: 4996265
[AniDB Send] Command: "NOTIFYGET nid=4996265&s=HE13W&tag=46"
[AniDB Response] 293 NOTIFYGET - NID: 4996265 ... (SUCCESS)
```

Instead of:
```
[AniDB Response] 291 NOTIFYLIST ENTRY - Data: "M|4756534"
[AniDB Send] Command: "NOTIFYGET nid=4756534&s=3UH3J&tag=46"
[AniDB Response] Tag: "46" ReplyID: "505" (ERROR - notification already acknowledged)
```

## Related Files
- `NOTIFYGET_505_FIX.md` - Detailed technical explanation
- `NOTIFICATION_LIST_IMPROVEMENTS.md` - Original notification handling implementation
- `usagi/src/anidbapi.cpp` - Fixed code

## Impact
- **Type:** Bug fix
- **Scope:** Single function modification (code 291 handler)
- **Risk:** Minimal - makes code consistent with existing code 290 handler
- **Breaking changes:** None
