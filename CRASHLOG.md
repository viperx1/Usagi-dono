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
  - Crash reason (signal or exception type)
  - Application name and version
  - Note: Stack traces, Qt version, and timestamps are not included in the immediate crash log to maintain async-signal-safety, but may be added through future enhancements using separate processes or core dump analysis

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
- **Windows**: Catches Windows-specific exceptions using SetUnhandledExceptionFilter
- **Linux/Unix**: Catches POSIX signals including SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS
- **macOS**: Catches POSIX signals including SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS

Note: Stack traces via DbgHelp (Windows) or backtrace (Unix) are available in the codebase but not currently used in the async-signal-safe handlers to prevent secondary crashes.

## Usage

The crash handler is automatically installed when the application starts. No additional configuration is required. In case of a crash:

1. The crash handler catches the signal/exception
2. A crash message is immediately printed to stderr
3. A simple crash log is written to `crash.log` in the current directory
4. The application terminates

## Future Enhancements

Possible future improvements:
- Add stack traces using a separate process or post-crash analysis tool
- Include more system information (memory usage, CPU info, timestamps)
- Write crash logs to timestamped files in the application data directory
- Add a crash report dialog allowing users to submit crash reports
- Compress old crash logs automatically
- Add crash log viewer in the application UI
- Use core dump analysis tools for detailed diagnostics
