#include "crashlog.h"
#include <QDir>
#include <QStandardPaths>
#include <QSysInfo>
#include <QScreen>
#include <QGuiApplication>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <execinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

// Pre-formatted system information buffers for async-signal-safe crash logging
// These are populated at startup by initSystemInfoBuffers() and used by writeSafeCrashLog()
static char g_systemInfoBuffer[4096] = {0};
static bool g_systemInfoInitialized = false;

// Safe string write function for signal handlers
static void safeWrite(int fd, const char* str)
{
    if (str)
    {
        size_t len = strlen(str);
#ifdef Q_OS_WIN
        // On Windows, ensure the file descriptor is in binary mode to prevent
        // text mode conversions (which could cause UTF-16LE encoding issues)
        // This is safe to call multiple times - it will only change the mode once
        _setmode(fd, _O_BINARY);
        
        // Use _write directly to avoid encoding issues with _get_osfhandle + WriteFile
        // This ensures single-byte ASCII/UTF-8 output without UTF-16LE conversion
        _write(fd, str, (unsigned int)len);
#else
        write(fd, str, len);
#endif
    }
}

// Safe function to convert a pointer address to hex string
// Uses only async-signal-safe operations
static void pointerToHex(void* ptr, char* buffer, size_t bufSize)
{
    if (!buffer || bufSize < 19) return; // Need at least "0x" + 16 hex digits + null
    
    buffer[0] = '0';
    buffer[1] = 'x';
    
    uintptr_t addr = (uintptr_t)ptr;
    const char hexDigits[] = "0123456789abcdef";
    
    // Write hex digits from most significant to least significant
    for (int i = 15; i >= 0; i--)
    {
        buffer[2 + (15 - i)] = hexDigits[(addr >> (i * 4)) & 0xF];
    }
    buffer[18] = '\0';
}

// Safe function to format timestamp using async-signal-safe operations
// Format: YYYY-MM-DD HH:MM:SS (in local time)
static void formatTimestamp(char* buffer, size_t bufSize)
{
    if (!buffer || bufSize < 20) return; // Need "YYYY-MM-DD HH:MM:SS\0"
    
    time_t now = time(NULL);
    struct tm* tm_info;
    
#ifdef Q_OS_WIN
    // On Windows, localtime is not thread-safe but there's no async-signal-safe alternative
    // We use it anyway as it's the best option available
    // Using localtime to show local time instead of UTC
    tm_info = localtime(&now);
#else
    // On Unix/Linux, localtime_r is async-signal-safe
    struct tm tm_storage;
    tm_info = localtime_r(&now, &tm_storage);
#endif
    
    if (!tm_info) {
        // Fallback: write "Unknown" if time functions fail
        const char* fallback = "Unknown";
        size_t len = 7; // strlen("Unknown")
        for (size_t i = 0; i < len && i < bufSize - 1; i++) {
            buffer[i] = fallback[i];
        }
        buffer[len] = '\0';
        return;
    }
    
    // Format: YYYY-MM-DD HH:MM:SS
    // Year
    int year = 1900 + tm_info->tm_year;
    buffer[0] = '0' + (year / 1000);
    buffer[1] = '0' + ((year / 100) % 10);
    buffer[2] = '0' + ((year / 10) % 10);
    buffer[3] = '0' + (year % 10);
    buffer[4] = '-';
    
    // Month
    int month = tm_info->tm_mon + 1;
    buffer[5] = '0' + (month / 10);
    buffer[6] = '0' + (month % 10);
    buffer[7] = '-';
    
    // Day
    int day = tm_info->tm_mday;
    buffer[8] = '0' + (day / 10);
    buffer[9] = '0' + (day % 10);
    buffer[10] = ' ';
    
    // Hour
    int hour = tm_info->tm_hour;
    buffer[11] = '0' + (hour / 10);
    buffer[12] = '0' + (hour % 10);
    buffer[13] = ':';
    
    // Minute
    int min = tm_info->tm_min;
    buffer[14] = '0' + (min / 10);
    buffer[15] = '0' + (min % 10);
    buffer[16] = ':';
    
    // Second
    int sec = tm_info->tm_sec;
    buffer[17] = '0' + (sec / 10);
    buffer[18] = '0' + (sec % 10);
    buffer[19] = '\0';
}

