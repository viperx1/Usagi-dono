# Truncated API Response Handling Implementation

## Problem Statement
When AniDB UDP API responses reach the 1400-byte UDP MTU limit, responses are truncated. The last field in the response is incomplete and should not be processed. The system needs to:
1. Detect when a response is truncated (datagram size = 1400 bytes)
2. Skip processing the incomplete last field
3. Process all complete fields normally
4. Log appropriate warnings

## Solution Overview

### Detection (anidbapi.cpp:1616-1653)
```cpp
// In Recv() function:
if(data.size() >= 1400 || bytesRead >= 1400)
{
    isTruncated = true;
    Logger::log("[AniDB Recv] TRUNCATION DETECTED: Datagram at MTU limit...");
}
```

### Processing (anidbapi.cpp:302-860)
Modified `ParseMessage()` to accept `isTruncated` parameter (defaults to false for backward compatibility).

For each response type (220 FILE, 221 MYLIST, 230 ANIME, 240 EPISODE):
```cpp
// Handle truncated responses - remove the last field as it's likely incomplete
if(isTruncated && token2.size() > 0)
{
    Logger::log("[AniDB Response] XXX - Truncated response, removing last field...");
    token2.removeLast();
}
```

### Data Structure (anidbapi.h:71-79)
Added `TruncatedResponseInfo` structure for potential future enhancements:
```cpp
struct TruncatedResponseInfo {
    bool isTruncated;
    QString tag;
    QString command;
    int fieldsParsed;
    unsigned int fmaskReceived;
    unsigned int amaskReceived;
} truncatedResponse;
```

## Implementation Details

### Response Handlers Modified
1. **FILE (220)** - Lines 530-587
   - Removes last field if truncated
   - Parses file, anime, episode, and group data
   - Stores complete fields only
   - Logs warning with field count

2. **MYLIST (221)** - Lines 595-661
   - Removes last field if truncated
   - Parses mylist entry fields
   - Stores complete fields only
   - Logs warning

3. **ANIME (230)** - Lines 728-808
   - Removes last field if truncated
   - Parses anime metadata
   - Updates database with complete fields
   - Logs warning with field count

4. **EPISODE (240)** - Lines 810-862
   - Removes last field if truncated
   - Parses episode information
   - Stores complete fields only
   - Logs warning

### Test Coverage
Created comprehensive test suite in `tests/test_truncated_response.cpp`:

1. **testTruncatedFileResponse()** 
   - Simulates truncated FILE response
   - Verifies complete fields are stored
   - Confirms incomplete last field is not processed

2. **testTruncatedAnimeResponse()**
   - Simulates truncated ANIME response
   - Verifies anime type is stored correctly
   - Tests database update with partial data

3. **testTruncatedMylistResponse()**
   - Simulates truncated MYLIST response
   - Verifies mylist entry stored with complete fields
   - Confirms storage field truncation handled

4. **testTruncatedEpisodeResponse()**
   - Simulates truncated EPISODE response
   - Verifies episode data stored correctly
   - Tests handling of truncated kanji title

5. **testNonTruncatedResponse()**
   - Verifies normal responses still work correctly
   - Ensures no regression in non-truncated case
   - Tests all fields including filename

## Behavioral Changes

### Before
- Truncated responses would attempt to parse incomplete last field
- Could lead to corrupted data or parsing errors
- No warning about truncation
- Last field with partial data might be stored incorrectly

### After
- Truncation is detected at 1400-byte threshold
- Incomplete last field is removed before parsing
- Clear logging indicates truncation occurred
- Only complete, valid fields are processed and stored
- Application continues to function with partial but correct data

## Logging Example
```
[AniDB Recv] TRUNCATION DETECTED: Datagram at MTU limit (1400 bytes), response is truncated
[AniDB Response] TRUNCATED response detected for Tag: 1001 ReplyID: 220
[AniDB Response] 220 FILE - Truncated response, removing last field (was: 'test_truncat')
[AniDB Response] 220 FILE - Original field count: 24, processing 23 fields
[AniDB Response] 220 FILE - WARNING: Response was truncated, some fields may be missing. Processed 23 fields successfully.
```

## Future Enhancements
The `TruncatedResponseInfo` structure is in place for potential future improvements:
- Track which fields were successfully parsed
- Calculate remaining mask bits for missing fields
- Potentially implement automatic re-request with reduced mask
- Merge multiple partial responses

## Testing
To test the implementation:
1. Build the project with CMake
2. Run: `ctest -R test_truncated_response -V`
3. All 5 test cases should pass

## Known Limitations
- Does not automatically re-request missing fields
- Cannot determine exactly which mask bits correspond to the incomplete field
- Logs warnings but does not notify user through UI
- Some data may be permanently incomplete if it only exists in truncated response

## Backward Compatibility
- `ParseMessage()` signature changed but maintains backward compatibility with default parameter
- No changes to API or database schema
- Existing code calling `ParseMessage()` without truncation flag continues to work
- No impact on non-truncated responses
