// API definition available at https://wiki.anidb.net/UDP_API_Definition
#include "anidbapi.h"
#include "logger.h"
#include <cmath>
#include <map>
#include <QThread>
#include <QElapsedTimer>
#include <QRegularExpression>

AniDBApi::AniDBApi(QString client_, int clientver_)
{
	Logger::log("[AniDB Init] Constructor started", __FILE__, __LINE__);
	protover = 3;
	client = client_; //"usagi";
	clientver = clientver_; //1;
	enc = "utf8";
	
	// Skip DNS lookup in test mode to avoid network-related crashes on Windows
	if (qgetenv("USAGI_TEST_MODE") != "1")
	{
		Logger::log("[AniDB Init] Starting DNS lookup for api.anidb.net (this may block)", __FILE__, __LINE__);
		host = QHostInfo::fromName("api.anidb.net");
		Logger::log("[AniDB Init] DNS lookup completed", __FILE__, __LINE__);
		if(!host.addresses().isEmpty())
		{
			anidbaddr.setAddress(host.addresses().first().toIPv4Address());
			Logger::log("[AniDB Init] DNS resolved successfully to " + host.addresses().first().toString(), __FILE__, __LINE__);
		}
		else
		{
			// Fallback to a default IP or leave uninitialized
			// DNS resolution failed, socket operations will be skipped
			Logger::log("[AniDB Error] DNS resolution for api.anidb.net failed", __FILE__, __LINE__);
		}
	}
	else
	{
		Logger::log("[AniDB Init] Test mode: Skipping DNS lookup for api.anidb.net", __FILE__, __LINE__);
	}
	
	anidbport = 9000;
	loggedin = 0;
	Socket = nullptr;
	currentTag = ""; // Initialize current tag tracker

	Logger::log("[AniDB Init] Setting up database connection", __FILE__, __LINE__);
	// Check if default database connection already exists (e.g., in tests)
	if(QSqlDatabase::contains(QSqlDatabase::defaultConnection))
	{
		db = QSqlDatabase::database();
	}
	else
	{
		db = QSqlDatabase::addDatabase("QSQLITE");
	}

	// Only set database name if it hasn't been set already (e.g., by tests)
	if(db.databaseName().isEmpty())
	{
		QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
		QDir().mkpath(path); // make sure it exists
		QString dbPath = path + "/usagi.sqlite";
		
		db.setDatabaseName(dbPath);
	}
	QSqlQuery query;

    aes_key = "8fsd789f7sd7f6sd78695g35345g34gf4";

	Logger::log("[AniDB Init] Opening database", __FILE__, __LINE__);
	if(db.open())
	{
		Logger::log("[AniDB Init] Database opened, starting transaction", __FILE__, __LINE__);
		db.transaction();
//		Debug("AniDBApi: Database opened");
		query = QSqlQuery(db);
		query.exec("CREATE TABLE IF NOT EXISTS `mylist`(`lid` INTEGER PRIMARY KEY, `fid` INTEGER, `eid` INTEGER, `aid` INTEGER, `gid` INTEGER, `date` INTEGER, `state` INTEGER, `viewed` INTEGER, `viewdate` INTEGER, `storage` TEXT, `source` TEXT, `other` TEXT, `filestate` INTEGER)");
		query.exec("CREATE TABLE IF NOT EXISTS `anime`(`aid` INTEGER PRIMARY KEY, `eptotal` INTEGER, `eps` INTEGER, `eplast` INTEGER, `year` TEXT, `type` TEXT, `relaidlist` TEXT, `relaidtype` TEXT, `category` TEXT, `nameromaji` TEXT, `namekanji` TEXT, `nameenglish` TEXT, `nameother` TEXT, `nameshort` TEXT, `synonyms` TEXT, `typename` TEXT, `startdate` TEXT, `enddate` TEXT);");
        query.exec("CREATE TABLE IF NOT EXISTS `file`(`fid` INTEGER PRIMARY KEY, `aid` INTEGER, `eid` INTEGER, `gid` INTEGER, `lid` INTEGER, `othereps` TEXT, `isdepr` INTEGER, `state` INTEGER, `size` BIGINT, `ed2k` TEXT, `md5` TEXT, `sha1` TEXT, `crc` TEXT, `quality` TEXT, `source` TEXT, `codec_audio` TEXT, `bitrate_audio` INTEGER, `codec_video` TEXT, `bitrate_video` INTEGER, `resolution` TEXT, `filetype` TEXT, `lang_dub` TEXT, `lang_sub` TEXT, `length` INTEGER, `description` TEXT, `airdate` INTEGER, `filename` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `episode`(`eid` INTEGER PRIMARY KEY, `name` TEXT, `nameromaji` TEXT, `namekanji` TEXT, `rating` INTEGER, `votecount` INTEGER, `epno` TEXT);");
		// Add epno column if it doesn't exist (for existing databases)
		query.exec("ALTER TABLE `episode` ADD COLUMN `epno` TEXT");
		// Add eps column if it doesn't exist (for existing databases)
		query.exec("ALTER TABLE `anime` ADD COLUMN `eps` INTEGER");
		// Add typename, startdate, enddate columns if they don't exist (for existing databases)
		query.exec("ALTER TABLE `anime` ADD COLUMN `typename` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `startdate` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `enddate` TEXT");
		// Create local_files table for directory watcher feature
		// Status: 0=not hashed, 1=hashed but not checked by API, 2=in anidb, 3=not in anidb
		query.exec("CREATE TABLE IF NOT EXISTS `local_files`(`id` INTEGER PRIMARY KEY AUTOINCREMENT, `path` TEXT UNIQUE, `filename` TEXT, `status` INTEGER DEFAULT 0, `ed2k_hash` TEXT)");
		// Add ed2k_hash column to local_files if it doesn't exist (for existing databases)
		query.exec("ALTER TABLE `local_files` ADD COLUMN `ed2k_hash` TEXT");
		// Add local_file column to mylist if it doesn't exist (references local_files.id)
		query.exec("ALTER TABLE `mylist` ADD COLUMN `local_file` INTEGER");
		query.exec("CREATE TABLE IF NOT EXISTS `group`(`gid` INTEGER PRIMARY KEY, `name` TEXT, `shortname` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `anime_titles`(`aid` INTEGER, `type` INTEGER, `language` TEXT, `title` TEXT, PRIMARY KEY(`aid`, `type`, `language`, `title`));");
		query.exec("CREATE TABLE IF NOT EXISTS `packets`(`tag` INTEGER PRIMARY KEY, `str` TEXT, `processed` BOOL DEFAULT 0, `sendtime` INTEGER, `got_reply` BOOL DEFAULT 0, `reply` TEXT, `retry_count` INTEGER DEFAULT 0);");
		// Add retry_count column to packets if it doesn't exist (for existing databases)
		query.exec("ALTER TABLE `packets` ADD COLUMN `retry_count` INTEGER DEFAULT 0");
		query.exec("CREATE TABLE IF NOT EXISTS `settings`(`id` INTEGER PRIMARY KEY, `name` TEXT UNIQUE, `value` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `notifications`(`nid` INTEGER PRIMARY KEY, `type` TEXT, `from_user_id` INTEGER, `from_user_name` TEXT, `date` INTEGER, `message_type` INTEGER, `title` TEXT, `body` TEXT, `received_at` INTEGER, `acknowledged` BOOL DEFAULT 0);");
		query.exec("UPDATE `packets` SET `processed` = 1 WHERE `processed` = 0;");
		
		// Add playback tracking columns to mylist if they don't exist
		query.exec("ALTER TABLE `mylist` ADD COLUMN `playback_position` INTEGER DEFAULT 0");
		query.exec("ALTER TABLE `mylist` ADD COLUMN `playback_duration` INTEGER DEFAULT 0");
		query.exec("ALTER TABLE `mylist` ADD COLUMN `last_played` INTEGER DEFAULT 0");
		
		Logger::log("[AniDB Init] Committing database transaction", __FILE__, __LINE__);
		db.commit();
		Logger::log("[AniDB Init] Database transaction committed", __FILE__, __LINE__);
	}
//	QStringList names = QStringList()<<"username"<<"password";
	Logger::log("[AniDB Init] Reading settings from database", __FILE__, __LINE__);
	
	// Execute SELECT query after transaction commit to read settings
	query.exec("SELECT `name`, `value` FROM `settings` ORDER BY `name` ASC");
	
	// Initialize directory watcher settings with defaults
	watcherEnabled = false;
	watcherDirectory = QString();
	watcherAutoStart = false;
	
	while(query.next())
	{
		if(query.value(0).toString() == "username")
		{
			username = query.value(1).toString();
		}
		if(query.value(0).toString() == "password")
		{
			password = query.value(1).toString();
		}
		if(query.value(0).toString() == "lastdirectory")
		{
			lastdirectory = query.value(1).toString();
		}
		if(query.value(0).toString() == "last_anime_titles_update")
		{
			lastAnimeTitlesUpdate = QDateTime::fromSecsSinceEpoch(query.value(1).toLongLong());
		}
		if(query.value(0).toString() == "watcherEnabled")
		{
			watcherEnabled = (query.value(1).toString() == "1");
		}
		if(query.value(0).toString() == "watcherDirectory")
		{
			watcherDirectory = query.value(1).toString();
		}
		if(query.value(0).toString() == "watcherAutoStart")
		{
			watcherAutoStart = (query.value(1).toString() == "1");
		}
	}

	// Initialize network manager for anime titles download
	Logger::log("[AniDB Init] Initializing network manager", __FILE__, __LINE__);
	networkManager = new QNetworkAccessManager(this);
	connect(networkManager, &QNetworkAccessManager::finished, this, &AniDBApi::onAnimeTitlesDownloaded);

	Logger::log("[AniDB Init] Setting up packet sender timer", __FILE__, __LINE__);
	packetsender = new QTimer();
	connect(packetsender, SIGNAL(timeout()), this, SLOT(SendPacket()));

	packetsender->setInterval(2100);
	packetsender->start();
	
	// Initialize notification checking timer
	Logger::log("[AniDB Init] Setting up notification check timer", __FILE__, __LINE__);
	notifyCheckTimer = new QTimer();
	connect(notifyCheckTimer, SIGNAL(timeout()), this, SLOT(checkForNotifications()));
	isExportQueued = false;
	requestedExportTemplate = QString(); // No template requested yet
	notifyCheckAttempts = 0;
	notifyCheckIntervalMs = 60000; // Start with 1 minute
	exportQueuedTimestamp = 0;
	
	// Load any persisted export queue state from previous session
	loadExportQueueState();

	// Check and download anime titles if needed (automatically on startup)
	Logger::log("[AniDB Init] Checking if anime titles need update", __FILE__, __LINE__);
	if(shouldUpdateAnimeTitles())
	{
		Logger::log("[AniDB Init] Starting anime titles download", __FILE__, __LINE__);
		downloadAnimeTitles();
	}
	else
	{
		Logger::log("[AniDB Init] Anime titles are up to date, skipping download", __FILE__, __LINE__);
	}

	Logger::log("[AniDB Init] Constructor completed successfully", __FILE__, __LINE__);

//	codec = QTextCodec::codecForName("UTF-8");
}

AniDBApi::~AniDBApi()
{
	// Clean up the UDP socket to prevent memory leaks
	if(Socket != nullptr)
	{
		delete Socket;
		Socket = nullptr;
	}
}

int AniDBApi::ed2khash(QString filepath)
{
	// Check if we already have a hash for this file in the database
	QString existingHash = getLocalFileHash(filepath);
	
	if (!existingHash.isEmpty())
	{
		// We have an existing hash, reuse it
		LOG(QString("Reusing existing hash for file: %1").arg(filepath));
		
		// Get file info to populate the struct
		QFileInfo fileinfo(filepath);
		
		// Verify the file still exists before reusing the hash
		if (!fileinfo.exists())
		{
			LOG(QString("File no longer exists: %1 - delegating to base class which will return error code 2").arg(filepath));
			// Fall through to let the base class handle the missing file case
		}
		else
		{
			// Calculate the number of parts for this file
			qint64 fileSize = fileinfo.size();
			qint64 numParts = calculateHashParts(fileSize);
			
			// For pre-hashed files, emit only completion signal to avoid flooding the UI event queue
			// Emitting thousands of signals in a tight loop causes UI freeze even with throttling
			// The throttling in getNotifyPartsDone() controls UI updates but not signal processing
			// So we emit only the final signal to indicate completion
			emit notifyPartsDone(numParts, numParts);
			
			// Populate the hash data structure with the existing hash
			ed2kfilestruct hash;
			hash.filename = fileinfo.fileName();
			hash.size = fileSize;
			hash.hexdigest = existingHash;
			
			// Emit the signal as if we just computed the hash
			emit notifyFileHashed(hash);
			
			// Set ed2khashstr for compatibility
			ed2khashstr = QString("ed2k://|file|%1|%2|%3|/").arg(hash.filename).arg(hash.size).arg(existingHash);
			
			return 1; // Success
		}
	}
	
	// No existing hash, or file doesn't exist - compute it using the parent class method
	return ed2k::ed2khash(filepath);
}

int AniDBApi::CreateSocket()
{
	if(Socket != nullptr)
	{
		LOG("AniDBApi: Socket already created");
		return 1;
	}
 	Socket = new QUdpSocket;
 	if(!Socket->bind(QHostAddress::Any, 3962))
 	{
		LOG("AniDBApi: Can't bind socket");
		LOG("AniDBApi: " + Socket->errorString());
		// Clean up the socket if binding failed
		delete Socket;
		Socket = nullptr;
		return 0;
 	}
 	else
 	{
		if(Socket->isValid())
	 	{
			LOG("AniDBApi: UDP socket created");
		}
		else
		{
			LOG("AniDBApi: ERROR: failed to create UDP socket");
			LOG("AniDBApi: " + Socket->errorString());
			// Clean up the socket if it's invalid
			delete Socket;
			Socket = nullptr;
			return 0;
		}
	}
	Socket->connectToHost(anidbaddr, anidbport);
/*	if(Socket->error())
	{
		LOG("AniDBApi: " + Socket->errorString());
		return 0;
	}*/
	connect(Socket, SIGNAL(readyRead()), this, SLOT(Recv()));
	return 1;
}

