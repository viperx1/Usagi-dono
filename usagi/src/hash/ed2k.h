#ifndef ED2K_H
#define ED2K_H

#include "md4.h"
#include <QString>
#include <QFileInfo>
#include <QFile>
#include <QMutex>

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
	
	// Global mutex for serializing file I/O across all threads
	// This improves performance on HDDs by preventing disk head thrashing
	static QMutex fileIOMutex;
	
	// Flag to enable/disable serialized I/O (useful for HDD vs SSD)
	static bool useSerializedIO;
	
protected:
	static qint64 calculateHashParts(qint64 fileSize);
public:
	// Enable or disable serialized I/O (default: false for backward compatibility)
	static void setSerializedIO(bool enabled);
	static bool getSerializedIO();
	QString ed2khashstr;
	void Init();
	void Update(unsigned char *input, unsigned int inputLen);
	void Final();
	virtual int ed2khash(QString);
	std::string HexDigest();
	QString FileName();
    qint64 FileSize();
    ed2k();
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
