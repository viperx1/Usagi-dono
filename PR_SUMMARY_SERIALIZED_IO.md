# PR Summary: Serialized I/O for HDD Performance

## Overview
This PR addresses issue about hashing performance where users experience low CPU usage (~30%) when hashing files with multiple threads on traditional HDDs due to disk head thrashing.

## Problem Analysis

### User Report
- Running 6 threads for file hashing
- CPU usage only ~30% despite multithreading
- Suspected cause: Hitting HDD read limits with non-linear access patterns

### Root Cause
On traditional HDDs (not SSDs):
- **Sequential read**: ~100-150 MB/s
- **Random read**: ~1-5 MB/s (20-100x slower!)
- **Multiple threads** reading different files = constant disk seeking
- Each 100KB chunk read requires repositioning disk head
- Threads spend most time waiting for I/O, not CPU-bound

### Why Multiple Threads Hurt Performance on HDD
```
Thread 1: Reads File A chunk 1 → File A chunk 2 → File A chunk 3
Thread 2: Reads File B chunk 1 → File B chunk 2 → File B chunk 3
Thread 3: Reads File C chunk 1 → File C chunk 2 → File C chunk 3

Actual disk access pattern:
Seek to A chunk 1 → Read → Seek to B chunk 1 → Read → Seek to C chunk 1 → Read
Seek to A chunk 2 → Read → Seek to B chunk 2 → Read → Seek to C chunk 2 → Read
...
Constant seeking = terrible performance!
```

## Solution

### Implementation
Added an **optional** serialized I/O feature that:
1. Uses a global static mutex to serialize file I/O operations
2. Only locks during file reading (QFile::read operations)
3. Keeps MD4 hash finalization parallel (CPU work)
4. Can be enabled/disabled via UI checkbox
5. Default: **disabled** (backward compatible, good for SSDs)

### How It Works
```
Without Serialized I/O (default - good for SSD):
Thread 1: [Read File A]→[Hash A]
Thread 2:    [Read File B]→[Hash B]
Thread 3:       [Read File C]→[Hash C]
Parallel I/O, thrashing on HDD ❌

With Serialized I/O (optional - good for HDD):
Thread 1: [Read File A]→[Hash A]
Thread 2:              [Read File B]→[Hash B]
Thread 3:                           [Read File C]→[Hash C]
Linear reads, parallel hashing ✅
```

### Key Design Decisions
1. **Optional**: User must enable via checkbox (not automatic)
2. **Global**: All threads share same mutex
3. **Static**: Mutex and flag are static members of ed2k class
4. **Safe**: Proper unlocking on all exit paths (error, stop, success)
5. **Backward compatible**: Default disabled, no impact to existing behavior

## Changes Made

### 1. Core Implementation (ed2k.h, ed2k.cpp)
- Added `static QMutex fileIOMutex` - Global mutex for I/O serialization
- Added `static bool useSerializedIO` - Flag to enable/disable (default: false)
- Added `static void setSerializedIO(bool enabled)` - Configuration method
- Added `static bool getSerializedIO()` - Query method
- Modified `ed2khash()` to lock/unlock mutex around file I/O
- Added logging when feature is enabled/disabled

### 2. UI Integration (window.h, window.cpp)
- Added `QCheckBox *serializedIO` to Window class
- Created checkbox with label "Serialize I/O (for HDD)"
- Added tooltip explaining when to use the feature
- Positioned in hasher settings grid (row 0, column 3)
- Connected to `ButtonHasherStartClick()` to apply setting

### 3. Testing (test_serialized_io.cpp)
- `testSerializedIOFlag()` - Verify get/set functionality
- `testSerializedIODisabled()` - Hash with feature disabled
- `testSerializedIOEnabled()` - Hash with feature enabled
- `testMultipleFilesSerializedIO()` - Verify identical hashes regardless of setting

### 4. Documentation (SERIALIZED_IO_FEATURE.md)
- Complete problem analysis
- Architecture diagrams
- Usage guidelines (when to enable/disable)
- Performance characteristics
- Technical details
- Testing instructions
- Future enhancement ideas

## Statistics
```
6 files changed, 431 insertions(+), 3 deletions(-)
- SERIALIZED_IO_FEATURE.md: 208 lines (documentation)
- test_serialized_io.cpp: 145 lines (tests)
- ed2k.cpp: +59 lines (implementation)
- ed2k.h: +12 lines (declarations)
- window.cpp: +6 lines (UI integration)
- window.h: +1 line (UI member)
```

## Expected Performance Impact

### On HDD (with feature ENABLED):
- ✅ Sequential disk reads at full speed (~100-150 MB/s)
- ✅ Higher CPU utilization (less I/O wait)
- ✅ Better overall throughput
- ✅ Especially beneficial for many small-medium files

### On SSD (with feature DISABLED - default):
- ✅ No impact (feature not enabled)
- ✅ Parallel I/O remains unchanged
- ✅ Full CPU utilization maintained
- ✅ Backward compatible behavior

### On HDD (with feature DISABLED - default):
- ⚠️ Current problematic behavior (user's issue)
- ⚠️ Low CPU usage due to I/O wait
- ⚠️ Poor throughput due to seeking

## Usage Instructions

### For HDD Users
1. Open Usagi-dono
2. Go to Hasher tab
3. Check ☑ "Serialize I/O (for HDD)"
4. Add files and click Start
5. Observe improved CPU usage and throughput

### For SSD Users
- Leave checkbox unchecked (default)
- No action needed
- Performance unchanged

## Testing

### Manual Testing
User should test with and without the feature enabled:
1. Add multiple large files to hasher
2. Observe CPU usage and throughput without checkbox
3. Clear and re-add files
4. Enable checkbox and observe improved performance

### Automated Testing
```bash
cd build
cmake ..
make test_serialized_io
./tests/test_serialized_io
```

## Backward Compatibility
- ✅ Feature disabled by default
- ✅ No changes to existing behavior when unchecked
- ✅ No database schema changes
- ✅ No API changes
- ✅ Existing tests pass unchanged

## Security Considerations
- No security vulnerabilities introduced
- Mutex properly locked/unlocked (no deadlocks)
- Static members properly initialized
- Thread-safe implementation

## Future Enhancements
Potential improvements for future PRs:
1. Auto-detect HDD vs SSD and enable automatically
2. Per-disk control (different settings for different drives)
3. Dynamic adjustment based on I/O wait monitoring
4. Statistics display showing I/O throughput
5. Remember user's checkbox preference between sessions

## References
- Issue: User reported 30% CPU usage with 6 threads on HDD
- Solution: Optional serialized I/O with global mutex
- Default: Disabled (backward compatible)
- Implementation date: 2025-11-13

## Checklist
- [x] Problem analyzed and root cause identified
- [x] Solution designed (global I/O mutex)
- [x] Core implementation (ed2k.h, ed2k.cpp)
- [x] UI integration (window.h, window.cpp)
- [x] Comprehensive tests (test_serialized_io.cpp)
- [x] Full documentation (SERIALIZED_IO_FEATURE.md)
- [x] Backward compatibility maintained
- [x] Default behavior unchanged (feature disabled)
- [x] Thread safety verified
- [x] No security vulnerabilities
- [x] Code follows project conventions
- [x] Minimal changes (surgical approach)