QString AniDBApi::ParseMessage(QString Message, QString ReplyTo, QString ReplyToMsg, bool isTruncated)
{
	if(Message.length() == 0)
	{
        LOG("AniDBApi: ParseMessage: Message empty");
		return 0;
	}
//    Debug("AniDBApi: ParseMessage: " + Message);

	QStringList token = Message.split(" ");

	QString Tag = token.first();
	token.pop_front();

    QString ReplyID = token.first();//.first();//Message.Mid(0, Message.Find(" "));

	// Handle cases where AniDB responds without a tag (e.g., "598 UNKNOWN COMMAND")
	// In this case, what we parsed as Tag is actually the ReplyID
	// This can happen when the command is so malformed that the server cannot extract the tag
	bool tagIsNumeric = false;
	Tag.toInt(&tagIsNumeric);
	
	if(tagIsNumeric && token.size() > 0 && !ReplyID.isEmpty())
	{
		// Check if ReplyID looks like a text response rather than a numeric code
		bool replyIsNumeric = false;
		ReplyID.toInt(&replyIsNumeric);
		
		if(!replyIsNumeric)
		{
			// What we thought was the Tag is actually the ReplyID
			// Common case: "598 UNKNOWN COMMAND" becomes Tag="598", ReplyID="UNKNOWN"
			ReplyID = Tag;
			Tag = "0"; // Use default tag since none was provided
			Logger::log("[AniDB Response] Tagless response detected - Tag: " + Tag + " ReplyID: " + ReplyID, __FILE__, __LINE__);
		}
		else
		{
			Logger::log("[AniDB Response] Tag: " + Tag + " ReplyID: " + ReplyID, __FILE__, __LINE__);
		}
	}
	else
	{
		Logger::log("[AniDB Response] Tag: " + Tag + " ReplyID: " + ReplyID, __FILE__, __LINE__);
	}
	
	// Log truncation status
	if(isTruncated)
	{
		Logger::log("[AniDB Response] TRUNCATED response detected for Tag: " + Tag + " ReplyID: " + ReplyID, __FILE__, __LINE__);
	}

	token.pop_front();

	if(ReplyID == "200"){ // 200 {str session_key} LOGIN ACCEPTED
		SID = token.first();
		loggedin = 1;
        notifyLoggedIn(Tag, 200);
	}
	else if(ReplyID == "201"){ // 201 {str session_key} LOGIN ACCEPTED - NEW VERSION AVAILABLE
		SID = token.first();
		loggedin = 1;
        notifyLoggedIn(Tag, 201);
	}
    else if(ReplyID == "203"){ // 203 LOGGED OUT
        Logger::log("[AniDB Response] 203 LOGGED OUT - Tag: " + Tag, __FILE__, __LINE__);
		loggedin = 0;
        notifyLoggedOut(Tag, 203);
	}
	else if(ReplyID == "210"){ // 210 MYLIST ENTRY ADDED
		// Parse lid from response message
		QStringList token2 = Message.split("\n");
		token2.pop_front(); // Remove the status line
		QString lid = token2.first().trimmed();
		
		// Get the original MYLISTADD command from packets table
		QString q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(Tag);
		QSqlQuery query(db);
		if(query.exec(q) && query.next())
		{
			QString mylistAddCmd = query.value(0).toString();
			
			// Parse parameters from the MYLISTADD command
			// Format: MYLISTADD size=X&ed2k=Y&viewed=Z&state=W&storage=S
			QStringList params = mylistAddCmd.split("&");
			QString size, ed2k, viewed = "0", state = "0", storage = "";
			
			for(const QString& param : params)
			{
				if(param.contains("size="))
					size = param.mid(param.indexOf("size=") + 5).split("&").first();
				else if(param.contains("ed2k="))
					ed2k = param.mid(param.indexOf("ed2k=") + 5).split("&").first();
				else if(param.contains("viewed="))
					viewed = param.mid(param.indexOf("viewed=") + 7).split("&").first();
				else if(param.contains("state="))
					state = param.mid(param.indexOf("state=") + 6).split("&").first();
				else if(param.contains("storage="))
					storage = param.mid(param.indexOf("storage=") + 8).split("&").first();
			}
			
			// Look up file info (fid, eid, aid, gid) from file table using size and ed2k
			QString fid, eid, aid, gid;
			q = QString("SELECT `fid`, `eid`, `aid`, `gid` FROM `file` WHERE `size` = '%1' AND `ed2k` = '%2'")
				.arg(size).arg(ed2k);
			QSqlQuery fileQuery(db);
			if(fileQuery.exec(q) && fileQuery.next())
			{
				fid = fileQuery.value(0).toString();
				eid = fileQuery.value(1).toString();
				aid = fileQuery.value(2).toString();
				gid = fileQuery.value(3).toString();
				
				// Insert into mylist table, preserving local_file and playback data if they exist
				q = QString("INSERT OR REPLACE INTO `mylist` "
					"(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`, `local_file`, `playback_position`, `playback_duration`, `last_played`) "
					"VALUES (%1, %2, %3, %4, %5, %6, %7, '%8', "
					"(SELECT `local_file` FROM `mylist` WHERE `lid` = %1), "
					"COALESCE((SELECT `playback_position` FROM `mylist` WHERE `lid` = %1), 0), "
					"COALESCE((SELECT `playback_duration` FROM `mylist` WHERE `lid` = %1), 0), "
					"COALESCE((SELECT `last_played` FROM `mylist` WHERE `lid` = %1), 0))")
					.arg(lid)
					.arg(fid.isEmpty() ? "0" : fid)
					.arg(eid.isEmpty() ? "0" : eid)
					.arg(aid.isEmpty() ? "0" : aid)
					.arg(gid.isEmpty() ? "0" : gid)
					.arg(state)
					.arg(viewed)
					.arg(QString(storage).replace("'", "''"));
				
				QSqlQuery insertQuery(db);
				if(!insertQuery.exec(q))
				{
					LOG("Failed to insert mylist entry: " + insertQuery.lastError().text());
				}
				else
				{
					LOG(QString("Successfully added mylist entry - lid=%1, fid=%2").arg(lid).arg(fid));
				}
			}
			else
			{
				LOG("Could not find file info for size=" + size + " ed2k=" + ed2k);
			}
		}
		
		notifyMylistAdd(Tag, 210);
	}
	else if(ReplyID == "217"){ // 217 EXPORT QUEUED
		Logger::log("[AniDB Response] 217 EXPORT QUEUED - Tag: " + Tag, __FILE__, __LINE__);
		// Export has been queued and will be generated by AniDB
		// When ready, a notification will be sent with the download link
		
		// Start periodic notification checking
		isExportQueued = true;
		notifyCheckAttempts = 0;
		notifyCheckIntervalMs = 60000; // Start with 1 minute
		exportQueuedTimestamp = QDateTime::currentSecsSinceEpoch();
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		notifyCheckTimer->start();
		Logger::log("[AniDB Export] Started periodic notification checking (every 1 minute initially)", __FILE__, __LINE__);
		
		// Save state to persist across restarts
		saveExportQueueState();
		
		emit notifyExportQueued(Tag);
	}
	else if(ReplyID == "218"){ // 218 EXPORT CANCELLED
		Logger::log("[AniDB Response] 218 EXPORT CANCELLED - Tag: " + Tag, __FILE__, __LINE__);
		// Reply to cancel=1 parameter - export was cancelled
	}
	else if(ReplyID == "317"){ // 317 EXPORT NO SUCH TEMPLATE
		Logger::log("[AniDB Response] 317 EXPORT NO SUCH TEMPLATE - Tag: " + Tag, __FILE__, __LINE__);
		// The requested template name does not exist
		// Valid templates: xml-plain-cs, xml, csv, json, anidb
		emit notifyExportNoSuchTemplate(Tag);
	}
	else if(ReplyID == "318"){ // 318 EXPORT ALREADY IN QUEUE
		Logger::log("[AniDB Response] 318 EXPORT ALREADY IN QUEUE - Tag: " + Tag, __FILE__, __LINE__);
		// An export is already queued - cannot queue another until current one completes
		// Client should wait for notification when current export is ready
		emit notifyExportAlreadyInQueue(Tag);
	}
	else if(ReplyID == "319"){ // 319 EXPORT NO EXPORT QUEUED OR IS PROCESSING
		Logger::log("[AniDB Response] 319 EXPORT NO EXPORT QUEUED OR IS PROCESSING - Tag: " + Tag, __FILE__, __LINE__);
		// Reply to cancel=1 when there's no export to cancel
		// No export is currently queued or being processed
	}
	else if(ReplyID == "220"){ // 220 FILE
		// Get the original FILE command to extract masks
		QString q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(Tag);
		QSqlQuery query(db);
		unsigned int fmask = 0;
		unsigned int amask = 0;
		
		if(query.exec(q) && query.next())
		{
			QString fileCmd = query.value(0).toString();
			if(!extractMasksFromCommand(fileCmd, fmask, amask))
			{
				LOG("Failed to extract masks from FILE command for Tag: " + Tag);
				// Fall back to default masks (all fields)
				fmask = fAID | fEID | fGID | fLID | fOTHEREPS | fISDEPR | fSTATE | fSIZE | fED2K | fMD5 | fSHA1 |
						fCRC32 | fQUALITY | fSOURCE | fCODEC_AUDIO | fBITRATE_AUDIO | fCODEC_VIDEO | fBITRATE_VIDEO |
						fRESOLUTION | fFILETYPE | fLANG_DUB | fLANG_SUB | fLENGTH | fDESCRIPTION | fAIRDATE | fFILENAME;
				amask = aEPISODE_TOTAL | aEPISODE_LAST | aANIME_YEAR | aANIME_TYPE | aANIME_RELATED_LIST |
						aANIME_RELATED_TYPE | aANIME_CATAGORY | aANIME_NAME_ROMAJI | aANIME_NAME_KANJI |
						aANIME_NAME_ENGLISH | aANIME_NAME_OTHER | aANIME_NAME_SHORT | aANIME_SYNONYMS |
						aEPISODE_NUMBER | aEPISODE_NAME | aEPISODE_NAME_ROMAJI | aEPISODE_NAME_KANJI |
						aEPISODE_RATING | aEPISODE_VOTE_COUNT | aGROUP_NAME | aGROUP_NAME_SHORT |
						aDATE_AID_RECORD_UPDATED;
			}
		}
		else
		{
			LOG("Could not find packet for Tag: " + Tag);
			// Use default masks
			fmask = fAID | fEID | fGID | fLID | fOTHEREPS | fISDEPR | fSTATE | fSIZE | fED2K | fMD5 | fSHA1 |
					fCRC32 | fQUALITY | fSOURCE | fCODEC_AUDIO | fBITRATE_AUDIO | fCODEC_VIDEO | fBITRATE_VIDEO |
					fRESOLUTION | fFILETYPE | fLANG_DUB | fLANG_SUB | fLENGTH | fDESCRIPTION | fAIRDATE | fFILENAME;
			amask = aEPISODE_TOTAL | aEPISODE_LAST | aANIME_YEAR | aANIME_TYPE | aANIME_RELATED_LIST |
					aANIME_RELATED_TYPE | aANIME_CATAGORY | aANIME_NAME_ROMAJI | aANIME_NAME_KANJI |
					aANIME_NAME_ENGLISH | aANIME_NAME_OTHER | aANIME_NAME_SHORT | aANIME_SYNONYMS |
					aEPISODE_NUMBER | aEPISODE_NAME | aEPISODE_NAME_ROMAJI | aEPISODE_NAME_KANJI |
					aEPISODE_RATING | aEPISODE_VOTE_COUNT | aGROUP_NAME | aGROUP_NAME_SHORT |
					aDATE_AID_RECORD_UPDATED;
		}
		
		// Parse response using mask-aware parsing
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		token2 = token2.first().split("|");
		
		// Handle truncated responses - remove the last field as it's likely incomplete
		if(isTruncated && token2.size() > 0)
		{
			Logger::log(QString("[AniDB Response] 220 FILE - Truncated response, removing last field (was: '%1')")
				.arg(token2.last()), __FILE__, __LINE__);
			Logger::log(QString("[AniDB Response] 220 FILE - Original field count: %1, processing %2 fields")
				.arg(token2.size()).arg(token2.size() - 1), __FILE__, __LINE__);
			token2.removeLast();
		}
		
		// FID is always the first field in FILE responses
		int index = 1;  // Start parsing after FID
		
		// Parse file data using fmask
		FileData fileData = parseFileMask(token2, fmask, index);
		
		// Parse anime data using amask
		AnimeData animeData = parseFileAmaskAnimeData(token2, amask, index);
		
		// Parse episode data using amask
		EpisodeData episodeData = parseFileAmaskEpisodeData(token2, amask, index);
		
		// Parse group data using amask
		GroupData groupData = parseFileAmaskGroupData(token2, amask, index);
		
		// Store all parsed data
		storeFileData(fileData);
		
		if(!animeData.aid.isEmpty())
		{
			animeData.aid = fileData.aid;  // Ensure aid is set from file data
			storeAnimeData(animeData);
		}
		
		if(!episodeData.eid.isEmpty())
		{
			episodeData.eid = fileData.eid;  // Ensure eid is set from file data
			storeEpisodeData(episodeData);
		}
		
		if(!groupData.gid.isEmpty())
		{
			groupData.gid = fileData.gid;  // Ensure gid is set from file data
			storeGroupData(groupData);
		}
		
		// Handle truncated response - log warning about incomplete data
		if(isTruncated)
		{
			Logger::log(QString("[AniDB Response] 220 FILE - WARNING: Response was truncated, some fields may be missing. "
				"Processed %1 fields successfully.").arg(index), __FILE__, __LINE__);
		}
		
		// Check if episode data is missing (no epno) and queue EPISODE API request
		if(!fileData.eid.isEmpty() && fileData.eid != "0" && episodeData.epno.isEmpty())
		{
			LOG(QString("Episode data incomplete for EID %1, queuing EPISODE API request").arg(fileData.eid));
			Episode(fileData.eid.toInt());
		}
	}
	else if(ReplyID == "221"){ // 221 MYLIST
		// Get the original MYLIST command to extract the lid parameter
		QString q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(Tag);
		QSqlQuery query(db);
		QString lid;
		
		if(query.exec(q) && query.next())
		{
			QString mylistCmd = query.value(0).toString();
			// Extract lid from command like "MYLIST lid=12345"
			int lidStart = mylistCmd.indexOf("lid=");
			if(lidStart != -1)
			{
				lidStart += 4; // Move past "lid="
				int lidEnd = mylistCmd.indexOf("&", lidStart);
				if(lidEnd == -1)
					lidEnd = mylistCmd.indexOf(" ", lidStart);
				if(lidEnd == -1)
					lidEnd = mylistCmd.length();
				lid = mylistCmd.mid(lidStart, lidEnd - lidStart);
			}
		}
		
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		token2 = token2.first().split("|");
		
		// Handle truncated responses - remove the last field as it's likely incomplete
		if(isTruncated && token2.size() > 0)
		{
			Logger::log(QString("[AniDB Response] 221 MYLIST - Truncated response, removing last field (was: '%1')")
				.arg(token2.last()), __FILE__, __LINE__);
			Logger::log(QString("[AniDB Response] 221 MYLIST - Original field count: %1, processing %2 fields")
				.arg(token2.size()).arg(token2.size() - 1), __FILE__, __LINE__);
			token2.removeLast();
		}
		
		// Parse mylist entry: fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
		// Note: lid is NOT included in the response - it's extracted from the query command
		if(token2.size() >= 11 && !lid.isEmpty())
		{
			q = QString("INSERT OR REPLACE INTO `mylist` (`lid`, `fid`, `eid`, `aid`, `gid`, `date`, `state`, `viewed`, `viewdate`, `storage`, `source`, `other`, `filestate`, `local_file`, `playback_position`, `playback_duration`, `last_played`) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10', '%11', '%12', '%13', (SELECT `local_file` FROM `mylist` WHERE `lid` = '%1'), COALESCE((SELECT `playback_position` FROM `mylist` WHERE `lid` = '%1'), 0), COALESCE((SELECT `playback_duration` FROM `mylist` WHERE `lid` = '%1'), 0), COALESCE((SELECT `last_played` FROM `mylist` WHERE `lid` = '%1'), 0))")
						.arg(lid)
						.arg(QString(token2.at(0)).replace("'", "''"))
						.arg(QString(token2.at(1)).replace("'", "''"))
						.arg(QString(token2.at(2)).replace("'", "''"))
						.arg(QString(token2.at(3)).replace("'", "''"))
						.arg(QString(token2.at(4)).replace("'", "''"))
						.arg(QString(token2.at(5)).replace("'", "''"))
						.arg(token2.size() > 6 ? QString(token2.at(6)).replace("'", "''") : "0")
						.arg(token2.size() > 7 ? QString(token2.at(7)).replace("'", "''") : "0")
						.arg(token2.size() > 8 ? QString(token2.at(8)).replace("'", "''") : "")
						.arg(token2.size() > 9 ? QString(token2.at(9)).replace("'", "''") : "")
						.arg(token2.size() > 10 ? QString(token2.at(10)).replace("'", "''") : "")
						.arg(token2.size() > 11 ? QString(token2.at(11)).replace("'", "''") : "0");
			QSqlQuery insertQuery(db);
			if(!insertQuery.exec(q))
			{
				LOG("Database query error: " + insertQuery.lastError().text());
			}
			else
			{
				LOG(QString("Successfully stored mylist entry - lid=%1, fid=%2").arg(lid).arg(QString(token2.at(0))));
			}
		}
		else if(lid.isEmpty())
		{
			LOG("Could not extract lid from MYLIST command");
		}
	}
	else if(ReplyID == "222"){ // 222 MYLISTSTATS
		// Response format: entries|watched|size|viewed size|viewed%|watched%|episodes watched
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		Logger::log("[AniDB Response] 222 MYLISTSTATS - Tag: " + Tag + " Data: " + token2.first(), __FILE__, __LINE__);
	}
	else if(ReplyID == "223"){ // 223 WISHLIST
		// Parse wishlist response
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		Logger::log("[AniDB Response] 223 WISHLIST - Tag: " + Tag + " Data: " + token2.first(), __FILE__, __LINE__);
	}
	else if(ReplyID == "230"){ // 230 ANIME
		// Get the original ANIME command to extract amask
		QString q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(Tag);
		QSqlQuery query(db);
		unsigned int fmask = 0;  // Not used for ANIME commands
		unsigned int amask = 0;
		QString amaskString;
		
		if(query.exec(q) && query.next())
		{
			QString animeCmd = query.value(0).toString();
			Logger::log("[AniDB Response] 230 ANIME command: " + animeCmd, __FILE__, __LINE__);
			
			// Extract amask as string for proper 7-byte parsing
			QRegularExpression amaskRegex("amask=([0-9a-fA-F]+)");
			QRegularExpressionMatch amaskMatch = amaskRegex.match(animeCmd);
			if (amaskMatch.hasMatch())
			{
				amaskString = amaskMatch.captured(1);
				Logger::log("[AniDB Response] 230 ANIME extracted amask string: " + amaskString, __FILE__, __LINE__);
			}
			
			// Also extract as uint for fallback/logging
			if(!extractMasksFromCommand(animeCmd, fmask, amask))
			{
				LOG("Failed to extract amask from ANIME command for Tag: " + Tag);
				// Fall back to default amask (all defined fields)
				amask = ANIME_AID | ANIME_DATEFLAGS | ANIME_YEAR | ANIME_TYPE |
						ANIME_RELATED_AID_LIST | ANIME_RELATED_AID_TYPE |
						ANIME_ROMAJI_NAME | ANIME_KANJI_NAME | ANIME_ENGLISH_NAME |
						ANIME_OTHER_NAME | ANIME_SHORT_NAME_LIST | ANIME_SYNONYM_LIST |
						ANIME_EPISODES | ANIME_HIGHEST_EPISODE | ANIME_SPECIAL_EP_COUNT |
						ANIME_AIR_DATE | ANIME_END_DATE | ANIME_URL | ANIME_PICNAME |
						ANIME_RATING | ANIME_VOTE_COUNT | ANIME_TEMP_RATING | ANIME_TEMP_VOTE_COUNT |
						ANIME_AVG_REVIEW_RATING | ANIME_REVIEW_COUNT | ANIME_AWARD_LIST | ANIME_IS_18_RESTRICTED |
						ANIME_ANN_ID | ANIME_ALLCINEMA_ID | ANIME_ANIMENFO_ID |
						ANIME_TAG_NAME_LIST | ANIME_TAG_ID_LIST | ANIME_TAG_WEIGHT_LIST | ANIME_DATE_RECORD_UPDATED |
						ANIME_CHARACTER_ID_LIST |
						ANIME_SPECIALS_COUNT | ANIME_CREDITS_COUNT | ANIME_OTHER_COUNT |
						ANIME_TRAILER_COUNT | ANIME_PARODY_COUNT;
				amaskString = QString::number(amask, 16);
			}
			Logger::log("[AniDB Response] 230 ANIME extracted amask: 0x" + QString::number(amask, 16), __FILE__, __LINE__);
		}
		else
		{
			LOG("Could not find packet for Tag: " + Tag);
			// Use default amask (all defined fields)
			amask = ANIME_AID | ANIME_DATEFLAGS | ANIME_YEAR | ANIME_TYPE |
					ANIME_RELATED_AID_LIST | ANIME_RELATED_AID_TYPE |
					ANIME_ROMAJI_NAME | ANIME_KANJI_NAME | ANIME_ENGLISH_NAME |
					ANIME_OTHER_NAME | ANIME_SHORT_NAME_LIST | ANIME_SYNONYM_LIST |
					ANIME_EPISODES | ANIME_HIGHEST_EPISODE | ANIME_SPECIAL_EP_COUNT |
					ANIME_AIR_DATE | ANIME_END_DATE | ANIME_URL | ANIME_PICNAME |
					ANIME_RATING | ANIME_VOTE_COUNT | ANIME_TEMP_RATING | ANIME_TEMP_VOTE_COUNT |
					ANIME_AVG_REVIEW_RATING | ANIME_REVIEW_COUNT | ANIME_AWARD_LIST | ANIME_IS_18_RESTRICTED |
					ANIME_ANN_ID | ANIME_ALLCINEMA_ID | ANIME_ANIMENFO_ID |
					ANIME_TAG_NAME_LIST | ANIME_TAG_ID_LIST | ANIME_TAG_WEIGHT_LIST | ANIME_DATE_RECORD_UPDATED |
					ANIME_CHARACTER_ID_LIST |
					ANIME_SPECIALS_COUNT | ANIME_CREDITS_COUNT | ANIME_OTHER_COUNT |
					ANIME_TRAILER_COUNT | ANIME_PARODY_COUNT;
			amaskString = QString::number(amask, 16);
			Logger::log("[AniDB Response] 230 ANIME using default amask: 0x" + QString::number(amask, 16), __FILE__, __LINE__);
		}
		
		// Parse response using mask-aware parsing
		// NOTE: UDP responses are limited to ~1400 bytes. Long anime titles may be truncated.
		// The AniDB API returns fields in mask bit order (MSB to LSB).
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		QString responseData = token2.first();
		token2 = responseData.split("|");
		
		// Handle truncated responses - remove the last field as it's likely incomplete
		if(isTruncated && token2.size() > 0)
		{
			Logger::log(QString("[AniDB Response] 230 ANIME - Truncated response detected, removing last field (was: '%1')")
				.arg(token2.last()), __FILE__, __LINE__);
			Logger::log(QString("[AniDB Response] 230 ANIME - Original field count: %1, processing %2 fields")
				.arg(token2.size()).arg(token2.size() - 1), __FILE__, __LINE__);
			token2.removeLast();
		}
		
		// Debug logging
		Logger::log("[AniDB Response] 230 ANIME raw data: " + responseData, __FILE__, __LINE__);
		Logger::log("[AniDB Response] 230 ANIME field count: " + QString::number(token2.size()), __FILE__, __LINE__);
		
		// Log first few fields for debugging
		for(int i = 0; i < qMin(10, token2.size()); i++)
		{
			Logger::log("[AniDB Response] 230 ANIME token[" + QString::number(i) + "]: '" + token2.at(i) + "'", __FILE__, __LINE__);
		}
		
		if(token2.size() >= 1)
		{
			QString aid = token2.at(0);  // AID is always first
			int index = 1;  // Start parsing after AID
			int startIndex = index;
			
			// Check if response might be truncated (UDP 1400 byte limit)
			if(responseData.length() >= 1350 && !isTruncated)
			{
				Logger::log("[AniDB Response] 230 ANIME WARNING: Response near UDP size limit (" + 
					QString::number(responseData.length()) + " chars), may be truncated", __FILE__, __LINE__);
			}
			
			// Parse anime data using amask string for proper 7-byte handling
			AnimeData animeData = parseAnimeMaskFromString(token2, amaskString, index);
			animeData.aid = aid;  // Ensure aid is set
			
			Logger::log("[AniDB Response] 230 ANIME parsed " + QString::number(index - startIndex) + " fields (index: " + QString::number(startIndex) + " -> " + QString::number(index) + ")", __FILE__, __LINE__);
			Logger::log("[AniDB Response] 230 ANIME parsed - AID: " + aid + " Year: '" + animeData.year + "' Type: '" + animeData.type + "'", __FILE__, __LINE__);
			
			// NOTE: UDP responses are limited to ~1400 bytes
			// Long anime titles can be truncated. The last field in a truncated response
			// will be incomplete. This is a known limitation of the AniDB UDP API.
			// Alternative: Use HTTP API or mylist export for complete data.
			
			// Handle truncated response - log warning
			if(isTruncated)
			{
				Logger::log(QString("[AniDB Response] 230 ANIME - WARNING: Response was truncated, some fields may be missing. "
					"Processed %1 fields successfully.").arg(index), __FILE__, __LINE__);
			}
			
			// For ANIME command, we only update typename since other fields may not be returned
			// The type field is returned as a string (e.g., "TV Series") by the API
			if(!aid.isEmpty() && !animeData.type.isEmpty())
			{
				QSqlQuery updateQuery(db);
				updateQuery.prepare("UPDATE `anime` SET `typename` = ? WHERE `aid` = ?");
				updateQuery.addBindValue(animeData.type);
				updateQuery.addBindValue(aid.toInt());
				
				if(!updateQuery.exec())
				{
					LOG("Anime metadata update error: " + updateQuery.lastError().text());
				}
				else
				{
					Logger::log("[AniDB Response] 230 ANIME metadata updated - AID: " + aid + " Type: " + animeData.type, __FILE__, __LINE__);
					// Emit signal to notify UI that anime data was updated
					emit notifyAnimeUpdated(aid.toInt());
				}
			}
		}
	}
	else if(ReplyID == "240"){ // 240 EPISODE
		// Response format: eid|aid|length|rating|votes|epno|eng|romaji|kanji|aired|type
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		token2 = token2.first().split("|");
		
		// Handle truncated responses - remove the last field as it's likely incomplete
		if(isTruncated && token2.size() > 0)
		{
			Logger::log(QString("[AniDB Response] 240 EPISODE - Truncated response, removing last field (was: '%1')")
				.arg(token2.last()), __FILE__, __LINE__);
			Logger::log(QString("[AniDB Response] 240 EPISODE - Original field count: %1, processing %2 fields")
				.arg(token2.size()).arg(token2.size() - 1), __FILE__, __LINE__);
			token2.removeLast();
		}
		
		if(token2.size() >= 7)
		{
			QString eid = token2.at(0);
			QString aid = token2.at(1);
			// length, rating, votes are at indices 2, 3, 4
			QString epno = token2.at(5);
			QString epname = token2.at(6);  // English name
			QString epnameromaji = token2.size() > 7 ? token2.at(7) : "";
			QString epnamekanji = token2.size() > 8 ? token2.at(8) : "";
			QString rating = token2.size() > 3 ? token2.at(3) : "";
			QString votecount = token2.size() > 4 ? token2.at(4) : "";
			
			// Store episode data in database
			QString q_episode = QString("INSERT OR REPLACE INTO `episode` (`eid`, `name`, `nameromaji`, `namekanji`, `rating`, `votecount`, `epno`) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7')")
				.arg(QString(eid).replace("'", "''"))
				.arg(QString(epname).replace("'", "''"))
				.arg(QString(epnameromaji).replace("'", "''"))
				.arg(QString(epnamekanji).replace("'", "''"))
				.arg(QString(rating).replace("'", "''"))
				.arg(QString(votecount).replace("'", "''"))
				.arg(QString(epno).replace("'", "''"));
			
			QSqlQuery query(db);
			if(!query.exec(q_episode))
			{
				LOG("Episode database query error: " + query.lastError().text());
			}
			else
			{
				Logger::log("[AniDB Response] 240 EPISODE stored - EID: " + eid + " AID: " + aid + " EPNO: " + epno + " Name: " + epname, __FILE__, __LINE__);
				
				// Log warning if truncated
				if(isTruncated)
				{
					Logger::log(QString("[AniDB Response] 240 EPISODE - WARNING: Response was truncated, some fields may be missing"), __FILE__, __LINE__);
				}
				
				// Emit signal to notify UI that episode data was updated
				emit notifyEpisodeUpdated(eid.toInt(), aid.toInt());
			}
		}
	}
	else if(ReplyID == "310"){ // 310 FILE ALREADY IN MYLIST
		// resend with tag and &edit=1
		QString q;
		q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(Tag);
		QSqlQuery query(db);
		query.exec(q);
		if(query.isSelect())
		{
			if(query.next())
			{
                q = QString("UPDATE `packets` SET `processed` = 0, `str` = '%1' WHERE `tag` = '%2'").arg( QString("%1&edit=1").arg(query.value(0).toString()) ).arg(Tag);
				query.exec(q);
			}
		}
		notifyMylistAdd(Tag, 310);
	}
	else if(ReplyID == "311"){ // 311 MYLIST ENTRY EDITED
		// Parse lid from response message
		QStringList token2 = Message.split("\n");
		token2.pop_front(); // Remove the status line
		QString lid = token2.first().trimmed();
		
		// Get the original MYLISTADD command from packets table
		QString q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(Tag);
		QSqlQuery query(db);
		if(query.exec(q) && query.next())
		{
			QString mylistAddCmd = query.value(0).toString();
			
			// Parse parameters from the MYLISTADD command
			// Format: MYLISTADD size=X&ed2k=Y&viewed=Z&state=W&storage=S
			QStringList params = mylistAddCmd.split("&");
			QString size, ed2k, viewed = "0", state = "0", storage = "";
			
			for(const QString& param : params)
			{
				if(param.contains("size="))
					size = param.mid(param.indexOf("size=") + 5).split("&").first();
				else if(param.contains("ed2k="))
					ed2k = param.mid(param.indexOf("ed2k=") + 5).split("&").first();
				else if(param.contains("viewed="))
					viewed = param.mid(param.indexOf("viewed=") + 7).split("&").first();
				else if(param.contains("state="))
					state = param.mid(param.indexOf("state=") + 6).split("&").first();
				else if(param.contains("storage="))
					storage = param.mid(param.indexOf("storage=") + 8).split("&").first();
			}
			
			// Look up file info (fid, eid, aid, gid) from file table using size and ed2k
			QString fid, eid, aid, gid;
			q = QString("SELECT `fid`, `eid`, `aid`, `gid` FROM `file` WHERE `size` = '%1' AND `ed2k` = '%2'")
				.arg(size).arg(ed2k);
			QSqlQuery fileQuery(db);
			if(fileQuery.exec(q) && fileQuery.next())
			{
				fid = fileQuery.value(0).toString();
				eid = fileQuery.value(1).toString();
				aid = fileQuery.value(2).toString();
				gid = fileQuery.value(3).toString();
				
				// Update mylist table, preserving local_file and playback data if they exist
				q = QString("INSERT OR REPLACE INTO `mylist` "
					"(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`, `local_file`, `playback_position`, `playback_duration`, `last_played`) "
					"VALUES (%1, %2, %3, %4, %5, %6, %7, '%8', "
					"(SELECT `local_file` FROM `mylist` WHERE `lid` = %1), "
					"COALESCE((SELECT `playback_position` FROM `mylist` WHERE `lid` = %1), 0), "
					"COALESCE((SELECT `playback_duration` FROM `mylist` WHERE `lid` = %1), 0), "
					"COALESCE((SELECT `last_played` FROM `mylist` WHERE `lid` = %1), 0))")
					.arg(lid)
					.arg(fid.isEmpty() ? "0" : fid)
					.arg(eid.isEmpty() ? "0" : eid)
					.arg(aid.isEmpty() ? "0" : aid)
					.arg(gid.isEmpty() ? "0" : gid)
					.arg(state)
					.arg(viewed)
					.arg(QString(storage).replace("'", "''"));
				
				QSqlQuery insertQuery(db);
				if(!insertQuery.exec(q))
				{
					LOG("Failed to update mylist entry: " + insertQuery.lastError().text());
				}
				else
				{
					LOG(QString("Successfully updated mylist entry - lid=%1, fid=%2").arg(lid).arg(fid));
				}
			}
			else
			{
				LOG("Could not find file info for size=" + size + " ed2k=" + ed2k);
			}
		}
		
		notifyMylistAdd(Tag, 311);
	}
	else if(ReplyID == "312"){ // 312 NO SUCH MYLIST ENTRY
		Logger::log("[AniDB Response] 312 NO SUCH MYLIST ENTRY - Tag: " + Tag, __FILE__, __LINE__);
	}
    else if(ReplyID == "320"){ // 320 NO SUCH FILE
        notifyMylistAdd(Tag, 320);
        // Mark packet as processed and received reply instead of deleting
        QString q = QString("UPDATE `packets` SET `processed` = 1, `got_reply` = 1, `reply` = '%1' WHERE `tag` = '%2'").arg(ReplyID).arg(Tag);
        LOG("Database update query: " + q + " Tag: " + Tag);
        QSqlQuery query(db);
        query.exec(q);
    }
	else if(ReplyID == "270"){ // 270 NOTIFICATION - {int4 nid}|{int2 type}|{int4 fromuid}|{int4 date}|{str title}|{str body}
		// Parse notification message
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		QStringList parts = token2.first().split("|");
		if(parts.size() >= 6)
		{
			int nid = parts[0].toInt();
			int type = parts[1].toInt();
			int fromuid = parts[2].toInt();
			int date = parts[3].toInt();
			QString title = parts[4];
			QString body = parts[5];
			
			Logger::log("[AniDB Response] 270 NOTIFICATION - NID: " + QString::number(nid) + " Title: " + title + " Body: " + body, __FILE__, __LINE__);
			
			// Store notification in database
			QSqlQuery query(db);
			QString q = QString("INSERT OR REPLACE INTO `notifications` (`nid`, `type`, `from_user_id`, `date`, `message_type`, `title`, `body`, `received_at`, `acknowledged`) VALUES (%1, 'PUSH', %2, %3, %4, '%5', '%6', %7, 0);")
				.arg(nid)
				.arg(fromuid)
				.arg(date)
				.arg(type)
				.arg(QString(title).replace("'", "''"))
				.arg(QString(body).replace("'", "''"))
				.arg(QDateTime::currentSecsSinceEpoch());
			query.exec(q);
			if(query.lastError().isValid())
			{
				Logger::log("[AniDB Database] Error storing notification: " + query.lastError().text(), __FILE__, __LINE__);
			}
			
			// Check if this is an export notification (contains .tgz link)
			if(body.contains(".tgz", Qt::CaseInsensitive) && isExportQueued)
			{
				Logger::log("[AniDB Export] Export notification received, stopping periodic checks", __FILE__, __LINE__);
				isExportQueued = false;
				notifyCheckTimer->stop();
				notifyCheckIntervalMs = 60000; // Reset to 1 minute for next export
				notifyCheckAttempts = 0;
				exportQueuedTimestamp = 0;
				saveExportQueueState();
			}
			
			// Emit signal for notification
			emit notifyMessageReceived(nid, body);
			
			// Acknowledge the notification
			PushAck(nid);
		}
	}
	else if(ReplyID == "271"){ // 271 NOTIFYACK - NOTIFICATION ACKNOWLEDGED
		Logger::log("[AniDB Response] 271 NOTIFICATION ACKNOWLEDGED - Tag: " + Tag, __FILE__, __LINE__);
	}
	else if(ReplyID == "272"){ // 272 NO SUCH NOTIFICATION
		Logger::log("[AniDB Response] 272 NO SUCH NOTIFICATION - Tag: " + Tag, __FILE__, __LINE__);
	}
	else if(ReplyID == "290"){ // 290 NOTIFYLIST
		// Parse notification list - show all entries
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		Logger::log("[AniDB Response] 290 NOTIFYLIST - Tag: " + Tag + " Entry count: " + QString::number(token2.size()), __FILE__, __LINE__);
		
		// Log all notification entries
		for(int i = 0; i < token2.size(); i++)
		{
			Logger::log("[AniDB Response] 290 NOTIFYLIST Entry " + QString::number(i+1) + " of " + QString::number(token2.size()) + ": " + token2[i], __FILE__, __LINE__);
		}
		
		// Collect all message notification IDs (M|nid entries)
		// Export notifications could be in any of these messages, not just the last one
		QStringList messageNids;
		for(int i = 0; i < token2.size(); i++)
		{
			if(token2[i].startsWith("M|"))
			{
				messageNids.append(token2[i].mid(2)); // Extract nid after "M|"
			}
		}
		
		// Check database to filter out already-fetched notifications
		QStringList newNids;
		QSqlQuery query(db);
		for(int i = 0; i < messageNids.size(); i++)
		{
			QString nid = messageNids[i];
			query.exec(QString("SELECT nid FROM notifications WHERE nid = %1").arg(nid));
			if(!query.next())
			{
				// Not in database, this is a new notification
				newNids.append(nid);
			}
		}
		
		Logger::log("[AniDB Response] 290 NOTIFYLIST - Total messages: " + QString::number(messageNids.size()) + ", New messages: " + QString::number(newNids.size()), __FILE__, __LINE__);
		
		// Fetch only new message notifications to search for export link
		// The export notification is most likely recent, but not guaranteed to be the very last one
		const int maxNotificationsToFetch = 10;
		int notificationsToFetch = qMin(newNids.size(), maxNotificationsToFetch);
		
		if(notificationsToFetch > 0)
		{
			Logger::log("[AniDB Response] 290 NOTIFYLIST - Fetching " + QString::number(notificationsToFetch) + " new message notifications", __FILE__, __LINE__);
			emit notifyCheckStarting(notificationsToFetch);
			for(int i = 0; i < notificationsToFetch; i++)
			{
				// Fetch from the end of the list (most recent first)
				QString nid = newNids[newNids.size() - 1 - i];
				Logger::log("[AniDB Response] 290 NOTIFYLIST - Fetching new message notification " + QString::number(i+1) + " of " + QString::number(notificationsToFetch) + ": " + nid, __FILE__, __LINE__);
				NotifyGet(nid.toInt());
			}
		}
		else if(messageNids.size() > 0)
		{
			Logger::log("[AniDB Response] 290 NOTIFYLIST - No new notifications to fetch, all are already in database", __FILE__, __LINE__);
		}
	}
	else if(ReplyID == "291"){ // 291 NOTIFYLIST ENTRY
		// Parse notification list - can contain single entry (pagination) or full list
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		Logger::log("[AniDB Response] 291 NOTIFYLIST - Tag: " + Tag + " Entry count: " + QString::number(token2.size()), __FILE__, __LINE__);
		
		// Log all notification entries
		for(int i = 0; i < token2.size(); i++)
		{
			Logger::log("[AniDB Response] 291 NOTIFYLIST Entry " + QString::number(i+1) + " of " + QString::number(token2.size()) + ": " + token2[i], __FILE__, __LINE__);
		}
		
		// Collect all message notification IDs (M|nid entries)
		// Export notifications could be in any of these messages, not just the last one
		QStringList messageNids;
		for(int i = 0; i < token2.size(); i++)
		{
			if(token2[i].startsWith("M|"))
			{
				messageNids.append(token2[i].mid(2)); // Extract nid after "M|"
			}
		}
		
		// Check database to filter out already-fetched notifications
		QStringList newNids;
		QSqlQuery query(db);
		for(int i = 0; i < messageNids.size(); i++)
		{
			QString nid = messageNids[i];
			query.exec(QString("SELECT nid FROM notifications WHERE nid = %1").arg(nid));
			if(!query.next())
			{
				// Not in database, this is a new notification
				newNids.append(nid);
			}
		}
		
		Logger::log("[AniDB Response] 291 NOTIFYLIST - Total messages: " + QString::number(messageNids.size()) + ", New messages: " + QString::number(newNids.size()), __FILE__, __LINE__);
		
		// Fetch only new message notifications to search for export link
		// The export notification is most likely recent, but not guaranteed to be the very last one
		const int maxNotificationsToFetch = 10;
		int notificationsToFetch = qMin(newNids.size(), maxNotificationsToFetch);
		
		if(notificationsToFetch > 0)
		{
			Logger::log("[AniDB Response] 291 NOTIFYLIST - Fetching " + QString::number(notificationsToFetch) + " new message notifications", __FILE__, __LINE__);
			emit notifyCheckStarting(notificationsToFetch);
			for(int i = 0; i < notificationsToFetch; i++)
			{
				// Fetch from the end of the list (most recent first)
				QString nid = newNids[newNids.size() - 1 - i];
				Logger::log("[AniDB Response] 291 NOTIFYLIST - Fetching new message notification " + QString::number(i+1) + " of " + QString::number(notificationsToFetch) + ": " + nid, __FILE__, __LINE__);
				NotifyGet(nid.toInt());
			}
		}
		else if(messageNids.size() > 0)
		{
			Logger::log("[AniDB Response] 291 NOTIFYLIST - No new notifications to fetch, all are already in database", __FILE__, __LINE__);
		}
	}
	else if(ReplyID == "292"){ // 292 NOTIFYGET (type=M) - {int4 id}|{int4 from_user_id}|{str from_user_name}|{int4 date}|{int4 type}|{str title}|{str body}
		// Parse message notification details (type=M)
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		QStringList parts = token2.first().split("|");
		if(parts.size() >= 7)
		{
			int id = parts[0].toInt();
			int from_user_id = parts[1].toInt();
			QString from_user_name = parts[2];
			int date = parts[3].toInt();
			int type = parts[4].toInt();
			QString title = parts[5];
			QString body = parts[6];
			
			Logger::log("[AniDB Response] 292 NOTIFYGET - ID: " + QString::number(id) + " Title: " + title + " Body: " + body, __FILE__, __LINE__);
			
			// Store notification in database
			QSqlQuery query(db);
			QString q = QString("INSERT OR REPLACE INTO `notifications` (`nid`, `type`, `from_user_id`, `from_user_name`, `date`, `message_type`, `title`, `body`, `received_at`, `acknowledged`) VALUES (%1, 'FETCHED', %2, '%3', %4, %5, '%6', '%7', %8, 0);")
				.arg(id)
				.arg(from_user_id)
				.arg(QString(from_user_name).replace("'", "''"))
				.arg(date)
				.arg(type)
				.arg(QString(title).replace("'", "''"))
				.arg(QString(body).replace("'", "''"))
				.arg(QDateTime::currentSecsSinceEpoch());
			query.exec(q);
			if(query.lastError().isValid())
			{
				Logger::log("[AniDB Database] Error storing notification: " + query.lastError().text(), __FILE__, __LINE__);
			}
			
			// Check if this is an export notification (contains .tgz link)
			if(body.contains(".tgz", Qt::CaseInsensitive) && isExportQueued)
			{
				Logger::log("[AniDB Export] Export notification received, stopping periodic checks", __FILE__, __LINE__);
				isExportQueued = false;
				requestedExportTemplate.clear(); // Clear the requested template
				notifyCheckTimer->stop();
				notifyCheckIntervalMs = 60000; // Reset to 1 minute for next export
				notifyCheckAttempts = 0;
				exportQueuedTimestamp = 0;
				saveExportQueueState();
			}
			
			// Emit signal for notification (same as 270 for automatic download/import)
			emit notifyMessageReceived(id, body);
			
			// Note: PUSHACK is only for PUSH notifications (code 270), not for fetched notifications via NOTIFYGET
		}
		else
		{
			Logger::log("[AniDB Response] 292 NOTIFYGET - Invalid format, parts count: " + QString::number(parts.size()), __FILE__, __LINE__);
		}
	}
	else if(ReplyID == "293"){ // 293 NOTIFYGET (type=N) - {int4 relid}|{int4 type}|{int2 count}|{int4 date}|{str relidname}|{str fids}
		// Parse notification details (type=N)
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		QStringList parts = token2.first().split("|");
		if(parts.size() >= 6)
		{
			int relid = parts[0].toInt();
			int type = parts[1].toInt();
			int count = parts[2].toInt();
			int date = parts[3].toInt();
			QString relidname = parts[4];
			QString fids = parts[5];
			
			Logger::log("[AniDB Response] 293 NOTIFYGET - RelID: " + QString::number(relid) + " Type: " + QString::number(type) + " Count: " + QString::number(count) + " Name: " + relidname + " FIDs: " + fids, __FILE__, __LINE__);
			
			// Store file notification in database
			QSqlQuery query(db);
			QString body = QString("File notification - RelID: %1, Count: %2, Name: %3, FIDs: %4").arg(relid).arg(count).arg(relidname).arg(fids);
			QString q = QString("INSERT OR REPLACE INTO `notifications` (`nid`, `type`, `date`, `message_type`, `title`, `body`, `received_at`, `acknowledged`) VALUES (%1, 'FILE', %2, %3, 'File Notification', '%4', %5, 0);")
				.arg(relid)
				.arg(date)
				.arg(type)
				.arg(QString(body).replace("'", "''"))
				.arg(QDateTime::currentSecsSinceEpoch());
			query.exec(q);
			if(query.lastError().isValid())
			{
				Logger::log("[AniDB Database] Error storing file notification: " + query.lastError().text(), __FILE__, __LINE__);
			}
			
			// Note: For N-type notifications, we don't emit notifyMessageReceived as these are file notifications
			// They would need different handling for new file notifications
			
			// Note: PUSHACK is only for PUSH notifications (code 270), not for fetched notifications via NOTIFYGET
		}
		else
		{
			Logger::log("[AniDB Response] 293 NOTIFYGET - Invalid format, parts count: " + QString::number(parts.size()), __FILE__, __LINE__);
		}
	}
	else if(ReplyID == "403"){ // 403 NOT LOGGED IN
		loggedin = 0;
		if(ReplyTo != "LOGOUT"){
			Auth();
		}
	}
	else if(ReplyID == "500"){ // 500 LOGIN FAILED
	}
	else if(ReplyID == "501"){ // 501 LOGIN FIRST
		Auth();
//		Send(ReplyToMsg, "", Tag);
	}
	else if(ReplyID == "503"){ // 503 CLIENT VERSION OUTDATED
	}
	else if(ReplyID == "504"){ // 504 CLIENT BANNED - {str reason}
		QStringList token2 = Message.split("-");
		token2.pop_front();
		bannedfor = token2.first();
		LOG("AniDBApi: Client banned: "+ bannedfor);
	}
	else if(ReplyID == "505"){ // 505 ILLEGAL INPUT OR ACCESS DENIED
	}
	else if(ReplyID == "506"){ // 506 INVALID SESSION
		Auth();
		Send(ReplyToMsg, "", Tag);
	}
    else if(ReplyID == "555"){ // 555 BANNED - {str reason}
        banned = true;
    }
	else if(ReplyID == "598"){ // 598 UNKNOWN COMMAND
		// This typically means the command was malformed or not recognized
		Logger::log("[AniDB Error] 598 UNKNOWN COMMAND - Tag: " + Tag + " - check request format", __FILE__, __LINE__);
	}
	else if(ReplyID == "601"){ // 601 ANIDB OUT OF SERVICE - TRY AGAIN LATER
	}
	else if(ReplyID == "702"){ // 702 NO SUCH PACKET PENDING
		// This occurs when trying to PUSHACK a notification that wasn't sent via PUSH
		// PUSHACK is only for notifications received via code 270 (PUSH), not for notifications fetched via NOTIFYGET
		Logger::log("[AniDB Response] 702 NO SUCH PACKET PENDING - Tag: " + Tag, __FILE__, __LINE__);
	}
    else
    {
        Logger::log("[AniDB Error] ParseMessage - UNSUPPORTED ReplyID: " + ReplyID + " Tag: " + Tag, __FILE__, __LINE__);
    }
    waitingForReply.isWaiting = false;
    currentTag = ""; // Reset current tag when response is received
	return ReplyID;
}

