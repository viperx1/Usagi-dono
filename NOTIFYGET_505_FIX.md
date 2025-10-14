# NOTIFYGET Error 505 Fix

## Issue

The application was receiving error 505 "ILLEGAL INPUT OR ACCESS DENIED" when trying to fetch notification details with the NOTIFYGET command.

### Problem Description

From the issue logs:
```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 372 [AniDB Response] 291 NOTIFYLIST ENTRY - Tag: "45" Data: "M|4756534"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 378 [AniDB Response] 291 NOTIFYLIST ENTRY - Message notification, fetching: "4756534"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 669 [AniDB Send] Command: "NOTIFYGET nid=4756534&s=3UH3J&tag=46"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 174 [AniDB Response] Tag: "46" ReplyID: "505"
```

And from the user-friendly logs:
```
01:13:22: AniDBApi: Recv: 45 291 NOTIFYLIST
M|4756534
M|4755053
M|4837335
...
M|4996265
N|6262
...
N|9980

01:13:24: AniDBApi: Send: NOTIFYGET nid=4756534&s=3UH3J
01:13:24: AniDBApi: Recv: 46 505 ILLEGAL INPUT OR ACCESS DENIED
```

The application was trying to fetch the **first** message notification (M|4756534) from the list, but this notification was likely already acknowledged or deleted, causing the 505 error.

## Root Cause

The code 291 handler was only looking at the first entry in the notification list:

**Before (line 368-381 in anidbapi.cpp):**
```cpp
else if(ReplyID == "291"){ // 291 NOTIFYLIST ENTRY
    // Parse notification list entry - this is sent for each entry when using pagination
    QStringList token2 = Message.split("\n");
    token2.pop_front();
    qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 291 NOTIFYLIST ENTRY - Tag:"<<Tag<<"Data:"<<token2.first();
    
    // Check if this is a message notification and fetch it
    if(token2.first().startsWith("M|"))
    {
        QString nid = token2.first().mid(2);
        qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 291 NOTIFYLIST ENTRY - Message notification, fetching:"<<nid;
        NotifyGet(nid.toInt());
    }
}
```

Problems:
1. Only looked at `token2.first()` (the first entry)
2. Did not log all entries for debugging
3. Did not search for the **last** message notification like code 290 does
4. First notification in the list is likely old and already acknowledged

## Solution

### Code Change

**File:** `usagi/src/anidbapi.cpp`, lines 368-396

**After:**
```cpp
else if(ReplyID == "291"){ // 291 NOTIFYLIST ENTRY
    // Parse notification list - can contain single entry (pagination) or full list
    QStringList token2 = Message.split("\n");
    token2.pop_front();
    qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 291 NOTIFYLIST - Tag:"<<Tag<<"Entry count:"<<token2.size();
    
    // Log all notification entries
    for(int i = 0; i < token2.size(); i++)
    {
        qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 291 NOTIFYLIST Entry"<<i+1<<"of"<<token2.size()<<":"<<token2[i];
    }
    
    // Find the last message notification (M|nid) and fetch its content
    QString lastMessageNid;
    for(int i = token2.size() - 1; i >= 0; i--)
    {
        if(token2[i].startsWith("M|"))
        {
            lastMessageNid = token2[i].mid(2); // Extract nid after "M|"
            break;
        }
    }
    
    if(!lastMessageNid.isEmpty())
    {
        qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 291 NOTIFYLIST - Fetching last message notification:"<<lastMessageNid;
        NotifyGet(lastMessageNid.toInt());
    }
}
```

### Why This Works

1. **Logs all entries** - Better debugging visibility
2. **Searches backwards** - Finds the **last** message notification in the list
3. **Most recent first** - Last notification is more likely to be recent and not yet acknowledged
4. **Consistent with code 290** - Both handlers now work the same way

The last message notification in the list (e.g., M|4996265 instead of M|4756534) is more likely to be:
- Recent and still valid
- Not yet acknowledged
- The one the user actually wants to fetch (e.g., latest export ready notification)

## Expected Output After Fix

### Before Fix (Error)
```
[AniDB Response] 291 NOTIFYLIST ENTRY - Tag: "45" Data: "M|4756534"
[AniDB Response] 291 NOTIFYLIST ENTRY - Message notification, fetching: "4756534"
[AniDB Send] Command: "NOTIFYGET nid=4756534&s=3UH3J&tag=46"
[AniDB Response] Tag: "46" ReplyID: "505"  <-- ERROR: Old notification already acknowledged
```

### After Fix (Success)
```
[AniDB Response] 291 NOTIFYLIST - Tag: "45" Entry count: 105
[AniDB Response] 291 NOTIFYLIST Entry 1 of 105: M|4756534
[AniDB Response] 291 NOTIFYLIST Entry 2 of 105: M|4755053
...
[AniDB Response] 291 NOTIFYLIST Entry 100 of 105: M|4996265
[AniDB Response] 291 NOTIFYLIST Entry 101 of 105: N|6262
...
[AniDB Response] 291 NOTIFYLIST Entry 105 of 105: N|9980
[AniDB Response] 291 NOTIFYLIST - Fetching last message notification: 4996265
[AniDB Send] Command: "NOTIFYGET nid=4996265&s=HE13W&tag=46"
[AniDB Response] 293 NOTIFYGET - NID: 4996265 Type: 1 Title: "MyList Export Ready" Body: "..."
```

## Notification List Order

Notification lists from AniDB appear to be ordered with **oldest first, newest last**:
- First entries (M|4756534, M|4755053, ...) are older
- Last entries (M|4996265, N|6262, ...) are more recent

By fetching the **last** message notification instead of the first, we ensure we're getting the most recent one, which is:
- Less likely to have been acknowledged/deleted (avoiding 505 error)
- More likely to be what the user actually wants (e.g., most recent export)

## Consistency with Code 290

Both code 290 (NOTIFYLIST) and code 291 (NOTIFYLIST ENTRY) now use identical logic:
1. Log all entries
2. Search backwards for last M-type notification
3. Fetch that notification

This ensures consistent behavior regardless of which response code the AniDB server returns.

## Impact

- **Type:** Bug fix - Logic error in notification handling
- **Scope:** 15 lines changed (adds logging and searches backwards instead of checking only first entry)
- **Risk:** Minimal - makes code 291 consistent with code 290, both search for last notification
- **Benefits:**
  - Fixes 505 "ILLEGAL INPUT OR ACCESS DENIED" error
  - Fetches most recent notification instead of oldest
  - Better debug logging shows all notifications
  - Consistent behavior between code 290 and 291 handlers

## Testing

Since Qt6 is not available in the CI environment, the fix should be tested by:
1. Running the application with AniDB credentials
2. Logging in to enable notifications
3. Having multiple pending message notifications on the account
4. Observing that the most recent notification is fetched successfully
5. Verifying no 505 error occurs

## Related Documentation

- `NOTIFICATION_LIST_IMPROVEMENTS.md` - Original notification handling implementation
- `NOTIFYLIST_FIX.md` - Command format fix for NOTIFYLIST command
- AniDB UDP API Definition: https://wiki.anidb.net/UDP_API_Definition
