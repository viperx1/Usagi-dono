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
  - Trace/breakpoint traps (SIGTRAP) on Unix-like systems
  - Windows-specific exceptions (access violations, divide by zero, etc.)

- **Detailed Crash Information**: Each crash log includes:
  - Crash reason (signal or exception type)
  - Timestamp (when the crash occurred)
  - Application name and version
  - Qt version
  - Operating system details (name, kernel type/version, product type/version)
  - Hardware information:
    - CPU architecture
    - Number of CPU cores
    - Total and available physical memory (in MB)
  - Display information (screen resolution and DPI for each monitor)
  - Stack trace with function names and offsets (on all platforms)

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

### Debug Symbols

To ensure that crash logs show function names from the Usagi codebase (not just Qt library functions), debug symbols must be enabled during compilation. The CMakeLists.txt files are configured to automatically include debug symbols:

- **MSVC**: Uses `/Zi` flag for debug info and `/DEBUG` linker flag
- **GCC/Clang**: Uses `-g` flag for debug symbols

These flags are included in both Debug and Release builds, ensuring that crash logs always contain meaningful function names and offsets. Without debug symbols, the crash log would only show memory addresses and Qt library function names, making it difficult to identify the source of crashes in the Usagi application code.

On Windows, the crash handler is configured to use `SymSetOptions` with `SYMOPT_UNDNAME`, `SYMOPT_DEFERRED_LOADS`, `SYMOPT_LOAD_LINES`, `SYMOPT_FAIL_CRITICAL_ERRORS`, and `SYMOPT_NO_PROMPTS` flags before initializing the symbol handler. These additional flags prevent error dialogs during crash handling and disable interactive prompts. The symbol handler is initialized with a comprehensive search path that includes both the executable directory and the current working directory, ensuring PDB files are found regardless of where the application is run from. This configuration ensures that the Windows Debug Help Library (DbgHelp) properly loads symbols from PDB files, enabling function names from the Usagi codebase to appear in crash logs alongside Qt library function names.

### Crypto++ Library Compatibility

The application uses the Crypto++ encryption library which has a debug assertion system (`CRYPTOPP_ASSERT`). By default, when debug symbols are enabled, Crypto++ automatically enables `CRYPTOPP_DEBUG`, which causes `CRYPTOPP_ASSERT` to raise `SIGTRAP` on Unix-like systems or call `DebugBreak()` on Windows. This can interfere with our crash log handler.

To prevent this interference while still maintaining debug symbols for our own crash logging:

- `CRYPTOPP_DEBUG` is explicitly disabled by defining `CRYPTOPP_DEBUG=0` in CMakeLists.txt
- A `SIGTRAP` handler is installed on Unix-like systems to catch any trap signals that may still occur
- This ensures that our crash log handler takes precedence and can properly capture stack traces

This configuration allows us to have debug symbols for meaningful crash logs without triggering Crypto++'s assertion system that could mask the real cause of crashes.

### Async-Signal-Safety

The signal handlers (`signalHandler` and `windowsExceptionHandler`) are implemented using only **async-signal-safe** functions to prevent secondary crashes when handling the original crash. This means:

- No Qt functions (QString, QFile, etc.) are called from within signal handlers
- Only low-level system calls (write, open, close on Unix; _write, _open, _close on Windows) are used
- Stack traces use `backtrace()` and `backtrace_symbols_fd()` on Unix (both are async-signal-safe per POSIX)
- Stack traces use `CaptureStackBackTrace()` and `SymFromAddr()` on Windows (safe to call from exception handlers)
- No dynamic memory allocation occurs in the signal handlers (except what backtrace_symbols_fd uses internally, which is safe)
- String operations use only static, pre-allocated buffers
- On Windows, `_write` is used directly (instead of `WriteFile`) to avoid text encoding conversions that could produce garbled output

This design ensures that even if the application is in a severely corrupted state when a crash occurs, the crash handler can still safely write diagnostic information including stack traces to help identify the problem.

## Platform Support

The crash handler supports:
- **Windows**: Catches Windows-specific exceptions using SetUnhandledExceptionFilter. Stack traces show function names with offsets and addresses.
- **Linux/Unix**: Catches POSIX signals including SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, SIGTRAP. Stack traces show symbol names and addresses.
- **macOS**: Catches POSIX signals including SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, SIGTRAP. Stack traces show symbol names and addresses.

## Usage

The crash handler is automatically installed when the application starts. No additional configuration is required. In case of a crash:

1. The crash handler catches the signal/exception
2. A crash message is immediately printed to stderr
3. A crash log with stack trace is written to `crash.log` in the current directory
4. The application terminates

### Example Crash Log

On Unix/Linux/macOS systems, the crash log will look like:

```
=== CRASH LOG ===

Crash Reason: Segmentation Fault (SIGSEGV)

Application: Usagi-dono
Version: 1.0.0
Timestamp: 2025-01-15 14:30:22

Qt Version: 6.4.2
OS: Ubuntu 24.04 LTS
Kernel Type: linux
Kernel Version: 6.8.0-49-generic
Product Type: ubuntu
Product Version: 24.04
CPU Architecture: x86_64
Build CPU Architecture: x86_64
CPU Cores: 8
Total Physical Memory: 16384 MB

Display Information:
  Screen 1: 1920x1080 @ 96.0 DPI

Stack Trace:
./usagi(+0x1234)[0x5555555551234]
./usagi(+0x5678)[0x5555555555678]
/lib/x86_64-linux-gnu/libc.so.6(+0x45330)[0x7f1234545330]
...

=== END OF CRASH LOG ===
```

On Windows systems, the crash log will show function names with offsets:

```
=== CRASH LOG ===

Crash Reason: Access Violation

Application: Usagi-dono
Version: 1.0.0
Timestamp: 2025-01-15 14:30:22

Qt Version: 6.4.2
OS: Windows 11 (10.0)
Kernel Type: winnt
Kernel Version: 10.0.22621
Product Type: windows
Product Version: 11
CPU Architecture: x86_64
Build CPU Architecture: x86_64
CPU Cores: 12
Total Physical Memory: 32768 MB
Available Physical Memory: 16384 MB

Display Information:
  Screen 1: 2560x1440 @ 96.0 DPI
  Screen 2: 1920x1080 @ 96.0 DPI

Stack Trace:
  [0] MainWindow::onButtonClick + 0x000000000000001a
  [1] QWidget::event + 0x0000000000000123
  [2] QApplicationPrivate::notify_helper + 0x000000000000004f
...

=== END OF CRASH LOG ===
```

## Future Enhancements

Possible future improvements:
- Include more system information (memory usage, CPU info)
- Write crash logs to timestamped files in the application data directory
- Add a crash report dialog allowing users to submit crash reports
- Compress old crash logs automatically
- Add crash log viewer in the application UI
- Use core dump analysis tools for detailed diagnostics