QString AniDBApi::Auth()
{
	QString msg = buildAuthCommand(AniDBApi::username, AniDBApi::password, AniDBApi::protover, AniDBApi::client, AniDBApi::clientver, AniDBApi::enc);
	QString q;
	q = QString("INSERT OR REPLACE INTO `packets` (`tag`, `str`) VALUES ('0', '%1');").arg(msg);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Logger::log("[AniDB Auth] Database query error: " + query.lastError().text(), __FILE__, __LINE__);
	}

//	Send(msg, "AUTH", "xxx");

	return 0;
}

QString AniDBApi::Logout()
{
	QString msg = buildLogoutCommand();
    Logger::log("[AniDB API] Sending LOGOUT command", __FILE__, __LINE__);
    Send(msg, "LOGOUT", "0");

	return 0;
}

QString AniDBApi::MylistAdd(qint64 size, QString ed2khash, int viewed, int state, QString storage, bool edit)
{
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	QString msg = buildMylistAddCommand(size, ed2khash, viewed, state, storage, edit);
	QString q;
	q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Logger::log("[AniDB MylistAdd] Database insert error: " + query.lastError().text(), __FILE__, __LINE__);
		return "0";
	}

/*	q = QString("SELECT `tag` FROM `packets` WHERE `str` = '%1' AND `processed` = 0").arg(msg);
	query.exec(q);
	LOG("Database query error: " + query.lastError().text());

	if(query.isSelect())
	{
		if(query.next())
		{
			return query.value(0).toString();
		}
	}*/
	return GetTag(msg);
}