// Safe function to write stack trace using async-signal-safe operations
static void writeSafeStackTrace(int fd)
{
#ifdef Q_OS_WIN
    // Windows: Use CaptureStackBackTrace which is safe to call from exception handlers
    const int maxFrames = 64;
    void* stack[maxFrames];
    WORD frames = CaptureStackBackTrace(0, maxFrames, stack, NULL);
    
    safeWrite(fd, "\nStack Trace:\n");
    
    // Initialize symbol handler for this process
    HANDLE process = GetCurrentProcess();
    
    // Configure symbol options for better symbol resolution
    // SYMOPT_UNDNAME: Undecorate (demangle) symbol names
    // SYMOPT_DEFERRED_LOADS: Load symbols only when needed
    // SYMOPT_LOAD_LINES: Load line number information
    // SYMOPT_FAIL_CRITICAL_ERRORS: Don't display system error message boxes
    // SYMOPT_NO_PROMPTS: Don't prompt for symbol search paths
    // SYMOPT_INCLUDE_32BIT_MODULES: Include 32-bit modules in 64-bit process enumeration
    // SYMOPT_AUTO_PUBLICS: Automatically search public symbols if private symbols not found
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | 
                  SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS |
                  SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_AUTO_PUBLICS);
    
    // Build comprehensive symbol search path
    // Include: executable directory, current directory
    char exePath[MAX_PATH];
    char currentDir[MAX_PATH];
    char searchPath[MAX_PATH * 4];
    searchPath[0] = '\0'; // Initialize to empty string
    
    // Get the executable directory
    DWORD pathLen = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (pathLen > 0 && pathLen < MAX_PATH)
    {
        // Find the last backslash to get directory
        char* lastSlash = NULL;
        for (DWORD i = 0; i < pathLen; i++)
        {
            if (exePath[i] == '\\' || exePath[i] == '/')
            {
                lastSlash = &exePath[i];
            }
        }
        if (lastSlash)
        {
            *lastSlash = '\0'; // Null-terminate at the last slash to get directory
            // Copy executable directory to searchPath
            size_t len = strlen(exePath);
            if (len < sizeof(searchPath) - 1)
            {
                memcpy(searchPath, exePath, len);
                searchPath[len] = '\0';
            }
        }
    }
    
    // Add current directory to search path
    DWORD cwdLen = GetCurrentDirectoryA(MAX_PATH, currentDir);
    if (cwdLen > 0 && cwdLen < MAX_PATH)
    {
        size_t currentLen = strlen(searchPath);
        // Add semicolon separator if we already have exe path
        if (currentLen > 0 && currentLen < sizeof(searchPath) - 2)
        {
            searchPath[currentLen] = ';';
            searchPath[currentLen + 1] = '\0';
            currentLen++;
        }
        // Append current directory
        size_t remainingSpace = sizeof(searchPath) - currentLen - 1;
        if (cwdLen < remainingSpace)
        {
            memcpy(searchPath + currentLen, currentDir, cwdLen);
            searchPath[currentLen + cwdLen] = '\0';
        }
    }
    
    // Initialize symbol handler with comprehensive search path
    // This ensures PDB files are found in executable directory and current directory
    // TRUE parameter: automatically enumerate and load symbols for all loaded modules
    // This allows resolving symbols from Qt libraries and other DLLs in the stack trace
    BOOL symInitResult = SymInitialize(process, searchPath[0] != '\0' ? searchPath : NULL, TRUE);
    
    // Write symbol search path for debugging
    if (symInitResult)
    {
        safeWrite(fd, "Symbol search path: ");
        safeWrite(fd, searchPath[0] != '\0' ? searchPath : "(default)");
        safeWrite(fd, "\n");
    }
    else
    {
        safeWrite(fd, "Warning: SymInitialize failed\n");
    }
    
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
    
    // Track how many symbols were resolved
    int resolvedSymbols = 0;
    
    for (WORD i = 0; i < frames; i++)
    {
        safeWrite(fd, "  [");
        
        // Convert frame number to string
        char frameNum[16];
        int num = i;
        int pos = 0;
        if (num == 0) {
            frameNum[pos++] = '0';
        } else {
            char temp[16];
            int tempPos = 0;
            while (num > 0) {
                temp[tempPos++] = '0' + (num % 10);
                num /= 10;
            }
            // Reverse the digits
            for (int j = tempPos - 1; j >= 0; j--) {
                frameNum[pos++] = temp[j];
            }
        }
        frameNum[pos] = '\0';
        safeWrite(fd, frameNum);
        safeWrite(fd, "] ");
        
        // Try to resolve symbol name
        DWORD64 address = (DWORD64)(stack[i]);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        
        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol))
        {
            // Write function name
            safeWrite(fd, symbol->Name);
            safeWrite(fd, " + 0x");
            
            // Convert displacement to hex string
            char dispBuf[32];
            pointerToHex((void*)displacement, dispBuf, sizeof(dispBuf));
            // Skip "0x" prefix since we already wrote it
            safeWrite(fd, dispBuf + 2);
            
            resolvedSymbols++;
        }
        else
        {
            // If symbol resolution fails, just write the address
            char addrBuf[32];
            pointerToHex(stack[i], addrBuf, sizeof(addrBuf));
            safeWrite(fd, addrBuf);
        }
        
        safeWrite(fd, "\n");
    }
    
    // Write diagnostic information about symbol resolution
    safeWrite(fd, "\nSymbol resolution: ");
    char resolvedCount[16];
    char totalCount[16];
    int rPos = 0, tPos = 0;
    
    // Convert resolvedSymbols to string
    if (resolvedSymbols == 0) {
        resolvedCount[rPos++] = '0';
    } else {
        char temp[16];
        int tempPos = 0;
        int num = resolvedSymbols;
        while (num > 0) {
            temp[tempPos++] = '0' + (num % 10);
            num /= 10;
        }
        for (int j = tempPos - 1; j >= 0; j--) {
            resolvedCount[rPos++] = temp[j];
        }
    }
    resolvedCount[rPos] = '\0';
    
    // Convert frames to string
    if (frames == 0) {
        totalCount[tPos++] = '0';
    } else {
        char temp[16];
        int tempPos = 0;
        int num = frames;
        while (num > 0) {
            temp[tempPos++] = '0' + (num % 10);
            num /= 10;
        }
        for (int j = tempPos - 1; j >= 0; j--) {
            totalCount[tPos++] = temp[j];
        }
    }
    totalCount[tPos] = '\0';
    
    safeWrite(fd, resolvedCount);
    safeWrite(fd, " of ");
    safeWrite(fd, totalCount);
    safeWrite(fd, " frames resolved\n");
    
    // Add note if few symbols were resolved
    if (resolvedSymbols == 0 && frames > 0)
    {
        safeWrite(fd, "Note: No symbols resolved. This usually means:\n");
        safeWrite(fd, "  - PDB file is missing (MSVC builds)\n");
        safeWrite(fd, "  - Debug symbols were stripped (MinGW/GCC builds)\n");
        safeWrite(fd, "  - Executable was built without debug information\n");
    }
    else if (resolvedSymbols > 0 && resolvedSymbols < frames / 2)
    {
        safeWrite(fd, "Note: Few symbols resolved. Check if PDB file exists alongside executable.\n");
    }
    
    SymCleanup(process);
