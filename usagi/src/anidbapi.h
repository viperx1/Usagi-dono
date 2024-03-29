#ifndef ANIDBAPI_H
#define ANIDBAPI_H

#include <QtNetwork>
#include <QHostAddress>
#include <QHostInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QtWidgets/QMessageBox>
#include <QElapsedTimer>
//#include <QTextCodec>
#include <bitset>
#include "hash/ed2k.h"
#include "Qt-AES-master/qaesencryption.h"


/*struct status_codes_
{
	const unsigned int STATUS_CRCOK = 0x01;
	const unsigned int STATUS_CRCERR = 0x02;
	const unsigned int STATUS_ISV2 = 0x04;
	const unsigned int STATUS_ISV3 = 0x08;
	const unsigned int STATUS_ISV4 = 0x10;
	const unsigned int STATUS_ISV5 = 0x20;
	const unsigned int STATUS_UNC = 0x40;
	const unsigned int STATUS_CEN = 0x80;
}status_codes;*/


class AniDBApi : public ed2k
{
	Q_OBJECT
private:
	/* === Settings Start */
    QByteArray aes_key;
//    QAESEncryption encryption(QAESEncryption::Aes::AES_256, QAESEncryption::ECB);
	QString username;
	QString password;
	QString lastdirectory;
    /* Settings End === */

	int protover; // AniDB API version = 3
	QString client; // = usagi
	int clientver; // = 1
	QString enc; // = utf8
	QHostAddress anidbaddr; // api.anidb.net
	int anidbport; // 9000

	QString SID; // session id
	char buf[20000];
	QString qbuf;
	bool loggedin;

	QHostInfo host;
	QUdpSocket *Socket;

	QSqlDatabase db;

	QString lastSentPacket;

//    QTextCodec *codec;
	enum progress
	{
		pSkip = 0,
		pDo = 1,
		pDone = 2,
        pFailed = 3
	};

	enum acodes
	{
		aEPISODE_TOTAL =			0x80000000,
		aEPISODE_LAST =				0x40000000,
		aANIME_YEAR =				0x20000000,
		aANIME_TYPE =				0x10000000,
		aANIME_RELATED_LIST =		0x08000000,
		aANIME_RELATED_TYPE =		0x04000000,
		aANIME_CATAGORY =			0x02000000,
		aANIME_NAME_ROMAJI =		0x00800000,
		aANIME_NAME_KANJI =			0x00400000,
		aANIME_NAME_ENGLISH =		0x00200000,
		aANIME_NAME_OTHER =			0x00100000,
		aANIME_NAME_SHORT =			0x00080000,
		aANIME_SYNONYMS =			0x00040000,
		aEPISODE_NUMBER =			0x00008000,
		aEPISODE_NAME =				0x00004000,
		aEPISODE_NAME_ROMAJI =		0x00002000,
		aEPISODE_NAME_KANJI =		0x00001000,
		aEPISODE_RATING =			0x00000800,
		aEPISODE_VOTE_COUNT =		0x00000400,
		aGROUP_NAME =				0x00000080,
		aGROUP_NAME_SHORT =			0x00000040,
        aDATE_AID_RECORD_UPDATED =	0x00000001
	};

	enum fcodes
	{
		fAID =				0x40000000,
		fEID =				0x20000000,
		fGID =				0x10000000,
		fLID =				0x08000000,
		fOTHEREPS =			0x04000000,
		fISDEPR =			0x02000000,
		fSTATE =			0x01000000,
		fSIZE =				0x00800000,
		fED2K =				0x00400000,
		fMD5 =				0x00200000,
		fSHA1 =				0x00100000,
		fCRC32 =			0x00080000,
		fQUALITY =			0x00008000,
		fSOURCE =			0x00004000,
		fCODEC_AUDIO =		0x00002000,
		fBITRATE_AUDIO =	0x00001000,
		fCODEC_VIDEO =		0x00000800,
		fBITRATE_VIDEO =	0x00000400,
		fRESOLUTION =		0x00000200,
		fFILETYPE =			0x00000100,
		fLANG_DUB =			0x00000080,
		fLANG_SUB =			0x00000040,
		fLENGTH =			0x00000020,
		fDESCRIPTION =		0x00000010,
		fAIRDATE =			0x00000008,
        fFILENAME =			0x00000001
	};

	//wxDatagramSocket *Socket; // UDP socket
	//wxIPV4address addrLocal; // local address/port
	//wxIPV4address addrPeer; // remote address/port

	// typedef unsigned short int2; // TODO: int to
	// typedef unsigned int int4;   // TODO:   string class
	// typedef bool boolean; // 0/1
	// str - String (UDP packet length restricts string size to 1400 bytes)
	// hexstr -- a hex representation of a decimal value, two characters per byte. If multiple bytes are represented, byte one is the first two characters of the string, byte two the next two, and so on.
protected:
    bool banned;
	QString bannedfor;
public:
	AniDBApi(QString client_, int clientver_);
	~AniDBApi();

	/* === Api Start */
	QString Auth();
	QString Logout();
    QString MylistAdd(qint64 size, QString ed2khash, int viewed, int state, QString storage, bool edit = 0);
    QString File(qint64, QString);
	/* Api End === */

	unsigned long LocalIdentify(int size, QString ed2khash);
	void UpdateFile(int size, QString ed2khash, int viewed, int state, QString storage);

	/* === Socket Start */
	int CreateSocket();
	QString ParseMessage(QString Message, QString ReplyTo, QString ReplyToMsg);
	int Send(QString, QString, QString);
    struct _waitingForReply
    {
        bool isWaiting;
        QElapsedTimer start;
    }waitingForReply;

	/* Socket End === */

	/* === Settings Start */
	QString getUsername();
	QString getPassword();
	QString getLastDirectory();
	void setUsername(QString);
	void setPassword(QString);
	void setLastDirectory(QString);
    /* Settings End === */

	QString GetSID();
    QString GetTag(QString);
	virtual void Debug(QString msg);
	virtual int LoginStatus();
	bool LoggedIn();
//	int SetUserData(QString username, QString password);
	QTimer *packetsender;
public slots:
	int SendPacket();
	int Recv();
signals:
	void notifyMylistAdd(QString, int);
    void notifyLoggedIn(QString, int);
    void notifyLoggedOut(QString, int);
};

#endif // ANIDBAPI_H