QString AniDBApi::File(qint64 size, QString ed2k)
{
	unsigned int amask = aEPISODE_TOTAL | aEPISODE_LAST | aANIME_YEAR | aANIME_TYPE | aANIME_RELATED_LIST |
				aANIME_RELATED_TYPE | aANIME_CATAGORY | aANIME_NAME_ROMAJI | aANIME_NAME_KANJI |
				aANIME_NAME_ENGLISH | aANIME_NAME_OTHER | aANIME_NAME_SHORT | aANIME_SYNONYMS |
				aEPISODE_NUMBER | aEPISODE_NAME | aEPISODE_NAME_ROMAJI | aEPISODE_NAME_KANJI |
				aEPISODE_RATING | aEPISODE_VOTE_COUNT | aGROUP_NAME | aGROUP_NAME_SHORT |
				aDATE_AID_RECORD_UPDATED;
	unsigned int fmask = fAID | fEID | fGID | fLID | fOTHEREPS | fISDEPR | fSTATE | fSIZE | fED2K | fMD5 | fSHA1 |
				fCRC32 | fQUALITY | fSOURCE | fCODEC_AUDIO | fBITRATE_AUDIO | fCODEC_VIDEO | fBITRATE_VIDEO |
				fRESOLUTION | fFILETYPE | fLANG_DUB | fLANG_SUB | fLENGTH | fDESCRIPTION | fAIRDATE |
				fFILENAME;
	QString msg = buildFileCommand(size, ed2k, fmask, amask);
	LOG(msg);
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Logger::log("[AniDB File] Database insert error: " + query.lastError().text(), __FILE__, __LINE__);
		return "0";
	}
//	Send(a, "", "zzz");
	return GetTag(msg);
}

QString AniDBApi::Mylist(int lid)
{
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	QString msg;
	if(lid > 0)
	{
		msg = buildMylistCommand(lid);
	}
	else
	{
		// Query all mylist entries - we'll need to do this iteratively or use MYLISTSTATS first
		msg = buildMylistStatsCommand();
	}
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	query.exec(q);
	return GetTag(msg);
}

QString AniDBApi::PushAck(int nid)
{
	// Acknowledge a received notification
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	QString msg = buildPushAckCommand(nid);
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	query.exec(q);
	return GetTag(msg);
}

QString AniDBApi::NotifyEnable()
{
	// Enable push notifications
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	// Request notification list to enable push notifications
	QString msg = buildNotifyListCommand();
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	query.exec(q);
	return GetTag(msg);
}

QString AniDBApi::NotifyGet(int nid)
{
	// Get details of a specific notification
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	QString msg = buildNotifyGetCommand(nid);
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	query.exec(q);
	return GetTag(msg);
}

QString AniDBApi::MylistExport(QString template_name)
{
	// Request mylist export with specified template
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	Logger::log("[AniDB API] Requesting MYLISTEXPORT with template: " + template_name, __FILE__, __LINE__);
	
	// Store the requested template so we can verify the notification matches
	requestedExportTemplate = template_name;
	
	QString msg = buildMylistExportCommand(template_name);
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	query.exec(q);
	return GetTag(msg);
}

QString AniDBApi::Episode(int eid)
{
	// Request episode information by episode ID
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	Logger::log("[AniDB API] Requesting EPISODE data for EID: " + QString::number(eid), __FILE__, __LINE__);
	QString msg = buildEpisodeCommand(eid);
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	query.exec(q);
	return GetTag(msg);
}

QString AniDBApi::Anime(int aid)
{
	// Request anime information by anime ID
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	Logger::log("[AniDB API] Requesting ANIME data for AID: " + QString::number(aid), __FILE__, __LINE__);
	QString msg = buildAnimeCommand(aid);
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	query.exec(q);
	return GetTag(msg);
}

/* === Command Builders === */
// These methods build formatted command strings for testing and reuse

QString AniDBApi::buildAuthCommand(QString username, QString password, int protover, QString client, int clientver, QString enc)
{
	return QString("AUTH user=%1&pass=%2&protover=%3&client=%4&clientver=%5&enc=%6")
		.arg(username).arg(password).arg(protover).arg(client).arg(clientver).arg(enc);
}

QString AniDBApi::buildLogoutCommand()
{
	return QString("LOGOUT ");
}

QString AniDBApi::buildMylistAddCommand(qint64 size, QString ed2khash, int viewed, int state, QString storage, bool edit)
{
	QString msg = QString("MYLISTADD size=%1&ed2k=%2").arg(size).arg(ed2khash);
	if(viewed > 0 && viewed < 3)
	{
		msg += QString("&viewed=%1").arg(viewed-1);
	}
	if(storage.length() > 0)
	{
		msg += QString("&storage=%1").arg(storage);
	}
	if(edit == 1)
	{
		msg += QString("&edit=1");
	}
	msg += QString("&state=%1").arg(state);
	return msg;
}

QString AniDBApi::buildMylistCommand(int lid)
{
	return QString("MYLIST lid=%1").arg(lid);
}

QString AniDBApi::buildMylistStatsCommand()
{
	return QString("MYLISTSTATS ");
}

QString AniDBApi::buildFileCommand(qint64 size, QString ed2k, unsigned int fmask, unsigned int amask)
{
	return QString("FILE size=%1&ed2k=%2&fmask=%3&amask=%4")
		.arg(size).arg(ed2k).arg(fmask, 8, 16, QChar('0')).arg(amask, 8, 16, QChar('0'));
}

QString AniDBApi::buildPushAckCommand(int nid)
{
	return QString("PUSHACK nid=%1").arg(nid);
}

QString AniDBApi::buildNotifyListCommand()
{
	return QString("NOTIFYLIST ");
}

QString AniDBApi::buildNotifyGetCommand(int nid)
{
	// NOTIFYGET requires type parameter: type=M for messages, type=N for notifications
	// Currently only fetching message notifications from NOTIFYLIST
	return QString("NOTIFYGET type=M&id=%1").arg(nid);
}

QString AniDBApi::buildMylistExportCommand(QString template_name)
{
	// MYLISTEXPORT template={str template}
	// Request mylist export with specified template (e.g., "xml-plain-cs")
	return QString("MYLISTEXPORT template=%1").arg(template_name);
}

QString AniDBApi::buildEpisodeCommand(int eid)
{
	// EPISODE eid={int4 eid}
	// Request episode information for a specific episode ID
	return QString("EPISODE eid=%1").arg(eid);
}

QString AniDBApi::buildAnimeCommand(int aid)
{
	// ANIME aid={int4 aid}&amask={hexstr amask}
	// Request anime information for a specific anime ID
	// Build amask using the anime_amask_codes enum (all 7 bytes)
	// 
	// ANIME command uses byte-oriented mask (7 bytes):
	//   Byte 1: aid, dateflags, year, type, related lists
	//   Byte 2: name variations (romaji, kanji, english, other, short, synonym)
	//   Byte 3: episodes, highest episode, special ep count, dates, url, picname
	//   Byte 4: ratings, reviews, awards, is 18+ restricted
	//   Byte 5: external IDs (ANN, allcinema, animenfo), tags, date updated
	//   Byte 6: character ID list
	//   Byte 7: episode type counts (specials, credits, other, trailer, parody)
	// 
	// We request all defined fields per API specification
	unsigned int amask = 
		// Byte 1
		ANIME_AID | ANIME_DATEFLAGS |
		ANIME_YEAR | ANIME_TYPE |
		ANIME_RELATED_AID_LIST | ANIME_RELATED_AID_TYPE |
		// Byte 2
		ANIME_ROMAJI_NAME | ANIME_KANJI_NAME | ANIME_ENGLISH_NAME |
		ANIME_OTHER_NAME | ANIME_SHORT_NAME_LIST | ANIME_SYNONYM_LIST |
		// Byte 3
		ANIME_EPISODES | ANIME_HIGHEST_EPISODE | ANIME_SPECIAL_EP_COUNT |
		ANIME_AIR_DATE | ANIME_END_DATE | ANIME_URL | ANIME_PICNAME |
		// Byte 4
		ANIME_RATING | ANIME_VOTE_COUNT | ANIME_TEMP_RATING | ANIME_TEMP_VOTE_COUNT |
		ANIME_AVG_REVIEW_RATING | ANIME_REVIEW_COUNT | ANIME_AWARD_LIST | ANIME_IS_18_RESTRICTED |
		// Byte 5
		ANIME_ANN_ID | ANIME_ALLCINEMA_ID | ANIME_ANIMENFO_ID |
		ANIME_TAG_NAME_LIST | ANIME_TAG_ID_LIST | ANIME_TAG_WEIGHT_LIST | ANIME_DATE_RECORD_UPDATED |
		// Byte 6
		ANIME_CHARACTER_ID_LIST |
		// Byte 7
		ANIME_SPECIALS_COUNT | ANIME_CREDITS_COUNT | ANIME_OTHER_COUNT |
		ANIME_TRAILER_COUNT | ANIME_PARODY_COUNT;
	
	// Convert to hex string (no leading zeros for AniDB API)
	return QString("ANIME aid=%1&amask=%2").arg(aid).arg(amask, 0, 16);
}

/* === End Command Builders === */

QString AniDBApi::GetSID()
{
	return AniDBApi::SID;
}

QString AniDBApi::GetRequestedExportTemplate()
{
	return requestedExportTemplate;
}

int AniDBApi::Send(QString str, QString msgtype, QString tag)
{
	// Ensure socket is created
	if(Socket == nullptr)
	{
		Logger::log("[AniDB Error] Socket not initialized, attempting to create socket", __FILE__, __LINE__);
		if(CreateSocket() == 0)
		{
			Logger::log("[AniDB Error] Failed to create socket, cannot send - Check if port 3962 is available", __FILE__, __LINE__);
			return 0;
		}
	}
	
	// Verify socket is in a valid state before attempting to write
	// Note: UDP sockets may be in UnconnectedState even when functional
	// The short-circuit evaluation ensures we don't dereference nullptr
	if(Socket == nullptr || !Socket->isValid() || !Socket->isOpen())
	{
		QString errorMsg = "[AniDB Error] Socket is not valid or not open for writing";
		if(Socket != nullptr)
		{
			errorMsg += " - " + Socket->errorString();
		}
		Logger::log(errorMsg, __FILE__, __LINE__);
		return 0;
	}
	
	QString a;
	if(SID.length() > 0)
	{
		a = QString("%1&s=%2").arg(str).arg(SID);
	}
	else
	{
		a = str;
    }
    a = QString("%1&tag=%2").arg(a).arg(tag);
    LOG("[AniDB Send] Command: " + a);
//    QByteArray bytes = a.toUtf8();
//	const char *ptr = bytes.data();
//	Socket->write(ptr);
    Socket->write(a.toUtf8().constData());
    waitingForReply.isWaiting = true;
    waitingForReply.start.start();
    currentTag = tag; // Track the current tag

	lastSentPacket = a;

	QSqlQuery query(db);
    query.exec(QString("UPDATE `packets` SET `processed` = 1, `sendtime` = '%2' WHERE `tag` = '%1'").arg(tag).arg(QDateTime::currentDateTime().toSecsSinceEpoch()));

	Recv();
	return 1;
}