#else
    // Unix/Linux/macOS: Use backtrace and backtrace_symbols_fd
    // backtrace_symbols_fd is async-signal-safe and writes directly to fd
    const int maxFrames = 64;
    void* buffer[maxFrames];
    int frames = backtrace(buffer, maxFrames);
    
    safeWrite(fd, "\nStack Trace:\n");
    
    // backtrace_symbols_fd writes the symbols directly to the file descriptor
    // This is async-signal-safe according to POSIX
    backtrace_symbols_fd(buffer, frames, fd);
#endif
}

// Safe crash log function that only uses async-signal-safe operations
static void writeSafeCrashLog(const char* reason)
{
    // Get timestamp
    char timestamp[32];
    formatTimestamp(timestamp, sizeof(timestamp));
    
    // Write to stderr first (this is most important)
    safeWrite(2, "\n=== CRASH DETECTED ===\n");
    safeWrite(2, "Timestamp: ");
    safeWrite(2, timestamp);
    safeWrite(2, "\nReason: ");
    safeWrite(2, reason);
    safeWrite(2, "\nApplication: Usagi-dono\nVersion: 1.0.0\n");
    safeWrite(2, "======================\n\n");
    
    // Try to write to a file as well
    // Use a simple fixed filename to avoid complex path operations
#ifdef Q_OS_WIN
    const char* logPath = "crash.log";
    // Use _open with _O_BINARY flag to avoid text encoding conversions
    int fd = _open(logPath, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd >= 0)
    {
        safeWrite(fd, "=== CRASH LOG ===\n\nCrash Reason: ");
        safeWrite(fd, reason);
        safeWrite(fd, "\n\nApplication: Usagi-dono\nVersion: 1.0.0\nTimestamp: ");
        safeWrite(fd, timestamp);
        safeWrite(fd, "\n\n");
        
        // Write pre-formatted system information
        if (g_systemInfoInitialized && g_systemInfoBuffer[0] != '\0')
        {
            safeWrite(fd, g_systemInfoBuffer);
            safeWrite(fd, "\n");
        }
        
        // Add stack trace
        writeSafeStackTrace(fd);
        
        safeWrite(fd, "\n=== END OF CRASH LOG ===\n");
        _close(fd);
        
        safeWrite(2, "Crash log saved to: crash.log\n");
    }
#else
    const char* logPath = "crash.log";
    int fd = open(logPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd >= 0)
    {
        safeWrite(fd, "=== CRASH LOG ===\n\nCrash Reason: ");
        safeWrite(fd, reason);
        safeWrite(fd, "\n\nApplication: Usagi-dono\nVersion: 1.0.0\nTimestamp: ");
        safeWrite(fd, timestamp);
        safeWrite(fd, "\n\n");
        
        // Write pre-formatted system information
        if (g_systemInfoInitialized && g_systemInfoBuffer[0] != '\0')
        {
            safeWrite(fd, g_systemInfoBuffer);
            safeWrite(fd, "\n");
        }
        
        // Add stack trace
        writeSafeStackTrace(fd);
        
        safeWrite(fd, "\n=== END OF CRASH LOG ===\n");
        close(fd);
        
        safeWrite(2, "Crash log saved to: crash.log\n");
    }
#endif
}

