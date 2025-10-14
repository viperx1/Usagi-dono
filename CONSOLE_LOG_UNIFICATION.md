# Console and Log Tab Output Unification

## Issue
The console (via qDebug) and the Log tab in the UI were showing different debug information. Console showed comprehensive debug output while the Log tab showed limited information, making it difficult to troubleshoot issues without access to the console output.

## Problem Analysis

### Before Fix
- **qDebug() calls**: Output to console only (stderr)
- **Debug() calls**: In the overridden `myAniDBApi::Debug()`, output to Log tab only via signal
- **Result**: Console and Log tab showed different information

### Example Output Discrepancy
**Console showed:**
```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 343 [AniDB Response] 290 NOTIFYLIST - Tag: 44 Entry count: 105
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 348 [AniDB Response] 290 NOTIFYLIST Entry 1 of 105: M|4756534
```

**Log tab showed:**
```
AniDBApi: Socket already created
AniDBApi: UDP socket created
```

## Solution

### 1. Modified myAniDBApi::Debug() Function (window.cpp)
**Before:**
```cpp
void myAniDBApi::Debug(QString msg)
{
    emit notifyLogAppend(msg);  // Log tab only
}
```

**After:**
```cpp
void myAniDBApi::Debug(QString msg)
{
    // Output to console (for development and debugging)
    qDebug() << msg;
    // Also emit signal to update Log tab in UI
    emit notifyLogAppend(msg);
}
```

### 2. Unified Debug Output in anidbapi.cpp
Converted all `qDebug()` calls to use `Debug()` function instead:

**Before:**
```cpp
qDebug()<<__FILE__<<__LINE__<<"[AniDB Response] Tag:"<<Tag<<"ReplyID:"<<ReplyID;
```

**After:**
```cpp
Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] Tag: " + Tag + " ReplyID: " + ReplyID);
```

### 3. Extended to Window Debug Messages (window.cpp)
Updated window-specific debug messages to also appear in Log tab:

**Before:**
```cpp
void Window::getNotifyLoggedIn(QString tag, int code)
{
    qDebug()<<__FILE__<<__LINE__<<"[Window] Login notification received - Tag:"<<tag<<"Code:"<<code;
    // ... rest of function
}
```

**After:**
```cpp
void Window::getNotifyLoggedIn(QString tag, int code)
{
    QString logMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " [Window] Login notification received - Tag: " + tag + " Code: " + QString::number(code);
    qDebug() << logMsg;
    getNotifyLogAppend(logMsg);
    // ... rest of function
}
```

## Changes Made

### Files Modified
1. **usagi/src/window.cpp**
   - Modified `myAniDBApi::Debug()` to output to both console and Log tab
   - Updated `Window::ButtonLoginClick()` debug message
   - Updated `Window::getNotifyMylistAdd()` debug messages
   - Updated `Window::getNotifyLoggedIn()` debug message
   - Updated `Window::getNotifyLoggedOut()` debug message
   - Updated `Window::getNotifyMessageReceived()` debug message

2. **usagi/src/anidbapi.cpp**
   - Converted 27+ `qDebug()` calls to `Debug()` calls
   - Categories include:
     - AniDB Error messages (DNS, socket, unknown commands)
     - AniDB Response messages (login, logout, mylist, notifications)
     - AniDB Protocol messages (send, sent, timeout)
     - Database query error messages

## Expected Output After Fix

Both console and Log tab now show the same comprehensive information:

```
/path/to/usagi/src/anidbapi.cpp 343 [AniDB Response] 290 NOTIFYLIST - Tag: 44 Entry count: 105
/path/to/usagi/src/anidbapi.cpp 348 [AniDB Response] 290 NOTIFYLIST Entry 1 of 105: M|4756534
/path/to/usagi/src/anidbapi.cpp 348 [AniDB Response] 290 NOTIFYLIST Entry 2 of 105: M|4755053
...
/path/to/usagi/src/anidbapi.cpp 684 [AniDB Send] Command: NOTIFYLIST&s=zaycS&tag=41
/path/to/usagi/src/anidbapi.cpp 772 [AniDB Sent] Command: NOTIFYLIST&s=zaycS&tag=41
```

## Benefits

1. **Unified Debugging Experience**: Both console and Log tab show the same information
2. **Better Troubleshooting**: Users can see comprehensive debug info in the UI without needing console access
3. **Persistent Logging**: Debug messages are also written to crash log files via `CrashLog::logMessage()`
4. **No Breaking Changes**: All existing functionality preserved
5. **Consistent Format**: All debug messages follow the same pattern with file/line information

## Testing

- ✅ All tests pass (4/4)
- ✅ Build successful with no compilation errors
- ✅ Only pre-existing warnings remain (unrelated to this change)

## Impact

- **Type**: Debug output improvement
- **Scope**: Debug logging infrastructure
- **Risk**: Minimal - only affects debug output format and destination
- **Backward Compatibility**: Full - no breaking changes to public API