int AniDBApi::Recv()
{
	if(Socket == nullptr)
	{
		return 0;
	}
	QByteArray data;
//    Socket->waitForReadyRead(10000);
	QString result;
	bool isTruncated = false;
	while(Socket->hasPendingDatagrams())
	{
		data.resize(Socket->pendingDatagramSize());
		qint64 bytesRead = Socket->readDatagram(data.data(), data.size());
//		result = codec->toUnicode(data);
//		result.toUtf8();
        result = QString::fromUtf8(data.data());
		LOG("AniDBApi: Recv: " + result);
		Logger::log(QString("[AniDB Recv] Datagram size: %1 bytes, Read: %2 bytes, Result length: %3 chars").arg(data.size()).arg(bytesRead).arg(result.length()), __FILE__, __LINE__);
		
		// Detect truncation: UDP MTU limit is typically 1400 bytes
		// If datagram size is exactly 1400 bytes, the response is likely truncated
		if(data.size() >= 1400 || bytesRead >= 1400)
		{
			isTruncated = true;
			Logger::log(QString("[AniDB Recv] TRUNCATION DETECTED: Datagram at MTU limit (%1 bytes), response is truncated").arg(data.size()), __FILE__, __LINE__);
		}
    }
	if(result.length() > 0)
	{
		ParseMessage(result,"", lastSentPacket, isTruncated);
		return 1;
	}
	return 0;
}

int AniDBApi::LoginStatus()
{
	return 1;
}

bool AniDBApi::LoggedIn()
{
	return AniDBApi::loggedin;
}

int AniDBApi::SendPacket()
{
    // Check for timeout and handle retry logic
    if(waitingForReply.isWaiting && waitingForReply.start.elapsed() > 10000)
    {
        qint64 elapsed = waitingForReply.start.elapsed();
        Logger::log("[AniDB Timeout] Waited for reply for more than 10 seconds - Elapsed: " + QString::number(elapsed) + " ms", __FILE__, __LINE__);
        
        // Get retry count for the current packet
        QSqlQuery retryQuery(db);
        retryQuery.prepare("SELECT `retry_count` FROM `packets` WHERE `tag` = ?");
        retryQuery.addBindValue(currentTag);
        
        int retryCount = 0;
        if(retryQuery.exec() && retryQuery.next())
        {
            retryCount = retryQuery.value(0).toInt();
        }
        else
        {
            Logger::log("[AniDB Error] Failed to query retry count for Tag: " + currentTag, __FILE__, __LINE__);
        }
        
        const int MAX_RETRIES = 3;
        
        if(retryCount < MAX_RETRIES)
        {
            // Increment retry count and reset processed flag to retry
            Logger::log("[AniDB Retry] Resending packet (attempt " + QString::number(retryCount + 2) + "/" + QString::number(MAX_RETRIES + 1) + ") - Tag: " + currentTag, __FILE__, __LINE__);
            
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE `packets` SET `processed` = 0, `retry_count` = ? WHERE `tag` = ?");
            updateQuery.addBindValue(retryCount + 1);
            updateQuery.addBindValue(currentTag);
            if(!updateQuery.exec())
            {
                Logger::log("[AniDB Error] Failed to update packet for retry - Tag: " + currentTag, __FILE__, __LINE__);
            }
            
            // Reset waiting state to allow resend
            waitingForReply.isWaiting = false;
            currentTag = "";
        }
        else
        {
            // Max retries reached, mark as failed
            Logger::log("[AniDB Error] Maximum retries (" + QString::number(MAX_RETRIES) + ") reached for Tag: " + currentTag + " - Giving up", __FILE__, __LINE__);
            
            QSqlQuery failQuery(db);
            failQuery.prepare("UPDATE `packets` SET `got_reply` = 1, `reply` = 'TIMEOUT' WHERE `tag` = ?");
            failQuery.addBindValue(currentTag);
            if(!failQuery.exec())
            {
                Logger::log("[AniDB Error] Failed to mark packet as timed out - Tag: " + currentTag, __FILE__, __LINE__);
            }
            
            // Reset waiting state to continue processing queue
            waitingForReply.isWaiting = false;
            currentTag = "";
        }
    }
    
    if(!waitingForReply.isWaiting)
    {
/*        if(banned == true)
        {
            packetsender->stop();
            return 0;
        }*/
        QString q = "SELECT `tag`,`str` FROM `packets` WHERE `processed` = 0 AND `got_reply` = 0 ORDER BY `tag` ASC LIMIT 1";
        QString tag, str;
        QSqlQuery query(db);
        query.exec(q);

        if(query.isSelect())
        {
            if(query.next())
            {
                tag = query.value(0).toString();
                str = query.value(1).toString();
                Logger::log("[AniDB Queue] Sending query - Tag: " + tag + " Command: " + str, __FILE__, __LINE__);
                if(!LoggedIn() && !str.contains("AUTH"))
                {
                    Auth();
                    return 0;
                }
                Send(str,"", tag);
                Logger::log("[AniDB Sent] Command: " + lastSentPacket, __FILE__, __LINE__);
            }
        }
    }
	Recv();
	return 0;
}

std::bitset<2> AniDBApi::LocalIdentify(int size, QString ed2khash)
{
	std::bitset<2> ret;
	QString q = QString("SELECT `fid` FROM `file` WHERE `size` = '%1' AND `ed2k` = '%2'").arg(size).arg(ed2khash);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Logger::log("[AniDB LocalIdentify] Database query error: " + query.lastError().text(), __FILE__, __LINE__);
		return ret;
	}
	
	int fid = 0;
	if(query.next())
	{
		fid = query.value(0).toInt();
		if(fid > 0)
		{
			ret[LI_FILE_IN_DB] = 1;  // File exists in local 'file' table
		}
	}
	
	q = QString("SELECT `lid` FROM `mylist` WHERE `fid` = '%1'").arg(fid);
	if(!query.exec(q))
	{
		Logger::log("[AniDB LocalIdentify] Database query error: " + query.lastError().text(), __FILE__, __LINE__);
		return ret;
	}
	
	if(query.next())
	{
		if(query.value(0).toInt() > 0)
		{
			ret[LI_FILE_IN_MYLIST] = 1;  // File exists in local 'mylist' table
		}
	}
//	}
	return ret;
}

QMap<QString, std::bitset<2>> AniDBApi::batchLocalIdentify(const QList<QPair<qint64, QString>>& sizeHashPairs)
{
	QMap<QString, std::bitset<2>> results;
	
	// Check if database is valid and open
	if (!db.isValid() || !db.isOpen())
	{
		LOG("Database not available, cannot perform batch LocalIdentify");
		return results;
	}
	
	if (sizeHashPairs.isEmpty())
	{
		return results;
	}
	
	// Initialize results for all files
	for (const auto& pair : sizeHashPairs)
	{
		QString key = QString("%1:%2").arg(pair.first).arg(pair.second);
		results[key] = std::bitset<2>(); // Initialize with 0,0
	}
	
	// Build a parameterized query to get all file information
	// We'll use multiple executions with bind values to avoid SQL injection
	QSqlQuery query(db);
	query.prepare("SELECT `fid`, `size`, `ed2k` FROM `file` WHERE `size` = ? AND `ed2k` = ?");
	
	// Store fids and mark files as in DB
	QMap<int, QString> fidToKey; // Map fid to our key
	
	for (const auto& pair : sizeHashPairs)
	{
		query.addBindValue(pair.first);  // size
		query.addBindValue(pair.second); // ed2k hash
		
		if (!query.exec())
		{
			LOG(QString("Batch LocalIdentify file query error: %1").arg(query.lastError().text()));
			continue; // Skip this file but continue with others
		}
		
		if (query.next())
		{
			int fid = query.value(0).toInt();
			qint64 size = query.value(1).toLongLong();
			QString ed2k = query.value(2).toString();
			QString key = QString("%1:%2").arg(size).arg(ed2k);
			
			if (fid > 0 && results.contains(key))
			{
				results[key][LI_FILE_IN_DB] = 1;
				fidToKey[fid] = key;
			}
		}
	}
	
	// Now check mylist for all found fids
	if (!fidToKey.isEmpty())
	{
		QStringList fidPlaceholders;
		QList<int> fids = fidToKey.keys();
		for (int i = 0; i < fids.size(); ++i)
		{
			fidPlaceholders.append("?");
		}
		
		QString mylistQuery = QString("SELECT `fid`, `lid` FROM `mylist` WHERE `fid` IN (%1)").arg(fidPlaceholders.join(","));
		query.prepare(mylistQuery);
		
		// Bind all fid values
		for (int fid : fids)
		{
			query.addBindValue(fid);
		}
		
		if (!query.exec())
		{
			LOG(QString("Batch LocalIdentify mylist query error: %1").arg(query.lastError().text()));
			return results;
		}
		
		while (query.next())
		{
			int fid = query.value(0).toInt();
			int lid = query.value(1).toInt();
			
			if (lid > 0 && fidToKey.contains(fid))
			{
				QString key = fidToKey[fid];
				results[key][LI_FILE_IN_MYLIST] = 1;
			}
		}
	}
	
	LOG(QString("Batch LocalIdentify completed for %1 file(s)").arg(sizeHashPairs.size()));
	return results;
}

void AniDBApi::UpdateFile(int size, QString ed2khash, int viewed, int state, QString storage)
{
	QString q = QString("SELECT `fid`,`lid` FROM `file` WHERE `size` = %1 AND `ed2k` = %2").arg(size).arg(ed2khash);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Logger::log("[AniDB UpdateFile] Database query error: " + query.lastError().text(), __FILE__, __LINE__);
		return;
	}
	
	if(query.next())
	{
		int lid = query.value(0).toInt();
		if(lid > 0)
		{
			q = QString("UPDATE `mylist` SET `viewed` = '%1', `state` = '%2', `storage` = '%3' WHERE `lid` = %4").arg(viewed).arg(state).arg(storage).arg(lid);
			if(!query.exec(q))
			{
				Logger::log("[AniDB UpdateFile] Database update error: " + query.lastError().text(), __FILE__, __LINE__);
				return;
			}
			if(query.numRowsAffected() == 1)
			{
				MylistAdd(size, ed2khash, viewed, state, storage, 1);
			}
			else if(query.numRowsAffected() == 0)
			{
				MylistAdd(size, ed2khash, viewed, state, storage, 0);
				//q = QString("INSERT INTO `mylist` ")
			}
		}
	}
}

int AniDBApi::UpdateLocalPath(QString tag, QString localPath)
{
	// Check if database is valid and open before using it
	if (!db.isValid() || !db.isOpen())
	{
		LOG("Database not available, cannot update local path");
		return 0;
	}
	
	// Get the original MYLISTADD command from packets table using the tag
	QSqlQuery query(db);
	query.prepare("SELECT `str` FROM `packets` WHERE `tag` = ?");
	query.addBindValue(tag);
	
	if(query.exec() && query.next())
	{
		QString mylistAddCmd = query.value(0).toString();
		
		// Parse size and ed2k from the MYLISTADD command
		QStringList params = mylistAddCmd.split("&");
		QString sizeStr, ed2k;
		
		for(const QString& param : params)
		{
			if(param.contains("size="))
				sizeStr = param.mid(param.indexOf("size=") + 5).split("&").first();
			else if(param.contains("ed2k="))
				ed2k = param.mid(param.indexOf("ed2k=") + 5).split("&").first();
		}
		
		// Find the lid using the file info
		// Convert size string to qint64 for proper type matching with database BIGINT column
		bool sizeOk = false;
		qint64 size = sizeStr.toLongLong(&sizeOk);
		if(!sizeOk)
		{
			LOG(QString("Error: Invalid size value in MYLISTADD command: %1").arg(sizeStr));
			return 0;
		}
		
		QSqlQuery lidQuery(db);
		lidQuery.prepare("SELECT m.lid FROM mylist m "
						 "INNER JOIN file f ON m.fid = f.fid "
						 "WHERE f.size = ? AND f.ed2k = ?");
		lidQuery.addBindValue(size);
		lidQuery.addBindValue(ed2k);
		
		if(lidQuery.exec() && lidQuery.next())
		{
			int lid = lidQuery.value(0).toInt();
			
			// Get the local_file id from local_files table
			QSqlQuery localFileQuery(db);
			localFileQuery.prepare("SELECT id FROM local_files WHERE path = ?");
			localFileQuery.addBindValue(localPath);
			
			if(localFileQuery.exec() && localFileQuery.next())
			{
				int localFileId = localFileQuery.value(0).toInt();
				
				// Update the local_file reference in mylist table
				QSqlQuery updateQuery(db);
				updateQuery.prepare("UPDATE `mylist` SET `local_file` = ? WHERE `lid` = ?");
				updateQuery.addBindValue(localFileId);
				updateQuery.addBindValue(lid);
				
				if(updateQuery.exec())
				{
					LOG(QString("Updated local_file for lid=%1 to local_file_id=%2 (path: %3)").arg(lid).arg(localFileId).arg(localPath));
					
					// Update status in local_files table to 2 (in anidb)
					QSqlQuery statusQuery(db);
					statusQuery.prepare("UPDATE `local_files` SET `status` = 2 WHERE `id` = ?");
					statusQuery.addBindValue(localFileId);
					statusQuery.exec();
					
					// Return the lid for use by the caller
					return lid;
				}
				else
				{
					LOG("Failed to update local_file: " + updateQuery.lastError().text());
				}
			}
			else
			{
				LOG("Could not find local_file entry for path=" + localPath);
			}
		}
		else
		{
			LOG("Could not find mylist entry for tag=" + tag);
		}
	}
	else
	{
		LOG("Could not find packet for tag=" + tag);
	}
	
	// Return 0 if we couldn't find or update the lid
	return 0;
}

int AniDBApi::LinkLocalFileToMylist(qint64 size, QString ed2kHash, QString localPath)
{
	// Check if database is valid and open before using it
	if (!db.isValid() || !db.isOpen())
	{
		LOG("Database not available, cannot link local file to mylist");
		return 0;
	}
	
	// Find the lid using the file info
	QSqlQuery lidQuery(db);
	lidQuery.prepare("SELECT m.lid FROM mylist m "
					 "INNER JOIN file f ON m.fid = f.fid "
					 "WHERE f.size = ? AND f.ed2k = ?");
	lidQuery.addBindValue(size);
	lidQuery.addBindValue(ed2kHash);
	
	if(lidQuery.exec() && lidQuery.next())
	{
		int lid = lidQuery.value(0).toInt();
		
		// Get the local_file id from local_files table
		QSqlQuery localFileQuery(db);
		localFileQuery.prepare("SELECT id FROM local_files WHERE path = ?");
		localFileQuery.addBindValue(localPath);
		
		if(localFileQuery.exec() && localFileQuery.next())
		{
			int localFileId = localFileQuery.value(0).toInt();
			
			// Update the local_file reference in mylist table
			QSqlQuery updateQuery(db);
			updateQuery.prepare("UPDATE `mylist` SET `local_file` = ? WHERE `lid` = ?");
			updateQuery.addBindValue(localFileId);
			updateQuery.addBindValue(lid);
			
			if(updateQuery.exec())
			{
				LOG(QString("Linked local_file for lid=%1 to local_file_id=%2 (path: %3)").arg(lid).arg(localFileId).arg(localPath));
				
				// Update status in local_files table to 2 (in anidb)
				QSqlQuery statusQuery(db);
				statusQuery.prepare("UPDATE `local_files` SET `status` = 2 WHERE `id` = ?");
				statusQuery.addBindValue(localFileId);
				statusQuery.exec();
				
				// Return the lid for use by the caller
				return lid;
			}
			else
			{
				LOG("Failed to link local_file: " + updateQuery.lastError().text());
			}
		}
		else
		{
			LOG("Could not find local_file entry for path=" + localPath);
		}
	}
	else
	{
		LOG(QString("Could not find mylist entry for size=%1 ed2k=%2").arg(size).arg(ed2kHash));
	}
	
	// Return 0 if we couldn't find or update the lid
	return 0;
}


void AniDBApi::UpdateLocalFileStatus(QString localPath, int status)
{
	// Check if database is valid and open before using it
	if (!db.isValid() || !db.isOpen())
	{
		LOG("Database not available, cannot update local file status");
		return;
	}
	
	// Update the status in local_files table
	QSqlQuery query(db);
	query.prepare("UPDATE `local_files` SET `status` = ? WHERE `path` = ?");
	query.addBindValue(status);
	query.addBindValue(localPath);
	
	if(query.exec())
	{
		LOG(QString("Updated local_files status for path=%1 to status=%2").arg(localPath).arg(status));
	}
	else
	{
		LOG("Failed to update local_files status: " + query.lastError().text());
	}
}

void AniDBApi::updateLocalFileHash(QString localPath, QString ed2kHash, int status)
{
	// Check if database is valid and open before using it
	if (!db.isValid() || !db.isOpen())
	{
		LOG("Database not available, cannot update local file hash");
		return;
	}
	
	// Update the ed2k_hash and status in local_files table
	// Status: 0=not hashed, 1=hashed but not checked by API, 2=in anidb, 3=not in anidb
	QSqlQuery query(db);
	query.prepare("UPDATE `local_files` SET `ed2k_hash` = ?, `status` = ? WHERE `path` = ?");
	query.addBindValue(ed2kHash);
	query.addBindValue(status);
	query.addBindValue(localPath);
	
	if(query.exec())
	{
		LOG(QString("Updated local_files hash and status for path=%1 to status=%2").arg(localPath).arg(status));
	}
	else
	{
		LOG("Failed to update local_files hash and status: " + query.lastError().text());
	}
}

void AniDBApi::batchUpdateLocalFileHashes(const QList<QPair<QString, QString>>& pathHashPairs, int status)
{
	// Check if database is valid and open before using it
	if (!db.isValid() || !db.isOpen())
	{
		LOG("Database not available, cannot batch update local file hashes");
		return;
	}
	
	if (pathHashPairs.isEmpty())
	{
		return;
	}
	
	// Begin transaction for batch update
	if (!db.transaction())
	{
		LOG("Failed to begin transaction for batch update: " + db.lastError().text());
		return;
	}
	
	QSqlQuery query(db);
	query.prepare("UPDATE `local_files` SET `ed2k_hash` = ?, `status` = ? WHERE `path` = ?");
	
	int successCount = 0;
	int failCount = 0;
	bool hasFailure = false;
	
	// Batch all updates in a single transaction
	for (const auto& pair : pathHashPairs)
	{
		query.addBindValue(pair.second); // ed2k hash
		query.addBindValue(status);
		query.addBindValue(pair.first);  // path
		
		if (query.exec())
		{
			successCount++;
		}
		else
		{
			failCount++;
			hasFailure = true;
			LOG(QString("Failed to update file %1: %2").arg(pair.first).arg(query.lastError().text()));
		}
	}
	
	// Rollback if any failures occurred to maintain consistency
	if (hasFailure)
	{
		LOG(QString("Rolling back batch update due to %1 failure(s)").arg(failCount));
		db.rollback();
		return;
	}
	
	// Commit transaction only if all updates succeeded
	if (!db.commit())
	{
		LOG("Failed to commit batch update transaction: " + db.lastError().text());
		db.rollback();
		return;
	}
	
	// Log summary
	Logger::log(QString("Batch updated %1 file(s) to status=%2 (all successful)")
	            .arg(pathHashPairs.size()).arg(status), __FILE__, __LINE__);
}

