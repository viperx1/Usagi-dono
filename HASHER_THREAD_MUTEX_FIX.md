# Hasher Thread Mutex Fix

## Issue Description
Threads would sometimes stop after finishing hashing a file and not start hashing a new file until all other threads finished. This resulted in poor CPU utilization and longer hashing times.

## Root Cause

The `HasherThreadPool::addFile()` method used a static round-robin counter to distribute files across worker threads. This approach had a critical flaw:

**The round-robin assignment didn't consider which thread actually requested the file.**

### Problem Scenario

With 3 threads (T1, T2, T3) and 5 files (A, B, C, D, E):

1. **Initial state**: All threads start and emit `requestNextFile()`
   - T1 requests → Round-robin assigns File A to T2
   - T2 requests → Round-robin assigns File B to T3  
   - T3 requests → Round-robin assigns File C to T1

2. **Processing**: All threads busy with one file each
   - T1 hashing File C
   - T2 hashing File A
   - T3 hashing File B

3. **T2 finishes first** and requests next file
   - Round-robin assigns File D to T1 (currently busy with File C!)
   - **T2 now sits IDLE** with empty queue

4. **T1 finishes File C**
   - T1 finds File D in its queue (from step 3)
   - T1 starts processing File D **without requesting another file**
   - **T2 still IDLE**

5. **T3 finishes File B** and requests next file
   - Round-robin assigns File E to T2
   - T2 finally wakes up and processes File E

**Result**: T2 was idle during steps 3-5 while T1 had queued work!

## Solution

Track which thread is requesting a file and assign it directly to that thread:

1. **In `onThreadRequestNextFile()`**: Use Qt's `sender()` to identify the requesting worker and store it in `requestingWorker`
2. **In `addFile()`**: Check if we have a `requestingWorker` and assign the file directly to it
3. **Fallback**: If no requesting worker (e.g., initial startup), use round-robin

## Solution

Track which thread is requesting a file and assign it directly to that thread:

1. **In `onThreadRequestNextFile()`**: Use Qt's `sender()` to identify the requesting worker and enqueue it in a FIFO queue
2. **In `addFile()`**: Dequeue from the request queue and assign the file to that worker
3. **Fallback**: If queue is empty (e.g., initial startup), use round-robin

### Important: Deadlock Fix

The initial implementation had a critical bug that caused the app to freeze on startup:

**Deadlock Scenario:**
1. `onThreadRequestNextFile()` held `requestMutex` while emitting `requestNextFile()` signal
2. Signal was processed **synchronously** (direct connection from HasherThreadPool to Window in same thread)
3. `Window::provideNextFileToHash()` was called immediately (still holding `requestMutex`)
4. `Window` called `HasherThreadPool::addFile()`
5. `addFile()` tried to lock `requestMutex` → **DEADLOCK!**

**Solution:** Release the mutex BEFORE emitting the signal.

**Race Condition Fix:**

Using a single `requestingWorker` variable had a race where multiple queued requests could overwrite each other:
1. Thread A request → sets `requestingWorker = A`
2. Thread B request → sets `requestingWorker = B` (overwrites!)
3. Signal from step 1 processed → file assigned to B instead of A
4. Thread A sits idle

**Solution:** Use a FIFO queue (`requestQueue`) to preserve order of requests.

### Code Changes

#### hasherthreadpool.h
```cpp
private:
    QMutex requestMutex;              // Protects requestQueue
    QQueue<HasherThread*> requestQueue;   // FIFO queue of workers requesting files
```

#### hasherthreadpool.cpp
```cpp
void HasherThreadPool::onThreadRequestNextFile()
{
    // Enqueue the requesting worker in FIFO order
    {
        QMutexLocker locker(&requestMutex);
        HasherThread* requestingWorker = qobject_cast<HasherThread*>(sender());
        if (requestingWorker != nullptr)
        {
            requestQueue.enqueue(requestingWorker);
        }
    }
    
    // Release mutex BEFORE emitting to avoid deadlock
    emit requestNextFile();
}

void HasherThreadPool::addFile(const QString &filePath)
{
    // ... empty check ...
    
    if (isStarted && !workers.isEmpty())
    {
        HasherThread* targetWorker = nullptr;
        
        {
            QMutexLocker locker(&requestMutex);
            if (!requestQueue.isEmpty())
            {
                targetWorker = requestQueue.dequeue();
            }
        }
        
        if (targetWorker != nullptr)
        {
            // Assign to the worker that requested it
            targetWorker->addFile(filePath);
        }
        else
        {
            // Fallback to round-robin for initial distribution
            static int lastUsedWorker = 0;
            lastUsedWorker = (lastUsedWorker + 1) % workers.size();
            workers[lastUsedWorker]->addFile(filePath);
        }
    }
}
```

## Benefits

1. **No idle threads**: When a thread finishes and requests work, it gets work immediately
2. **No deadlock**: Mutex released before signal emission prevents synchronous call deadlock
3. **No race conditions**: FIFO queue preserves request order
4. **Better CPU utilization**: All threads stay busy when files are available
5. **Faster overall hashing**: No threads waiting unnecessarily
6. **Simpler logic**: Direct assignment is clearer than round-robin distribution

## Thread Safety

- `requestMutex` protects access to `requestQueue`
- Each thread still has its own `mutex` and `condition` variable for queue operations
- Mutex is released before emitting signal to prevent deadlock
- FIFO queue ensures proper ordering even with concurrent requests

## Testing

Existing tests in `test_hasher_threadpool.cpp` verify:
- Multiple threads are created
- Parallel hashing works correctly  
- All threads can be stopped
- Multiple unique thread IDs are used

The fix maintains backward compatibility while eliminating the idle thread issue.

## Verification

To verify the fix works:
1. Add 10+ files to hash with 3 worker threads
2. Monitor thread activity (e.g., via progress updates with thread IDs)
3. Confirm all threads remain active throughout the hashing process
4. Verify no threads sit idle while work is available
