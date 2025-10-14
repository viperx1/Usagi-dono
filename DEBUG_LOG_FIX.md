# Debug Log Fix - AniDB Sent Command

## Issue
The debug log was showing incorrect information about the command actually sent to the AniDB server.

### Problem Description
From the issue logs:
```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 550 [AniDB Send] Command: "NOTIFYLIST&s=zaycS&tag=41"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 638 [AniDB Sent] Command: "NOTIFYLIST"
```

The `[AniDB Send]` log showed the complete command with session and tag parameters, but the `[AniDB Sent]` log only showed the base command without parameters. This made debugging difficult as it appeared the wrong command was being sent.

## Root Cause

In the `AniDBApi::SendPacket()` function:

1. **Line 630:** The original command `str` is retrieved from the database (e.g., `"NOTIFYLIST"`)
2. **Line 631:** Logs the queue message with the original command
3. **Line 637:** Calls `Send(str, "", tag)` which:
   - Adds session parameter: `NOTIFYLIST&s=zaycS`
   - Adds tag parameter: `NOTIFYLIST&s=zaycS&tag=41`
   - Stores the complete command in `lastSentPacket`
   - Logs at line 550: `[AniDB Send] Command: "NOTIFYLIST&s=zaycS&tag=41"`
4. **Line 638 (before fix):** Logged `str` instead of `lastSentPacket`

The problem was that line 638 was logging the **original** command string (`str`) before parameters were added, not the **actual** sent command.

## Solution

### Code Change
**File:** `usagi/src/anidbapi.cpp`, line 638

**Before:**
```cpp
Send(str,"", tag);
qDebug()<<__FILE__<<__LINE__<<"[AniDB Sent] Command:"<<str;
```

**After:**
```cpp
Send(str,"", tag);
qDebug()<<__FILE__<<__LINE__<<"[AniDB Sent] Command:"<<lastSentPacket;
```

### Why This Works

The `Send()` function stores the complete, final command in the `lastSentPacket` member variable (line 558):
```cpp
lastSentPacket = a;  // a contains the full command with all parameters
```

By using `lastSentPacket` instead of `str`, the log now shows exactly what was sent to the server.

## Expected Output After Fix

```
[AniDB Queue] Sending query - Tag: "41" Command: "NOTIFYLIST"
[AniDB Send] Command: "NOTIFYLIST&s=zaycS&tag=41"
[AniDB Sent] Command: "NOTIFYLIST&s=zaycS&tag=41"
```

Both `[AniDB Send]` and `[AniDB Sent]` now show the same complete command, making debugging much clearer.

## Impact

- **Type:** Debug logging improvement
- **Scope:** One line change
- **Risk:** Minimal - only affects debug output, no functional changes
- **Benefits:** 
  - Accurate debugging information
  - Easier troubleshooting of AniDB API issues
  - Consistent log messages
  - Prevents confusion about what was actually sent

## Testing

While Qt6 was not available for full integration testing, the fix was verified by:
1. Code analysis of the data flow
2. Confirmation that `lastSentPacket` is set correctly in `Send()` 
3. Verification that the variable is accessible in `SendPacket()`
4. Review of the minimal scope of the change

The fix is a straightforward variable substitution with no side effects.
