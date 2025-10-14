# Notification List Improvements - Quick Reference

## Issue Addressed
The original issue showed that NOTIFYLIST responses contained many notifications but only the first one was being logged:

```
00:39:56: AniDBApi: Recv: 44 291 NOTIFYLIST
M|4756534     <-- Only this was logged
M|4755053     <-- These were ignored
M|4837335
...
N|6262
N|2618
```

## Solution Summary

### 1. Enhanced Logging
**Now logs ALL notification entries** with count and individual details:
```
[AniDB Response] 290 NOTIFYLIST - Tag: "44" Entry count: 105
[AniDB Response] 290 NOTIFYLIST Entry 1 of 105: M|4756534
[AniDB Response] 290 NOTIFYLIST Entry 2 of 105: M|4755053
...
[AniDB Response] 290 NOTIFYLIST Entry 105 of 105: N|9980
```

### 2. Automatic Message Fetching
**Automatically fetches the last message notification** (which contains export URLs):
```
[AniDB Response] 290 NOTIFYLIST - Fetching last message notification: 4996265
[AniDB Queue] Sending query - Tag: "45" Command: "NOTIFYGET nid=4996265"
```

### 3. Message Content Retrieval
**New NOTIFYGET command** retrieves full notification content:
```
[AniDB Response] 293 NOTIFYGET - NID: 4996265 Type: 1 Title: "MyList Export Ready" Body: "https://..."
```

### 4. Automatic Download/Import
**Existing functionality** then takes over:
```
[Window] Notification received: 4996265
[Window] MyList export link found: https://anidb.net/export/12345-export.tgz
[Window] Export downloaded and imported
```

## New API Methods

### AniDBApi::NotifyGet(int nid)
```cpp
QString nid = adbapi->NotifyGet(12345);
// Sends: NOTIFYGET nid=12345
// Receives: 293 NOTIFYGET response with full notification details
```

### AniDBApi::buildNotifyGetCommand(int nid)
```cpp
QString cmd = buildNotifyGetCommand(12345);
// Returns: "NOTIFYGET nid=12345"
```

## Notification Types

| Prefix | Type | Description | Example |
|--------|------|-------------|---------|
| M\| | Message | Contains announcements, export URLs | M\|4756534 |
| N\| | New File | New episode available for watched anime | N\|6262 |

## Code Flow

```
Login
  ↓
NOTIFYLIST sent (automatic)
  ↓
290 NOTIFYLIST response received
  ↓
Log ALL entries (improvement 1) ✅
  ↓
Find last M| notification
  ↓
NOTIFYGET sent for last M| (improvement 2) ✅
  ↓
293 NOTIFYGET response received (improvement 3) ✅
  ↓
Parse notification body
  ↓
Extract export URL
  ↓
Download & import (existing) ✅
```

## Files Changed

| File | Changes | Lines |
|------|---------|-------|
| usagi/src/anidbapi.h | + NotifyGet()<br>+ buildNotifyGetCommand() | +2 |
| usagi/src/anidbapi.cpp | Enhanced 290/291 handlers<br>+ 293 handler<br>+ NotifyGet() impl<br>+ buildNotifyGetCommand() impl | +80 |

## Testing Checklist

- [ ] Login shows "Notifications enabled"
- [ ] NOTIFYLIST response logs entry count
- [ ] All notification entries are logged individually
- [ ] Last M| notification is identified
- [ ] NOTIFYGET command is sent for last M|
- [ ] 293 NOTIFYGET response is received
- [ ] Export URL is extracted from notification body
- [ ] Export is downloaded and imported automatically

## Benefits

✅ **Complete visibility** - See all pending notifications
✅ **Automatic retrieval** - Last export message fetched automatically  
✅ **No manual steps** - Fully automated from login to import
✅ **Better debugging** - Clear logs show exactly what's happening

## Comparison: Before vs After

### Before (Issue)
```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 349 [AniDB Response] 291 NOTIFYLIST ENTRY - Tag: "44" Data: "M|4756534"
```
❌ Only first notification logged  
❌ No automatic fetching of message content  
❌ Manual work required to get export URL  

### After (Fix)
```
[AniDB Response] 290 NOTIFYLIST - Tag: "44" Entry count: 105
[AniDB Response] 290 NOTIFYLIST Entry 1 of 105: M|4756534
[AniDB Response] 290 NOTIFYLIST Entry 2 of 105: M|4755053
...
[AniDB Response] 290 NOTIFYLIST Entry 105 of 105: N|9980
[AniDB Response] 290 NOTIFYLIST - Fetching last message notification: 4996265
[AniDB Response] 293 NOTIFYGET - NID: 4996265 Type: 1 Title: "MyList Export Ready" Body: "https://..."
[Window] MyList export link found: https://anidb.net/export/12345-export.tgz
[Window] Successfully imported 150 mylist entries
```
✅ All notifications logged  
✅ Automatic message retrieval  
✅ Fully automated workflow  
