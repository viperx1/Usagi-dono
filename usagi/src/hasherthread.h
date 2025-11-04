#ifndef HASHERTHREAD_H
#define HASHERTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QQueue>

class HasherThread : public QThread
{
    Q_OBJECT
public:
    HasherThread();
    void stop();
    void addFile(const QString &filePath);
    
protected:
    void run() override;
    
signals:
    void sendHash(QString);
    void requestNextFile();
    
private:
    QMutex mutex;
    QWaitCondition condition;
    QQueue<QString> fileQueue;
    bool shouldStop;
};

#endif // HASHERTHREAD_H
