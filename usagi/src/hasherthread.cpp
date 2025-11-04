#include "hasherthread.h"
#include "main.h"
#include "logger.h"

extern myAniDBApi *adbapi;

HasherThread::HasherThread()
    : shouldStop(false)
{
}

void HasherThread::run()
{
    // Reset shouldStop flag and clear queue for this thread execution
    {
        QMutexLocker locker(&mutex);
        shouldStop = false;
        fileQueue.clear();
    }
    
    LOG("HasherThread started processing files [hasherthread.cpp]");
    
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
        
        // Perform the actual hashing in this worker thread
        switch(adbapi->ed2khash(filePath))
        {
        case 1:
            emit sendHash(adbapi->ed2khashstr);
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
    
    LOG("HasherThread finished processing files [hasherthread.cpp]");
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
