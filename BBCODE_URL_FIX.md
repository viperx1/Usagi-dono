# BBCode URL Extraction Fix

## Issue
AniDB notifications use BBCode format for URLs, like:
```
[url=https://anidb.net/export/12345.tgz]Download here[/url]
```

The old regex pattern `https?://[^\s]+\.tgz` could not match URLs wrapped in BBCode tags, causing mylist export links to not be detected.

## Solution
Updated the URL extraction logic in `window.cpp` to handle both:
1. **BBCode format**: `[url=URL]text[/url]` - extracts URL from within BBCode tags
2. **Plain format**: Direct URLs in the text (fallback)

### Regex Patterns
- BBCode: `\[url=(https?://[^\]]+\.tgz)\]` - captures URL from group 1
- Plain: `https?://[^\s]+\.tgz` - captures entire match

## Test Cases
Created comprehensive tests in `test_url_extraction.cpp`:
- ✅ Plain URL extraction
- ✅ BBCode URL extraction  
- ✅ No URL in message (returns empty)
- ✅ Multiple URLs (matches first .tgz)
- ✅ Real notification from issue (no .tgz, returns empty correctly)

## Expected Behavior

### Before Fix
```
Notification 4998280 received
No mylist export link found in notification  ❌
```

### After Fix
**For export notifications with BBCode:**
```
Notification 5000001 received
MyList export link found: https://anidb.net/export/12345.tgz  ✅
```

**For other notifications without .tgz URLs:**
```
Notification 4998280 received
No mylist export link found in notification  ✅ (correct)
```

## Files Changed
- `usagi/src/window.cpp` - Updated URL extraction logic
- `tests/test_url_extraction.cpp` - New test suite
- `tests/CMakeLists.txt` - Added new test to build system
