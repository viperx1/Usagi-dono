# NOTIFYLIST Command Format Fix

## Issue

The NOTIFYLIST command was being rejected by the AniDB server with error "598 UNKNOWN COMMAND".

### Problem Description

From the issue logs:
```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 550 [AniDB Send] Command: "NOTIFYLIST&s=cIWKW&tag=42"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 638 [AniDB Sent] Command: "NOTIFYLIST&s=cIWKW&tag=42"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 170 [AniDB Response] Tagless response detected - Tag: "0" ReplyID: "598"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 382 [AniDB Error] 598 UNKNOWN COMMAND - Tag: "0" - check request format
```

The server returned "598 UNKNOWN COMMAND" because the command format was incorrect. The command name (NOTIFYLIST) was not followed by a space before parameters.

## Root Cause

The AniDB UDP API requires commands to follow this format:
```
COMMAND param1=value1&param2=value2
```

There **must** be a space between the command name and the first parameter. When a command has no inherent parameters (like NOTIFYLIST, LOGOUT, MYLISTSTATS), the command string must end with a space so that when session and tag parameters are appended, they are properly separated.

### In the Code

**NotifyEnable() function** (line 523 in anidbapi.cpp):
```cpp
QString msg = QString("NOTIFYLIST");  // ❌ Missing trailing space
```

When this command is sent via the `Send()` function:
1. Line 545: Session parameter added: `"NOTIFYLIST" + "&s=cIWKW"` = `"NOTIFYLIST&s=cIWKW"`
2. Line 549: Tag parameter added: `"NOTIFYLIST&s=cIWKW" + "&tag=42"` = `"NOTIFYLIST&s=cIWKW&tag=42"`

Result: **`NOTIFYLIST&s=cIWKW&tag=42`** ❌ (no space after command name)

The server cannot parse this as a valid command because there's no space separating the command name from parameters.

## Solution

### Code Change

**File:** `usagi/src/anidbapi.cpp`, line 523

**Before:**
```cpp
QString msg = QString("NOTIFYLIST");
```

**After:**
```cpp
QString msg = QString("NOTIFYLIST ");
```

### Why This Works

With the trailing space, when the `Send()` function adds parameters:
1. Line 545: Session parameter added: `"NOTIFYLIST " + "&s=cIWKW"` = `"NOTIFYLIST &s=cIWKW"`
2. Line 549: Tag parameter added: `"NOTIFYLIST &s=cIWKW" + "&tag=42"` = `"NOTIFYLIST &s=cIWKW&tag=42"`

Result: **`NOTIFYLIST &s=cIWKW&tag=42`** ✅ (space after command name)

The server can now properly parse the command.

## Pattern Consistency

This fix makes NOTIFYLIST consistent with other parameterless commands in the codebase:

| Command | Line | Format | Status |
|---------|------|--------|--------|
| LOGOUT | 411 | `QString("LOGOUT ")` | ✅ Has space |
| MYLISTSTATS | 493 | `QString("MYLISTSTATS ")` | ✅ Has space |
| NOTIFYLIST | 523 | `QString("NOTIFYLIST ")` | ✅ Has space (after fix) |

Commands with parameters also have a space before the first parameter:

| Command | Line | Format | Example |
|---------|------|--------|---------|
| MYLISTADD | 425 | `QString("MYLISTADD size=...")` | `MYLISTADD size=123&ed2k=abc` |
| FILE | 471 | `QString("FILE size=...")` | `FILE size=123&ed2k=abc&fmask=...` |
| MYLIST | 488 | `QString("MYLIST lid=...")` | `MYLIST lid=123` |
| PUSHACK | 508 | `QString("PUSHACK nid=...")` | `PUSHACK nid=123` |

All follow the pattern: `COMMAND param1=value1&param2=value2`

## Expected Output After Fix

### Before Fix (Error)
```
[AniDB Queue] Sending query - Tag: "42" Command: "NOTIFYLIST"
[AniDB Send] Command: "NOTIFYLIST&s=cIWKW&tag=42"
[AniDB Sent] Command: "NOTIFYLIST&s=cIWKW&tag=42"
[AniDB Response] Tagless response detected - Tag: "0" ReplyID: "598"
[AniDB Error] 598 UNKNOWN COMMAND - Tag: "0" - check request format
```

### After Fix (Success)
```
[AniDB Queue] Sending query - Tag: "42" Command: "NOTIFYLIST "
[AniDB Send] Command: "NOTIFYLIST &s=cIWKW&tag=42"
[AniDB Sent] Command: "NOTIFYLIST &s=cIWKW&tag=42"
[AniDB Response] 290 NOTIFYLIST - Tag: "42" Data: ...
```

## Impact

- **Type:** Bug fix - Command format error
- **Scope:** Single character change (1 space added)
- **Risk:** Minimal - only affects NOTIFYLIST command, makes it consistent with other commands
- **Benefits:** 
  - Fixes "598 UNKNOWN COMMAND" error for NOTIFYLIST
  - Enables automatic notification list retrieval
  - Makes code consistent with other parameterless commands
  - Follows AniDB UDP API specification

## Testing

While Qt6 was not available for full integration testing, the fix was verified by:
1. Code analysis confirming the space is required by the API
2. Comparison with other commands (LOGOUT, MYLISTSTATS) that work correctly
3. Analysis of the command construction flow in Send() function
4. Review of existing tests (no changes needed - tests only validate command names)

The fix can be fully tested by:
1. Running the application with the fix
2. Logging in to AniDB
3. Observing the NOTIFYLIST command is sent and accepted
4. Verifying no "598 UNKNOWN COMMAND" error occurs

## Related Documentation

- AniDB UDP API Definition: https://wiki.anidb.net/UDP_API_Definition
- Notifications API Implementation: `NOTIFICATIONS_API_IMPLEMENTATION.md`
- API Tests: `tests/API_TESTS.md`
