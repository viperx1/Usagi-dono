# Tagless Response Detection Fix

## Issue
When AniDB receives a malformed or unrecognized command, it responds with an error message without a tag. For example:
```
598 UNKNOWN COMMAND
```

The parser was incorrectly interpreting this as:
- Tag: "598"
- ReplyID: "UNKNOWN"

This caused the response to fall through to the "UNSUPPORTED ReplyID" error handler, resulting in:
```
[AniDB Error] ParseMessage - UNSUPPORTED ReplyID: "UNKNOWN" Tag: "598"
```

Instead of the proper 598 error handler:
```
[AniDB Error] 598 UNKNOWN COMMAND - Tag: "598" - check request format
```

## Root Cause
The AniDB UDP API normally responds in the format:
```
{tag} {replycode} {message}
```

However, when a command is so malformed that the server cannot extract the tag, it responds in the format:
```
{replycode} {message}
```

The parser was not detecting this case and was treating the replycode as the tag.

## Solution
Added detection logic in `ParseMessage()` (lines 152-180) to identify tagless responses:

1. Check if the first token (Tag) is numeric
2. Check if the second token (ReplyID) is non-numeric
3. If both conditions are true, we have a tagless response
4. Swap them: ReplyID = Tag, Tag = "0" (default tag)

This allows tagless error responses to be properly handled by their corresponding error handlers.

## Examples

### Before Fix
**Input:** `598 UNKNOWN COMMAND`
- Parsed as: Tag="598", ReplyID="UNKNOWN"
- Result: Falls through to "UNSUPPORTED ReplyID" error ❌

### After Fix
**Input:** `598 UNKNOWN COMMAND`
- Parsed as: Tag="598", ReplyID="UNKNOWN"
- **Detection:** Tag is numeric (598), ReplyID is non-numeric ("UNKNOWN")
- **Correction:** Tag="0", ReplyID="598"
- Result: Handled by 598 error handler ✅
- Output: `[AniDB Response] Tagless response detected - Tag: "0" ReplyID: "598"`
- Output: `[AniDB Error] 598 UNKNOWN COMMAND - Tag: "0" - check request format`

## Normal Responses Still Work
**Input:** `40 200 LOGIN ACCEPTED`
- Parsed as: Tag="40", ReplyID="200"
- **Detection:** Tag is numeric (40), ReplyID is numeric (200)
- **Result:** No correction needed, Tag="40", ReplyID="200" ✅

## Other Tagless Errors Handled
This fix also handles other potential tagless error responses:
- `601 OUT OF SERVICE`
- `555 BANNED`
- Any other numeric error code followed by text

## Impact
- **Fixes:** Issue where "598 UNKNOWN COMMAND" responses were incorrectly reported as "UNSUPPORTED ReplyID"
- **Benefits:** Better error handling and more accurate error messages
- **Side effects:** None - normal responses are unaffected
- **Backward compatibility:** Full - only improves error handling

## Testing
The fix can be tested by:
1. Sending a malformed command to AniDB (e.g., wrong parameter format)
2. Observing the error response is now properly identified as 598
3. Verifying normal responses still work correctly

## Code Location
- File: `usagi/src/anidbapi.cpp`
- Function: `ParseMessage()`
- Lines: 152-180
