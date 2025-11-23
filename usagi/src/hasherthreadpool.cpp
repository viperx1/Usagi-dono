#include "hasherthreadpool.h"
#include "hasherthread.h"
#include "logger.h"
#include <QThread>
#include <algorithm>

HasherThreadPool::HasherThreadPool(int numThreads, QObject *parent)
    : QObject(parent), activeThreads(0), finishedThreads(0), isStarted(false), isStopping(false)
{
    // Determine optimal number of threads
    if (numThreads <= 0)
    {
        // Use number of CPU cores, but limit to reasonable range
        numThreads = QThread::idealThreadCount();
        if (numThreads <= 0) numThreads = 4; // Default if idealThreadCount fails
    }
    
    // Clamp to reasonable range: minimum 1, maximum 16
    numThreads = std::max(1, std::min(numThreads, 16));
    
    LOG(QString("HasherThreadPool: Creating pool with %1 worker threads").arg(numThreads));
    
    // Create worker threads with sequential thread IDs
    for (int i = 0; i < numThreads; ++i)
    {
        HasherThread *worker = new HasherThread(i); // Pass thread ID
        
        // Connect signals from worker to pool
        connect(worker, &HasherThread::requestNextFile, 
                this, &HasherThreadPool::onThreadRequestNextFile, Qt::QueuedConnection);
        connect(worker, &HasherThread::sendHash, 
                this, &HasherThreadPool::onThreadSendHash, Qt::QueuedConnection);
        connect(worker, &QThread::finished, 
                this, &HasherThreadPool::onThreadFinished, Qt::QueuedConnection);
        connect(worker, &HasherThread::threadStarted,
                this, &HasherThreadPool::onThreadStarted, Qt::QueuedConnection);
        connect(worker, &HasherThread::notifyPartsDone,
                this, &HasherThreadPool::onThreadPartsDone, Qt::QueuedConnection);
        connect(worker, &HasherThread::notifyFileHashed,
                this, &HasherThreadPool::onThreadFileHashed, Qt::QueuedConnection);
        
        workers.append(worker);
    }
}

HasherThreadPool::~HasherThreadPool()
{
    // Stop all threads if still running
    stop();
    wait();
    
    // Clean up worker threads
    for (HasherThread* const worker : std::as_const(workers))
    {
        delete worker;
    }
    workers.clear();
}

void HasherThreadPool::addFile(const QString &filePath)
{
    // Empty file path signals completion to all threads
    if (filePath.isEmpty())
    {
        LOG("HasherThreadPool: Signaling completion to all worker threads");
        for (HasherThread* const worker : std::as_const(workers))
        {
            worker->addFile(QString());
        }
        return;
    }
    
    if (isStarted && !workers.isEmpty())
    {
        // Assign the file to the worker that requested it from the queue
        // This ensures threads don't sit idle while other threads have queued work
        HasherThread* targetWorker = nullptr;
        
        {
            QMutexLocker locker(&requestMutex);
            if (!requestQueue.isEmpty())
            {
                targetWorker = requestQueue.dequeue();
            }
        }
        
        // If we have a specific requesting worker, assign to it
        // Otherwise fall back to round-robin (e.g., initial file distribution)
        if (targetWorker != nullptr)
        {
            targetWorker->addFile(filePath);
        }
        else
        {
            // Fallback to round-robin for initial file distribution
            static int lastUsedWorker = 0;
            lastUsedWorker = (lastUsedWorker + 1) % workers.size();
            workers[lastUsedWorker]->addFile(filePath);
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
    
    LOG(QString("HasherThreadPool: Starting %1 worker threads").arg(workers.size()));
    
    QMutexLocker locker(&mutex);
    isStarted = true;
    isStopping = false;
    activeThreads = workers.size();
    finishedThreads = 0;
    
    // Start all worker threads
    for (HasherThread* const worker : std::as_const(workers))
    {
        worker->start();
    }
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
    
    // Stop all worker threads
    for (HasherThread* const worker : std::as_const(workers))
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
    if (finishedThreads >= activeThreads)
    {
        LOG("HasherThreadPool: All worker threads finished");
        isStarted = false;
        isStopping = false;
        emit finished();
    }
}
