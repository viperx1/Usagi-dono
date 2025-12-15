#include "hasherthreadpool.h"
#include "hasherthread.h"
#include "logger.h"
#include <QThread>
#include <algorithm>

HasherThreadPool::HasherThreadPool(int maxThreads, QObject *parent)
    : QObject(parent), maxThreads(0), nextThreadId(0), activeThreads(0), finishedThreads(0), isStarted(false), isStopping(false)
{
    // Determine optimal maximum number of threads
    if (maxThreads <= 0)
    {
        // Use number of CPU cores, but limit to reasonable range
        maxThreads = QThread::idealThreadCount();
        if (maxThreads <= 0) maxThreads = 4; // Default if idealThreadCount fails
    }
    
    // Clamp to reasonable range: minimum 1, maximum 16
    this->maxThreads = std::max(1, std::min(maxThreads, 16));
    
    LOG(QString("HasherThreadPool: Configured for up to %1 worker threads").arg(this->maxThreads));
}

HasherThreadPool::~HasherThreadPool()
{
    // Stop all threads if still running
    stop();
    wait();
    
    // Clean up worker threads
    cleanupFinishedThreads();
}

void HasherThreadPool::addFile(const QString &filePath)
{
    if (!isStarted || isStopping)
    {
        return;
    }
    
    // Empty file path signals completion
    if (filePath.isEmpty())
    {
        // Get a worker from the request queue
        HasherThread* targetWorker = nullptr;
        {
            QMutexLocker requestLocker(&requestMutex);
            if (!requestQueue.isEmpty())
            {
                targetWorker = requestQueue.dequeue();
            }
        }
        
        if (targetWorker != nullptr)
        {
            // Signal this specific worker to finish (no more files)
            targetWorker->addFile(QString());
            LOG(QString("HasherThreadPool: Signaling thread to finish - no more work"));
        }
        else
        {
            // No worker waiting for work, which is expected if all threads are busy
            // They will finish naturally when they complete their current file and request next
            LOG("HasherThreadPool: No worker waiting when signaling completion (threads may be busy)");
        }
        return;
    }
    
    // We have a file to hash
    HasherThread* targetWorker = nullptr;
    
    {
        QMutexLocker requestLocker(&requestMutex);
        if (!requestQueue.isEmpty())
        {
            // A worker is waiting for work
            targetWorker = requestQueue.dequeue();
        }
    }
    
    if (targetWorker != nullptr)
    {
        // Assign to the waiting worker
        targetWorker->addFile(filePath);
    }
    else
    {
        // No worker is currently waiting
        // Check if we can create a new thread
        QMutexLocker locker(&mutex);
        if (workers.size() < maxThreads)
        {
            // Create a new thread and give it this file immediately
            // The thread's file queue can hold the file before the thread even starts
            locker.unlock();
            targetWorker = createThreadAndReturnIt();
            if (targetWorker != nullptr)
            {
                targetWorker->addFile(filePath);
            }
        }
        else if (!workers.isEmpty())
        {
            // All threads are busy but at max capacity
            // Assign to a thread in round-robin fashion
            // Each thread has a queue, so it can hold multiple files
            static int lastUsedWorker = 0;
            lastUsedWorker = (lastUsedWorker + 1) % workers.size();
            workers[lastUsedWorker]->addFile(filePath);
            LOG(QString("HasherThreadPool: All threads busy, queueing file to thread %1").arg(workers[lastUsedWorker]->getThreadId()));
        }
        else
        {
            // No threads at all - this shouldn't happen
            LOG("HasherThreadPool: ERROR - No threads available to assign file");
        }
    }
}

void HasherThreadPool::start()
{
    if (isStarted)
    {
        LOG("HasherThreadPool: Already started");
        return;
    }
    
    LOG(QString("HasherThreadPool: Starting pool (max %1 threads, on-demand creation)").arg(maxThreads));
    
    QMutexLocker locker(&mutex);
    isStarted = true;
    isStopping = false;
    activeThreads = 0;
    finishedThreads = 0;
    
    // Create the first thread to kick off the process
    // This thread will request work, which triggers the coordinator to provide files
    // Additional threads will be created on-demand if more work is available
    locker.unlock();
    createThread();
}

void HasherThreadPool::stop()
{
    if (!isStarted || isStopping)
    {
        return;
    }
    
    LOG("HasherThreadPool: Stopping all worker threads");
    
    {
        QMutexLocker locker(&mutex);
        isStopping = true;
    }
    
    // Stop all active worker threads
    QVector<HasherThread*> workersCopy;
    {
        QMutexLocker locker(&mutex);
        workersCopy = workers;
    }
    
    for (HasherThread* const worker : std::as_const(workersCopy))
    {
        worker->stop();
    }
}

void HasherThreadPool::broadcastStopHasher()
{
    LOG("HasherThreadPool: Broadcasting stop hasher signal to all workers");
    
    // Signal all workers to stop their current hashing operations
    for (HasherThread* const worker : std::as_const(workers))
    {
        worker->stopHashing();
    }
}

