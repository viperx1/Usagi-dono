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
#include <QTimer>
//#include <QTextCodec>
#include <bitset>
#include <zlib.h>
#include "hash/ed2k.h"
#include "Qt-AES-master/qaesencryption.h"
#include "mask.h"


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
	
	// Directory watcher settings
	bool watcherEnabled;
	QString watcherDirectory;
	bool watcherAutoStart;
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
	QString currentTag; // Track the tag of the currently pending request
	
	// Truncated response handling
	struct TruncatedResponseInfo {
		bool isTruncated;
		QString tag;
		QString command;
		int fieldsParsed;
		unsigned int fmaskReceived;
		unsigned int amaskReceived;
		TruncatedResponseInfo() : isTruncated(false), fieldsParsed(0), fmaskReceived(0), amaskReceived(0) {}
	} truncatedResponse;
	
	// Anime titles download and management
	QNetworkAccessManager *networkManager;
	QDateTime lastAnimeTitlesUpdate;
	
	// Export notification checking
	QTimer *notifyCheckTimer;
	bool isExportQueued;
	QString requestedExportTemplate; // Track which template was requested
	int notifyCheckAttempts;
	int notifyCheckIntervalMs; // Current check interval in milliseconds
	qint64 exportQueuedTimestamp; // Timestamp when export was queued
	
	void saveExportQueueState();
	void loadExportQueueState();
	void checkForExistingExport();

