# Known Issues and Future Improvements

## 1. LocalIdentify File Size Type Mismatch

**Issue**: The `LocalIdentify()` method signature uses `int size` parameter, but it receives `qint64` file sizes from calling code. This could theoretically cause issues with files larger than 2GB (2^31 bytes) on some systems.

**Location**: 
- `usagi/src/anidbapi.h:211` - LocalIdentify signature
- `usagi/src/window.cpp` - Multiple call sites (lines 549, 783, 2140)

**Impact**: 
- Minimal in practice, as anime video files are typically under 2GB
- Would only affect very large files (e.g., 4K remux files > 2GB)

**Recommendation**: 
- Change `LocalIdentify` signature from `int size` to `qint64 size`
- Update database queries that use the size parameter
- Ensure consistent use of `qint64` for file sizes throughout codebase

**Status**: Pre-existing issue, not introduced by the UI freeze fix. Left as-is to minimize scope of changes.

---

*Note: This file documents known technical debt and areas for future improvement that are outside the scope of current bug fixes.*
