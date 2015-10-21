#ifndef ED2K_H
#define ED2K_H

#include "md4.h"
#include <QString>
#include <QFileInfo>
#include <QFile>

class ed2k:public QObject, private MD4
{
	Q_OBJECT
private:
	int b;
	int block;
	char buffer[102400];
	MD4_CTX context1;
	MD4_CTX context2;
	unsigned char digest1[16];
	unsigned char digest2[16];
    qint64 fileSize;
	QString fileName;
	bool dohash;
public:
	QString ed2khashstr;
	void Init();
	void Update(unsigned char *input, unsigned int inputLen);
	void Final();
	int ed2khash(QString);
	std::string HexDigest();
	QString FileName();
    qint64 FileSize();
	ed2k();
	virtual void Debug(QString msg);
	struct ed2kfilestruct
	{
		QString filename;
        qint64 size;
		QString hexdigest;
	};

public slots:
	void getNotifyStopHasher();
signals:
	void notifyPartsDone(int, int);
	void notifyFileHashed(ed2k::ed2kfilestruct);
};

#endif // ED2K_H
