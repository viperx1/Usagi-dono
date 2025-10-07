# Crash Log Encoding Fix Methods

This document tracks all methods attempted to fix the crash log encoding issue where logs were saved with incorrect encoding (UTF-16LE instead of UTF-8), resulting in garbled output. Update document as necessary. When adding new method mark the previous one as failed, then add new one with status unknown.

## Problem Description

### Symptoms
When crash logs were written on Windows, they appeared garbled when opened in text editors:
- Text appeared as Chinese/Unicode characters mixed with symbols
- File size was approximately double what it should be
- Opening in hex editor showed UTF-16LE encoding (0xFF 0xFE BOM, followed by bytes like 0x3D 0x00 for '=')
- Example garbled output from issue: `㴽‽剃十⁈佌⁇㴽਽䌊慲桳删慥潳㩮匠来敭瑮瑡潩⁮慆汵⁴匨䝉䕓噇਩`

### Root Cause
On Windows, `QTextStream` defaults to UTF-16LE encoding when no explicit encoding is set. Additionally, stdout/stderr in text mode can perform conversions that corrupt binary data.

## Fix Methods Tracking

### Method 1: Explicit UTF-8 Encoding with QTextStream and Binary Mode for File Descriptors

**Status:** Working ✓
**Implementation Date:** 2025-01-07
**Files Modified:**
- `usagi/src/crashlog.cpp`
- `usagi/src/crashlog.h`

**Changes Made:**

1. **QTextStream UTF-8 Encoding (generateCrashLog and logMessage functions):**
   - Added `stream.setEncoding(QStringConverter::Utf8)` to explicitly set UTF-8 encoding
   - Added `stream.setGenerateByteOrderMark(false)` to disable BOM generation
   - Files are opened with `QIODevice::WriteOnly` or `QIODevice::Append` **without** `QIODevice::Text` flag
   - This prevents QTextStream from defaulting to UTF-16LE on Windows

2. **Binary Mode for stdout/stderr (CrashLog::install function):**
   - On Windows, added `_setmode(_fileno(stderr), _O_BINARY)` at startup
   - On Windows, added `_setmode(_fileno(stdout), _O_BINARY)` at startup
   - This prevents text mode conversions that could corrupt output throughout the application lifetime

3. **Async-Signal-Safe File Operations (writeSafeCrashLog and safeWrite functions):**
   - Low-level file operations use `_open()` with `_O_BINARY` flag on Windows
   - Added `_setmode(fd, _O_BINARY)` in `safeWrite()` function as defensive measure
   - Uses `_write()` directly instead of higher-level functions to avoid encoding conversions
   - On Unix/Linux, uses `open()` and `write()` which don't have text mode issues

**Technical Details:**

The fix addresses the root cause on multiple levels:

- **QTextStream Level:** Without explicit encoding, QTextStream on Windows defaults to UTF-16LE (little endian) with a byte order mark (BOM: 0xFF 0xFE). This causes ASCII text like "=== CRASH LOG ===" to be written as `3D 00 3D 00 3D 00...` instead of `3D 3D 3D...`, which appears garbled when read as UTF-8.

- **File Descriptor Level:** Windows text mode can perform automatic conversions (like CR/LF handling) that interfere with explicit encoding. Setting binary mode ensures raw bytes are written exactly as specified.

- **Signal Handler Level:** The async-signal-safe crash logging (used during actual crashes) bypasses QTextStream entirely and uses low-level system calls with binary mode to ensure no encoding corruption occurs even in a severely damaged application state.

**Why this method works:**
- Explicitly setting UTF-8 encoding overrides the platform default
- Disabling BOM ensures the file starts with actual content, not encoding markers
- Binary mode prevents any intermediate text mode conversions
- Using QIODevice flags without Text mode avoids conflicts with QTextStream's explicit encoding
- Low-level system calls in signal handlers guarantee no Qt-level encoding issues

## Summary of Working Solution

The crash log encoding issue has been resolved through a comprehensive multi-layered approach:

**Primary Fix:**
- All QTextStream instances now explicitly use UTF-8 encoding via `setEncoding(QStringConverter::Utf8)`
- BOM generation is explicitly disabled via `setGenerateByteOrderMark(false)`
- Files are opened without the `QIODevice::Text` flag when using QTextStream with explicit encoding

**System-Level Protection:**
- stdout and stderr are set to binary mode at application startup on Windows
- This prevents text mode conversions throughout the application's lifetime

**Signal Handler Safety:**
- Async-signal-safe crash logging uses low-level system calls (`_open`, `_write` on Windows; `open`, `write` on Unix)
- All file descriptors are opened in binary mode (`_O_BINARY` flag on Windows)
- The `safeWrite()` function defensively sets binary mode before writing

This layered approach ensures crash logs are always written in readable UTF-8 encoding, whether generated from normal application code paths (using Qt functions) or from signal handlers during actual crashes (using async-signal-safe system calls).

## Testing

The fix is validated by tests in `tests/test_crashlog.cpp`:

All tests pass, confirming the encoding issues are resolved.

All attempted methods are preserved in this document per project requirements, with incorrect methods clearly marked.
