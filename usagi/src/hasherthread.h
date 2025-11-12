#ifndef HASHERTHREAD_H
#define HASHERTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QQueue>

class myAniDBApi;

class HasherThread : public QThread
{
    Q_OBJECT
public:
    HasherThread(myAniDBApi *api = nullptr);
    void stop();
    void addFile(const QString &filePath);
    
protected:
    void run() override;
    
signals:
    void sendHash(QString);
    void requestNextFile();
    void threadStarted(Qt::HANDLE threadId);
    
private:
    QMutex mutex;
    QWaitCondition condition;
    QQueue<QString> fileQueue;
    bool shouldStop;
    myAniDBApi *apiInstance;
    bool ownsApiInstance;
};

#endif // HASHERTHREAD_H
