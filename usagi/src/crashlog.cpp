#include "crashlog.h"
#include <QDir>
#include <QStandardPaths>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdint>

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

// Safe string write function for signal handlers
static void safeWrite(int fd, const char* str)
{
    if (str)
    {
        size_t len = strlen(str);
#ifdef Q_OS_WIN
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
// Format: YYYY-MM-DD HH:MM:SS
static void formatTimestamp(char* buffer, size_t bufSize)
{
    if (!buffer || bufSize < 20) return; // Need "YYYY-MM-DD HH:MM:SS\0"
    
    time_t now = time(NULL);
    struct tm* tm_info;
    
#ifdef Q_OS_WIN
    // On Windows, gmtime is not thread-safe but there's no async-signal-safe alternative
    // We use it anyway as it's the best option available
    tm_info = gmtime(&now);
#else
    // On Unix/Linux, gmtime_r is async-signal-safe
    struct tm tm_storage;
    tm_info = gmtime_r(&now, &tm_storage);
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
    SymInitialize(process, NULL, TRUE);
    
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
    
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
    // Use _open and _write to avoid text encoding conversions (same as stderr approach)
    int fd = _open(logPath, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd >= 0)
    {
        _write(fd, "=== CRASH LOG ===\n\nCrash Reason: ", 33);
        _write(fd, reason, (unsigned int)strlen(reason));
        _write(fd, "\nTimestamp: ", 12);
        _write(fd, timestamp, (unsigned int)strlen(timestamp));
        _write(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n", 40);
        
        // Add stack trace
        writeSafeStackTrace(fd);
        
        _write(fd, "\n=== END OF CRASH LOG ===\n", 26);
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
        safeWrite(fd, "\nTimestamp: ");
        safeWrite(fd, timestamp);
        safeWrite(fd, "\nApplication: Usagi-dono\nVersion: 1.0.0\n");
        
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

void CrashLog::install()
{
    // Install signal handlers
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGFPE, signalHandler);
    std::signal(SIGILL, signalHandler);
    
#ifndef Q_OS_WIN
    std::signal(SIGBUS, signalHandler);
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
    if (logFile.open(QIODevice::Append | QIODevice::Text))
    {
        QTextStream stream(&logFile);
        stream << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") 
               << " - " << message << "\n";
        logFile.close();
    }
}

QString CrashLog::getSystemInfo()
{
    QString info;
    info += "Application: " + QCoreApplication::applicationName() + "\n";
    info += "Version: " + QCoreApplication::applicationVersion() + "\n";
    info += "Qt Version: " + QString(qVersion()) + "\n";
    info += "System: ";
    
#ifdef Q_OS_WIN
    info += "Windows";
#elif defined(Q_OS_LINUX)
    info += "Linux";
#elif defined(Q_OS_MAC)
    info += "macOS";
#else
    info += "Unknown";
#endif
    
    info += "\n";
    info += "Timestamp: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "\n";
    
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
    
    SymInitialize(process, NULL, TRUE);
    
    WORD frames = CaptureStackBackTrace(0, maxFrames, stack, NULL);
    
    trace += "\nStack Trace:\n";
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
        }
        else
        {
            trace += QString("  [%1] 0x%2\n")
                .arg(i)
                .arg(address, 0, 16);
        }
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
    
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream stream(&logFile);
        
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
