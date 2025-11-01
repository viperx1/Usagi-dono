# Thread Safety Fix for SQLite Access Crash

## Problem Description

The application was experiencing an Access Violation crash when the `HasherThread` worker thread attempted to access the SQLite database. The crash occurred in the SQLite query parser when `getLocalFileHash()` was called from a background thread.

### Stack Trace Analysis
```
13 AniDBApi::getLocalFileHash(QString)     anidbapi.cpp      1700
12 QSqlQuery::prepare(QString const&)      qsqlquery.cpp     1008
11 QSqlResult::savePrepare(QString const&) qsqlresult.cpp    576
10 QSQLiteResult::prepare(QString const&)  qsql_sqlite.cpp   397
...
[SQLite internal crash]
```

### Root Cause

The crash was caused by a **thread-safety violation** in database access:

1. The `AniDBApi` class creates a single `QSqlDatabase` connection (`db`) in its constructor, which runs in the main thread
2. The `HasherThread` worker thread calls `ed2khash()` → `getLocalFileHash()` 
3. `getLocalFileHash()` attempted to use the main thread's database connection from the worker thread
4. SQLite connections in Qt are **not thread-safe** by default - they must not be shared across threads
5. This resulted in an Access Violation crash in SQLite's query parser

## Solution

Modified `getLocalFileHash()` to create **thread-local database connections**:

```cpp
QString AniDBApi::getLocalFileHash(QString localPath)
{
    // Get the database connection for this thread
    QSqlDatabase threadDb;
    QString connectionName = QString("hash_query_thread_%1")
        .arg((quintptr)QThread::currentThreadId());
    
    if (QSqlDatabase::contains(connectionName))
    {
        threadDb = QSqlDatabase::database(connectionName);
    }
    else
    {
        // Create a new connection for this thread
        threadDb = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        threadDb.setDatabaseName(db.databaseName());
        
        if (!threadDb.open())
        {
            Logger::log(QString("Failed to open thread-local database connection: %1")
                .arg(threadDb.lastError().text()));
            return QString();
        }
    }
    
    // Use threadDb instead of db for queries...
}
```

### Key Implementation Details

1. **Thread-local connection naming**: Each thread gets a unique connection name based on `QThread::currentThreadId()`
2. **Connection reuse**: If a connection already exists for the thread, it's reused
3. **Automatic creation**: New connections are created on-demand when accessed from a new thread
4. **Same database file**: All connections point to the same database file, which SQLite handles safely for concurrent reads
5. **Error handling**: Proper error checking and logging if connection creation fails

### Why This Works

- SQLite supports **multiple simultaneous readers** from different connections
- Qt's `QSqlDatabase` manages connection lifetime automatically
- Each thread maintains its own connection, eliminating race conditions
- Read-only queries (SELECT) are safe with this approach

## Testing

Created `test_thread_safety.cpp` which:
1. Creates an `AniDBApi` instance (main thread connection)
2. Inserts test data into the database
3. Spawns a worker thread that calls `getLocalFileHash()`
4. Verifies the data is retrieved successfully without crashing

## Files Changed

1. **usagi/src/anidbapi.cpp**
   - Added `#include <QThread>`
   - Modified `getLocalFileHash()` to use thread-local connections

2. **tests/test_thread_safety.cpp** (new file)
   - Comprehensive test for thread-safe database access

3. **tests/CMakeLists.txt**
   - Added test_thread_safety target

## Impact Analysis

### Functions Analyzed for Thread Safety
- ✅ `getLocalFileHash()` - **FIXED** (called from worker thread)
- ✅ `updateLocalFileHash()` - No fix needed (only called from main thread via window.cpp)
- ✅ `UpdateLocalFileStatus()` - No fix needed (only called from main thread via window.cpp)

### Minimal Change Philosophy
This fix follows the "minimal change" principle:
- Only the function with the thread-safety issue was modified
- No architectural changes were required
- No changes to the public API
- No impact on existing functionality

## References

- Qt Documentation: [Thread Support in Qt Modules - SQL Module](https://doc.qt.io/qt-6/threads-modules.html#sql-module)
- Qt Documentation: [QSqlDatabase](https://doc.qt.io/qt-6/qsqldatabase.html#thread-safety)
- SQLite Documentation: [Using SQLite In Multi-Threaded Applications](https://www.sqlite.org/threadsafe.html)
