# Fix: Search Multiple Notifications for Export Link

## Issue
The export notification might not be the last message notification in the list. The user reported that the link might be in a different message, most likely in recent notifications but not guaranteed to be the very last one.

## Previous Approach (Reverted)
- Only fetched the last message notification
- Added BBCode URL parsing
- This was the wrong approach - the issue was not about URL format, but about which notification to check

## New Approach
Instead of changing how we read links, we now **check multiple notifications** to find the export link.

### Changes in `anidbapi.cpp`
Modified both `290 NOTIFYLIST` and `291 NOTIFYLIST ENTRY` handlers to:

1. **Collect all message notification IDs** (M|nid entries) from the list
2. **Fetch the last 10 message notifications** (configurable via `maxNotificationsToFetch`)
3. Fetch from most recent first, working backwards through the list
4. Each notification body is emitted to the UI for checking

### Changes in `window.cpp`
Added a simple flag to prevent downloading multiple exports:
- Added `static bool isDownloadingExport` to track if a download is in progress
- Only triggers download if not already downloading
- First notification with a .tgz link will trigger the download

## Expected Behavior

### Before Fix
```
[AniDB Response] 290 NOTIFYLIST - Entry count: 102
[AniDB Response] 290 NOTIFYLIST - Fetching last message notification: 4998280
[AniDB Response] 292 NOTIFYGET - ID: 4998280 Title: New anime relation added!
Notification 4998280 received
No mylist export link found in notification  ❌
```

### After Fix
```
[AniDB Response] 290 NOTIFYLIST - Entry count: 102
[AniDB Response] 290 NOTIFYLIST - Fetching last 10 message notifications
[AniDB Response] 290 NOTIFYLIST - Fetching message notification 1 of 10: 4998280
[AniDB Response] 290 NOTIFYLIST - Fetching message notification 2 of 10: 4996265
[AniDB Response] 290 NOTIFYLIST - Fetching message notification 3 of 10: 4987059
...

[AniDB Response] 292 NOTIFYGET - ID: 4998280 Title: New anime relation added!
Notification 4998280 received
No mylist export link found in notification

[AniDB Response] 292 NOTIFYGET - ID: 4996265 Title: MyList Export Ready
Notification 4996265 received
MyList export link found: https://anidb.net/export/12345.tgz  ✅
MyList Status: Downloading export...
```

## Benefits
- ✅ Searches through multiple recent notifications to find export links
- ✅ Prioritizes recent notifications (most likely to contain export)
- ✅ Configurable limit (10) prevents excessive API calls
- ✅ Prevents downloading multiple exports simultaneously
- ✅ No changes to URL parsing - works with existing format

## Configuration
The number of notifications to check is configurable in `anidbapi.cpp`:
```cpp
const int maxNotificationsToFetch = 10;
```

This can be adjusted based on user needs and API rate limits.
