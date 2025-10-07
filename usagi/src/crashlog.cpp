#include "crashlog.h"
#include <QDir>
#include <QStandardPaths>
#include <csignal>
#include <cstdlib>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#else
#include <execinfo.h>
#include <unistd.h>
#endif

// Signal handler function
static void signalHandler(int signal)
{
    QString reason;
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
            reason = QString("Unknown Signal (%1)").arg(signal);
            break;
    }
    
    CrashLog::generateCrashLog(reason);
    
    // Restore default handler and re-raise signal
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

#ifdef Q_OS_WIN
// Windows exception handler
static LONG WINAPI windowsExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
    QString reason;
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
            reason = QString("Unknown Exception (0x%1)").arg(exceptionInfo->ExceptionRecord->ExceptionCode, 0, 16);
            break;
    }
    
    CrashLog::generateCrashLog(reason);
    
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