QString AniDBApi::getLocalFileHash(QString localPath)
{
	// Get the database connection for this thread
	// SQLite connections are not thread-safe, so we need a separate connection per thread
	QSqlDatabase threadDb;
	// Cast to quintptr ensures thread ID can be safely converted to string on all platforms
	QString connectionName = QString("hash_query_thread_%1").arg((quintptr)QThread::currentThreadId());
	
	if (QSqlDatabase::contains(connectionName))
	{
		threadDb = QSqlDatabase::database(connectionName);
	}
	else
	{
		// Verify the main database connection is valid before cloning it
		if (!db.isValid() || db.databaseName().isEmpty())
		{
			LOG("Main database connection is invalid, cannot create thread-local connection");
			return QString();
		}
		
		// Create a new connection for this thread
		threadDb = QSqlDatabase::addDatabase("QSQLITE", connectionName);
		threadDb.setDatabaseName(db.databaseName());
		
		if (!threadDb.open())
		{
			LOG(QString("Failed to open thread-local database connection: %1").arg(threadDb.lastError().text()));
			return QString();
		}
	}
	
	// Check if database is valid and open
	if (!threadDb.isValid() || !threadDb.isOpen())
	{
		LOG(QString("Database not available, cannot retrieve hash for path=%1").arg(localPath));
		return QString();
	}
	
	// Retrieve the ed2k_hash from local_files table for the given path
	QSqlQuery query(threadDb);
	query.prepare("SELECT `ed2k_hash` FROM `local_files` WHERE `path` = ? AND `ed2k_hash` IS NOT NULL AND `ed2k_hash` != ''");
	query.addBindValue(localPath);
	
	if(!query.exec())
	{
		LOG(QString("Database query failed for path=%1, error: %2").arg(localPath).arg(query.lastError().text()));
		return QString();
	}
	
	if(query.next())
	{
		QString hash = query.value(0).toString();
		LOG(QString("Retrieved existing hash for path=%1").arg(localPath));
		return hash;
	}
	
	// No hash found for this file
//	LOG(QString("No existing hash found for path=%1").arg(localPath));
	return QString();
}

QMap<QString, AniDBApi::FileHashInfo> AniDBApi::batchGetLocalFileHashes(const QStringList& filePaths)
{
	QElapsedTimer overallTimer;
	overallTimer.start();
	
	QMap<QString, FileHashInfo> results;
	
	// Check if database is valid and open
	if (!db.isValid() || !db.isOpen())
	{
		LOG("Database not available, cannot perform batch hash retrieval");
		return results;
	}
	
	if (filePaths.isEmpty())
	{
		return results;
	}
	
	qint64 checkTime = overallTimer.elapsed();
	LOG(QString("[TIMING] batchGetLocalFileHashes initial checks: %1 ms [anidbapi.cpp]").arg(checkTime));
	
	// Build a query with IN clause for batch retrieval
	QElapsedTimer buildQueryTimer;
	buildQueryTimer.start();
	QStringList placeholders;
	placeholders.reserve(filePaths.size());  // Pre-allocate to avoid reallocations
	for (int i = 0; i < filePaths.size(); ++i)
	{
		placeholders.append("?");
	}
	
	QString queryStr = QString("SELECT `path`, `ed2k_hash`, `status` FROM `local_files` WHERE `path` IN (%1)")
	                   .arg(placeholders.join(","));
	
	qint64 buildQueryTime = buildQueryTimer.elapsed();
	LOG(QString("[TIMING] Query string build for %1 paths: %2 ms [anidbapi.cpp]")
		.arg(filePaths.size()).arg(buildQueryTime));
	
	QElapsedTimer prepareTimer;
	prepareTimer.start();
	QSqlQuery query(db);
	query.prepare(queryStr);
	
	// Bind all file paths
	for (const QString& path : filePaths)
	{
		query.addBindValue(path);
	}
	qint64 prepareTime = prepareTimer.elapsed();
	LOG(QString("[TIMING] Query prepare and bind for %1 paths: %2 ms [anidbapi.cpp]")
		.arg(filePaths.size()).arg(prepareTime));
	
	QElapsedTimer execTimer;
	execTimer.start();
	if (!query.exec())
	{
		LOG(QString("Batch hash retrieval query failed: %1").arg(query.lastError().text()));
		return results;
	}
	qint64 execTime = execTimer.elapsed();
	LOG(QString("[TIMING] Query exec for %1 paths: %2 ms [anidbapi.cpp]")
		.arg(filePaths.size()).arg(execTime));
	
	// Process results
	QElapsedTimer processTimer;
	processTimer.start();
	while (query.next())
	{
		FileHashInfo info;
		info.path = query.value(0).toString();
		info.hash = query.value(1).toString();
		info.status = query.value(2).toInt();
		
		results[info.path] = info;
	}
	qint64 processTime = processTimer.elapsed();
	LOG(QString("[TIMING] Query result processing: %1 ms [anidbapi.cpp]").arg(processTime));
	
	qint64 totalTime = overallTimer.elapsed();
	Logger::log(QString("[TIMING] batchGetLocalFileHashes TOTAL for %1 paths (found %2): %3 ms [anidbapi.cpp]")
	            .arg(filePaths.size()).arg(results.size()).arg(totalTime), __FILE__, __LINE__);
	
	return results;
}

QString AniDBApi::GetTag(QString str)
{
	QString q = QString("SELECT `tag` FROM `packets` WHERE `str` = '%1' AND `processed` = '0' ORDER BY `tag` ASC LIMIT 1").arg(str);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Logger::log("[AniDB GetTag] Database query error: " + query.lastError().text(), __FILE__, __LINE__);
		return "0";
	}
	
	if(query.next())
	{
		return query.value(0).toString();
	}
	return "0";
}

// Anime Titles Download Implementation

bool AniDBApi::shouldUpdateAnimeTitles()
{
	Logger::log("[AniDB Anime Titles] Checking if anime titles need update", __FILE__, __LINE__);
	// Check if we should download anime titles
	// Download if: never downloaded before OR last update was more than 24 hours ago
	// Read from database to ensure we have the most up-to-date timestamp
	QSqlQuery query(db);
	query.exec("SELECT `value` FROM `settings` WHERE `name` = 'last_anime_titles_update'");
	
	if(!query.next())
	{
		// Never downloaded before
		Logger::log("[AniDB Anime Titles] No previous download found, download needed", __FILE__, __LINE__);
		return true;
	}
	
	QDateTime lastUpdate = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
	qint64 secondsSinceLastUpdate = lastUpdate.secsTo(QDateTime::currentDateTime());
	bool needsUpdate = secondsSinceLastUpdate > 86400; // 86400 seconds = 24 hours
	Logger::log("[AniDB Anime Titles] Last update was " + QString::number(secondsSinceLastUpdate) + " seconds ago, needs update: " + (needsUpdate ? "yes" : "no"), __FILE__, __LINE__);
	return needsUpdate;
}

void AniDBApi::downloadAnimeTitles()
{
	LOG("Downloading anime titles from AniDB...");
	
	QNetworkRequest request{QUrl("http://anidb.net/api/anime-titles.dat.gz")};
	request.setHeader(QNetworkRequest::UserAgentHeader, QString("Usagi/%1").arg(clientver));
	
	networkManager->get(request);
}

void AniDBApi::onAnimeTitlesDownloaded(QNetworkReply *reply)
{
	// Check if this reply is for the anime titles download
	QUrl requestUrl = reply->request().url();
	if(requestUrl.toString() != "http://anidb.net/api/anime-titles.dat.gz")
	{
		// This reply is for a different request, ignore it
		reply->deleteLater();
		return;
	}
	
	Logger::log("[AniDB Anime Titles] Download callback triggered", __FILE__, __LINE__);
	if(reply->error() != QNetworkReply::NoError)
	{
		LOG(QString("Failed to download anime titles: %1").arg(reply->errorString()));
		reply->deleteLater();
		return;
	}
	
	QByteArray compressedData = reply->readAll();
	reply->deleteLater();
	
	LOG(QString("Downloaded %1 bytes of compressed anime titles data").arg(compressedData.size()));
	
	// The file is in gzip format, which uses deflate compression
	// We need to decompress it properly using zlib
	QByteArray decompressedData;
	
	// Check if it's gzip format (starts with 0x1f 0x8b)
	Logger::log("[AniDB Anime Titles] Starting decompression", __FILE__, __LINE__);
	if(compressedData.size() >= 2 && 
	   (unsigned char)compressedData[0] == 0x1f && 
	   (unsigned char)compressedData[1] == 0x8b)
	{
		Logger::log("[AniDB Anime Titles] Detected gzip format, using zlib decompression", __FILE__, __LINE__);
		// It's gzip format - use zlib to decompress
		// Initialize zlib stream
		z_stream stream;
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;
		stream.avail_in = compressedData.size();
		stream.next_in = (unsigned char*)compressedData.data();
		
		// Initialize for gzip decompression
		// windowBits = 15 (default) + 16 (gzip format) = 31
		int ret = inflateInit2(&stream, 15 + 16);
		if(ret != Z_OK)
		{
			LOG(QString("Failed to initialize gzip decompression: %1").arg(ret));
			return;
		}
		
		// Allocate output buffer - start with 4MB which should handle most anime titles files
		const int CHUNK = 4 * 1024 * 1024;
		unsigned char* out = new unsigned char[CHUNK];
		
		Logger::log("[AniDB Anime Titles] Starting inflate operation (this may take a moment)", __FILE__, __LINE__);
		// Decompress
		do {
			stream.avail_out = CHUNK;
			stream.next_out = out;
			
			ret = inflate(&stream, Z_NO_FLUSH);
			if(ret == Z_STREAM_ERROR || ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
			{
				LOG(QString("Gzip decompression failed: %1").arg(ret));
				delete[] out;
				inflateEnd(&stream);
				return;
			}
			
			int have = CHUNK - stream.avail_out;
			decompressedData.append((char*)out, have);
		} while(ret != Z_STREAM_END);
		
		Logger::log("[AniDB Anime Titles] Decompression completed successfully", __FILE__, __LINE__);
		// Clean up
		delete[] out;
		inflateEnd(&stream);
	}
	else
	{
		Logger::log("[AniDB Anime Titles] Not gzip format, trying qUncompress", __FILE__, __LINE__);
		// Try direct decompression with qUncompress (for zlib format)
		decompressedData = qUncompress(compressedData);
	}
	
	if(decompressedData.isEmpty())
	{
		LOG("Failed to decompress anime titles data. Will retry on next startup.");
		return;
	}
	
	LOG(QString("Decompressed to %1 bytes").arg(decompressedData.size()));
	
	Logger::log("[AniDB Anime Titles] Starting to parse and store titles", __FILE__, __LINE__);
	parseAndStoreAnimeTitles(decompressedData);
	Logger::log("[AniDB Anime Titles] Finished parsing and storing titles", __FILE__, __LINE__);
	
	// Update last download timestamp
	lastAnimeTitlesUpdate = QDateTime::currentDateTime();
	QSqlQuery query(db);
	QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'last_anime_titles_update', '%1');")
				.arg(lastAnimeTitlesUpdate.toSecsSinceEpoch());
	query.exec(q);
	
	LOG(QString("Anime titles updated successfully at %1").arg(lastAnimeTitlesUpdate.toString()));
}

void AniDBApi::parseAndStoreAnimeTitles(const QByteArray &data)
{
	if(data.isEmpty())
	{
		LOG("No data to parse for anime titles");
		return;
	}
	
	Logger::log("[AniDB Anime Titles] Starting to parse anime titles data (" + QString::number(data.size()) + " bytes)", __FILE__, __LINE__);
	QString content = QString::fromUtf8(data);
	QStringList lines = content.split('\n', Qt::SkipEmptyParts);
	
	Logger::log("[AniDB Anime Titles] Starting database transaction for " + QString::number(lines.size()) + " lines", __FILE__, __LINE__);
	db.transaction();
	
	// Clear old titles
	QSqlQuery query(db);
	Logger::log("[AniDB Anime Titles] Clearing old anime titles from database", __FILE__, __LINE__);
	query.exec("DELETE FROM `anime_titles`");
	
	int count = 0;
	int progressInterval = 1000; // Report progress every 1000 titles
	for(const QString &line : lines)
	{
		// Skip comments and empty lines
		if(line.startsWith('#') || line.trimmed().isEmpty())
		{
			continue;
		}
		
		// Format: aid|type|language|title
		QStringList parts = line.split('|');
		if(parts.size() >= 4)
		{
			QString aid = parts[0].trimmed();
			QString type = parts[1].trimmed();
			QString language = parts[2].trimmed();
			QString title = parts[3].trimmed();
			
			// Escape single quotes for SQL
			title = title.replace("'", "''");
			language = language.replace("'", "''");
			
			QString q = QString("INSERT OR IGNORE INTO `anime_titles` (`aid`, `type`, `language`, `title`) VALUES ('%1', '%2', '%3', '%4')")
						.arg(aid).arg(type).arg(language).arg(title);
			query.exec(q);
			count++;
			
			// Report progress periodically to show it's not frozen
			if(count % progressInterval == 0)
			{
				Logger::log("[AniDB Anime Titles] Processing progress: " + QString::number(count) + " titles inserted", __FILE__, __LINE__);
			}
		}
	}
	
	Logger::log("[AniDB Anime Titles] Committing database transaction with " + QString::number(count) + " titles", __FILE__, __LINE__);
	db.commit();
	Logger::log("[AniDB Anime Titles] Parsed and stored " + QString::number(count) + " anime titles successfully", __FILE__, __LINE__);
}

void AniDBApi::checkForNotifications()
{
	if(!isExportQueued)
	{
		Logger::log("[AniDB Export] No export queued, stopping notification checks", __FILE__, __LINE__);
		notifyCheckTimer->stop();
		return;
	}
	
	// Check if we've exceeded 48 hours since export was queued
	qint64 elapsedSeconds = QDateTime::currentSecsSinceEpoch() - exportQueuedTimestamp;
	qint64 elapsedHours = elapsedSeconds / 3600;
	if(elapsedSeconds > 48 * 3600)  // 48 hours = 172800 seconds
	{
		Logger::log("[AniDB Export] Stopping notification checks after 48 hours", __FILE__, __LINE__);
		notifyCheckTimer->stop();
		isExportQueued = false;
		notifyCheckAttempts = 0;
		notifyCheckIntervalMs = 60000; // Reset to 1 minute for next export
		exportQueuedTimestamp = 0;
		saveExportQueueState();
		return;
	}
	
	notifyCheckAttempts++;
	int intervalMinutes = notifyCheckIntervalMs / 60000;
	Logger::log("[AniDB Export] Periodic notification check (attempt " + QString::number(notifyCheckAttempts) + ", interval: " + QString::number(intervalMinutes) + " minutes, elapsed: " + QString::number(elapsedHours) + " hours)", __FILE__, __LINE__);
	
	// Check for new notifications by requesting NOTIFYLIST
	if(SID.length() > 0 && LoginStatus() > 0)
	{
		// Request notification list to check for export ready notification
		QString msg = buildNotifyListCommand();
		QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
		QSqlQuery query(db);
		query.exec(q);
		Logger::log("[AniDB Export] Requesting NOTIFYLIST to check for export notification", __FILE__, __LINE__);
		
		// Increase check interval by 1 minute after each successful check attempt
		// Cap at 60 minutes to avoid very long waits
		notifyCheckIntervalMs += 60000;
		if(notifyCheckIntervalMs > 3600000)  // Cap at 60 minutes
		{
			notifyCheckIntervalMs = 3600000;
		}
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		int nextIntervalMinutes = notifyCheckIntervalMs / 60000;
		Logger::log("[AniDB Export] Next check will be in " + QString::number(nextIntervalMinutes) + " minutes", __FILE__, __LINE__);
	}
	else
	{
		Logger::log("[AniDB Export] Not logged in, skipping notification check", __FILE__, __LINE__);
		// Don't increase interval when not logged in - keep trying at the same interval
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		int nextIntervalMinutes = notifyCheckIntervalMs / 60000;
		Logger::log("[AniDB Export] Will retry in " + QString::number(nextIntervalMinutes) + " minutes after login", __FILE__, __LINE__);
	}
	
	// Save state after each check
	saveExportQueueState();
}

void AniDBApi::saveExportQueueState()
{
	QSqlQuery query(db);
	
	// Save isExportQueued
	QString q1 = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'export_queued', '%1');").arg(isExportQueued ? "1" : "0");
	query.exec(q1);
	
	// Save notifyCheckAttempts
	QString q2 = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'export_check_attempts', '%1');").arg(notifyCheckAttempts);
	query.exec(q2);
	
	// Save notifyCheckIntervalMs
	QString q3 = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'export_check_interval_ms', '%1');").arg(notifyCheckIntervalMs);
	query.exec(q3);
	
	// Save exportQueuedTimestamp
	QString q4 = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'export_queued_timestamp', '%1');").arg(exportQueuedTimestamp);
	query.exec(q4);
	
	Logger::log("[AniDB Export] Saved export queue state to database", __FILE__, __LINE__);
}

void AniDBApi::loadExportQueueState()
{
	QSqlQuery query(db);
	query.exec("SELECT `name`, `value` FROM `settings` WHERE `name` IN ('export_queued', 'export_check_attempts', 'export_check_interval_ms', 'export_queued_timestamp')");
	
	bool hadExportQueued = false;
	
	while(query.next())
	{
		QString name = query.value(0).toString();
		QString value = query.value(1).toString();
		
		if(name == "export_queued")
		{
			isExportQueued = (value == "1");
			hadExportQueued = isExportQueued;
		}
		else if(name == "export_check_attempts")
		{
			notifyCheckAttempts = value.toInt();
		}
		else if(name == "export_check_interval_ms")
		{
			notifyCheckIntervalMs = value.toInt();
		}
		else if(name == "export_queued_timestamp")
		{
			exportQueuedTimestamp = value.toLongLong();
		}
	}
	
	if(hadExportQueued)
	{
		Logger::log("[AniDB Export] Loaded export queue state from database - queued since " + QDateTime::fromSecsSinceEpoch(exportQueuedTimestamp).toString(), __FILE__, __LINE__);
		
		// Check if export has already been ready (check for existing notification first)
		// This will be triggered after login when we have a session
		// Use QTimer with single-shot mode instead of QTimer::singleShot to avoid chrono overload
		// The chrono overload is not available in static Qt 6.9.2 builds with LLVM MinGW
		QTimer *delayTimer = new QTimer(this);
		delayTimer->setSingleShot(true);
		connect(delayTimer, &QTimer::timeout, this, &AniDBApi::checkForExistingExport);
		connect(delayTimer, &QTimer::timeout, delayTimer, &QTimer::deleteLater);
		delayTimer->start(5000);
	}
	else
	{
		Logger::log("[AniDB Export] No pending export found in database", __FILE__, __LINE__);
	}
}

