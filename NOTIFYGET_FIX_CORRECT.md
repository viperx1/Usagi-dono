# NOTIFYGET Command Fix - Correct Implementation

## Issue
The application was receiving error 505 "ILLEGAL INPUT OR ACCESS DENIED" when attempting to fetch notification details using the NOTIFYGET command.

## Root Cause
The NOTIFYGET command was using incorrect parameter format according to the AniDB UDP API Definition.

### Incorrect Implementation
```cpp
return QString("NOTIFYGET nid=%1").arg(nid);
```

Command sent: `NOTIFYGET nid=4998280`

### Correct Implementation (According to AniDB UDP API)
According to https://wiki.anidb.net/UDP_API_Definition, the NOTIFYGET command requires TWO parameters:

```
NOTIFYGET type={str type}&id={int4 id}
```

Where:
- `type` is either "M" (for message entries) or "N" (for notification entries)
- `id` is the notification/message identifier

## Solution

### 1. Updated Command Builder
```cpp
QString AniDBApi::buildNotifyGetCommand(int nid)
{
    // NOTIFYGET requires type parameter: type=M for messages, type=N for notifications
    // Currently only fetching message notifications from NOTIFYLIST
    return QString("NOTIFYGET type=M&id=%1").arg(nid);
}
```

Command sent: `NOTIFYGET type=M&id=4998280`

### 2. Updated Response Handlers

According to the API definition, NOTIFYGET returns different response codes based on type:

#### For type=M (Messages) - Code 292
Format: `{int4 id}|{int4 from_user_id}|{str from_user_name}|{int4 date}|{int4 type}|{str title}|{str body}`

```cpp
else if(ReplyID == "292"){ // 292 NOTIFYGET (type=M)
    // Parse message notification details
    // Extract: id, from_user_id, from_user_name, date, type, title, body
    emit notifyMessageReceived(id, body);
    PushAck(id);
}
```

#### For type=N (Notifications) - Code 293
Format: `{int4 relid}|{int4 type}|{int2 count}|{int4 date}|{str relidname}|{str fids}`

```cpp
else if(ReplyID == "293"){ // 293 NOTIFYGET (type=N)
    // Parse notification details (file notifications)
    // Extract: relid, type, count, date, relidname, fids
    PushAck(relid);
}
```

## Changes Made

### File: usagi/src/anidbapi.cpp

1. **buildNotifyGetCommand()** - Lines 684-689
   - Changed from `NOTIFYGET nid=%1` to `NOTIFYGET type=M&id=%1`
   - Added comments explaining the API requirements

2. **Added 292 Response Handler** - Lines 397-422
   - New handler for type=M (message) responses
   - Correctly parses 7-part format: id|from_user_id|from_user_name|date|type|title|body
   - Emits notifyMessageReceived signal for export URL extraction

3. **Updated 293 Response Handler** - Lines 423-448
   - Changed to handle type=N (notification) responses
   - Correctly parses 6-part format: relid|type|count|date|relidname|fids
   - For file notifications (not message notifications)

### File: tests/test_anidbapi.cpp

1. **Added testNotifyGetCommandFormat()** - Lines 493-518
   - Validates NOTIFYGET command format
   - Verifies presence of `type=` parameter
   - Verifies presence of `id=` parameter (not `nid=`)
   - Confirms `type=M` for message notifications

2. **Added NOTIFYGET to global validation** - Lines 554-555
   - Added to testAllCommandsHaveProperSpacing()

## Expected Behavior

### Before Fix (Error)
```
Command sent:  NOTIFYGET nid=4998280&s=SESSION&tag=50
Response:      505 ILLEGAL INPUT OR ACCESS DENIED ❌
```

### After Fix (Success)
```
Command sent:  NOTIFYGET type=M&id=4998280&s=SESSION&tag=50
Response:      292 NOTIFYGET
               {id}|{from_user_id}|{from_user_name}|{date}|{type}|{title}|{body} ✓
```

## API Reference

From https://wiki.anidb.net/UDP_API_Definition:

**Command String:**
```
NOTIFYGET type={str type}&id={int4 id}
```

**Possible Replies:**

**when type = M (messages)**
- 292 NOTIFYGET: `{int4 id}|{int4 from_user_id}|{str from_user_name}|{int4 date}|{int4 type}|{str title}|{str body}`
- 392 NO SUCH ENTRY

**when type = N (notifications)**
- 293 NOTIFYGET: `{int4 relid}|{int4 type}|{int2 count}|{int4 date}|{str relidname}|{str fids}`
- 393 NO SUCH ENTRY

**Info:**
- type is: M for message entries, N for notification entries
- id is the identifier for the notification/message as given by NOTIFYLIST

## Testing

Tests added but cannot be run due to Qt6 not being available in CI. Manual verification shows:
- Command format is now correct according to API definition
- Response handlers properly distinguish between type=M (292) and type=N (293)
- Message notifications will be properly parsed and signal emitted

## Related Documentation

- Previous incorrect documentation in NOTIFYGET_505_FIX.md (showed 293 for messages, which is wrong)
- AniDB UDP API Definition: https://wiki.anidb.net/UDP_API_Definition
