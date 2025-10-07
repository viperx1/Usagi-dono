# Crash Log Functionality

## Overview

The crash log functionality provides automatic crash detection and logging for the Usagi-dono application. When the application crashes due to an unhandled exception or signal, a detailed crash log is generated and saved to disk.

## Features

- **Automatic Crash Detection**: Catches various types of crashes including:
  - Segmentation faults (SIGSEGV)
  - Abort signals (SIGABRT)
  - Floating point exceptions (SIGFPE)
  - Illegal instructions (SIGILL)
  - Bus errors (SIGBUS) on Unix-like systems
  - Windows-specific exceptions (access violations, divide by zero, etc.)

- **Detailed Crash Information**: Each crash log includes:
  - Crash reason and timestamp
  - Application name and version
  - Qt version
  - Operating system information
  - Stack trace (where available)

- **Persistent Logging**: Application logs are also written to a persistent file (`usagi.log`) for debugging purposes.

## Crash Log Location

When a crash occurs, the crash handler writes logs in two locations:

1. **Immediate crash log**: A simple `crash.log` file is written to the current working directory using only async-signal-safe functions. This ensures the crash can be logged even in the most severe failure scenarios.

2. **Detailed application logs**: Regular application logs (non-crash) are saved to `usagi.log` in the application's data directory:
   - **Windows**: `%APPDATA%/Usagi-dono/usagi.log`
   - **Linux**: `~/.local/share/Usagi-dono/usagi.log`
   - **macOS**: `~/Library/Application Support/Usagi-dono/usagi.log`

## Implementation Details

The crash handler is implemented in:
- `usagi/src/crashlog.h` - Header file with interface
- `usagi/src/crashlog.cpp` - Implementation with signal/exception handlers
- `usagi/src/main.cpp` - Integration into application startup

The handler is installed at application startup before the main window is shown, ensuring that crashes are caught throughout the application lifetime.

### Async-Signal-Safety

The signal handlers (`signalHandler` and `windowsExceptionHandler`) are implemented using only **async-signal-safe** functions to prevent secondary crashes when handling the original crash. This means:

- No Qt functions (QString, QFile, etc.) are called from within signal handlers
- Only low-level system calls (write, open, close on Unix; WriteFile, CreateFile on Windows) are used
- No dynamic memory allocation occurs in the signal handlers
- String operations use only static, pre-allocated buffers

This design ensures that even if the application is in a severely corrupted state when a crash occurs, the crash handler can still safely write diagnostic information to help identify the problem.

## Platform Support

The crash handler supports:
- **Windows**: Full support with detailed exception information and stack traces via DbgHelp API
- **Linux/Unix**: Full support with stack traces via backtrace API
- **macOS**: Full support with stack traces via backtrace API

## Usage

The crash handler is automatically installed when the application starts. No additional configuration is required. In case of a crash:

1. The crash handler catches the signal/exception
2. A detailed crash log is written to disk
3. A message is printed to stderr with the crash log location
4. The application terminates gracefully

## Future Enhancements

Possible future improvements:
- Add a crash report dialog allowing users to submit crash reports
- Include more system information (memory usage, CPU info, etc.)
- Compress old crash logs automatically
- Add crash log viewer in the application UI
