#ifndef HASHERTHREAD_H
#define HASHERTHREAD_H
#include <QtGui>

class HasherThread : public QThread
{
    Q_OBJECT
public:
	void run();
	QStringList files;
public slots:
	void getFiles(QStringList);
signals:
	void sendHash(QString);
};

#endif // HASHERTHREAD_H