// Signal handler function - MUST be async-signal-safe
static void signalHandler(int signal)
{
    const char* reason;
    switch(signal)
    {
        case SIGSEGV:
            reason = "Segmentation Fault (SIGSEGV)";
            break;
        case SIGABRT:
            reason = "Abnormal Termination (SIGABRT)";
            break;
        case SIGFPE:
            reason = "Floating Point Exception (SIGFPE)";
            break;
        case SIGILL:
            reason = "Illegal Instruction (SIGILL)";
            break;
#ifndef Q_OS_WIN
        case SIGBUS:
            reason = "Bus Error (SIGBUS)";
            break;
        case SIGTRAP:
            reason = "Trace/Breakpoint Trap (SIGTRAP)";
            break;
#endif
        default:
            reason = "Unknown Signal";
            break;
    }
    
    // Use only async-signal-safe functions
    writeSafeCrashLog(reason);
    
    // Restore default handler and re-raise signal
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

#ifdef Q_OS_WIN
// Windows exception handler - should also be kept simple and safe
static LONG WINAPI windowsExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
    const char* reason;
    switch(exceptionInfo->ExceptionRecord->ExceptionCode)
    {
        case EXCEPTION_ACCESS_VIOLATION:
            reason = "Access Violation";
            break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            reason = "Array Bounds Exceeded";
            break;
        case EXCEPTION_BREAKPOINT:
            reason = "Breakpoint";
            break;
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            reason = "Datatype Misalignment";
            break;
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            reason = "Float Denormal Operand";
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            reason = "Float Divide by Zero";
            break;
        case EXCEPTION_FLT_INEXACT_RESULT:
            reason = "Float Inexact Result";
            break;
        case EXCEPTION_FLT_INVALID_OPERATION:
            reason = "Float Invalid Operation";
            break;
        case EXCEPTION_FLT_OVERFLOW:
            reason = "Float Overflow";
            break;
        case EXCEPTION_FLT_STACK_CHECK:
            reason = "Float Stack Check";
            break;
        case EXCEPTION_FLT_UNDERFLOW:
            reason = "Float Underflow";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            reason = "Illegal Instruction";
            break;
        case EXCEPTION_IN_PAGE_ERROR:
            reason = "In Page Error";
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            reason = "Integer Divide by Zero";
            break;
        case EXCEPTION_INT_OVERFLOW:
            reason = "Integer Overflow";
            break;
        case EXCEPTION_INVALID_DISPOSITION:
            reason = "Invalid Disposition";
            break;
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            reason = "Noncontinuable Exception";
            break;
        case EXCEPTION_PRIV_INSTRUCTION:
            reason = "Privileged Instruction";
            break;
        case EXCEPTION_SINGLE_STEP:
            reason = "Single Step";
            break;
        case EXCEPTION_STACK_OVERFLOW:
            reason = "Stack Overflow";
            break;
        default:
            reason = "Unknown Exception";
            break;
    }
    
    // Use safe crash logging
    writeSafeCrashLog(reason);
    
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

// Initialize system information buffers at startup for async-signal-safe crash logging
// This must be called before any crash can occur
static void initSystemInfoBuffers()
{
    if (g_systemInfoInitialized) {
        return; // Already initialized
    }
    
    // Get system info using Qt functions (safe to use here, at startup)
    QString info;
    
    // Software Information
    info += "Qt Version: " + QString(qVersion()) + "\n";
    
    // Operating System Information
    info += "OS: " + QSysInfo::prettyProductName() + "\n";
    info += "Kernel Type: " + QSysInfo::kernelType() + "\n";
    info += "Kernel Version: " + QSysInfo::kernelVersion() + "\n";
    info += "Product Type: " + QSysInfo::productType() + "\n";
    info += "Product Version: " + QSysInfo::productVersion() + "\n";
    
    // Hardware Information
    info += "CPU Architecture: " + QSysInfo::currentCpuArchitecture() + "\n";
    info += "Build CPU Architecture: " + QSysInfo::buildCpuArchitecture() + "\n";
    
#ifdef Q_OS_WIN
    // Windows-specific: Get CPU and memory information
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info += QString("CPU Cores: %1\n").arg(sysInfo.dwNumberOfProcessors);
    
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo))
    {
        qint64 totalPhysMem = memInfo.ullTotalPhys / (1024 * 1024); // Convert to MB
        qint64 availPhysMem = memInfo.ullAvailPhys / (1024 * 1024);
        info += QString("Total Physical Memory: %1 MB\n").arg(totalPhysMem);
        info += QString("Available Physical Memory: %1 MB\n").arg(availPhysMem);
    }
