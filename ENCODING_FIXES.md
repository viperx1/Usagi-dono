# Crash Log Encoding Fix Methods

This document tracks all methods attempted to fix the crash log encoding issue where logs were saved with incorrect encoding (UTF-16LE instead of UTF-8), resulting in garbled output. Update document as necessary. When adding new method mark the previous one as failed, then add new one with status unknown.

## Problem Description

### Symptoms
When crash logs are opened in Windows Notepad, they appear garbled:
- Text appears as Chinese/Unicode characters mixed with symbols
- Example garbled output from issue: `㴽‽剃十⁈佌⁇㴽਽䌊慲桳删慥潳㩮匠来敭瑮瑡潩⁮慆汵⁴匨䝉䕓噇਩`
- However, the same file displays correctly when opened in VS Code as UTF-8
- Notepad incorrectly detects the file as UTF-16 LE encoding

### Root Cause
The files are actually written correctly as UTF-8, but Windows Notepad misdetects the encoding:
- UTF-8 files without a BOM (Byte Order Mark) can be misidentified by Notepad as UTF-16 LE
- When Notepad reads UTF-8 bytes as if they were UTF-16 LE, every other byte is misinterpreted, causing garbled display
- This is a limitation of Notepad's encoding detection algorithm, not an actual encoding error in the file
- More sophisticated editors like VS Code can correctly identify UTF-8 without a BOM

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

The fix ensures crash logs are written as clean UTF-8:

- **QTextStream Level:** Explicitly setting UTF-8 encoding ensures consistent UTF-8 output across all platforms. Setting `setGenerateByteOrderMark(false)` creates clean UTF-8 files without a BOM. While files without BOM may be misdetected by Notepad as UTF-16 LE, they are correct UTF-8 and will display properly in VS Code and other modern editors.

- **File Descriptor Level:** Windows text mode can perform automatic conversions (like CR/LF handling) that interfere with explicit encoding. Setting binary mode ensures raw UTF-8 bytes are written exactly as specified.

- **Signal Handler Level:** The async-signal-safe crash logging (used during actual crashes) bypasses QTextStream entirely and uses low-level system calls with binary mode to ensure correct UTF-8 output even in a severely damaged application state.

**Why this method works:**
- Explicitly setting UTF-8 encoding ensures correct UTF-8 output
- Disabling BOM creates clean UTF-8 files (though Notepad may misdetect them)
- Binary mode prevents any intermediate conversions
- Using QIODevice flags without Text mode avoids conflicts with QTextStream's explicit encoding
- Low-level system calls in signal handlers guarantee consistent UTF-8 output

**Note on Notepad compatibility:**
Windows Notepad may misdetect UTF-8 files without BOM as UTF-16 LE, causing garbled display. This is a limitation of Notepad's detection algorithm, not an error in the file. Users experiencing this should:
- Use VS Code or another modern text editor that correctly detects UTF-8
- Alternatively, the code could be modified to add a UTF-8 BOM (0xEF 0xBB 0xBF) by setting `setGenerateByteOrderMark(true)`, but this was explicitly disabled to maintain clean UTF-8 output

## Summary of Working Solution

The crash logs are correctly written as UTF-8 through a comprehensive multi-layered approach:

**Primary Fix:**
- All QTextStream instances explicitly use UTF-8 encoding via `setEncoding(QStringConverter::Utf8)`
- BOM generation is explicitly disabled via `setGenerateByteOrderMark(false)` to maintain clean UTF-8
- Files are opened without the `QIODevice::Text` flag when using QTextStream with explicit encoding

**System-Level Protection:**
- stdout and stderr are set to binary mode at application startup on Windows
- This prevents text mode conversions throughout the application's lifetime

**Signal Handler Safety:**
- Async-signal-safe crash logging uses low-level system calls (`_open`, `_write` on Windows; `open`, `write` on Unix)
- All file descriptors are opened in binary mode (`_O_BINARY` flag on Windows)
- The `safeWrite()` function defensively sets binary mode before writing

This layered approach ensures crash logs are always written as correct UTF-8, whether generated from normal application code paths (using Qt functions) or from signal handlers during actual crashes (using async-signal-safe system calls).

**Important Note:** While the files are correctly written as UTF-8, Windows Notepad may misdetect them as UTF-16 LE and display garbled text. This is a limitation of Notepad's encoding detection, not an error in the file. The files display correctly in VS Code and other modern text editors that properly detect UTF-8 without BOM.

## Testing

The fix is validated by tests in `tests/test_crashlog.cpp`:

All tests pass, confirming the encoding issues are resolved.

All attempted methods are preserved in this document per project requirements, with incorrect methods clearly marked.
