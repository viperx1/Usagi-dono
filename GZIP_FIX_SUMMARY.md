# Gzip Decompression Fix for Anime Titles Download

## Issue
When downloading anime titles from AniDB (`anime-titles.dat.gz`), the application would fail with:
```
02:51:56: Downloaded 1349210 bytes of compressed anime titles data
02:51:56: Failed to decompress anime titles data. Will retry on next startup.
qUncompress: Input data is corrupted
```

## Root Cause
The original implementation attempted to manually decompress gzip files by:
1. Skipping a fixed 10 bytes for the gzip header
2. Removing the last 8 bytes (gzip footer)
3. Prepending a zlib header (`0x78 0x9C`) to the raw deflate data
4. Using Qt's `qUncompress()` function

**This approach was fundamentally flawed because:**
- Gzip headers are **variable length** (10 bytes minimum, but can be much longer)
- The FLG byte (byte 3) indicates which optional fields are present:
  - FEXTRA (bit 2): Extra field with variable length
  - FNAME (bit 3): Original filename (null-terminated string)
  - FCOMMENT (bit 4): File comment (null-terminated string)
  - FHCRC (bit 1): Header CRC16 (2 bytes)
- Simply prepending `0x78 0x9C` doesn't create a valid zlib stream
- `qUncompress()` expects a complete zlib format with checksums

## Solution
Use zlib's `inflateInit2()` function with the proper `windowBits` parameter:
```cpp
inflateInit2(&stream, 15 + 16)
```

Where:
- `15` = default windowBits for deflate algorithm
- `+16` = tells zlib to expect gzip format with header and trailer

This approach:
- ✅ Automatically handles variable-length gzip headers
- ✅ Properly parses all optional gzip fields
- ✅ Validates gzip CRC32 checksums
- ✅ Correctly decompresses the deflate stream
- ✅ Works with any valid gzip file regardless of compression options

## Code Changes

### 1. Added zlib Header (`usagi/src/anidbapi.h`)
```cpp
#include <zlib.h>
```

### 2. Replaced Manual Gzip Parsing (`usagi/src/anidbapi.cpp`)
**Before:** Manual header parsing + `qUncompress()`
**After:** Direct zlib decompression with `inflateInit2()`

Key changes:
- Initialize zlib stream with `inflateInit2(&stream, 15 + 16)`
- Use 4MB buffer for decompression (adequate for anime titles file)
- Loop with `inflate()` until `Z_STREAM_END`
- Proper error handling and cleanup

### 3. Updated CMakeLists.txt
Added zlib library linking:
- `usagi/CMakeLists.txt`: Added `z` to `target_link_libraries`
- `tests/CMakeLists.txt`: Added `z` to test executables

## Testing

### Unit Tests
All existing tests pass (4/4):
- ✅ test_hash
- ✅ test_crashlog  
- ✅ test_anidbapi
- ✅ test_anime_titles

### Manual Verification
Tested with various gzip formats:
- ✅ Gzip with filename (FNAME flag set)
- ✅ Gzip without filename (no optional fields)
- ✅ Large files (267KB+, similar to real anime titles file)
- ✅ All tests successfully decompress and match original data

## Impact
- **Minimal code changes**: Only modified decompression logic
- **No breaking changes**: All existing tests pass
- **Proper fix**: Uses standard zlib API instead of workarounds
- **Future-proof**: Will work with any gzip compression options

## References
- RFC 1952: GZIP file format specification
- zlib Manual: https://www.zlib.net/manual.html
- `inflateInit2()` documentation for `windowBits` parameter