#else
    // Unix-like: Get CPU count
    long numCores = sysconf(_SC_NPROCESSORS_ONLN);
    if (numCores > 0)
    {
        info += QString("CPU Cores: %1\n").arg(numCores);
    }
    
    // Unix-like: Get memory information (Linux-specific)
#ifdef Q_OS_LINUX
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageSize > 0)
    {
        qint64 totalMem = (qint64)pages * pageSize / (1024 * 1024); // Convert to MB
        info += QString("Total Physical Memory: %1 MB\n").arg(totalMem);
    }
#endif
#endif
    
    // Display Information (if available)
    QList<QScreen*> screens = QGuiApplication::screens();
    if (!screens.isEmpty())
    {
        info += QString("\nDisplay Information:\n");
        for (int i = 0; i < screens.size(); ++i)
        {
            QScreen* screen = screens[i];
            QSize size = screen->size();
            qreal dpi = screen->logicalDotsPerInch();
            info += QString("  Screen %1: %2x%3 @ %4 DPI\n")
                .arg(i + 1)
                .arg(size.width())
                .arg(size.height())
                .arg(dpi, 0, 'f', 1);
        }
    }
    
    // Convert to C string and store in static buffer
    QByteArray infoBytes = info.toUtf8();
    size_t copyLen = qMin((size_t)infoBytes.size(), sizeof(g_systemInfoBuffer) - 1);
    memcpy(g_systemInfoBuffer, infoBytes.constData(), copyLen);
    g_systemInfoBuffer[copyLen] = '\0';
    
    g_systemInfoInitialized = true;
}

