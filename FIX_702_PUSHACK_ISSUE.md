# Fix for AniDB API Error 702: NO SUCH PACKET PENDING

## Issue Description

When the application fetched notification details using `NOTIFYGET`, it would receive a successful response (code 292 or 293), but then immediately attempt to acknowledge the notification using `PUSHACK`. The AniDB server would respond with error `702 NO SUCH PACKET PENDING`, which was not handled by the application and resulted in an "UNSUPPORTED ReplyID" error message.

### Error Log Example
```
03:56:04: D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 794 [AniDB Queue] Sending query - Tag: 52 Command: NOTIFYGET type=M&id=4998280
03:56:04: AniDBApi: Recv: 52 292 NOTIFYGET
           4998280|0|-unknown-|1749195802|2|New anime relation added!|...
03:56:06: D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 794 [AniDB Queue] Sending query - Tag: 53 Command: PUSHACK nid=4998280
03:56:06: AniDBApi: Recv: 53 702 NO SUCH PACKET PENDING
03:56:06: D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 486 [AniDB Error] ParseMessage - UNSUPPORTED ReplyID: 702 Tag: 53
```

## Root Cause Analysis

The issue stems from a misunderstanding of when to use `PUSHACK`:

### PUSH Notifications (Code 270)
- **Unsolicited** notifications sent by the AniDB server to the client
- These are "pushed" to the client without being requested
- **MUST** be acknowledged using `PUSHACK` to confirm receipt
- Server expects acknowledgment and keeps the packet pending until acknowledged

### NOTIFYGET Notifications (Codes 292/293)
- **Actively fetched** notification details requested by the client
- Client explicitly asks for notification content using `NOTIFYGET type=M&id=XXX`
- Server responds with the notification data
- **NO acknowledgment needed** - the request-response cycle is complete
- There is no pending packet to acknowledge

### The Problem
The code was calling `PushAck()` after receiving NOTIFYGET responses (codes 292 and 293), treating them as if they were PUSH notifications. The server correctly responded with `702 NO SUCH PACKET PENDING` because there was no pending packet to acknowledge.

## Solution

### 1. Removed Incorrect PUSHACK Calls

**Code 292 Handler** (Message Notifications - type=M)
```cpp
// BEFORE
emit notifyMessageReceived(id, body);
PushAck(id);  // ❌ Incorrect

// AFTER  
emit notifyMessageReceived(id, body);
// Note: PUSHACK is only for PUSH notifications (code 270), not for fetched notifications via NOTIFYGET
```

**Code 293 Handler** (File Notifications - type=N)
```cpp
// BEFORE
// Note: For N-type notifications, we don't emit notifyMessageReceived...
PushAck(relid);  // ❌ Incorrect

// AFTER
// Note: For N-type notifications, we don't emit notifyMessageReceived...
// Note: PUSHACK is only for PUSH notifications (code 270), not for fetched notifications via NOTIFYGET
```

### 2. Added Handler for Reply Code 702

To ensure that if error 702 is received for any reason in the future, it will be properly logged instead of showing as an "UNSUPPORTED" error:

```cpp
else if(ReplyID == "702"){ // 702 NO SUCH PACKET PENDING
    // This occurs when trying to PUSHACK a notification that wasn't sent via PUSH
    // PUSHACK is only for notifications received via code 270 (PUSH), not for notifications fetched via NOTIFYGET
    Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 702 NO SUCH PACKET PENDING - Tag: " + Tag);
}
```

## Impact

### Before Fix
1. NOTIFYGET sent: `NOTIFYGET type=M&id=4998280`
2. Server responds: `292 NOTIFYGET {notification data}`
3. Application processes notification and emits signal ✓
4. Application sends: `PUSHACK nid=4998280` ❌
5. Server responds: `702 NO SUCH PACKET PENDING` ❌
6. Application logs: `UNSUPPORTED ReplyID: 702` ❌

### After Fix
1. NOTIFYGET sent: `NOTIFYGET type=M&id=4998280`
2. Server responds: `292 NOTIFYGET {notification data}`
3. Application processes notification and emits signal ✓
4. No PUSHACK sent ✓
5. No error ✓

## Comparison with Code 270 (PUSH)

For reference, the code 270 handler (PUSH notifications) **correctly** uses PUSHACK:

```cpp
else if(ReplyID == "270"){ // 270 NOTIFICATION - PUSH notification
    // Parse notification message
    int nid = parts[0].toInt();
    QString title = parts[4];
    QString body = parts[5];
    
    emit notifyMessageReceived(nid, body);
    
    // Acknowledge the notification ✓ CORRECT - this is a PUSH notification
    PushAck(nid);
}
```

This is correct because:
- Code 270 is an **unsolicited** PUSH notification from the server
- The server expects acknowledgment via PUSHACK
- Server responds with `271 NOTIFYACK` when acknowledgment is received

## Testing Recommendations

To verify this fix:

1. **Login and enable notifications**
   - Verify `NOTIFYLIST` is sent and received
   
2. **Fetch notification via NOTIFYGET**
   - Command: `NOTIFYGET type=M&id=XXXXX`
   - Expected response: `292 NOTIFYGET {notification data}`
   - Verify notification is processed (signal emitted, export URL extracted)
   - **Verify NO PUSHACK is sent**
   - Verify no error 702 occurs
   
3. **Receive PUSH notification**
   - Wait for server to push a notification (code 270)
   - Verify PUSHACK is sent
   - Expected response: `271 NOTIFYACK`
   
4. **Verify export/import pipeline**
   - Confirm export URLs are still extracted from notification bodies
   - Confirm downloads are triggered
   - Confirm imports are performed

## Files Modified

- `usagi/src/anidbapi.cpp` (lines 415, 440, 482-486)
  - Removed `PushAck(id)` from code 292 handler
  - Removed `PushAck(relid)` from code 293 handler
  - Added code 702 handler
  - Added explanatory comments

## Conclusion

This fix ensures that:
1. ✅ NOTIFYGET responses are processed correctly without incorrect acknowledgment attempts
2. ✅ Error 702 is properly handled if it occurs for any reason
3. ✅ PUSH notifications (code 270) continue to be acknowledged correctly
4. ✅ The notification processing pipeline (signal emission, export URL extraction) remains unchanged
5. ✅ Code is more maintainable with clear comments explaining the distinction between PUSH and NOTIFYGET

The changes are minimal and surgical, addressing only the specific issue without affecting other functionality.
