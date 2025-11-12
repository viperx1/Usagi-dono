#ifndef HASHERTHREAD_H
#define HASHERTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QQueue>
#include "hash/ed2k.h"

class myAniDBApi;

class HasherThread : public QThread
{
    Q_OBJECT
public:
    HasherThread(myAniDBApi *api = nullptr);
    void stop();
    void addFile(const QString &filePath);
    void stopHashing(); // Interrupt any ongoing hash operation
    
protected:
    void run() override;
    
signals:
    void sendHash(QString);
    void requestNextFile();
    void threadStarted(Qt::HANDLE threadId);
    void notifyPartsDone(int total, int done);
    void notifyFileHashed(ed2k::ed2kfilestruct fileData);
    
private:
    QMutex mutex;
    QWaitCondition condition;
    QQueue<QString> fileQueue;
    bool shouldStop;
    myAniDBApi *apiInstance;
    bool ownsApiInstance;
};

#endif // HASHERTHREAD_H
