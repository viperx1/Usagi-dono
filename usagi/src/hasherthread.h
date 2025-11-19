#ifndef HASHERTHREAD_H
#define HASHERTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QQueue>
#include "hash/ed2k.h"

// Progress update throttle: emit progress signal every N parts to reduce UI overhead
// This constant should be used by both hasher and UI for consistent progress tracking
constexpr int HASHER_PROGRESS_UPDATE_INTERVAL = 10;

class HasherThread : public QThread
{
    Q_OBJECT
public:
    HasherThread(int threadId = 0);
    ~HasherThread();
    void stop();
    void addFile(const QString &filePath);
    void stopHashing(); // Interrupt any ongoing hash operation
    int getThreadId() const { return threadId; }
    
protected:
    void run() override;
    
signals:
    void sendHash(QString);
    void requestNextFile();
    void threadStarted(Qt::HANDLE threadId);
    void notifyPartsDone(int threadId, int total, int done);
    void notifyFileHashed(int threadId, ed2k::ed2kfilestruct fileData);
    
private:
    void cleanupHasher(); // Helper to clean up hasher instance
    
    QMutex mutex;
    QWaitCondition condition;
    QQueue<QString> fileQueue;
    bool shouldStop;
    int threadId; // Logical thread ID for UI identification
    ed2k *hasher; // Dedicated hasher instance (lightweight, no DB/network)
    int lastProgressUpdate; // Track last progress update to throttle
};

#endif // HASHERTHREAD_H
