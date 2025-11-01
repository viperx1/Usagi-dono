# Unified Logging System

## Overview

The Usagi-dono application uses a unified logging system implemented in `logger.h` and `logger.cpp`. This system provides a single, consistent way to log messages throughout the codebase, replacing the previous fragmented approach with multiple logging functions (Debug, logOutput, etc.).

## Features

The unified Logger provides:

1. **Console Output**: Messages are output to the console using `qDebug()` for development and debugging
2. **UI Log Tab**: Messages are displayed in the application's Log tab through Qt signal/slot mechanism
3. **File/Line Context**: Optional file and line number information for better debugging

**Note**: CrashLog is intentionally kept separate and is only used for emergency crash situations, not for regular logging.

## Usage

### Basic Logging

```cpp
#include "logger.h"

// Simple log message
Logger::log("Your message here");

// Log with variable formatting
Logger::log(QString("User %1 logged in with status %2").arg(username).arg(status));
```

### Logging with File and Line Information

```cpp
// Explicit file and line
Logger::log("Error occurred", __FILE__, __LINE__);

// Or use the convenient LOG macro
LOG("This automatically includes file and line info");
```

### In Class Methods

```cpp
void MyClass::someMethod()
{
    Logger::log("MyClass::someMethod called");
    
    // With context
    LOG("Processing data");
}
```

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

- `Debug(msg)` → `Logger::log(msg)`
- `logOutput->append(msg)` → `Logger::log(msg)`
- Direct `qDebug()` → Can still be used, but `Logger::log()` is preferred

All three old Debug() implementations (myAniDBApi, AniDBApi, ed2k) now use the unified Logger internally.

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
