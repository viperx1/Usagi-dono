# Notification List Improvements

## Overview
Enhanced notification handling to properly list all notifications and automatically fetch the last message notification containing export URLs.

## Changes Made

### 1. Improved Debug Logging for NOTIFYLIST (Code 290)

**Before:**
```cpp
qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 290 NOTIFYLIST - Tag:"<<Tag<<"Data:"<<token2.first();
```

**After:**
```cpp
qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 290 NOTIFYLIST - Tag:"<<Tag<<"Entry count:"<<token2.size();

// Log all notification entries
for(int i = 0; i < token2.size(); i++)
{
    qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 290 NOTIFYLIST Entry"<<i+1<<"of"<<token2.size()<<":"<<token2[i];
}
```

**Benefit:** Now logs **all** notification entries, not just the first one.

### 2. Automatic Fetching of Last Message Notification

After logging all notifications, the code now:
1. Searches backwards through the notification list for the last message notification (type `M|nid`)
2. Automatically calls `NotifyGet()` to fetch its content

```cpp
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
    qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 290 NOTIFYLIST - Fetching last message notification:"<<lastMessageNid;
    NotifyGet(lastMessageNid.toInt());
}
```

### 3. Enhanced NOTIFYLIST ENTRY (Code 291) Handling

Similar logic added for code 291 (single entry responses):
```cpp
// Check if this is a message notification and fetch it
if(token2.first().startsWith("M|"))
{
    QString nid = token2.first().mid(2);
    qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 291 NOTIFYLIST ENTRY - Message notification, fetching:"<<nid;
    NotifyGet(nid.toInt());
}
```

### 4. New NOTIFYGET Command Implementation

#### API Method
```cpp
QString AniDBApi::NotifyGet(int nid)
{
    // Get details of a specific notification
    if(SID.length() == 0 || LoginStatus() == 0)
    {
        Auth();
    }
    QString msg = buildNotifyGetCommand(nid);
    QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
    QSqlQuery query;
    query.exec(q);
    return GetTag(msg);
}
```

#### Command Builder
```cpp
QString AniDBApi::buildNotifyGetCommand(int nid)
{
    return QString("NOTIFYGET nid=%1").arg(nid);
}
```

### 5. NOTIFYGET Response Handler (Code 293)

Parses the notification details and emits the signal for automatic download/import:

```cpp
else if(ReplyID == "293"){ // 293 NOTIFYGET - {int4 nid}|{int2 type}|{int4 fromuid}|{int4 date}|{str title}|{str body}
    // Parse notification message details
    QStringList token2 = Message.split("\n");
    token2.pop_front();
    QStringList parts = token2.first().split("|");
    if(parts.size() >= 6)
    {
        int nid = parts[0].toInt();
        QString type = parts[1];
        QString title = parts[4];
        QString body = parts[5];
        
        qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] 293 NOTIFYGET - NID:"<<nid<<"Type:"<<type<<"Title:"<<title<<"Body:"<<body;
        
        // Emit signal for notification (same as 270 for automatic download/import)
        emit notifyMessageReceived(nid, body);
        
        // Acknowledge the notification
        PushAck(nid);
    }
}
```

## Notification Types

According to AniDB UDP API, notifications come in different types:
- `M|{nid}` - Message notifications (contain export URLs, announcements, etc.)
- `N|{nid}` - New file notifications (new episodes available for anime in watchlist)

## Workflow

### Complete Flow
1. **Login** → Notifications automatically enabled via `NOTIFYLIST` command
2. **NOTIFYLIST Response (290)** received with list like:
   ```
   M|4756534
   M|4755053
   N|6262
   N|2618
   ```
3. **Debug logs all entries** (addresses first part of issue)
4. **Finds last M-type notification** (message containing export)
5. **Sends NOTIFYGET command** for that notification ID
6. **NOTIFYGET Response (293)** received with full message details
7. **Extracts export URL** from message body
8. **Automatically downloads and imports** (existing functionality)

## Expected Debug Output

### Before Fix
```
[AniDB Response] 290 NOTIFYLIST - Tag: "44" Data: "M|4756534"
```

### After Fix
```
[AniDB Response] 290 NOTIFYLIST - Tag: "44" Entry count: 105
[AniDB Response] 290 NOTIFYLIST Entry 1 of 105: M|4756534
[AniDB Response] 290 NOTIFYLIST Entry 2 of 105: M|4755053
[AniDB Response] 290 NOTIFYLIST Entry 3 of 105: M|4837335
...
[AniDB Response] 290 NOTIFYLIST Entry 104 of 105: N|11982
[AniDB Response] 290 NOTIFYLIST Entry 105 of 105: N|9980
[AniDB Response] 290 NOTIFYLIST - Fetching last message notification: 4996265
[AniDB Queue] Sending query - Tag: "45" Command: "NOTIFYGET nid=4996265"
[AniDB Send] Command: "NOTIFYGET nid=4996265&s=HE13W&tag=45"
[AniDB Response] 293 NOTIFYGET - NID: 4996265 Type: 1 Title: "MyList Export Ready" Body: "Your export is ready: https://anidb.net/export/12345-export.tgz"
[Window] Notification received: 4996265
[Window] MyList export link found: https://anidb.net/export/12345-export.tgz
[Window] Export downloaded to: /tmp/mylist_export_...tgz
[Window] Successfully imported 150 mylist entries
```

## Files Modified

### usagi/src/anidbapi.h
- Added `NotifyGet(int nid)` method declaration
- Added `buildNotifyGetCommand(int nid)` method declaration

### usagi/src/anidbapi.cpp
- Enhanced code 290 handler to log all notifications and fetch last message
- Enhanced code 291 handler to fetch message notifications
- Added code 293 handler for NOTIFYGET responses
- Implemented `NotifyGet()` method
- Implemented `buildNotifyGetCommand()` method

## Benefits

### For Users
- ✅ **Complete visibility** into all pending notifications
- ✅ **Automatic fetching** of the most recent export notification
- ✅ **No manual intervention** needed to retrieve export URLs
- ✅ **Better debugging** when things go wrong

### For Developers
- ✅ **Clear logging** shows exactly what notifications are pending
- ✅ **Automatic workflow** from notification list to export download
- ✅ **Consistent with existing code** (uses same patterns as PushAck, NotifyEnable)
- ✅ **Minimal changes** - only ~80 lines added

## Testing

To test these changes:

1. **Login to application**
   - Verify notifications enabled
   
2. **Request export on AniDB website**
   - Go to mylist page
   - Click Export → csv-adborg
   
3. **Check debug logs after login**
   - Should see complete notification list
   - Should see "Fetching last message notification"
   - Should see NOTIFYGET command sent
   - Should see 293 response with notification details
   - Should see automatic download/import

## AniDB UDP API Reference

- **NOTIFYLIST**: Returns list of pending notifications
  - Response 290: Full list
  - Response 291: Single entry (pagination)
  
- **NOTIFYGET nid={nid}**: Retrieve details of specific notification
  - Response 293: Notification details in format `{nid}|{type}|{fromuid}|{date}|{title}|{body}`
  
- **PUSHACK nid={nid}**: Acknowledge notification
  - Response 271: Acknowledgment confirmed

## Status
✅ Implementation complete
⏳ Requires testing with Qt environment
🎯 Addresses both requirements from issue:
   1. "properly list all notifications" ✅
   2. "load message containing last export" ✅
