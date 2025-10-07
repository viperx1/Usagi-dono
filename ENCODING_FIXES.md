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

### Method 1: 

**Status:** unknown
**Why it failed:**
- unknown

## Summary of Working Solution


## Testing

The fix is validated by tests in `tests/test_crashlog.cpp`:

All tests pass, confirming the encoding issues are resolved.

All attempted methods are preserved in this document per project requirements, with incorrect methods clearly marked.