void CrashLog::install()
{
#ifdef Q_OS_WIN
    // On Windows, set stderr and stdout to binary mode immediately to prevent
    // text mode conversions that could cause UTF-16LE encoding issues.
    // This must be done before any crash can occur, and before initSystemInfoBuffers()
    // which might indirectly cause output (though it currently doesn't).
    _setmode(_fileno(stderr), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    
    // Initialize system info buffers for async-signal-safe crash logging
    initSystemInfoBuffers();
    
    // Install signal handlers
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGFPE, signalHandler);
    std::signal(SIGILL, signalHandler);
    
#ifndef Q_OS_WIN
    std::signal(SIGBUS, signalHandler);
    std::signal(SIGTRAP, signalHandler);
#endif

#ifdef Q_OS_WIN
    // Install Windows exception handler
    SetUnhandledExceptionFilter(windowsExceptionHandler);
#endif

    logMessage("Crash log handler installed successfully");
}

QString CrashLog::getLogFilePath()
{
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    if (!dir.exists(logDir))
    {
        dir.mkpath(logDir);
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    return logDir + "/crash_" + timestamp + ".log";
}

void CrashLog::logMessage(const QString &message)
{
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    if (!dir.exists(logDir))
    {
        dir.mkpath(logDir);
    }
    
    QString logPath = logDir + "/usagi.log";
    QFile logFile(logPath);
    if (logFile.open(QIODevice::Append))
    {
        QTextStream stream(&logFile);
        // Explicitly set UTF-8 encoding to prevent UTF-16LE on Windows
        stream.setEncoding(QStringConverter::Utf8);
        // Explicitly disable BOM to ensure clean UTF-8 output
        stream.setGenerateByteOrderMark(false);
        stream << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") 
               << " - " << message << "\n";
        logFile.close();
    }
}

QString CrashLog::getSystemInfo()
{
    QString info;
    
    // Application Information
    info += "Application: " + QCoreApplication::applicationName() + "\n";
    info += "Version: " + QCoreApplication::applicationVersion() + "\n";
    info += "Timestamp: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "\n\n";
    
    // Software Information
    info += "Qt Version: " + QString(qVersion()) + "\n";
    
    // Operating System Information
    info += "OS: " + QSysInfo::prettyProductName() + "\n";
    info += "Kernel Type: " + QSysInfo::kernelType() + "\n";
    info += "Kernel Version: " + QSysInfo::kernelVersion() + "\n";
    info += "Product Type: " + QSysInfo::productType() + "\n";
    info += "Product Version: " + QSysInfo::productVersion() + "\n";
    
    // Hardware Information
    info += "CPU Architecture: " + QSysInfo::currentCpuArchitecture() + "\n";
    info += "Build CPU Architecture: " + QSysInfo::buildCpuArchitecture() + "\n";
    
#ifdef Q_OS_WIN
    // Windows-specific: Get CPU and memory information
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info += QString("CPU Cores: %1\n").arg(sysInfo.dwNumberOfProcessors);
    
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo))
    {
        qint64 totalPhysMem = memInfo.ullTotalPhys / (1024 * 1024); // Convert to MB
        qint64 availPhysMem = memInfo.ullAvailPhys / (1024 * 1024);
        info += QString("Total Physical Memory: %1 MB\n").arg(totalPhysMem);
        info += QString("Available Physical Memory: %1 MB\n").arg(availPhysMem);
    }
