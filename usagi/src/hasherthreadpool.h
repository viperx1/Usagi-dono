#ifndef HASHERTHREADPOOL_H
#define HASHERTHREADPOOL_H

#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QString>
#include <QVector>
#include <QThread>
#include "hash/ed2k.h"

class HasherThread;

/**
 * HasherThreadPool manages multiple HasherThread workers to hash files in parallel.
 * This allows utilizing multiple CPU cores for faster hashing of large file collections.
 * 
 * Thread lifecycle:
 * - Threads are created on-demand when work is available
 * - When a thread finishes hashing and no more work is available, it terminates
 * - Maximum number of concurrent threads is limited by maxThreads
 */
class HasherThreadPool : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a thread pool that will spawn up to maxThreads workers.
     * @param maxThreads Maximum number of concurrent worker threads (defaults to available CPU cores, min 1, max 16)
     */
    explicit HasherThreadPool(int maxThreads = 0, QObject *parent = nullptr);
    ~HasherThreadPool();
    
    /**
     * Adds a file to the queue to be hashed by any available worker thread.
     * Creates a new thread if one is available and work is pending.
     * @param filePath Path to the file to hash (empty string signals completion)
     */
    void addFile(const QString &filePath);
    
    /**
     * Starts the pool to begin processing files.
     * Threads will be created on-demand as work becomes available.
     */
    void start();
    
    /**
     * Stops all worker threads gracefully.
     */
    void stop();
    
    /**
     * Broadcasts stop signal to all worker API instances.
     * This interrupts any ongoing hashing operations.
     */
    void broadcastStopHasher();
    
    /**
     * Waits for all threads to finish.
     * @param msecs Maximum time to wait in milliseconds
     * @return true if all threads finished, false if timeout
     */
    bool wait(unsigned long msecs = ULONG_MAX);
    
    /**
     * Returns the number of currently active worker threads.
     */
    int threadCount() const { return workers.size(); }
    
    /**
     * Returns true if any worker thread is currently running.
     */
    bool isRunning() const;
    
signals:
    /**
     * Emitted when a file has been successfully hashed.
     * @param hash The ed2k hash string
     */
    void sendHash(QString hash);
    
    /**
     * Emitted when a worker needs the next file to hash.
     * This is emitted by individual workers and forwarded by the pool.
     */
    void requestNextFile();
    
    /**
     * Emitted when all threads have finished processing.
     */
    void finished();
    
    /**
     * Emitted when a thread has started (for testing).
     */
    void threadStarted(Qt::HANDLE threadId);
    
    /**
     * Emitted when progress is made on hashing a file.
     * @param threadId Logical thread ID (0-based)
     * @param total Total parts to hash
     * @param done Parts completed
     */
    void notifyPartsDone(int threadId, int total, int done);
    
    /**
     * Emitted when a file has been completely hashed.
     * @param threadId Logical thread ID (0-based)
     * @param fileData Hashed file information
     */
    void notifyFileHashed(int threadId, ed2k::ed2kfilestruct fileData);
    
private slots:
    void onThreadRequestNextFile();
    void onThreadSendHash(QString hash);
    void onThreadFinished();
    void onThreadStarted(Qt::HANDLE threadId);
    void onThreadPartsDone(int threadId, int total, int done);
    void onThreadFileHashed(int threadId, ed2k::ed2kfilestruct fileData);
    
private:
    void checkAllThreadsFinished();
    void createThread();
    HasherThread* createThreadAndReturnIt();
    void cleanupFinishedThreads();
    
    QVector<HasherThread*> workers;
    QMutex mutex;
    QMutex requestMutex;  // Protects requestQueue
    QQueue<HasherThread*> requestQueue;  // FIFO queue of workers requesting files
    int maxThreads;  // Maximum number of concurrent threads
    int nextThreadId;  // Counter for assigning unique thread IDs
    int activeThreads;  // Number of threads currently running
    int finishedThreads;  // Number of threads that have finished
    bool isStarted;
    bool isStopping;
};

#endif // HASHERTHREADPOOL_H
