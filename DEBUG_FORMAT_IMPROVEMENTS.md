# Debug Format Improvements

## Issue
The debug output from the AniDB API was difficult to read and understand due to lack of context and unclear formatting.

## Example of the Problem
**Before:**
```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 152 "0"   "200"
D:/a/Usagi-dono/Usagi-dono/usagi/src/window.cpp 655 getNotifyLoggedIn
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 603 got query to send: "39" "NOTIFYLIST"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 522 "NOTIFYLIST&s=ufc7n&tag=39"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 610 "NOTIFYLIST"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 152 "598"   "UNKNOWN"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 360 ParseMessage - not supported ReplyID ( "UNKNOWN" )
```

**After:**
```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 152 [AniDB Response] Tag: "0" ReplyID: "200"
D:/a/Usagi-dono/Usagi-dono/usagi/src/window.cpp 655 [Window] Login notification received - Tag: "0" Code: 200
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 603 [AniDB Queue] Sending query - Tag: "39" Command: "NOTIFYLIST"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 522 [AniDB Send] Command: "NOTIFYLIST&s=ufc7n&tag=39"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 610 [AniDB Sent] Command: "NOTIFYLIST"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 170 [AniDB Response] Tagless response detected - Tag: "0" ReplyID: "598"
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 366 [AniDB Error] 598 UNKNOWN COMMAND - Tag: "0" - check request format
```

**Note:** With the tagless response detection fix (Oct 2024), responses like "598 UNKNOWN COMMAND" without a tag are now properly detected and handled. The parser detects when the first token is numeric (error code) and the second token is non-numeric (error text), and correctly identifies this as a tagless response.

## Changes Made

### 1. Added Category Prefixes
All debug messages now start with a category prefix in square brackets:
- `[AniDB Response]` - Messages received from AniDB API
- `[AniDB Send]` - Commands being sent to AniDB
- `[AniDB Queue]` - Commands being queued for sending
- `[AniDB Sent]` - Commands that have been sent
- `[AniDB Error]` - Error conditions
- `[AniDB Timeout]` - Timeout messages
- `[AniDB API]` - General API operations
- `[Window]` - UI-related messages

### 2. Added Clear Labels
All values now have clear labels:
- `Tag:` prefix for tag values
- `ReplyID:` prefix for reply IDs
- `Command:` prefix for commands
- `Code:` prefix for status codes
- `Data:` prefix for data fields
- `NID:` prefix for notification IDs
- `Title:` and `Body:` for notification content
- `Elapsed:` for timing information

### 3. Improved Error Messages
Error messages are now more descriptive:
- Changed "ParseMessage - not supported ReplyID" to "[AniDB Error] ParseMessage - UNSUPPORTED ReplyID"
- Added Tag information to error messages
- Changed "UNKNOWN COMMAND" to "[AniDB Error] 598 UNKNOWN COMMAND"

## Files Modified

### usagi/src/anidbapi.cpp
Updated 20+ debug statements to use the new format:
- Line 19: DNS resolution error
- Line 152: Response parsing
- Line 152-180: **Tagless response detection** (added Oct 2024) - Detects and handles error responses without tags like "598 UNKNOWN COMMAND"
- Line 167: Logout response
- Line 248: MYLISTSTATS response
- Line 254: WISHLIST response
- Line 275: NO SUCH MYLIST ENTRY
- Line 296: Notification received
- Line 306: Notification acknowledged
- Line 309: No such notification
- Line 315: NOTIFYLIST response
- Line 321: NOTIFYLIST ENTRY
- Line 354: 598 UNKNOWN COMMAND error
- Line 360: Unsupported ReplyID error
- Line 384: Logout command
- Line 511: Socket not initialized error
- Line 522: Send command
- Line 603: Queue command
- Line 610: Sent command
- Line 617: Timeout message

### usagi/src/window.cpp
Updated 1 debug statement:
- Line 655: Login notification in window

## Benefits

1. **Easier Debugging**: Category prefixes make it easy to filter and search log output
2. **Clear Context**: Labels provide immediate understanding of what each value represents
3. **Better Error Tracking**: Error messages clearly indicate the category and provide more context
4. **Consistent Format**: All debug messages follow the same pattern
5. **Grep-Friendly**: Easy to search for specific categories (e.g., `grep "\[AniDB Error\]"`)

## Impact

- **Code Changes**: Minimal - only modified debug output strings
- **Functionality**: No functional changes, only improved debug output
- **Testing**: No new tests needed - this is purely cosmetic improvement
- **Backward Compatibility**: Full - only changes debug output format