#else
    // Unix-like: Get CPU count
    long numCores = sysconf(_SC_NPROCESSORS_ONLN);
    if (numCores > 0)
    {
        info += QString("CPU Cores: %1\n").arg(numCores);
    }
    
    // Unix-like: Get memory information (Linux-specific)
#ifdef Q_OS_LINUX
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageSize > 0)
    {
        qint64 totalMem = (qint64)pages * pageSize / (1024 * 1024); // Convert to MB
        info += QString("Total Physical Memory: %1 MB\n").arg(totalMem);
    }
#endif
#endif
    
    // Display Information (if available)
    QList<QScreen*> screens = QGuiApplication::screens();
    if (!screens.isEmpty())
    {
        info += QString("\nDisplay Information:\n");
        for (int i = 0; i < screens.size(); ++i)
        {
            QScreen* screen = screens[i];
            QSize size = screen->size();
            qreal dpi = screen->logicalDotsPerInch();
            info += QString("  Screen %1: %2x%3 @ %4 DPI\n")
                .arg(i + 1)
                .arg(size.width())
                .arg(size.height())
                .arg(dpi, 0, 'f', 1);
        }
    }
    
    info += "\n";
    
    return info;
}

QString CrashLog::getStackTrace()
{
    QString trace;
    
#ifdef Q_OS_WIN
    // Windows stack trace using SymFromAddr
    const int maxFrames = 64;
    void* stack[maxFrames];
    HANDLE process = GetCurrentProcess();
    
    // Configure symbol options for better symbol resolution
    // SYMOPT_UNDNAME: Undecorate (demangle) symbol names
    // SYMOPT_DEFERRED_LOADS: Load symbols only when needed
    // SYMOPT_LOAD_LINES: Load line number information
    // SYMOPT_FAIL_CRITICAL_ERRORS: Don't display system error message boxes
    // SYMOPT_NO_PROMPTS: Don't prompt for symbol search paths
    // SYMOPT_INCLUDE_32BIT_MODULES: Include 32-bit modules in 64-bit process enumeration
    // SYMOPT_AUTO_PUBLICS: Automatically search public symbols if private symbols not found
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | 
                  SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS |
                  SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_AUTO_PUBLICS);
    
    // Build comprehensive symbol search path
    // Include: executable directory, current directory
    char exePath[MAX_PATH];
    char currentDir[MAX_PATH];
    char searchPath[MAX_PATH * 4];
    searchPath[0] = '\0'; // Initialize to empty string
    
    // Get the executable directory
    DWORD pathLen = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (pathLen > 0 && pathLen < MAX_PATH)
    {
        // Find the last backslash to get directory
        char* lastSlash = NULL;
        for (DWORD i = 0; i < pathLen; i++)
        {
            if (exePath[i] == '\\' || exePath[i] == '/')
            {
                lastSlash = &exePath[i];
            }
        }
        if (lastSlash)
        {
            *lastSlash = '\0'; // Null-terminate at the last slash to get directory
            // Copy executable directory to searchPath
            size_t len = strlen(exePath);
            if (len < sizeof(searchPath) - 1)
            {
                memcpy(searchPath, exePath, len);
                searchPath[len] = '\0';
            }
        }
    }
    
    // Add current directory to search path
    DWORD cwdLen = GetCurrentDirectoryA(MAX_PATH, currentDir);
    if (cwdLen > 0 && cwdLen < MAX_PATH)
    {
        size_t currentLen = strlen(searchPath);
        // Add semicolon separator if we already have exe path
        if (currentLen > 0 && currentLen < sizeof(searchPath) - 2)
        {
            searchPath[currentLen] = ';';
            searchPath[currentLen + 1] = '\0';
            currentLen++;
        }
        // Append current directory
        size_t remainingSpace = sizeof(searchPath) - currentLen - 1;
        if (cwdLen < remainingSpace)
        {
            memcpy(searchPath + currentLen, currentDir, cwdLen);
            searchPath[currentLen + cwdLen] = '\0';
        }
    }
    
    // Initialize symbol handler with comprehensive search path
    // This ensures PDB files are found in executable directory and current directory
    // TRUE parameter: automatically enumerate and load symbols for all loaded modules
    // This allows resolving symbols from Qt libraries and other DLLs in the stack trace
    BOOL symInitResult = SymInitialize(process, searchPath[0] != '\0' ? searchPath : NULL, TRUE);
    
    WORD frames = CaptureStackBackTrace(0, maxFrames, stack, NULL);
    
    trace += "\nStack Trace:\n";
    
    // Track symbol resolution for diagnostics
    int resolvedSymbols = 0;
    
    for (WORD i = 0; i < frames; i++)
    {
        DWORD64 address = (DWORD64)(stack[i]);
        
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        
        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol))
        {
            trace += QString("  [%1] %2 + 0x%3\n")
                .arg(i)
                .arg(QString::fromLatin1(symbol->Name))
                .arg(displacement, 0, 16);
            resolvedSymbols++;
        }
        else
        {
            trace += QString("  [%1] 0x%2\n")
                .arg(i)
                .arg(address, 0, 16);
        }
    }
    
    // Add symbol resolution diagnostics
    trace += QString("\nSymbol resolution: %1 of %2 frames resolved\n")
        .arg(resolvedSymbols).arg(frames);
    
    if (searchPath[0] != '\0')
    {
        trace += QString("Symbol search path: %1\n").arg(QString::fromLatin1(searchPath));
    }
    
    if (resolvedSymbols == 0 && frames > 0)
    {
        trace += "\nNote: No symbols resolved. This usually means:\n";
        trace += "  - PDB file is missing (MSVC builds)\n";
        trace += "  - Debug symbols were stripped (MinGW/GCC builds)\n";
        trace += "  - Executable was built without debug information\n";
        trace += "For MSVC builds: Ensure usagi.pdb is in the same directory as usagi.exe\n";
    }
    else if (resolvedSymbols > 0 && resolvedSymbols < frames / 2)
    {
        trace += "\nNote: Few symbols resolved. Check if PDB file exists alongside executable.\n";
    }
    
    SymCleanup(process);
