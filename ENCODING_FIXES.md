# Crash Log Encoding Fix Methods

This document tracks all methods attempted to fix the crash log encoding issue where logs were saved with incorrect encoding (UTF-16LE instead of UTF-8), resulting in garbled output.

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

### ❌ Method 1: Default QTextStream (INCORRECT)
**Status:** INCORRECT - Causes UTF-16LE encoding on Windows

**Code:**
```cpp
QFile logFile(logPath);
if (logFile.open(QIODevice::WriteOnly | QIODevice::Text))
{
    QTextStream stream(&logFile);
    stream << "=== CRASH LOG ===\n";
    // ...
}
```

**Why it failed:**
- No explicit encoding set, QTextStream defaults to UTF-16LE on Windows
- `QIODevice::Text` flag can interfere with encoding behavior
- Results in garbled output when read as UTF-8

### ❌ Method 2: Using QTextStream with Text flag and UTF-8 (INCORRECT)
**Status:** INCORRECT - Text flag conflicts with explicit encoding

**Code:**
```cpp
QFile logFile(logPath);
if (logFile.open(QIODevice::WriteOnly | QIODevice::Text))
{
    QTextStream stream(&logFile);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "=== CRASH LOG ===\n";
    // ...
}
```

**Why it failed:**
- The `QIODevice::Text` flag conflicts with explicit encoding settings
- Can still result in encoding issues on some platforms
- QTextStream's encoding behavior is affected by the Text flag

### ❌ Method 3: fprintf without binary mode stderr (INCORRECT)
**Status:** INCORRECT - Text mode conversions can corrupt output

**Code:**
```cpp
fprintf(stderr, "Crash log saved to: %s\n", logPath.toUtf8().constData());
```

**Why it failed:**
- On Windows, stderr in text mode can perform LF to CRLF conversions
- Can cause issues with binary data or special characters
- Not reliable for all character encodings

### ❌ Method 4: Using _write without _O_BINARY flag (INCORRECT)
**Status:** INCORRECT - Missing binary mode flag

**Code:**
```cpp
#ifdef Q_OS_WIN
int fd = _open(logPath, _O_WRONLY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
_write(fd, data, len);
#endif
```

**Why it failed:**
- Without `_O_BINARY` flag, file descriptor operates in text mode
- Text mode can perform unwanted conversions (LF to CRLF, etc.)
- Can corrupt binary data or non-ASCII characters

### ✅ Method 5: QTextStream with explicit UTF-8, no Text flag, no BOM (CORRECT)
**Status:** CORRECT - Primary fix for QTextStream-based logging

**Code:**
```cpp
QFile logFile(logPath);
if (logFile.open(QIODevice::WriteOnly))  // No Text flag!
{
    QTextStream stream(&logFile);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(false);
    stream << "=== CRASH LOG ===\n";
    // ...
}
```

**Why it works:**
- Explicit UTF-8 encoding overrides Windows default
- No `QIODevice::Text` flag avoids conflicts
- `setGenerateByteOrderMark(false)` ensures clean UTF-8 without BOM
- Used in `generateCrashLog()` and `logMessage()` functions

**Implementation locations:**
- `usagi/src/crashlog.cpp`: `generateCrashLog()` function (lines 888-915)
- `usagi/src/crashlog.cpp`: `logMessage()` function (lines 641-663)

### ✅ Method 6: Set stderr/stdout to binary mode early (CORRECT)
**Status:** CORRECT - Prevents text mode conversions throughout application lifetime

**Code:**
```cpp
void CrashLog::install()
{
#ifdef Q_OS_WIN
    // Set stderr and stdout to binary mode immediately
    _setmode(_fileno(stderr), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    // ... rest of installation
}
```

**Why it works:**
- Sets binary mode on stderr/stdout once at startup
- Prevents all text mode conversions (LF to CRLF, etc.)
- Ensures consistent output behavior throughout application lifetime
- Safe to call early, before any output occurs

**Implementation location:**
- `usagi/src/crashlog.cpp`: `CrashLog::install()` function (lines 595-626)

### ✅ Method 7: Use _write with _O_BINARY flag for signal handlers (CORRECT)
**Status:** CORRECT - Async-signal-safe logging with correct encoding

**Code:**
```cpp
#ifdef Q_OS_WIN
int fd = _open(logPath, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
if (fd >= 0)
{
    _write(fd, "=== CRASH LOG ===\n", 18);
    // ... more writes
    _close(fd);
}
#endif
```

**Why it works:**
- `_O_BINARY` flag ensures no text mode conversions
- Direct byte-by-byte writing preserves UTF-8 encoding
- Async-signal-safe (safe to use in signal handlers)
- No Qt functions involved, reducing crash risk

**Implementation location:**
- `usagi/src/crashlog.cpp`: `writeSafeCrashLog()` function (lines 317-388)

### ✅ Method 8: Defensive binary mode in safeWrite (CORRECT)
**Status:** CORRECT - Extra safety layer for all file descriptor writes

**Code:**
```cpp
static void safeWrite(int fd, const char* str)
{
#ifdef Q_OS_WIN
    // Ensure binary mode to prevent text conversions
    _setmode(fd, _O_BINARY);
    _write(fd, str, (unsigned int)strlen(str));
#else
    write(fd, str, strlen(str));
#endif
}
```

**Why it works:**
- Defensively sets binary mode on every write
- Safe to call multiple times (mode change is idempotent)
- Ensures binary mode even if caller forgot to set it
- Provides extra protection against text mode issues

**Implementation location:**
- `usagi/src/crashlog.cpp`: `safeWrite()` function (lines 34-52)

## Summary of Working Solution

The complete fix involves multiple layers:

1. **QTextStream-based logging** (generateCrashLog, logMessage):
   - Open files WITHOUT `QIODevice::Text` flag
   - Explicitly set UTF-8 encoding with `setEncoding(QStringConverter::Utf8)`
   - Disable BOM with `setGenerateByteOrderMark(false)`

2. **Early binary mode setup**:
   - Set stderr/stdout to binary mode in `CrashLog::install()` at application startup
   - Prevents text mode conversions throughout application lifetime

3. **Signal handler logging**:
   - Open files with `_O_BINARY` flag on Windows
   - Use `_write()` directly for async-signal-safe operation
   - No Qt functions to avoid secondary crashes

4. **Defensive programming**:
   - `safeWrite()` function sets binary mode on every write as extra protection
   - Multiple layers ensure encoding correctness even if one layer fails

## Testing

The fix is validated by tests in `tests/test_crashlog.cpp`:

1. **testCrashLogEncoding()** - Verifies UTF-8/ASCII encoding is used
2. **testCrashLogNotUTF16LE()** - Verifies UTF-16LE is NOT used
3. **testDetectIncorrectUTF16LEEncoding()** - Documents UTF-16LE problem characteristics
4. **testQTextStreamUTF8WithoutTextFlag()** - Validates the QTextStream fix approach

All tests pass, confirming the encoding issues are resolved.

## Historical Context

This issue was discovered when crash logs appeared garbled with Chinese/Unicode characters. Investigation revealed:
- Windows QTextStream default encoding is UTF-16LE
- Text mode file operations can corrupt output
- Multiple fix layers needed for complete solution

All attempted methods are preserved in this document per project requirements, with incorrect methods clearly marked.
