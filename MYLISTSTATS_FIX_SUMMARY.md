# MYLISTSTATS Command Fix Summary

## Issue Description

When calling the MYLISTSTATS command, the AniDB server was returning error code **222 (MYLIST - MULTIPLE ENTRIES FOUND)** instead of the expected **223 (MYLISTSTATS)**, resulting in no data being displayed.

### Error Log
```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 511 got query to send: "34" "MYLISTSTATS "
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 430 "MYLISTSTATS &s=fIV4H&tag=34"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 518 "MYLISTSTATS "
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 129 "34"   "222"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 223 MYLIST - MULTIPLE ENTRIES FOUND

nothing showed up on the list.
```

## Root Cause

The MYLISTSTATS command was being generated with a trailing space in `usagi/src/anidbapi.cpp` line 402:

```cpp
msg = QString("MYLISTSTATS ");  // ← Trailing space causes the issue
```

When this command is sent through the `Send()` function, it becomes:
```
MYLISTSTATS &s={SID}&tag={tag}
            ↑ Space before & confuses the server
```

The AniDB server interprets this as a MYLIST query with empty parameters, which results in error code 222 (MYLIST - MULTIPLE ENTRIES FOUND) instead of recognizing it as a MYLISTSTATS command.

## Solution

### Code Change
**File:** `usagi/src/anidbapi.cpp` line 402

**Before:**
```cpp
msg = QString("MYLISTSTATS ");
```

**After:**
```cpp
msg = QString("MYLISTSTATS");
```

Now the command is properly formatted:
```
MYLISTSTATS&s={SID}&tag={tag}
           ↑ No space - correct format
```

### Test Enhancement
**File:** `tests/test_anidbapi.cpp`

Added explicit verification that the MYLISTSTATS command has no trailing space:

```cpp
void TestAniDBApiCommands::testMylistStatCommandFormat()
{
    // Call Mylist() with no lid (or lid <= 0) to get MYLISTSTATS
    api->Mylist(-1);
    
    // Get the command that was inserted
    QString msg = getLastPacketCommand();
    
    // Verify command is not empty
    QVERIFY(!msg.isEmpty());
    
    // Verify command is MYLISTSTATS
    QVERIFY(msg.startsWith("MYLISTSTATS"));
    
    // Verify command does not have trailing space (issue fix)
    // The command should be exactly "MYLISTSTATS" without trailing space
    // to prevent AniDB server from returning error 222 instead of 223
    QCOMPARE(msg, QString("MYLISTSTATS"));  // ← New assertion
}
```

## Expected Result

After this fix:
- ✅ MYLISTSTATS command will be recognized correctly by AniDB server
- ✅ Server will respond with code **223** (MYLISTSTATS) instead of error 222
- ✅ Statistics data will be returned and can be displayed:
  - Total entries
  - Watched count
  - Total file size
  - Watched file size
  - Percentages
  - Episodes watched

## Changes Summary

| File | Lines Changed | Description |
|------|---------------|-------------|
| `usagi/src/anidbapi.cpp` | 1 (-1 char) | Removed trailing space from MYLISTSTATS command |
| `tests/test_anidbapi.cpp` | +5 | Added test assertion to verify no trailing space |

**Total:** 2 files changed, 6 insertions(+), 1 deletion(-)

## AniDB UDP API Specification

According to the AniDB UDP API Definition (https://wiki.anidb.net/UDP_API_Definition):

**MYLISTSTATS Command:**
- **Format:** `MYLISTSTATS`
- **Response:** `223 MYLISTSTATS` with data: `entries|watched|size|viewed size|viewed%|watched%|episodes watched`
- **No parameters required** - only session ID is needed

The command name should not have trailing spaces. Parameters are appended with `&` separator by the Send() function.

## Testing

The fix has been validated by:
1. Code review - correct command format according to AniDB API spec
2. Enhanced unit test - explicitly checks for no trailing space
3. Minimal change principle - only 1 character removed

**Note:** Full integration testing with actual AniDB server requires proper credentials and network access.

## Related Information

- AniDB UDP API Definition: https://wiki.anidb.net/UDP_API_Definition
- Issue tracking: API response code 222 instead of 223 for MYLISTSTATS
- Related files: `MYLIST_API_GUIDELINES.md`, `IMPLEMENTATION_SUMMARY.md`
