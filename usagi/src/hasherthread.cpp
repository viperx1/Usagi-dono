#include "hasherthread.h"
#include "main.h"
#include "logger.h"
#include <QMetaObject>

extern myAniDBApi *adbapi;

HasherThread::HasherThread(int threadId)
    : shouldStop(false), threadId(threadId), hasher(nullptr), lastProgressUpdate(0)
{
    // Create a lightweight ed2k hasher instance for this thread
    hasher = new ed2k();
    
    // Connect hasher signals with thread ID parameter
    // Capture this and threadId explicitly for the lambda
    connect(hasher, &ed2k::notifyPartsDone, this, [this, threadId](int total, int done) {
        // Throttle progress updates: emit only every HASHER_PROGRESS_UPDATE_INTERVAL parts
        if (done - lastProgressUpdate >= HASHER_PROGRESS_UPDATE_INTERVAL || done == total) {
            lastProgressUpdate = done;
            emit notifyPartsDone(threadId, total, done);
        }
    });
    
    connect(hasher, &ed2k::notifyFileHashed, this, [this, threadId](ed2k::ed2kfilestruct fileData) {
        emit notifyFileHashed(threadId, fileData);
    });
}

void HasherThread::run()
{
    // Reset shouldStop flag and clear queue for this thread execution
    {
        QMutexLocker locker(&mutex);
        shouldStop = false;
        fileQueue.clear();
    }
    
    LOG(QString("HasherThread %1 started processing files [hasherthread.cpp]").arg(threadId));
    
    // Emit the thread ID so tests can verify we're running in a separate thread
    emit threadStarted(QThread::currentThreadId());
    
    // Request the first file to hash
    emit requestNextFile();
    
    while (!shouldStop)
    {
        QString filePath;
        
        // Get next file from queue
        {
            QMutexLocker locker(&mutex);
            
            // Wait for a file to be added or stop signal
            while (fileQueue.isEmpty() && !shouldStop)
            {
                condition.wait(&mutex);
            }
            
            if (shouldStop)
            {
                break;
            }
            
            filePath = fileQueue.dequeue();
        }
        
        // Empty file path signals no more files
        if (filePath.isEmpty())
        {
            break;
        }
        
        // Reset progress tracking for new file
        lastProgressUpdate = 0;
        
        // Perform the actual hashing in this worker thread using dedicated ed2k instance
        switch(hasher->ed2khash(filePath))
        {
        case 1:
            emit sendHash(hasher->ed2khashstr);
            // Request the next file after successfully hashing this one
            emit requestNextFile();
            break;
        case 2:
            // Error occurred, but continue with next file
            emit requestNextFile();
            break;
        case 3:
            // Stop requested during hashing
            shouldStop = true;
            break;
        default:
            // Unknown result, request next file
            emit requestNextFile();
            break;
        }
    }
    
    LOG(QString("HasherThread %1 finished processing files [hasherthread.cpp]").arg(threadId));
    
    // Clean up hasher instance
    if (hasher != nullptr)
    {
        delete hasher;
        hasher = nullptr;
    }
}

void HasherThread::stop()
{
    QMutexLocker locker(&mutex);
    shouldStop = true;
    condition.wakeAll();
}

void HasherThread::addFile(const QString &filePath)
{
    QMutexLocker locker(&mutex);
    fileQueue.enqueue(filePath);
    condition.wakeOne();
}

void HasherThread::stopHashing()
{
    // Signal the hasher instance to stop the current hashing operation
    if (hasher != nullptr)
    {
        // Call the slot that sets the dohash flag to 0
        QMetaObject::invokeMethod(hasher, "getNotifyStopHasher", Qt::QueuedConnection);
    }
}