//    QTextCodec *codec;
	enum progress
	{
		pSkip = 0,
		pDo = 1,
		pDone = 2,
        pFailed = 3
	};

	// FILE command fmask (file data fields)
	enum file_fmask_codes
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

	// FILE command amask (anime/episode data fields returned with file)
	enum file_amask_codes
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

	// ANIME command amask (anime data fields)
	// Based on AniDB UDP API definition - byte-oriented mask (7 bytes total)
	// Mask bytes are sent as hex string, e.g., "80" for Byte 1 bit 7
	enum anime_amask_codes : uint64_t
	{
		// Byte 1 (first byte of mask) - bits 7-0 in 64-bit notation
		ANIME_AID =					0x0000000000000080ULL,  // Byte 1, bit 7
		ANIME_DATEFLAGS =			0x0000000000000040ULL,  // Byte 1, bit 6
		ANIME_YEAR =				0x0000000000000020ULL,  // Byte 1, bit 5
		ANIME_TYPE =				0x0000000000000010ULL,  // Byte 1, bit 4
		ANIME_RELATED_AID_LIST =	0x0000000000000008ULL,  // Byte 1, bit 3
		ANIME_RELATED_AID_TYPE =	0x0000000000000004ULL,  // Byte 1, bit 2
		// Byte 1, bits 1-0 are retired
		
		// Byte 2 (second byte of mask) - bits 15-8 in 64-bit notation
		ANIME_ROMAJI_NAME =			0x0000000000008000ULL,  // Byte 2, bit 7
		ANIME_KANJI_NAME =			0x0000000000004000ULL,  // Byte 2, bit 6
		ANIME_ENGLISH_NAME =		0x0000000000002000ULL,  // Byte 2, bit 5
		ANIME_OTHER_NAME =			0x0000000000001000ULL,  // Byte 2, bit 4
		ANIME_SHORT_NAME_LIST =		0x0000000000000800ULL,  // Byte 2, bit 3
		ANIME_SYNONYM_LIST =		0x0000000000000400ULL,  // Byte 2, bit 2
		// Byte 2, bits 1-0 are retired
		
		// Byte 3 (third byte of mask) - bits 23-16 in 64-bit notation
		ANIME_EPISODES =			0x0000000000800000ULL,  // Byte 3, bit 7
		ANIME_HIGHEST_EPISODE =		0x0000000000400000ULL,  // Byte 3, bit 6
		ANIME_SPECIAL_EP_COUNT =	0x0000000000200000ULL,  // Byte 3, bit 5
		ANIME_AIR_DATE =			0x0000000000100000ULL,  // Byte 3, bit 4
		ANIME_END_DATE =			0x0000000000080000ULL,  // Byte 3, bit 3
		ANIME_URL =					0x0000000000040000ULL,  // Byte 3, bit 2
		ANIME_PICNAME =				0x0000000000020000ULL,  // Byte 3, bit 1
		// Byte 3, bit 0 is retired
		
		// Byte 4 (fourth byte of mask) - bits 31-24 in 64-bit notation
		ANIME_RATING =				0x0000000080000000ULL,  // Byte 4, bit 7
		ANIME_VOTE_COUNT =			0x0000000040000000ULL,  // Byte 4, bit 6
		ANIME_TEMP_RATING =			0x0000000020000000ULL,  // Byte 4, bit 5
		ANIME_TEMP_VOTE_COUNT =		0x0000000010000000ULL,  // Byte 4, bit 4
		ANIME_AVG_REVIEW_RATING =	0x0000000008000000ULL,  // Byte 4, bit 3
		ANIME_REVIEW_COUNT =		0x0000000004000000ULL,  // Byte 4, bit 2
		ANIME_AWARD_LIST =			0x0000000002000000ULL,  // Byte 4, bit 1
		ANIME_IS_18_RESTRICTED =	0x0000000001000000ULL,  // Byte 4, bit 0
		
		// Byte 5 (fifth byte of mask) - bits 39-32 in 64-bit notation
		// Byte 5, bit 7 (0x0000008000000000ULL) is retired
		ANIME_ANN_ID =				0x0000004000000000ULL,  // Byte 5, bit 6
		ANIME_ALLCINEMA_ID =		0x0000002000000000ULL,  // Byte 5, bit 5
		ANIME_ANIMENFO_ID =			0x0000001000000000ULL,  // Byte 5, bit 4
		ANIME_TAG_NAME_LIST =		0x0000000800000000ULL,  // Byte 5, bit 3
		ANIME_TAG_ID_LIST =			0x0000000400000000ULL,  // Byte 5, bit 2
		ANIME_TAG_WEIGHT_LIST =		0x0000000200000000ULL,  // Byte 5, bit 1
		ANIME_DATE_RECORD_UPDATED =	0x0000000100000000ULL,  // Byte 5, bit 0
		
		// Byte 6 (sixth byte of mask) - bits 47-40 in 64-bit notation
		ANIME_CHARACTER_ID_LIST =	0x0000800000000000ULL,  // Byte 6, bit 7
		// Byte 6, bits 6-4 (0x0000400000000000ULL, 0x0000200000000000ULL, 0x0000100000000000ULL) are retired
		// Byte 6, bits 3-0 (0x0000080000000000ULL, 0x0000040000000000ULL, 0x0000020000000000ULL, 0x0000010000000000ULL) are unused
		
		// Byte 7 (seventh byte of mask) - bits 55-48 in 64-bit notation
		ANIME_SPECIALS_COUNT =		0x0080000000000000ULL,  // Byte 7, bit 7
		ANIME_CREDITS_COUNT =		0x0040000000000000ULL,  // Byte 7, bit 6
		ANIME_OTHER_COUNT =			0x0020000000000000ULL,  // Byte 7, bit 5
		ANIME_TRAILER_COUNT =		0x0010000000000000ULL,  // Byte 7, bit 4
		ANIME_PARODY_COUNT =		0x0008000000000000ULL   // Byte 7, bit 3
		// Byte 7, bits 2-0 (0x0004000000000000ULL, 0x0002000000000000ULL, 0x0001000000000000ULL) are unused
	};

	// Data structures for parsed responses
	struct FileData {
		QString fid, aid, eid, gid, lid;
		QString othereps, isdepr, state, size, ed2k, md5, sha1, crc;
		QString quality, source, codec_audio, bitrate_audio;
		QString codec_video, bitrate_video, resolution, filetype;
		QString lang_dub, lang_sub, length, description, airdate, filename;
	};
	
	struct AnimeData {
		// Byte 1 fields
		QString aid, dateflags, year, type;
		QString relaidlist, relaidtype;
		// Byte 2 fields
		QString nameromaji, namekanji, nameenglish, nameother, nameshort, synonyms;
		// Byte 3 fields
		QString episodes, highest_episode, special_ep_count;
		QString air_date, end_date, url, picname;
		// Byte 4 fields
		QString rating, vote_count, temp_rating, temp_vote_count;
		QString avg_review_rating, review_count, award_list, is_18_restricted;
		// Byte 5 fields
		QString ann_id, allcinema_id, animenfo_id;
		QString tag_name_list, tag_id_list, tag_weight_list, date_record_updated;
		// Byte 6 fields
		QString character_id_list;
		// Byte 7 fields
		QString specials_count, credits_count, other_count, trailer_count, parody_count;
		// Legacy fields for backward compatibility
		QString eptotal, eplast, category;
	};
	
	struct EpisodeData {
		QString eid, epno, epname, epnameromaji, epnamekanji, eprating, epvotecount;
	};
	
	struct GroupData {
		QString gid, groupname, groupshortname;
	};

	// Helper methods for mask processing
	FileData parseFileMask(const QStringList& tokens, unsigned int fmask, int& index);
	AnimeData parseFileAmaskAnimeData(const QStringList& tokens, unsigned int amask, int& index);
	EpisodeData parseFileAmaskEpisodeData(const QStringList& tokens, unsigned int amask, int& index);
	GroupData parseFileAmaskGroupData(const QStringList& tokens, unsigned int amask, int& index);
	AnimeData parseMask(const QStringList& tokens, uint64_t amask, int& index);
	AnimeData parseMaskFromString(const QStringList& tokens, const QString& amaskHexString, int& index);
	AnimeData parseMaskFromString(const QStringList& tokens, const QString& amaskHexString, int& index, QByteArray& parsedMaskBytes);
	Mask calculateReducedMask(const Mask& originalMask, const QByteArray& parsedMaskBytes);
	
	void storeFileData(const FileData& data);
	void storeAnimeData(const AnimeData& data);
	void storeEpisodeData(const EpisodeData& data);
	void storeGroupData(const GroupData& data);
	
	bool extractMasksFromCommand(const QString& command, unsigned int& fmask, unsigned int& amask);

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
	// LocalIdentify return bitset indices
	enum LocalIdentifyBits
	{
		LI_FILE_IN_DB = 0,		// Bit 0: File exists in local 'file' table (has valid fid)
		LI_FILE_IN_MYLIST = 1	// Bit 1: File exists in local 'mylist' table (has valid lid)
	};
	AniDBApi(QString client_, int clientver_);
	~AniDBApi();

	/* === Hash function override === */
	int ed2khash(QString filepath) override;

	/* === Api Start */
	QString Auth();
	QString Logout();
    QString MylistAdd(qint64 size, QString ed2khash, int viewed, int state, QString storage, bool edit = 0);
    QString Mylist(int lid = -1);
    QString File(qint64, QString);
	QString PushAck(int nid);
	QString NotifyEnable();
	QString NotifyGet(int nid);
	QString MylistExport(QString template_name = "xml-plain-cs");
	QString Episode(int eid);
	QString Anime(int aid);
	
	// Command builders - return formatted command strings for testing
	QString buildAuthCommand(QString username, QString password, int protover, QString client, int clientver, QString enc);
	QString buildLogoutCommand();
	QString buildMylistAddCommand(qint64 size, QString ed2khash, int viewed, int state, QString storage, bool edit);
	QString buildMylistCommand(int lid);
	QString buildMylistStatsCommand();
	QString buildFileCommand(qint64 size, QString ed2k, unsigned int fmask, unsigned int amask);
	QString buildPushAckCommand(int nid);
	QString buildNotifyListCommand();
	QString buildNotifyGetCommand(int nid);
	QString buildMylistExportCommand(QString template_name);
	QString buildEpisodeCommand(int eid);
	QString buildAnimeCommand(int aid);
	/* Api End === */

	/**
	 * Check if a file exists in the local database.
	 * 
	 * @param size File size in bytes
	 * @param ed2khash ED2K hash of the file
	 * @return std::bitset<2> where:
	 *         - bit[LI_FILE_IN_DB] (0): true if file exists in local 'file' table
	 *         - bit[LI_FILE_IN_MYLIST] (1): true if file exists in local 'mylist' table
	 */
	std::bitset<2> LocalIdentify(int size, QString ed2khash);
	QMap<QString, std::bitset<2>> batchLocalIdentify(const QList<QPair<qint64, QString>>& sizeHashPairs);
	void UpdateFile(int size, QString ed2khash, int viewed, int state, QString storage);
	int UpdateLocalPath(QString tag, QString localPath);
	int LinkLocalFileToMylist(qint64 size, QString ed2kHash, QString localPath);
	void UpdateLocalFileStatus(QString localPath, int status);
	void updateLocalFileHash(QString localPath, QString ed2kHash, int status);
	void batchUpdateLocalFileHashes(const QList<QPair<QString, QString>>& pathHashPairs, int status);
	QString getLocalFileHash(QString localPath);
	
	struct FileHashInfo {
		QString path;
		QString hash;
		int status;
	};
	QMap<QString, FileHashInfo> batchGetLocalFileHashes(const QStringList& filePaths);

	/* === Socket Start */
	int CreateSocket();
	QString ParseMessage(QString Message, QString ReplyTo, QString ReplyToMsg, bool isTruncated = false);
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
	
	// Directory watcher settings
	bool getWatcherEnabled();
	QString getWatcherDirectory();
	bool getWatcherAutoStart();
	void setWatcherEnabled(bool enabled);
	void setWatcherDirectory(QString directory);
	void setWatcherAutoStart(bool autoStart);
    /* Settings End === */
	
	/* === Anime Titles Start */
	void downloadAnimeTitles();
	bool shouldUpdateAnimeTitles();
	void parseAndStoreAnimeTitles(const QByteArray &data);
	/* Anime Titles End === */

	QString GetSID();
	QString GetRequestedExportTemplate(); // Get the template name that was requested for export
    QString GetTag(QString);
	virtual int LoginStatus();
	bool LoggedIn();
//	int SetUserData(QString username, QString password);
	QTimer *packetsender;
public slots:
	int SendPacket();
	int Recv();
	void checkForNotifications();
private slots:
	void onAnimeTitlesDownloaded(QNetworkReply *reply);
signals:
	void notifyMylistAdd(QString, int);
    void notifyLoggedIn(QString, int);
    void notifyLoggedOut(QString, int);
	void notifyMessageReceived(int nid, QString message);
	void notifyCheckStarting(int count);
	void notifyExportQueued(QString tag);
	void notifyExportAlreadyInQueue(QString tag);
	void notifyExportNoSuchTemplate(QString tag);
	void notifyEpisodeUpdated(int eid, int aid);
	void notifyAnimeUpdated(int aid);
};

#endif // ANIDBAPI_H