bool HasherThreadPool::wait(unsigned long msecs)
{
    // Wait for all worker threads to finish
    bool allFinished = true;
    for (HasherThread* const worker : std::as_const(workers))
    {
        if (!worker->wait(msecs))
        {
            allFinished = false;
        }
    }
    return allFinished;
}

bool HasherThreadPool::isRunning() const
{
    // Check if any worker thread is running
    for (const HasherThread *worker : std::as_const(workers))
    {
        if (worker->isRunning())
        {
            return true;
        }
    }
    return false;
}

void HasherThreadPool::onThreadRequestNextFile()
{
    // Track which worker is requesting the next file in a FIFO queue
    // Use sender() to identify the worker that sent the signal
    {
        QMutexLocker locker(&requestMutex);
        HasherThread* requestingWorker = qobject_cast<HasherThread*>(sender());
        if (requestingWorker != nullptr)
        {
            requestQueue.enqueue(requestingWorker);
        }
    }
    
    // Forward the request to the Window class to provide the next file
    // IMPORTANT: Signal must be emitted AFTER releasing requestMutex to avoid deadlock
    // because Window::provideNextFileToHash() will call addFile() which also locks requestMutex
    emit requestNextFile();
}

void HasherThreadPool::onThreadSendHash(QString hash)
{
    // Forward the hash result to the Window class
    emit sendHash(hash);
}

void HasherThreadPool::onThreadFinished()
{
    QMutexLocker locker(&mutex);
    finishedThreads++;
    
    LOG(QString("HasherThreadPool: Worker thread finished (%1/%2 complete)")
        .arg(finishedThreads).arg(activeThreads));
    
    checkAllThreadsFinished();
}

void HasherThreadPool::onThreadStarted(Qt::HANDLE threadId)
{
    // Forward thread started signal for testing
    emit threadStarted(threadId);
}

void HasherThreadPool::onThreadPartsDone(int threadId, int total, int done)
{
    // Forward parts done signal to UI with thread ID
    emit notifyPartsDone(threadId, total, done);
}

void HasherThreadPool::onThreadFileHashed(int threadId, ed2k::ed2kfilestruct fileData)
{
    // Forward file hashed signal to UI with thread ID
    emit notifyFileHashed(threadId, fileData);
}

void HasherThreadPool::checkAllThreadsFinished()
{
    // Must be called with mutex locked
    if (finishedThreads >= activeThreads && activeThreads > 0)
    {
        LOG("HasherThreadPool: All worker threads finished");
        isStarted = false;
        isStopping = false;
        
        // Clean up finished threads
        cleanupFinishedThreads();
        
        emit finished();
    }
}

void HasherThreadPool::createThread()
{
    HasherThread* worker = createThreadAndReturnIt();
    // Thread created, it will request work when it starts
}

HasherThread* HasherThreadPool::createThreadAndReturnIt()
{
    // Must be called without mutex locked (will lock internally)
    QMutexLocker locker(&mutex);
    
    if (workers.size() >= maxThreads)
    {
        LOG(QString("HasherThreadPool: Cannot create thread - already at max (%1)").arg(maxThreads));
        return nullptr;
    }
    
    if (!isStarted || isStopping)
    {
        return nullptr;
    }
    
    int threadId = nextThreadId++;
    HasherThread *worker = new HasherThread(threadId);
    
    // Connect signals from worker to pool
    connect(worker, &HasherThread::requestNextFile, 
            this, &HasherThreadPool::onThreadRequestNextFile, Qt::QueuedConnection);
    connect(worker, &HasherThread::sendHash, 
            this, &HasherThreadPool::onThreadSendHash, Qt::QueuedConnection);
    connect(static_cast<QThread*>(worker), &QThread::finished, 
            this, &HasherThreadPool::onThreadFinished, Qt::QueuedConnection);
    connect(worker, &HasherThread::threadStarted,
            this, &HasherThreadPool::onThreadStarted, Qt::QueuedConnection);
    connect(worker, &HasherThread::notifyPartsDone,
            this, &HasherThreadPool::onThreadPartsDone, Qt::QueuedConnection);
    connect(worker, &HasherThread::notifyFileHashed,
            this, &HasherThreadPool::onThreadFileHashed, Qt::QueuedConnection);
    
    workers.append(worker);
    activeThreads++;
    
    LOG(QString("HasherThreadPool: Created thread %1 (%2/%3 active)").arg(threadId).arg(workers.size()).arg(maxThreads));
    
    // Start the thread
    worker->start();
    
    return worker;
}

void HasherThreadPool::cleanupFinishedThreads()
{
    // Clean up all finished threads
    // This should be called when all threads are done or from destructor
    for (HasherThread* const worker : std::as_const(workers))
    {
        if (worker->isFinished())
        {
            worker->wait(); // Ensure thread is completely finished
        }
        delete worker;
    }
    workers.clear();
    activeThreads = 0;
    finishedThreads = 0;
}
