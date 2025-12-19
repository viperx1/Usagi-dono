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

bool HasherThreadPool::addFile(const QString &filePath)
{
    if (!isStarted || isStopping)
    {
        return false;
    }
    
    // Empty file path signals completion
    if (filePath.isEmpty())
    {
        // Signal ALL workers waiting in the request queue that there are no more files
        QVector<HasherThread*> waitingWorkers;
        {
            QMutexLocker requestLocker(&requestMutex);
            while (!requestQueue.isEmpty())
            {
                waitingWorkers.append(requestQueue.dequeue());
            }
        }
        
        // Send completion signal to all waiting workers
        for (HasherThread* worker : waitingWorkers)
        {
            worker->addFile(QString());
        }
        
        return !waitingWorkers.isEmpty();
    }
    
    // We have a file to hash
    // A thread must have requested it, so it should be in the request queue
    HasherThread* targetWorker = nullptr;
    
    {
        QMutexLocker requestLocker(&requestMutex);
        if (!requestQueue.isEmpty())
        {
            // A worker is waiting for work - give it the file
            targetWorker = requestQueue.dequeue();
        }
    }
    
    if (targetWorker != nullptr)
    {
        // Assign to the waiting worker
        targetWorker->addFile(filePath);
        return true;
    }
    
    // No thread was waiting for this file
    return false;
}

void HasherThreadPool::start(int fileCount)
{
    if (isStarted)
    {
        LOG("HasherThreadPool: Already started");
        return;
    }
    
    QMutexLocker locker(&mutex);
    isStarted = true;
    isStopping = false;
    activeThreads = 0;
    finishedThreads = 0;
    
    // Determine how many threads to create
    // Create min(fileCount, maxThreads) threads
    // If fileCount is 0 or negative, create 0 threads (wait for files to be added)
    int threadsToCreate = 0;
    if (fileCount > 0)
    {
        threadsToCreate = std::min(fileCount, maxThreads);
    }
    
    LOG(QString("HasherThreadPool: Starting pool with %1 file(s) - creating %2 thread(s) (max %3)")
        .arg(fileCount).arg(threadsToCreate).arg(maxThreads));
    
    // Create all threads upfront
    locker.unlock();
    for (int i = 0; i < threadsToCreate; ++i)
    {
        createThread();
    }
    
    if (threadsToCreate == 0)
    {
        LOG("HasherThreadPool: No threads created - no files to hash");
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
        
        // If no threads were ever created, emit finished signal immediately
        if (activeThreads == 0)
        {
            LOG("HasherThreadPool: No active threads, emitting finished signal");
            isStarted = false;
            isStopping = false;
            locker.unlock();
            emit finished();
            return;
        }
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
    HasherThread* requestingWorker = qobject_cast<HasherThread*>(sender());
    if (requestingWorker != nullptr)
    {
        QMutexLocker locker(&requestMutex);
        requestQueue.enqueue(requestingWorker);
    }
    
    // Forward the request to the coordinator to provide the next file
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
    // Create and start a new thread
    // The thread will request work when it starts
    (void)createThreadAndReturnIt();
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
    
    int threadId = nextThreadId % maxThreads;  // Reuse thread IDs in range [0, maxThreads)
    nextThreadId++;
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
    
    // Clear the request queue to prevent dangling pointers
    // This is critical for pool restart scenarios where old thread pointers
    // would otherwise remain in the queue after deletion.
    // Note: At this point, all threads have finished their run() method and
    // cannot emit requestNextFile() anymore, so the queue cannot grow.
    {
        QMutexLocker locker(&requestMutex);
        requestQueue.clear();
    }
    
    // Now delete all worker threads
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