void AniDBApi::checkForExistingExport()
{
	if(!isExportQueued)
	{
		Logger::log("[AniDB Export] No export queued, skipping check for existing export", __FILE__, __LINE__);
		return;
	}
	
	// Check if we've exceeded 48 hours since export was queued
	qint64 elapsedSeconds = QDateTime::currentSecsSinceEpoch() - exportQueuedTimestamp;
	if(elapsedSeconds > 48 * 3600)  // 48 hours
	{
		Logger::log("[AniDB Export] Export queue expired (>48 hours), clearing state", __FILE__, __LINE__);
		isExportQueued = false;
		notifyCheckAttempts = 0;
		notifyCheckIntervalMs = 60000;
		exportQueuedTimestamp = 0;
		saveExportQueueState();
		return;
	}
	
	Logger::log("[AniDB Export] Checking for existing export notification on startup", __FILE__, __LINE__);
	
	// Check if we're logged in
	if(SID.length() > 0 && LoginStatus() > 0)
	{
		// Request notification list to check if export is already ready
		QString msg = buildNotifyListCommand();
		QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
		QSqlQuery query(db);
		query.exec(q);
		Logger::log("[AniDB Export] Requested NOTIFYLIST to check for existing export", __FILE__, __LINE__);
		
		// Resume periodic checking with the saved interval
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		notifyCheckTimer->start();
		Logger::log("[AniDB Export] Resumed periodic notification checking", __FILE__, __LINE__);
	}
	else
	{
		Logger::log("[AniDB Export] Not logged in yet, will check after login", __FILE__, __LINE__);
		// The check will be triggered again after login via the timer
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		notifyCheckTimer->start();
	}
}

// ===== Mask Processing Helper Functions =====

/**
 * Parse FILE command response using fmask to determine which fields are present.
 * Processes mask bits in strict MSB to LSB order using a loop to ensure correctness.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param fmask File mask indicating which fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return FileData structure with parsed fields
 */
AniDBApi::FileData AniDBApi::parseFileMask(const QStringList& tokens, unsigned int fmask, int& index)
{
	FileData data;
	
	// FID is always returned first in FILE responses, regardless of mask
	// It's not part of the fmask-controlled fields
	data.fid = tokens.value(0);
	
	// Process fmask bits from MSB (bit 30) to LSB (bit 0) in strict order using a loop
	// This ensures correctness even if individual if-statements are reordered
	
	// Define all mask bits in MSB to LSB order
	struct MaskBit {
		unsigned int bit;
		QString* field;
	};
	
	MaskBit maskBits[] = {
		{fAID,            &data.aid},              // Bit 30
		{fEID,            &data.eid},              // Bit 29
		{fGID,            &data.gid},              // Bit 28
		{fLID,            &data.lid},              // Bit 27
		{fOTHEREPS,       &data.othereps},         // Bit 26
		{fISDEPR,         &data.isdepr},           // Bit 25
		{fSTATE,          &data.state},            // Bit 24
		{fSIZE,           &data.size},             // Bit 23
		{fED2K,           &data.ed2k},             // Bit 22
		{fMD5,            &data.md5},              // Bit 21
		{fSHA1,           &data.sha1},             // Bit 20
		{fCRC32,          &data.crc},              // Bit 19
		// Bits 18-16 reserved
		{fQUALITY,        &data.quality},          // Bit 15
		{fSOURCE,         &data.source},           // Bit 14
		{fCODEC_AUDIO,    &data.codec_audio},      // Bit 13
		{fBITRATE_AUDIO,  &data.bitrate_audio},    // Bit 12
		{fCODEC_VIDEO,    &data.codec_video},      // Bit 11
		{fBITRATE_VIDEO,  &data.bitrate_video},    // Bit 10
		{fRESOLUTION,     &data.resolution},       // Bit 9
		{fFILETYPE,       &data.filetype},         // Bit 8
		{fLANG_DUB,       &data.lang_dub},         // Bit 7
		{fLANG_SUB,       &data.lang_sub},         // Bit 6
		{fLENGTH,         &data.length},           // Bit 5
		{fDESCRIPTION,    &data.description},      // Bit 4
		{fAIRDATE,        &data.airdate},          // Bit 3
		// Bits 2-1 reserved
		{fFILENAME,       &data.filename}          // Bit 0
	};
	
	// Process mask bits in order using a loop
	// This ensures fields are extracted in the correct sequence
	for (size_t i = 0; i < sizeof(maskBits) / sizeof(MaskBit); i++)
	{
		if (fmask & maskBits[i].bit)
		{
			*(maskBits[i].field) = tokens.value(index++);
		}
	}
	
	return data;
}

/**
 * Parse anime data from FILE command response using file_amask.
 * Processes mask bits in strict MSB to LSB order using a loop to ensure correctness.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amask Anime mask indicating which anime fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return AnimeData structure with parsed anime fields
 */
AniDBApi::AnimeData AniDBApi::parseFileAmaskAnimeData(const QStringList& tokens, unsigned int amask, int& index)
{
	AnimeData data;
	
	// Process file_amask bits from MSB to LSB for anime data using a loop
	// This ensures correctness even if individual if-statements are reordered
	
	// Define all mask bits in MSB to LSB order
	struct MaskBit {
		unsigned int bit;
		QString* field;
	};
	
	MaskBit maskBits[] = {
		{aEPISODE_TOTAL,      &data.eptotal},      // Bit 31
		{aEPISODE_LAST,       &data.eplast},       // Bit 30
		{aANIME_YEAR,         &data.year},         // Bit 29
		{aANIME_TYPE,         &data.type},         // Bit 28
		{aANIME_RELATED_LIST, &data.relaidlist},   // Bit 27
		{aANIME_RELATED_TYPE, &data.relaidtype},   // Bit 26
		{aANIME_CATAGORY,     &data.category},     // Bit 25
		// Bit 24 reserved
		{aANIME_NAME_ROMAJI,  &data.nameromaji},   // Bit 23
		{aANIME_NAME_KANJI,   &data.namekanji},    // Bit 22
		{aANIME_NAME_ENGLISH, &data.nameenglish},  // Bit 21
		{aANIME_NAME_OTHER,   &data.nameother},    // Bit 20
		{aANIME_NAME_SHORT,   &data.nameshort},    // Bit 19
		{aANIME_SYNONYMS,     &data.synonyms}      // Bit 18
		// Bits 17-14 reserved
	};
	
	// Process mask bits in order using a loop
	for (size_t i = 0; i < sizeof(maskBits) / sizeof(MaskBit); i++)
	{
		if (amask & maskBits[i].bit)
		{
			*(maskBits[i].field) = tokens.value(index++);
		}
	}
	
	return data;
}

/**
 * Parse episode data from FILE command response using file_amask.
 * Processes mask bits in strict MSB to LSB order using a loop to ensure correctness.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amask Anime mask indicating which episode fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return EpisodeData structure with parsed episode fields
 */
AniDBApi::EpisodeData AniDBApi::parseFileAmaskEpisodeData(const QStringList& tokens, unsigned int amask, int& index)
{
	EpisodeData data;
	
	// Process file_amask bits for episode data using a loop
	// This ensures correctness even if individual if-statements are reordered
	
	// Define all mask bits in MSB to LSB order
	struct MaskBit {
		unsigned int bit;
		QString* field;
	};
	
	MaskBit maskBits[] = {
		{aEPISODE_NUMBER,      &data.epno},         // Bit 15
		{aEPISODE_NAME,        &data.epname},       // Bit 14
		{aEPISODE_NAME_ROMAJI, &data.epnameromaji}, // Bit 13
		{aEPISODE_NAME_KANJI,  &data.epnamekanji},  // Bit 12
		{aEPISODE_RATING,      &data.eprating},     // Bit 11
		{aEPISODE_VOTE_COUNT,  &data.epvotecount}   // Bit 10
		// Bits 9-8 reserved
	};
	
	// Process mask bits in order using a loop
	for (size_t i = 0; i < sizeof(maskBits) / sizeof(MaskBit); i++)
	{
		if (amask & maskBits[i].bit)
		{
			*(maskBits[i].field) = tokens.value(index++);
		}
	}
	
	return data;
}

/**
 * Parse group data from FILE command response using file_amask.
 * Processes mask bits in strict MSB to LSB order using a loop to ensure correctness.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amask Anime mask indicating which group fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return GroupData structure with parsed group fields
 */
AniDBApi::GroupData AniDBApi::parseFileAmaskGroupData(const QStringList& tokens, unsigned int amask, int& index)
{
	GroupData data;
	
	// Process file_amask bits for group data using a loop
	// This ensures correctness even if individual if-statements are reordered
	
	// Define all mask bits in MSB to LSB order
	struct MaskBit {
		unsigned int bit;
		QString* field;
	};
	
	MaskBit maskBits[] = {
		{aGROUP_NAME,              &data.groupname},      // Bit 7
		{aGROUP_NAME_SHORT,        &data.groupshortname}, // Bit 6
		// Bits 5-1 reserved
		{aDATE_AID_RECORD_UPDATED, nullptr}              // Bit 0 - Not stored
	};
	
	// Process mask bits in order using a loop
	for (size_t i = 0; i < sizeof(maskBits) / sizeof(MaskBit); i++)
	{
		if (amask & maskBits[i].bit)
		{
			QString value = tokens.value(index++);
			if (maskBits[i].field != nullptr)
			{
				*(maskBits[i].field) = value;
			}
			// else: field is not stored (marked with nullptr)
		}
	}
	
	return data;
}

/**
 * Parse anime data from ANIME command response using anime_amask.
 * Processes mask bits in strict MSB to LSB order using a loop to ensure correctness.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amask Anime mask indicating which anime fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return AnimeData structure with parsed anime fields
 */
AniDBApi::AnimeData AniDBApi::parseAnimeMask(const QStringList& tokens, unsigned int amask, int& index)
{
	AnimeData data;
	
	// Note: The ANIME command response order is determined by the amask bits
	// Process from highest bit to lowest bit (MSB to LSB) in strict order
	// Byte 1 bits 7-0, then Byte 2 bits 7-0, then Byte 3 bits 7-0, etc.
	// This ensures correctness even if individual if-statements are reordered
	
	// Define all mask bits in MSB to LSB order (bit 7 of each byte first)
	struct MaskBit {
		unsigned int bit;
		QString* field;
		const char* name;  // For debug logging
	};
	
	MaskBit maskBits[] = {
		// Byte 1 (bits 7-0) - basic anime info
		{ANIME_AID,             &data.aid,                  "AID"},                   // Byte 1, bit 7 (dec 128)
		{ANIME_DATEFLAGS,       &data.dateflags,            "DATEFLAGS"},             // Byte 1, bit 6 (dec 64)
		{ANIME_YEAR,            &data.year,                 "YEAR"},                  // Byte 1, bit 5 (dec 32)
		{ANIME_TYPE,            &data.type,                 "TYPE"},                  // Byte 1, bit 4 (dec 16)
		{ANIME_RELATED_AID_LIST,&data.relaidlist,           "RELATED_AID_LIST"},      // Byte 1, bit 3 (dec 8)
		{ANIME_RELATED_AID_TYPE,&data.relaidtype,           "RELATED_AID_TYPE"},      // Byte 1, bit 2 (dec 4)
		// Byte 1, bits 1-0 are retired
		
		// Byte 2 (bits 7-0) - name variations
		{ANIME_ROMAJI_NAME,     &data.nameromaji,           "ROMAJI_NAME"},           // Byte 2, bit 7 (dec 128)
		{ANIME_KANJI_NAME,      &data.namekanji,            "KANJI_NAME"},            // Byte 2, bit 6 (dec 64)
		{ANIME_ENGLISH_NAME,    &data.nameenglish,          "ENGLISH_NAME"},          // Byte 2, bit 5 (dec 32)
		{ANIME_OTHER_NAME,      &data.nameother,            "OTHER_NAME"},            // Byte 2, bit 4 (dec 16)
		{ANIME_SHORT_NAME_LIST, &data.nameshort,            "SHORT_NAME_LIST"},       // Byte 2, bit 3 (dec 8)
		{ANIME_SYNONYM_LIST,    &data.synonyms,             "SYNONYM_LIST"},          // Byte 2, bit 2 (dec 4)
		// Byte 2, bits 1-0 are retired
		
		// Byte 3 (bits 7-0) - episodes and dates
		{ANIME_EPISODES,        &data.episodes,             "EPISODES"},              // Byte 3, bit 7 (dec 128)
		{ANIME_HIGHEST_EPISODE, &data.highest_episode,      "HIGHEST_EPISODE"},       // Byte 3, bit 6 (dec 64)
		{ANIME_SPECIAL_EP_COUNT,&data.special_ep_count,     "SPECIAL_EP_COUNT"},      // Byte 3, bit 5 (dec 32)
		{ANIME_AIR_DATE,        &data.air_date,             "AIR_DATE"},              // Byte 3, bit 4 (dec 16)
		{ANIME_END_DATE,        &data.end_date,             "END_DATE"},              // Byte 3, bit 3 (dec 8)
		{ANIME_URL,             &data.url,                  "URL"},                   // Byte 3, bit 2 (dec 4)
		{ANIME_PICNAME,         &data.picname,              "PICNAME"},               // Byte 3, bit 1 (dec 2)
		// Byte 3, bit 0 is retired
		
		// Byte 4 (bits 7-0) - ratings and reviews
		{ANIME_RATING,          &data.rating,               "RATING"},                // Byte 4, bit 7 (dec 128)
		{ANIME_VOTE_COUNT,      &data.vote_count,           "VOTE_COUNT"},            // Byte 4, bit 6 (dec 64)
		{ANIME_TEMP_RATING,     &data.temp_rating,          "TEMP_RATING"},           // Byte 4, bit 5 (dec 32)
		{ANIME_TEMP_VOTE_COUNT, &data.temp_vote_count,      "TEMP_VOTE_COUNT"},       // Byte 4, bit 4 (dec 16)
		{ANIME_AVG_REVIEW_RATING,&data.avg_review_rating,   "AVG_REVIEW_RATING"},     // Byte 4, bit 3 (dec 8)
		{ANIME_REVIEW_COUNT,    &data.review_count,         "REVIEW_COUNT"},          // Byte 4, bit 2 (dec 4)
		{ANIME_AWARD_LIST,      &data.award_list,           "AWARD_LIST"},            // Byte 4, bit 1 (dec 2)
		{ANIME_IS_18_RESTRICTED,&data.is_18_restricted,     "IS_18_RESTRICTED"},      // Byte 4, bit 0 (dec 1)
		
		// Byte 5 (bits 7-0) - external IDs and tags
		// Byte 5, bit 7 is retired
		{ANIME_ANN_ID,          &data.ann_id,               "ANN_ID"},                // Byte 5, bit 6 (dec 64)
		{ANIME_ALLCINEMA_ID,    &data.allcinema_id,         "ALLCINEMA_ID"},          // Byte 5, bit 5 (dec 32)
		{ANIME_ANIMENFO_ID,     &data.animenfo_id,          "ANIMENFO_ID"},           // Byte 5, bit 4 (dec 16)
		{ANIME_TAG_NAME_LIST,   &data.tag_name_list,        "TAG_NAME_LIST"},         // Byte 5, bit 3 (dec 8)
		{ANIME_TAG_ID_LIST,     &data.tag_id_list,          "TAG_ID_LIST"},           // Byte 5, bit 2 (dec 4)
		{ANIME_TAG_WEIGHT_LIST, &data.tag_weight_list,      "TAG_WEIGHT_LIST"},       // Byte 5, bit 1 (dec 2)
		{ANIME_DATE_RECORD_UPDATED,&data.date_record_updated,"DATE_RECORD_UPDATED"},  // Byte 5, bit 0 (dec 1)
		
		// Byte 6 (bits 7-0) - characters
		{ANIME_CHARACTER_ID_LIST,&data.character_id_list,   "CHARACTER_ID_LIST"},     // Byte 6, bit 7 (dec 128)
		// Byte 6, bits 6-4 are retired
		// Byte 6, bits 3-0 are unused
		
		// Byte 7 (bits 7-0) - episode type counts
		{ANIME_SPECIALS_COUNT,  &data.specials_count,       "SPECIALS_COUNT"},        // Byte 7, bit 7 (dec 128)
		{ANIME_CREDITS_COUNT,   &data.credits_count,        "CREDITS_COUNT"},         // Byte 7, bit 6 (dec 64)
		{ANIME_OTHER_COUNT,     &data.other_count,          "OTHER_COUNT"},           // Byte 7, bit 5 (dec 32)
		{ANIME_TRAILER_COUNT,   &data.trailer_count,        "TRAILER_COUNT"},         // Byte 7, bit 4 (dec 16)
		{ANIME_PARODY_COUNT,    &data.parody_count,         "PARODY_COUNT"}           // Byte 7, bit 3 (dec 8)
		// Byte 7, bits 2-0 are unused
	};
	
	// Process ALL mask bits in strict MSB to LSB order, including retired/unused bits
	// The AniDB server sends fields for ALL set bits in the mask, even retired ones
	// We must check every bit position to maintain correct token alignment
	//
	// NOTE: The amask is a 7-byte (56-bit) value, but we're limited to 32 bits in the current
	// implementation. This means bytes 5-7 are not properly handled. A full fix would require
	// using uint64_t or string parsing. For now, we handle bytes 1-4 correctly.
	
	// Define all possible bit positions for bytes 1-4 in MSB to LSB order
	unsigned int allBits[] = {
		// Byte 1 (bits 7-0)
		0x00000080, 0x00000040, 0x00000020, 0x00000010, 0x00000008, 0x00000004, 0x00000002, 0x00000001,
		// Byte 2 (bits 15-8)
		0x00008000, 0x00004000, 0x00002000, 0x00001000, 0x00000800, 0x00000400, 0x00000200, 0x00000100,
		// Byte 3 (bits 23-16)
		0x00800000, 0x00400000, 0x00200000, 0x00100000, 0x00080000, 0x00040000, 0x00020000, 0x00010000,
		// Byte 4 (bits 31-24)
		0x80000000, 0x40000000, 0x20000000, 0x10000000, 0x08000000, 0x04000000, 0x02000000, 0x01000000
	};
	
	// Create a map of bit values to field info for quick lookup
	std::map<unsigned int, const MaskBit*> bitMap;
	for (size_t i = 0; i < sizeof(maskBits) / sizeof(MaskBit); i++)
	{
		bitMap[maskBits[i].bit] = &maskBits[i];
	}
	
	// Process all bits in order
	for (size_t i = 0; i < sizeof(allBits) / sizeof(unsigned int); i++)
	{
		unsigned int currentBit = allBits[i];
		
		// Check if this bit is set in the mask
		if (amask & currentBit)
		{
			// Skip ANIME_AID bit - it's already extracted by the caller at token[0]
			if (currentBit == ANIME_AID)
			{
				Logger::log(QString("[AniDB parseAnimeMask] Skipping AID bit (already extracted by caller)"), __FILE__, __LINE__);
				continue;
			}
			
			// Check if we have a defined field for this bit
			auto it = bitMap.find(currentBit);
			if (it != bitMap.end())
			{
				// This is a defined field
				const MaskBit* maskBit = it->second;
				QString value = tokens.value(index);
				Logger::log(QString("[AniDB parseAnimeMask] Bit match: %1 (bit 0x%2) -> token[%3] = '%4'")
					.arg(maskBit->name)
					.arg(currentBit, 0, 16)
					.arg(index)
					.arg(value), __FILE__, __LINE__);
				
				if (maskBit->field != nullptr)
				{
					*(maskBit->field) = value;
				}
			}
			else
			{
				// This is a retired/unused bit - consume the token but don't store
				QString value = tokens.value(index);
				Logger::log(QString("[AniDB parseAnimeMask] Retired/unused bit 0x%1 -> token[%2] = '%3' (skipped)")
					.arg(currentBit, 0, 16)
					.arg(index)
					.arg(value), __FILE__, __LINE__);
			}
			
			// Always increment index for any bit set in the mask
			index++;
		}
	}
	
	// Handle bytes 5-7 using the remaining maskBits entries
	// These overlap with bytes 1-3 in the 32-bit representation, so we process them separately
	// after bytes 1-4 are done
	for (size_t i = 0; i < sizeof(maskBits) / sizeof(MaskBit); i++)
	{
		// Skip bits that belong to bytes 1-4 (already processed)
		if (maskBits[i].bit <= 0x01000000)
			continue;
			
		if (amask & maskBits[i].bit)
		{
			QString value = tokens.value(index);
			Logger::log(QString("[AniDB parseAnimeMask] Byte 5-7 field: %1 (bit 0x%2) -> token[%3] = '%4'")
				.arg(maskBits[i].name)
				.arg(maskBits[i].bit, 0, 16)
				.arg(index)
				.arg(value), __FILE__, __LINE__);
			index++;
			if (maskBits[i].field != nullptr)
			{
				*(maskBits[i].field) = value;
			}
		}
	}
	
	// Set legacy fields for backward compatibility
	data.eptotal = data.episodes;  // episodes (Byte 3, bit 7)
	data.eplast = data.highest_episode;  // highest episode (Byte 3, bit 6)
	
	return data;
}