#else
    // Unix-like stack trace using backtrace
    const int maxFrames = 64;
    void* buffer[maxFrames];
    int frames = backtrace(buffer, maxFrames);
    char** symbols = backtrace_symbols(buffer, frames);
    
    trace += "\nStack Trace:\n";
    for (int i = 0; i < frames; i++)
    {
        trace += QString("  [%1] %2\n").arg(i).arg(QString::fromLatin1(symbols[i]));
    }
    
    free(symbols);
#endif
    
    return trace;
}

void CrashLog::generateCrashLog(const QString &reason)
{
    QString logPath = getLogFilePath();
    QFile logFile(logPath);
    
    if (logFile.open(QIODevice::WriteOnly))
    {
        QTextStream stream(&logFile);
        // Explicitly set UTF-8 encoding to prevent UTF-16LE on Windows
        stream.setEncoding(QStringConverter::Utf8);
        // Explicitly disable BOM to ensure clean UTF-8 output
        stream.setGenerateByteOrderMark(false);
        
        stream << "=== CRASH LOG ===\n\n";
        stream << "Crash Reason: " << reason << "\n\n";
        stream << getSystemInfo();
        stream << getStackTrace();
        stream << "\n=== END OF CRASH LOG ===\n";
        
        logFile.close();
        
        // Also log to stderr
        fprintf(stderr, "\n=== CRASH DETECTED ===\n");
        fprintf(stderr, "Crash log saved to: %s\n", logPath.toUtf8().constData());
        fprintf(stderr, "Reason: %s\n", reason.toUtf8().constData());
        fprintf(stderr, "======================\n\n");
    }
}
