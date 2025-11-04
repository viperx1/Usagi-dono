# Unified Logging System

## Overview

The Usagi-dono application uses a unified logging system implemented in `logger.h` and `logger.cpp`. This system provides a single, consistent way to log messages throughout the codebase, replacing the previous fragmented approach with multiple logging functions (Debug, logOutput, etc.).

## Features

The unified Logger provides:

1. **Console Output**: Messages are output to the console using `qDebug()` for development and debugging
2. **UI Log Tab**: Messages are displayed in the application's Log tab through Qt signal/slot mechanism
3. **File/Line Context**: MANDATORY file and line number information for accurate debugging

**Note**: CrashLog is intentionally kept separate and is only used for emergency crash situations, not for regular logging.

## Usage

### Recommended: Use the LOG Macro (ALWAYS PREFER THIS)

The LOG macro is the **recommended and preferred way** to log messages. It automatically provides the required file and line information:

```cpp
#include "logger.h"

// Simple log message - RECOMMENDED
LOG("Your message here");

// Log with variable formatting - RECOMMENDED
LOG(QString("User %1 logged in with status %2").arg(username).arg(status));
```

### Direct Logger::log Call (Advanced - Use Only When Necessary)

The `Logger::log()` function requires **MANDATORY** file and line parameters. These parameters are enforced by assertions:

```cpp
// Direct call with explicit file and line - ONLY use when you cannot use LOG macro
Logger::log("Error occurred", __FILE__, __LINE__);
```

**CRITICAL**: Do NOT call `Logger::log()` with empty strings or zero for file/line parameters:

```cpp
// ❌ WRONG - Will trigger assertion failure at runtime
Logger::log("Message", "", 0);

// ✅ CORRECT - Use the LOG macro instead
LOG("Message");
```

### In Class Methods

```cpp
void MyClass::someMethod()
{
    // Always use the LOG macro - RECOMMENDED
    LOG("MyClass::someMethod called");
    
    // For processing messages
    LOG("Processing data");
}
```

## IMPORTANT: Mandatory File and Line Parameters

The `file` and `line` parameters in `Logger::log()` are **MANDATORY** and are enforced by assertions:

- `file` must not be empty
- `line` must be greater than 0

**If the application hits these assertions at runtime**, it means `Logger::log()` is being called **INCORRECTLY** and needs to be fixed. The assertions are intentionally there to catch incorrect usage during development.

**Always use the LOG(msg) macro** instead of calling `Logger::log()` directly. The macro automatically provides `__FILE__` and `__LINE__`.

## Implementation Details

### Architecture

The Logger is implemented as a singleton with the following components:

- **Logger::instance()**: Returns the singleton instance
- **Logger::log()**: Static method that logs a message
- **logMessage signal**: Emitted when a message is logged, connected to the Window's log tab

### Message Flow

1. Call `Logger::log(msg)` from anywhere in the code
2. Logger outputs to console via `qDebug()`
3. Logger emits `logMessage` signal
4. Window receives signal and updates the Log tab UI with timestamp

### Integration

The Logger is automatically integrated with:

- **Window class**: Connected to receive log messages and display in UI
- **All source files**: Every major source file includes at least one test log call

**CrashLog Separation**: CrashLog is intentionally kept separate from the unified logging system. It is only used for emergency crash situations and diagnostic information, not for regular application logging.

## Migration from Old System

The old logging functions have been replaced:

- `Debug(msg)` → `LOG(msg)` (preferred)
- `logOutput->append(msg)` → `LOG(msg)` (preferred)
- Direct `qDebug()` → Can still be used, but `LOG()` is preferred

All three old Debug() implementations (myAniDBApi, AniDBApi, ed2k) now use the unified Logger internally.

**IMPORTANT**: Never call `Logger::log(msg, "", 0)` as this will trigger assertion failures. Always use the `LOG(msg)` macro instead.

## Testing

The logging system includes comprehensive tests in `tests/test_logger.cpp`:

- Singleton pattern verification
- Signal emission tests
- File/line information handling
- LOG macro functionality
- Multiple consecutive log calls

Run tests with:
```bash
./test_logger -v2
```

## Files

- `usagi/src/logger.h` - Header file with Logger class and LOG macro
- `usagi/src/logger.cpp` - Implementation of the Logger
- `tests/test_logger.cpp` - Test suite for the Logger
