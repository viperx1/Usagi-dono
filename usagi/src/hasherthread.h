#ifndef HASHERTHREAD_H
#define HASHERTHREAD_H
#include <QtGui>

class HasherThread : public QThread
{
    Q_OBJECT
public:
	void run();
	void stop();
private:
	bool shouldStop;
	QString currentFile;
public slots:
	void hashFile(QString filePath);
signals:
	void sendHash(QString);
	void requestNextFile();
};

#endif // HASHERTHREAD_H
