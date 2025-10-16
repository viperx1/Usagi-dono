# Export URL Extraction Fix

## Issue
The application was failing to download MyList export files due to incorrect URL extraction from AniDB notification messages.

### Problem
The log showed:
```
MyList export link found: https://export.anidb.net/export/1760573760-1335-71837.tgz]1760573760-1335-71837.tgz
Error downloading export: Error transferring https://export.anidb.net/export/1760573760-1335-71837.tgz]1760573760-1335-71837.tgz - server replied:
```

The URL had extra characters `]1760573760-1335-71837.tgz` appended, making it invalid.

### Root Cause
AniDB sends notification messages in BBCode format:
```
Direct Download: [url=https://export.anidb.net/export/1760573760-1335-71837.tgz]1760573760-1335-71837.tgz[/url]
```

The original regex `https?://[^\\s]+\\.tgz` matched everything from `https` to `.tgz`, including the closing bracket and duplicate filename text.

## Solution
Updated the URL extraction logic in `usagi/src/window.cpp` to:
1. First try to match BBCode format: `\[url=(https?://[^\]]+\.tgz)\]`
2. If that fails, fallback to plain URL format: `https?://[^\s\]]+\.tgz`

### Changes Made
- Modified `Window::getNotifyMessageReceived()` in `usagi/src/window.cpp`
- The BBCode regex captures the URL from inside `[url=...]` tags
- Added bracket exclusion to plain URL regex as additional safety

### Test Results
All existing tests pass:
- `test_url_extraction`: All 7 tests pass
- Manual verification with actual notification format: Pass
- BBCode format extraction: Pass
- Plain URL format extraction: Pass
- No URL in message: Pass (returns empty string)

## Impact
- MyList export downloads will now work correctly
- Both BBCode and plain text URLs are supported
- No breaking changes to existing functionality
