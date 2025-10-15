# Fix: Reduce Excessive Notification Logging

## Issue
The notification system was generating excessive log output that cluttered the console and log files:
1. **138 individual notification entries** were logged when NOTIFYLIST was received
2. **"No mylist export link found in notification"** was logged 10 times when checking recent notifications

This made the logs difficult to read and provided little value since most notifications don't contain export links.

## Example of Excessive Logging (Before Fix)

```
23:58:17: [AniDB Response] 291 NOTIFYLIST Entry 1 of 138: M|4756534
23:58:17: [AniDB Response] 291 NOTIFYLIST Entry 2 of 138: M|4755053
23:58:17: [AniDB Response] 291 NOTIFYLIST Entry 3 of 138: M|4837335
... (135 more lines)
23:58:17: [AniDB Response] 291 NOTIFYLIST Entry 138 of 138: 

23:58:19: Notification 4998280 received
23:58:19: No mylist export link found in notification
23:58:21: Notification 4996265 received
23:58:21: No mylist export link found in notification
23:58:23: Notification 4987059 received
23:58:23: No mylist export link found in notification
... (7 more "No mylist export link found" messages)
```

## Solution

### 1. Removed Individual Notification Entry Logging
**File:** `usagi/src/anidbapi.cpp` (lines 405-409)

Removed the loop that logged every single notification entry:
```cpp
// REMOVED: Log all notification entries
// for(int i = 0; i < token2.size(); i++)
// {
//     Debug(...Entry " + i+1 + " of " + size + ": " + token2[i]);
// }
```

**Kept:** The summary count of entries is still logged:
```cpp
Debug(...NOTIFYLIST - Tag: " + Tag + " Entry count: " + QString::number(token2.size()));
```

### 2. Removed "No mylist export link found" Messages
**File:** `usagi/src/window.cpp` (lines 769-772)

Removed the else clause that logged a message for every notification that doesn't contain an export link:
```cpp
// REMOVED:
// else
// {
//     logOutput->append("No mylist export link found in notification");
// }
```

**Added explanatory comment:**
```cpp
// Don't log "No mylist export link found" for every notification - it creates log spam
// The notification is already logged via qDebug() and getNotifyLogAppend() above for debugging
```

### 3. Only Log When Export Link IS Found
**File:** `usagi/src/window.cpp` (line 693, 707)

Moved the "Notification X received" message to only log when an export link is actually found:
```cpp
if(match.hasMatch() && !isDownloadingExport)
{
    isDownloadingExport = true;
    QString exportUrl = match.captured(0);
    logOutput->append(QString("Notification %1 received").arg(nid));  // Moved here
    logOutput->append(QString("MyList export link found: %1").arg(exportUrl));
    // ... download logic
}
```

## Expected Behavior (After Fix)

### Minimal Logging for Normal Notifications
```
23:58:17: [AniDB Response] 291 NOTIFYLIST - Tag: 89 Entry count: 138
23:58:17: [AniDB Response] 291 NOTIFYLIST - Fetching last 10 message notifications
23:58:17: [AniDB Response] 291 NOTIFYLIST - Fetching message notification 1 of 10: 4998280
23:58:17: [AniDB Response] 291 NOTIFYLIST - Fetching message notification 2 of 10: 4996265
...
```

### Clear Logging When Export Link Found
```
Notification 4996265 received
MyList export link found: https://anidb.net/export/12345.tgz
MyList Status: Downloading export...
Export downloaded to: /tmp/mylist_export_1234567890.tgz
Successfully imported 1234 mylist entries
```

## Benefits

✅ **Reduced log clutter** - No more hundreds of lines of notification entries  
✅ **Less spam** - No "No mylist export link found" repeated 10 times  
✅ **Still debuggable** - All notifications are still logged via qDebug() for debugging  
✅ **Clear success path** - Export link detection is prominently logged  
✅ **Minimal change** - Only removed excessive logging, kept essential information  

## Debugging Information Still Available

If debugging is needed, the following information is still logged:
- Total notification count
- Which notifications are being fetched  
- Full notification details via qDebug() and getNotifyLogAppend()
- Success messages when export links are found and downloaded

## Files Changed
- `usagi/src/anidbapi.cpp` - Removed notification entry loop logging (6 lines removed)
- `usagi/src/window.cpp` - Removed "No export link" message, moved success message (5 lines changed)

Total: **11 lines removed, 3 lines added** for a net reduction of 8 lines and significantly cleaner logs.

## Testing

The changes are passive (removing log statements only) and do not affect:
- Notification fetching logic
- Export link detection
- Download functionality
- Import functionality

All existing functionality remains intact, only the verbosity of logging has been reduced.