/**
 * Parse anime data from AniDB ANIME response using the mask hex string.
 * This properly handles all 7 bytes of the anime mask by parsing the hex string directly.
 * 
 * The AniDB anime mask is a 7-byte value sent as a hex string (e.g., "fffffcfc00" for 7 bytes).
 * Each byte represents 8 bits, and fields are sent in MSB-to-LSB order across all bytes.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amaskHexString The anime mask as a hex string (e.g., "fffffcfc")
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return AnimeData structure with parsed anime fields
 */
AniDBApi::AnimeData AniDBApi::parseAnimeMaskFromString(const QStringList& tokens, const QString& amaskHexString, int& index)
{
	AnimeData data;
	
	// Parse the hex string into bytes (pad to 14 hex chars = 7 bytes if shorter)
	QString paddedMask = amaskHexString.rightJustified(14, '0');
	QByteArray maskBytes;
	for (int i = 0; i < paddedMask.length(); i += 2)
	{
		bool ok;
		unsigned char byte = paddedMask.mid(i, 2).toUInt(&ok, 16);
		if (ok)
			maskBytes.append(byte);
	}
	
	// Ensure we have 7 bytes
	while (maskBytes.size() < 7)
		maskBytes.append((char)0);
	
	Logger::log(QString("[AniDB parseAnimeMaskFromString] Mask string: %1 -> %2 bytes")
		.arg(amaskHexString)
		.arg(maskBytes.size()), __FILE__, __LINE__);
	
	// Define all mask bits in MSB to LSB order with their byte positions
	struct MaskBit {
		int byteIndex;     // 0-6 for bytes 1-7
		unsigned char bitMask;  // Bit mask within the byte (0x80, 0x40, etc.)
		QString* field;
		const char* name;
	};
	
	MaskBit maskBits[] = {
		// Byte 1 (byte 0 in array) - bits 7-0
		{0, 0x80, &data.aid,                  "AID"},
		{0, 0x40, &data.dateflags,            "DATEFLAGS"},
		{0, 0x20, &data.year,                 "YEAR"},
		{0, 0x10, &data.type,                 "TYPE"},
		{0, 0x08, &data.relaidlist,           "RELATED_AID_LIST"},
		{0, 0x04, &data.relaidtype,           "RELATED_AID_TYPE"},
		{0, 0x02, nullptr,                    "RETIRED_BYTE1_BIT1"},  // Retired
		{0, 0x01, nullptr,                    "RETIRED_BYTE1_BIT0"},  // Retired
		
		// Byte 2 (byte 1 in array) - bits 7-0
		{1, 0x80, &data.nameromaji,           "ROMAJI_NAME"},
		{1, 0x40, &data.namekanji,            "KANJI_NAME"},
		{1, 0x20, &data.nameenglish,          "ENGLISH_NAME"},
		{1, 0x10, &data.nameother,            "OTHER_NAME"},
		{1, 0x08, &data.nameshort,            "SHORT_NAME_LIST"},
		{1, 0x04, &data.synonyms,             "SYNONYM_LIST"},
		{1, 0x02, nullptr,                    "RETIRED_BYTE2_BIT1"},  // Retired
		{1, 0x01, nullptr,                    "RETIRED_BYTE2_BIT0"},  // Retired
		
		// Byte 3 (byte 2 in array) - bits 7-0
		{2, 0x80, &data.episodes,             "EPISODES"},
		{2, 0x40, &data.highest_episode,      "HIGHEST_EPISODE"},
		{2, 0x20, &data.special_ep_count,     "SPECIAL_EP_COUNT"},
		{2, 0x10, &data.air_date,             "AIR_DATE"},
		{2, 0x08, &data.end_date,             "END_DATE"},
		{2, 0x04, &data.url,                  "URL"},
		{2, 0x02, &data.picname,              "PICNAME"},
		{2, 0x01, nullptr,                    "RETIRED_BYTE3_BIT0"},  // Retired
		
		// Byte 4 (byte 3 in array) - bits 7-0
		{3, 0x80, &data.rating,               "RATING"},
		{3, 0x40, &data.vote_count,           "VOTE_COUNT"},
		{3, 0x20, &data.temp_rating,          "TEMP_RATING"},
		{3, 0x10, &data.temp_vote_count,      "TEMP_VOTE_COUNT"},
		{3, 0x08, &data.avg_review_rating,    "AVG_REVIEW_RATING"},
		{3, 0x04, &data.review_count,         "REVIEW_COUNT"},
		{3, 0x02, &data.award_list,           "AWARD_LIST"},
		{3, 0x01, &data.is_18_restricted,     "IS_18_RESTRICTED"},
		
		// Byte 5 (byte 4 in array) - bits 7-0
		{4, 0x80, nullptr,                    "RETIRED_BYTE5_BIT7"},  // Retired
		{4, 0x40, &data.ann_id,               "ANN_ID"},
		{4, 0x20, &data.allcinema_id,         "ALLCINEMA_ID"},
		{4, 0x10, &data.animenfo_id,          "ANIMENFO_ID"},
		{4, 0x08, &data.tag_name_list,        "TAG_NAME_LIST"},
		{4, 0x04, &data.tag_id_list,          "TAG_ID_LIST"},
		{4, 0x02, &data.tag_weight_list,      "TAG_WEIGHT_LIST"},
		{4, 0x01, &data.date_record_updated,  "DATE_RECORD_UPDATED"},
		
		// Byte 6 (byte 5 in array) - bits 7-0
		{5, 0x80, &data.character_id_list,    "CHARACTER_ID_LIST"},
		{5, 0x40, nullptr,                    "RETIRED_BYTE6_BIT6"},  // Retired
		{5, 0x20, nullptr,                    "RETIRED_BYTE6_BIT5"},  // Retired
		{5, 0x10, nullptr,                    "RETIRED_BYTE6_BIT4"},  // Retired
		{5, 0x08, nullptr,                    "UNUSED_BYTE6_BIT3"},   // Unused
		{5, 0x04, nullptr,                    "UNUSED_BYTE6_BIT2"},   // Unused
		{5, 0x02, nullptr,                    "UNUSED_BYTE6_BIT1"},   // Unused
		{5, 0x01, nullptr,                    "UNUSED_BYTE6_BIT0"},   // Unused
		
		// Byte 7 (byte 6 in array) - bits 7-0
		{6, 0x80, &data.specials_count,       "SPECIALS_COUNT"},
		{6, 0x40, &data.credits_count,        "CREDITS_COUNT"},
		{6, 0x20, &data.other_count,          "OTHER_COUNT"},
		{6, 0x10, &data.trailer_count,        "TRAILER_COUNT"},
		{6, 0x08, &data.parody_count,         "PARODY_COUNT"},
		{6, 0x04, nullptr,                    "UNUSED_BYTE7_BIT2"},   // Unused
		{6, 0x02, nullptr,                    "UNUSED_BYTE7_BIT1"},   // Unused
		{6, 0x01, nullptr,                    "UNUSED_BYTE7_BIT0"}    // Unused
	};
	
	// Process each bit in order
	for (size_t i = 0; i < sizeof(maskBits) / sizeof(MaskBit); i++)
	{
		int byteIdx = maskBits[i].byteIndex;
		if (byteIdx >= maskBytes.size())
			continue;
			
		unsigned char byte = (unsigned char)maskBytes[byteIdx];
		
		// Check if this bit is set in the mask
		if (byte & maskBits[i].bitMask)
		{
			// Skip ANIME_AID bit - it's already extracted by the caller at token[0]
			if (byteIdx == 0 && maskBits[i].bitMask == 0x80)
			{
				Logger::log(QString("[AniDB parseAnimeMaskFromString] Skipping AID bit (already extracted by caller)"), __FILE__, __LINE__);
				continue;
			}
			
			QString value = tokens.value(index);
			
			if (maskBits[i].field != nullptr)
			{
				// This is a defined field - store it
				Logger::log(QString("[AniDB parseAnimeMaskFromString] Bit match: %1 (byte %2, bit 0x%3) -> token[%4] = '%5'")
					.arg(maskBits[i].name)
					.arg(byteIdx + 1)
					.arg(maskBits[i].bitMask, 0, 16)
					.arg(index)
					.arg(value.left(80)), __FILE__, __LINE__);
				
				*(maskBits[i].field) = value;
			}
			else
			{
				// Retired/unused bit - consume the token but don't store
				Logger::log(QString("[AniDB parseAnimeMaskFromString] Retired/unused: %1 (byte %2, bit 0x%3) -> token[%4] = '%5' (skipped)")
					.arg(maskBits[i].name)
					.arg(byteIdx + 1)
					.arg(maskBits[i].bitMask, 0, 16)
					.arg(index)
					.arg(value.left(80)), __FILE__, __LINE__);
			}
			
			// Always increment index for any bit set in the mask
			index++;
		}
	}
	
	// Set legacy fields for backward compatibility
	data.eptotal = data.episodes;
	data.eplast = data.highest_episode;
	
	return data;
}

/**
 * Store file data in the database.
 */
void AniDBApi::storeFileData(const FileData& data)
{
	QString q = QString("INSERT OR REPLACE INTO `file` "
		"(`fid`, `aid`, `eid`, `gid`, `lid`, `othereps`, `isdepr`, `state`, "
		"`size`, `ed2k`, `md5`, `sha1`, `crc`, `quality`, `source`, "
		"`codec_audio`, `bitrate_audio`, `codec_video`, `bitrate_video`, "
		"`resolution`, `filetype`, `lang_dub`, `lang_sub`, `length`, "
		"`description`, `airdate`, `filename`) "
		"VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', "
		"'%9', '%10', '%11', '%12', '%13', '%14', '%15', "
		"'%16', '%17', '%18', '%19', "
		"'%20', '%21', '%22', '%23', '%24', "
		"'%25', '%26', '%27')")
		.arg(QString(data.fid).replace("'", "''"))
		.arg(QString(data.aid).replace("'", "''"))
		.arg(QString(data.eid).replace("'", "''"))
		.arg(QString(data.gid).replace("'", "''"))
		.arg(QString(data.lid).replace("'", "''"))
		.arg(QString(data.othereps).replace("'", "''"))
		.arg(QString(data.isdepr).replace("'", "''"))
		.arg(QString(data.state).replace("'", "''"))
		.arg(QString(data.size).replace("'", "''"))
		.arg(QString(data.ed2k).replace("'", "''"))
		.arg(QString(data.md5).replace("'", "''"))
		.arg(QString(data.sha1).replace("'", "''"))
		.arg(QString(data.crc).replace("'", "''"))
		.arg(QString(data.quality).replace("'", "''"))
		.arg(QString(data.source).replace("'", "''"))
		.arg(QString(data.codec_audio).replace("'", "''"))
		.arg(QString(data.bitrate_audio).replace("'", "''"))
		.arg(QString(data.codec_video).replace("'", "''"))
		.arg(QString(data.bitrate_video).replace("'", "''"))
		.arg(QString(data.resolution).replace("'", "''"))
		.arg(QString(data.filetype).replace("'", "''"))
		.arg(QString(data.lang_dub).replace("'", "''"))
		.arg(QString(data.lang_sub).replace("'", "''"))
		.arg(QString(data.length).replace("'", "''"))
		.arg(QString(data.description).replace("'", "''"))
		.arg(QString(data.airdate).replace("'", "''"))
		.arg(QString(data.filename).replace("'", "''"));
		
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		LOG("Database query error: " + query.lastError().text());
	}
}

/**
 * Store anime data in the database.
 */
void AniDBApi::storeAnimeData(const AnimeData& data)
{
	if(data.aid.isEmpty())
		return;
		
	QString q = QString("INSERT OR REPLACE INTO `anime` "
		"(`aid`, `eptotal`, `eplast`, `year`, `type`, `relaidlist`, "
		"`relaidtype`, `category`, `nameromaji`, `namekanji`, `nameenglish`, "
		"`nameother`, `nameshort`, `synonyms`) "
		"VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', "
		"'%9', '%10', '%11', '%12', '%13', '%14')")
		.arg(QString(data.aid).replace("'", "''"))
		.arg(QString(data.eptotal).replace("'", "''"))
		.arg(QString(data.eplast).replace("'", "''"))
		.arg(QString(data.year).replace("'", "''"))
		.arg(QString(data.type).replace("'", "''"))
		.arg(QString(data.relaidlist).replace("'", "''"))
		.arg(QString(data.relaidtype).replace("'", "''"))
		.arg(QString(data.category).replace("'", "''"))
		.arg(QString(data.nameromaji).replace("'", "''"))
		.arg(QString(data.namekanji).replace("'", "''"))
		.arg(QString(data.nameenglish).replace("'", "''"))
		.arg(QString(data.nameother).replace("'", "''"))
		.arg(QString(data.nameshort).replace("'", "''"))
		.arg(QString(data.synonyms).replace("'", "''"));
		
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		LOG("Anime database query error: " + query.lastError().text());
	}
}

/**
 * Store episode data in the database.
 */
void AniDBApi::storeEpisodeData(const EpisodeData& data)
{
	if(data.eid.isEmpty())
		return;
		
	QString q = QString("INSERT OR REPLACE INTO `episode` "
		"(`eid`, `name`, `nameromaji`, `namekanji`, `rating`, `votecount`, `epno`) "
		"VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7')")
		.arg(QString(data.eid).replace("'", "''"))
		.arg(QString(data.epname).replace("'", "''"))
		.arg(QString(data.epnameromaji).replace("'", "''"))
		.arg(QString(data.epnamekanji).replace("'", "''"))
		.arg(QString(data.eprating).replace("'", "''"))
		.arg(QString(data.epvotecount).replace("'", "''"))
		.arg(QString(data.epno).replace("'", "''"));
		
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		LOG("Episode database query error: " + query.lastError().text());
	}
}

/**
 * Store group data in the database.
 */
void AniDBApi::storeGroupData(const GroupData& data)
{
	if(data.gid.isEmpty() || data.gid == "0")
		return;
	if(data.groupname.isEmpty() && data.groupshortname.isEmpty())
		return;
		
	QString q = QString("INSERT OR REPLACE INTO `group` "
		"(`gid`, `name`, `shortname`) "
		"VALUES ('%1', '%2', '%3')")
		.arg(QString(data.gid).replace("'", "''"))
		.arg(QString(data.groupname).replace("'", "''"))
		.arg(QString(data.groupshortname).replace("'", "''"));
		
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		LOG("Group database query error: " + query.lastError().text());
	}
}

/**
 * Extract fmask and amask values from a FILE command string, or amask from ANIME command.
 * FILE format: "FILE size=X&ed2k=Y&fmask=ZZZZZZZZ&amask=WWWWWWWW"
 * ANIME format: "ANIME aid=X&amask=YYYYYYYY"
 * 
 * @param command The FILE or ANIME command string
 * @param fmask Output parameter for file mask (hex string converted to unsigned int). Set to 0 for ANIME commands.
 * @param amask Output parameter for anime mask (hex string converted to unsigned int)
 * @return true if masks were successfully extracted, false otherwise
 */
bool AniDBApi::extractMasksFromCommand(const QString& command, unsigned int& fmask, unsigned int& amask)
{
	// Default values
	fmask = 0;
	amask = 0;
	
	// Extract fmask using regex (only for FILE commands)
	QRegularExpression fmaskRegex("fmask=([0-9a-fA-F]+)");
	QRegularExpressionMatch fmaskMatch = fmaskRegex.match(command);
	
	// Extract amask using regex
	QRegularExpression amaskRegex("amask=([0-9a-fA-F]+)");
	QRegularExpressionMatch amaskMatch = amaskRegex.match(command);
	
	bool success = false;
	
	if (fmaskMatch.hasMatch())
	{
		QString fmaskStr = fmaskMatch.captured(1);
		bool ok;
		fmask = fmaskStr.toUInt(&ok, 16);  // Parse as hexadecimal
		success = ok;
	}
	
	if (amaskMatch.hasMatch())
	{
		QString amaskStr = amaskMatch.captured(1);
		bool ok;
		amask = amaskStr.toUInt(&ok, 16);  // Parse as hexadecimal
		if (!success)
			success = ok;
	}
	
	return success;
}
