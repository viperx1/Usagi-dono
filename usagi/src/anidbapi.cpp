// API definition available at https://wiki.anidb.net/UDP_API_Definition
#include "anidbapi.h"
#include "logger.h"
#include <cmath>
#include <map>
#include <QThread>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QSet>
#include <QHash>

// Global pointer to the AniDB API instance
// This is initialized by the application (window.cpp) or tests
// Core library files can use this via extern declaration in anidbapi.h
myAniDBApi *adbapi = nullptr;

namespace {
QMutex s_animeRequestMutex;
QHash<int, qint64> s_animeRequestInFlight;
constexpr qint64 ANIME_REQUEST_INFLIGHT_TIMEOUT_SECS = 300;
}

AniDBApi::AniDBApi(QString client_, int clientver_)
	: m_settings()  // Initialize ApplicationSettings (will set database later)
{
	protover = 3;
	client = client_; //"usagi";
	clientver = clientver_; //1;
	enc = "utf8";
	
	// Skip DNS lookup in test mode to avoid network-related crashes on Windows
	if (qgetenv("USAGI_TEST_MODE") != "1")
	{
		host = QHostInfo::fromName("api.anidb.net");
		const QList<QHostAddress> addresses = host.addresses();
		if(!addresses.isEmpty())
		{
			anidbaddr.setAddress(addresses.first().toIPv4Address());
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
		// Test mode - skip DNS lookup
	}
	
	anidbport = 9000;
	loggedin = 0;
	banned = false; // Initialize banned flag to false
	Socket = nullptr;
	currentTag = ""; // Initialize current tag tracker

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

	if(db.open())
	{
		db.transaction();
//		Debug("AniDBApi: Database opened");
		query = QSqlQuery(db);
		// Create mylist table with AniDB API fields: viewed/viewdate are synced from AniDB server
		query.exec("CREATE TABLE IF NOT EXISTS `mylist`(`lid` INTEGER PRIMARY KEY, `fid` INTEGER, `eid` INTEGER, `aid` INTEGER, `gid` INTEGER, `date` INTEGER, `state` INTEGER, `viewed` INTEGER, `viewdate` INTEGER, `storage` TEXT, `source` TEXT, `other` TEXT, `filestate` INTEGER)");
		query.exec("CREATE TABLE IF NOT EXISTS `anime`(`aid` INTEGER PRIMARY KEY, `eptotal` INTEGER, `eps` INTEGER, `eplast` INTEGER, `year` TEXT, `type` TEXT, `relaidlist` TEXT, `relaidtype` TEXT, `category` TEXT, `nameromaji` TEXT, `namekanji` TEXT, `nameenglish` TEXT, `nameother` TEXT, `nameshort` TEXT, `synonyms` TEXT, `typename` TEXT, `startdate` TEXT CHECK(startdate IS NULL OR startdate = '' OR startdate GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]Z'), `enddate` TEXT CHECK(enddate IS NULL OR enddate = '' OR enddate GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]Z'), `picname` TEXT, `poster_image` BLOB, `dateflags` TEXT, `episodes` INTEGER, `highest_episode` TEXT, `special_ep_count` INTEGER, `url` TEXT, `rating` TEXT, `vote_count` INTEGER, `temp_rating` TEXT, `temp_vote_count` INTEGER, `avg_review_rating` TEXT, `review_count` INTEGER, `award_list` TEXT, `is_18_restricted` INTEGER, `ann_id` INTEGER, `allcinema_id` INTEGER, `animenfo_id` TEXT, `tag_name_list` TEXT, `tag_id_list` TEXT, `tag_weight_list` TEXT, `date_record_updated` INTEGER, `character_id_list` TEXT, `specials_count` INTEGER, `credits_count` INTEGER, `other_count` INTEGER, `trailer_count` INTEGER, `parody_count` INTEGER);");
        query.exec("CREATE TABLE IF NOT EXISTS `file`(`fid` INTEGER PRIMARY KEY, `aid` INTEGER, `eid` INTEGER, `gid` INTEGER, `lid` INTEGER, `othereps` TEXT, `isdepr` INTEGER, `state` INTEGER, `size` BIGINT, `ed2k` TEXT, `md5` TEXT, `sha1` TEXT, `crc` TEXT, `quality` TEXT, `source` TEXT, `codec_audio` TEXT, `bitrate_audio` INTEGER, `codec_video` TEXT, `bitrate_video` INTEGER, `resolution` TEXT, `filetype` TEXT, `lang_dub` TEXT, `lang_sub` TEXT, `length` INTEGER, `description` TEXT, `airdate` INTEGER, `filename` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `episode`(`eid` INTEGER PRIMARY KEY, `name` TEXT, `nameromaji` TEXT, `namekanji` TEXT, `rating` INTEGER, `votecount` INTEGER, `epno` TEXT);");
		// Add epno column if it doesn't exist (for existing databases)
		query.exec("ALTER TABLE `episode` ADD COLUMN `epno` TEXT");
		// Add last_checked column for episode data caching (for existing databases)
		query.exec("ALTER TABLE `episode` ADD COLUMN `last_checked` INTEGER");
		// Add eps column if it doesn't exist (for existing databases)
		query.exec("ALTER TABLE `anime` ADD COLUMN `eps` INTEGER");
		// Add typename, startdate, enddate columns if they don't exist (for existing databases)
		query.exec("ALTER TABLE `anime` ADD COLUMN `typename` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `startdate` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `enddate` TEXT");
		// Add picname and poster_image columns if they don't exist (for existing databases)
		query.exec("ALTER TABLE `anime` ADD COLUMN `picname` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `poster_image` BLOB");
		// Add new ANIME command fields to anime table if they don't exist (for existing databases)
		query.exec("ALTER TABLE `anime` ADD COLUMN `dateflags` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `episodes` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `highest_episode` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `special_ep_count` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `url` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `rating` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `vote_count` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `temp_rating` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `temp_vote_count` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `avg_review_rating` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `review_count` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `award_list` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `is_18_restricted` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `ann_id` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `allcinema_id` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `animenfo_id` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `tag_name_list` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `tag_id_list` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `tag_weight_list` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `date_record_updated` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `character_id_list` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `specials_count` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `credits_count` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `other_count` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `trailer_count` INTEGER");
		query.exec("ALTER TABLE `anime` ADD COLUMN `parody_count` INTEGER");
		// Add cache tracking columns for anime data requests
		query.exec("ALTER TABLE `anime` ADD COLUMN `last_mask` TEXT");
		query.exec("ALTER TABLE `anime` ADD COLUMN `last_checked` INTEGER");
		// Add hidden column for card visibility (for existing databases)
		query.exec("ALTER TABLE `anime` ADD COLUMN `hidden` INTEGER DEFAULT 0");
		// Create local_files table for directory watcher feature
		// Status: 0=not hashed, 1=hashed but not checked by API, 2=in anidb, 3=not in anidb
		// binding_status: 0=not_bound, 1=bound_to_anime, 2=not_anime
		query.exec("CREATE TABLE IF NOT EXISTS `local_files`("
		           "`id` INTEGER PRIMARY KEY AUTOINCREMENT, "
		           "`path` TEXT UNIQUE, "
		           "`filename` TEXT, "
		           "`status` INTEGER DEFAULT 0, "
		           "`ed2k_hash` TEXT, "
		           "`binding_status` INTEGER DEFAULT 0, "
		           "`file_size` BIGINT)");
		// Add ed2k_hash column to local_files if it doesn't exist (for existing databases)
		query.exec("ALTER TABLE `local_files` ADD COLUMN `ed2k_hash` TEXT");
		// Add binding_status column to local_files if it doesn't exist (for existing databases)
		query.exec("ALTER TABLE `local_files` ADD COLUMN `binding_status` INTEGER DEFAULT 0");
		// Add file_size column to local_files for duplicate detection (for existing databases)
		query.exec("ALTER TABLE `local_files` ADD COLUMN `file_size` BIGINT");
		// Add local_file column to mylist if it doesn't exist (references local_files.id)
		query.exec("ALTER TABLE `mylist` ADD COLUMN `local_file` INTEGER");
		query.exec("CREATE TABLE IF NOT EXISTS `group`(`gid` INTEGER PRIMARY KEY, `name` TEXT, `shortname` TEXT);");
		// Add status column to group table (0=unknown, 1=ongoing, 2=stalled, 3=disbanded)
		query.exec("ALTER TABLE `group` ADD COLUMN `status` INTEGER DEFAULT 0");
		query.exec("CREATE TABLE IF NOT EXISTS `anime_titles`(`aid` INTEGER, `type` INTEGER, `language` TEXT, `title` TEXT, PRIMARY KEY(`aid`, `type`, `language`, `title`));");
		// Create index on anime_titles for faster lookups by aid and type
		query.exec("CREATE INDEX IF NOT EXISTS `idx_anime_titles_aid_type` ON `anime_titles`(`aid`, `type`);");
		query.exec("CREATE TABLE IF NOT EXISTS `packets`(`tag` INTEGER PRIMARY KEY, `str` TEXT, `processed` BOOL DEFAULT 0, `sendtime` INTEGER, `got_reply` BOOL DEFAULT 0, `reply` TEXT, `retry_count` INTEGER DEFAULT 0);");
		// Add retry_count column to packets if it doesn't exist (for existing databases)
		query.exec("ALTER TABLE `packets` ADD COLUMN `retry_count` INTEGER DEFAULT 0");
		query.exec("CREATE TABLE IF NOT EXISTS `settings`(`id` INTEGER PRIMARY KEY, `name` TEXT UNIQUE, `value` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `notifications`(`nid` INTEGER PRIMARY KEY, `type` TEXT, `from_user_id` INTEGER, `from_user_name` TEXT, `date` INTEGER, `message_type` INTEGER, `title` TEXT, `body` TEXT, `received_at` INTEGER, `acknowledged` BOOL DEFAULT 0);");
		query.exec("UPDATE `packets` SET `processed` = 1 WHERE `processed` = 0;");
		
		// Create indexes for JOIN performance optimization
		query.exec("CREATE INDEX IF NOT EXISTS `idx_mylist_aid` ON `mylist`(`aid`);");
		query.exec("CREATE INDEX IF NOT EXISTS `idx_mylist_eid` ON `mylist`(`eid`);");
		query.exec("CREATE INDEX IF NOT EXISTS `idx_mylist_fid` ON `mylist`(`fid`);");
		query.exec("CREATE INDEX IF NOT EXISTS `idx_mylist_gid` ON `mylist`(`gid`);");
		query.exec("CREATE INDEX IF NOT EXISTS `idx_episode_eid` ON `episode`(`eid`);");
		query.exec("CREATE INDEX IF NOT EXISTS `idx_file_fid` ON `file`(`fid`);");
		// Create index on local_files for duplicate detection by ed2k_hash
		query.exec("CREATE INDEX IF NOT EXISTS `idx_local_files_ed2k_hash` ON `local_files`(`ed2k_hash`);");
		
		// Add playback tracking columns to mylist if they don't exist
		query.exec("ALTER TABLE `mylist` ADD COLUMN `playback_position` INTEGER DEFAULT 0");
		query.exec("ALTER TABLE `mylist` ADD COLUMN `playback_duration` INTEGER DEFAULT 0");
		query.exec("ALTER TABLE `mylist` ADD COLUMN `last_played` INTEGER DEFAULT 0");
		
		// Add local watch status column - separate from AniDB viewed status
		query.exec("ALTER TABLE `mylist` ADD COLUMN `local_watched` INTEGER DEFAULT 0");
		
		// Create watch_chunks table for chunk-based watch tracking
		// Tracks which 1-minute chunks of a file have been watched
		query.exec("CREATE TABLE IF NOT EXISTS `watch_chunks`("
		           "`id` INTEGER PRIMARY KEY AUTOINCREMENT, "
		           "`lid` INTEGER NOT NULL, "
		           "`chunk_index` INTEGER NOT NULL, "
		           "`watched_at` INTEGER NOT NULL, "
		           "UNIQUE(`lid`, `chunk_index`)"
		           ");");
		
		// Create index for efficient chunk lookups
		query.exec("CREATE INDEX IF NOT EXISTS `idx_watch_chunks_lid` ON `watch_chunks`(`lid`);");
		
		// Create watched_episodes table for episode-level watch tracking
		// This persists watch state at episode level, independent of file replacements
		if (!query.exec("CREATE TABLE IF NOT EXISTS `watched_episodes`("
		                "`eid` INTEGER PRIMARY KEY, "
		                "`watched_at` INTEGER NOT NULL"
		                ");")) {
			LOG(QString("Error creating watched_episodes table: %1").arg(query.lastError().text()));
		}
		
		// Migrate existing local_watched data to episode-level tracking
		// This ensures previously watched episodes remain marked as watched
		// Use COALESCE to ensure we always have a valid timestamp even if viewdate is NULL
		if (!query.exec("INSERT OR IGNORE INTO `watched_episodes` (eid, watched_at) "
		                "SELECT DISTINCT m.eid, COALESCE(MAX(m.viewdate), strftime('%s', 'now')) "
		                "FROM mylist m "
		                "WHERE m.local_watched = 1 AND m.eid > 0 "
		                "GROUP BY m.eid")) {
			LOG(QString("Error migrating watched episodes data: %1").arg(query.lastError().text()));
		} else {
			int migrated = query.numRowsAffected();
			if (migrated > 0) {
				LOG(QString("Migrated %1 episode(s) to episode-level watch tracking").arg(migrated));
			}
		}
		
		db.commit();
	}
//	QStringList names = QStringList()<<"username"<<"password";
	
	// Execute SELECT query after transaction commit to read settings
	query.exec("SELECT `name`, `value` FROM `settings` ORDER BY `name` ASC");
	
	// Initialize directory watcher settings with defaults
	watcherEnabled = false;
	watcherDirectory = QString();
	watcherAutoStart = false;
	
	// Initialize auto-fetch settings with defaults (disabled by default)
	autoFetchEnabled = false;
	
	// Initialize tray settings with defaults
	trayMinimizeToTray = false;
	trayCloseToTray = false;
	trayStartMinimized = false;
	
	// Initialize auto-start settings with defaults
	autoStartEnabled = false;
	
	// Initialize filter bar visibility with defaults (visible by default)
	filterBarVisible = true;
	
	// Initialize file marking preferences with defaults
	preferredAudioLanguages = "japanese";  // Default to Japanese audio
	preferredSubtitleLanguages = "english";  // Default to English subtitles
	preferHighestVersion = true;  // Default to preferring highest version
	preferHighestQuality = true;  // Default to preferring highest quality
	preferredBitrate = 3.5;  // Default baseline bitrate of 3.5 Mbps for 1080p
	preferredResolution = "1080p";  // Default to 1080p resolution
	hasherFilterMasks = "*.!qB,*.tmp";  // Default masks for incomplete downloads
	
	// Initialize ApplicationSettings with database connection and load settings
	m_settings = ApplicationSettings(db);
	m_settings.load();
	
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
		if(query.value(0).toString() == "autoFetchEnabled")
		{
			autoFetchEnabled = (query.value(1).toString() == "1");
		}
		if(query.value(0).toString() == "trayMinimizeToTray")
		{
			trayMinimizeToTray = (query.value(1).toString() == "1");
		}
		if(query.value(0).toString() == "trayCloseToTray")
		{
			trayCloseToTray = (query.value(1).toString() == "1");
		}
		if(query.value(0).toString() == "trayStartMinimized")
		{
			trayStartMinimized = (query.value(1).toString() == "1");
		}
		if(query.value(0).toString() == "autoStartEnabled")
		{
			autoStartEnabled = (query.value(1).toString() == "1");
		}
		if(query.value(0).toString() == "filterBarVisible")
		{
			filterBarVisible = (query.value(1).toString() == "1");
		}
		if(query.value(0).toString() == "preferredAudioLanguages")
		{
			preferredAudioLanguages = query.value(1).toString();
		}
		if(query.value(0).toString() == "preferredSubtitleLanguages")
		{
			preferredSubtitleLanguages = query.value(1).toString();
		}
		if(query.value(0).toString() == "preferHighestVersion")
		{
			preferHighestVersion = (query.value(1).toString() == "1");
		}
		if(query.value(0).toString() == "preferHighestQuality")
		{
			preferHighestQuality = (query.value(1).toString() == "1");
		}
		if(query.value(0).toString() == "preferredBitrate")
		{
			preferredBitrate = query.value(1).toDouble();
		}
		else if(query.value(0).toString() == "preferredResolution")
		{
			preferredResolution = query.value(1).toString();
		}
		else if(query.value(0).toString() == "hasherFilterMasks")
		{
			hasherFilterMasks = query.value(1).toString();
		}
	}

	// Initialize network manager for anime titles download
	networkManager = new QNetworkAccessManager(this);
	connect(networkManager, &QNetworkAccessManager::finished, this, &AniDBApi::onAnimeTitlesDownloaded);

	packetsender = new QTimer();
	connect(packetsender, SIGNAL(timeout()), this, SLOT(SendPacket()));

	packetsender->setInterval(2100);
	packetsender->start();
	
	// Initialize notification checking timer
	notifyCheckTimer = new QTimer();
	connect(notifyCheckTimer, SIGNAL(timeout()), this, SLOT(checkForNotifications()));
	isExportQueued = false;
	requestedExportTemplate = QString(); // No template requested yet
	notifyCheckAttempts = 0;
	notifyCheckIntervalMs = 60000; // Start with 1 minute
	exportQueuedTimestamp = 0;
	
	// Load any persisted export queue state from previous session
	loadExportQueueState();

	// Check and download anime titles if needed (automatically on startup if auto-fetch is enabled)
	if(autoFetchEnabled && shouldUpdateAnimeTitles())
	{
		downloadAnimeTitles();
	}
	else if(!autoFetchEnabled)
	{
		// Auto-fetch disabled - skipping
	}
	else
	{
		// Anime titles are up to date
	}
	
	// Check if we need to perform a CALENDAR check (on startup)
	// Calendar check will happen after login, we just initialize the last check time here
	query.exec("SELECT `value` FROM `settings` WHERE `name` = 'last_calendar_check'");
	if(query.next())
	{
		lastCalendarCheck = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
	}
	else
	{
		lastCalendarCheck = QDateTime::fromSecsSinceEpoch(0); // Never checked
	}

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
			// UDP socket created successfully
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
		emit notifyLoggedIn(Tag, 200);
		
		// Check if calendar needs updating after successful login
		checkCalendarIfNeeded();
	}
	else if(ReplyID == "201"){ // 201 {str session_key} LOGIN ACCEPTED - NEW VERSION AVAILABLE
		SID = token.first();
		loggedin = 1;
		emit notifyLoggedIn(Tag, 201);
		
		// Check if calendar needs updating after successful login
		checkCalendarIfNeeded();
	}
    else if(ReplyID == "203"){ // 203 LOGGED OUT
        Logger::log("[AniDB Response] 203 LOGGED OUT - Tag: " + Tag, __FILE__, __LINE__);
		loggedin = 0;
		emit notifyLoggedOut(Tag, 203);
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
			
			for(const QString& param : std::as_const(params))
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
					LOG(QString("Successfully added mylist entry - lid=%1, fid=%2").arg(lid, fid));
				}
			}
			else
			{
				LOG("Could not find file info for size=" + size + " ed2k=" + ed2k);
			}
		}
		
		emit notifyMylistAdd(Tag, 210);
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
		
		// Parse file data using fmask (now returns AniDBFileInfo)
		AniDBFileInfo fileInfo = parseFileMask(token2, fmask, index);
		// Set FID which is returned separately
		fileInfo.setFileId(token2.value(0).toInt());
		
		// Parse anime data using amask
		AniDBAnimeInfo animeInfo = parseFileAmaskAnimeData(token2, amask, index);
		
		// Parse episode data using amask
		AniDBEpisodeInfo episodeInfo = parseFileAmaskEpisodeData(token2, amask, index);
		
		// Parse group data using amask
		AniDBGroupInfo groupInfo = parseFileAmaskGroupData(token2, amask, index);
		
		// Store all parsed data
		storeFileData(fileInfo);
		
		if(animeInfo.isValid() || fileInfo.animeId() > 0)
		{
			// Ensure AID is set from file data
			if(!animeInfo.isValid()) {
				animeInfo.setAnimeId(fileInfo.animeId());
			} else if(animeInfo.animeId() == 0) {
				animeInfo.setAnimeId(fileInfo.animeId());
			}
			storeAnimeData(animeInfo);
		}
		
		if(episodeInfo.isValid() || fileInfo.episodeId() > 0)
		{
			// Ensure EID is set from file data
			if(!episodeInfo.isValid()) {
				episodeInfo.setEpisodeId(fileInfo.episodeId());
			} else if(episodeInfo.episodeId() == 0) {
				episodeInfo.setEpisodeId(fileInfo.episodeId());
			}
			storeEpisodeData(episodeInfo);
		}
		
		if(groupInfo.isValid() || fileInfo.groupId() > 0)
		{
			// Ensure GID is set from file data
			if(!groupInfo.isValid()) {
				groupInfo.setGroupId(fileInfo.groupId());
			} else if(groupInfo.groupId() == 0) {
				groupInfo.setGroupId(fileInfo.groupId());
			}
			storeGroupData(groupInfo);
		}
		
		// Handle truncated response - log warning about incomplete data
		if(isTruncated)
		{
			Logger::log(QString("[AniDB Response] 220 FILE - WARNING: Response was truncated, some fields may be missing. "
				"Processed %1 fields successfully.").arg(index), __FILE__, __LINE__);
		}
		
		// Always queue EPISODE API request after FILE reply to ensure complete episode data
		if(fileInfo.episodeId() > 0)
		{
			LOG(QString("Queuing EPISODE API request for EID %1").arg(fileInfo.episodeId()));
			Episode(fileInfo.episodeId());
		}
		
		// Always queue ANIME API request after FILE reply to ensure complete anime data
		if(fileInfo.animeId() > 0)
		{
			LOG(QString("Queuing ANIME API request for AID %1").arg(fileInfo.animeId()));
			Anime(fileInfo.animeId());
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
		uint64_t amask = 0;
		QString amaskString;
		QString animeCmd;  // Declare here so it's available throughout the entire block
		Mask originalMask;  // Declare here so it's available throughout the entire block
		
		if(query.exec(q) && query.next())
		{
			animeCmd = query.value(0).toString();
			Logger::log("[AniDB Response] 230 ANIME command: " + animeCmd, __FILE__, __LINE__);
			
			// Extract amask as string for proper 7-byte parsing
			static const QRegularExpression amaskRegex("amask=([0-9a-fA-F]+)");
			QRegularExpressionMatch amaskMatch = amaskRegex.match(animeCmd);
			if (amaskMatch.hasMatch())
			{
				amaskString = amaskMatch.captured(1);
				Logger::log("[AniDB Response] 230 ANIME extracted amask string: " + amaskString, __FILE__, __LINE__);
			}
			else
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
				// Convert uint64 to hex string using Mask
				amaskString = Mask(amask).toString();
			}
			// Ensure extracted amask string is 14 chars (7 bytes)
			if (!amaskString.isEmpty())
			{
				amaskString = amaskString.leftJustified(14, '0');
			}
			
			// Set Mask object for proper handling
			originalMask.setFromString(amaskString);
			
			Logger::log("[AniDB Response] 230 ANIME extracted amask: 0x" + amaskString, __FILE__, __LINE__);
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
			// Convert uint64 to hex string using Mask
			amaskString = Mask(amask).toString();
			Logger::log("[AniDB Response] 230 ANIME using default amask: 0x" + amaskString, __FILE__, __LINE__);
			
			// Set Mask object from the default amask
			originalMask.setFromString(amaskString);
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
			// Check if AID bit is set in the mask
			// AID is Byte 1, bit 7 (0x80 in the first byte of the hex string)
			bool hasAidBit = false;
			if (amaskString.length() >= 2)
			{
				bool ok;
				uint8_t byte1 = amaskString.left(2).toUInt(&ok, 16);
				hasAidBit = ok && (byte1 & 0x80);
			}
			
			QString aid;
			int index;
			
			if (hasAidBit)
			{
				// AID is in the response at token[0]
				aid = token2.at(0);
				index = 1;  // Start parsing after AID
				Logger::log("[AniDB Response] 230 ANIME - AID bit set, extracting AID from token[0]: " + aid, __FILE__, __LINE__);
			}
			else
			{
				// AID is NOT in the response, need to get it from the command
				// Extract AID from the command string
				static const QRegularExpression aidRegex("aid=(\\d+)");
				QRegularExpressionMatch aidMatch = aidRegex.match(animeCmd);
				if (aidMatch.hasMatch())
				{
					aid = aidMatch.captured(1);
				}
				index = 0;  // Start parsing from token[0]
				Logger::log("[AniDB Response] 230 ANIME - AID bit NOT set, using AID from command: " + aid + ", starting parse at token[0]", __FILE__, __LINE__);
			}
			
			int startIndex = index;
			
			// Check if response might be truncated (UDP 1400 byte limit)
			if(responseData.length() >= 1350 && !isTruncated)
			{
				Logger::log("[AniDB Response] 230 ANIME WARNING: Response near UDP size limit (" + 
					QString::number(responseData.length()) + " chars), may be truncated", __FILE__, __LINE__);
			}
			
			// Parse anime data using amask string for proper 7-byte handling
			// Track which fields were successfully parsed for re-request logic
			QByteArray parsedMaskBytes;
			AniDBAnimeInfo animeInfo = parseMaskFromString(token2, amaskString, index, parsedMaskBytes);
			animeInfo.setAnimeId(aid.toInt());  // Ensure aid is set
			
			Logger::log("[AniDB Response] 230 ANIME parsed " + QString::number(index - startIndex) + " fields (index: " + QString::number(startIndex) + " -> " + QString::number(index) + ")", __FILE__, __LINE__);
			Logger::log("[AniDB Response] 230 ANIME parsed - AID: " + aid + " Year: '" + animeInfo.year() + "' Type: '" + animeInfo.type() + "'", __FILE__, __LINE__);
			
			// Handle truncated response - calculate missing fields and re-request
			if(isTruncated)
			{
				Logger::log(QString("[AniDB Response] 230 ANIME - WARNING: Response was truncated, some fields may be missing. "
					"Processed %1 fields successfully.").arg(index), __FILE__, __LINE__);
				
				// Calculate reduced mask with only missing fields
				Mask reducedMask = calculateReducedMask(originalMask, parsedMaskBytes);
				
				// Check if there are any missing fields
				if (!reducedMask.isEmpty())
				{
					// Queue re-request with reduced mask to fetch missing fields
					QString reducedMaskString = reducedMask.toString();
					QString reRequestCmd = QString("ANIME aid=%1&amask=%2").arg(aid).arg(reducedMaskString);
					Logger::log(QString("[AniDB Response] 230 ANIME - Queueing re-request for missing fields with reduced mask: %1")
						.arg(reducedMaskString), __FILE__, __LINE__);
					
					// Insert re-request into packets table
					QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(reRequestCmd);
					QSqlQuery reRequestQuery(db);
					if (reRequestQuery.exec(q))
					{
						Logger::log(QString("[AniDB Response] 230 ANIME - Re-request queued successfully for AID %1 (tag=%2)")
							.arg(aid)
							.arg(Tag), __FILE__, __LINE__);
					}
					else
					{
						Logger::log("[AniDB Response] 230 ANIME - ERROR: Failed to queue re-request: " + reRequestQuery.lastError().text(), __FILE__, __LINE__);
					}
				}
				else
				{
					Logger::log("[AniDB Response] 230 ANIME - No missing fields to re-request (all requested fields were received)", __FILE__, __LINE__);
				}
			}
			
			// Store all anime data to database
			if(!aid.isEmpty())
			{
				{
					QMutexLocker animeRequestLocker(&s_animeRequestMutex);
					Logger::log(QString("[AniDB API] Clearing in-flight ANIME guard for AID %1 on 230 response (beforeSize=%2)")
						.arg(aid).arg(s_animeRequestInFlight.size()), __FILE__, __LINE__);
					s_animeRequestInFlight.remove(aid.toInt());
				}
				animeInfo.setAnimeId(aid.toInt());  // Ensure aid is set
				storeAnimeData(animeInfo);
				Logger::log("[AniDB Response] 230 ANIME metadata saved to database - AID: " + aid + " Type: " + animeInfo.type(), __FILE__, __LINE__);
				// Emit signal to notify UI that anime data was updated
				Logger::log(QString("[AniDB Response] 230 ANIME emitting notifyAnimeUpdated for AID %1 (tag=%2)")
					.arg(aid).arg(Tag), __FILE__, __LINE__);
				emit notifyAnimeUpdated(aid.toInt());
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
		// Parse mylist entry data from 310 response
		// Format: 310 FILE ALREADY IN MYLIST\nlid|fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
		QStringList token2 = Message.split("\n");
		token2.pop_front(); // Remove the status line "310 FILE ALREADY IN MYLIST"
		if(!token2.isEmpty())
		{
			QStringList fields = token2.first().split("|");
			if(fields.size() >= 12)
			{
				QString lid = fields.at(0);
				QString fid = fields.at(1);
				QString eid = fields.at(2);
				QString aid = fields.at(3);
				QString gid = fields.at(4);
				QString date = fields.at(5);
				QString state = fields.at(6);
				QString viewdate = fields.at(7);
				QString storage = fields.at(8);
				QString source = fields.at(9);
				QString other = fields.at(10);
				QString filestate = fields.at(11);
				
				// Get ed2k and size from the original MYLISTADD command to create file entry
				QString mylistCmd;
				QSqlQuery packetQuery(db);
				packetQuery.prepare("SELECT `str` FROM `packets` WHERE `tag` = ?");
				packetQuery.addBindValue(Tag);
				if(packetQuery.exec() && packetQuery.next())
				{
					mylistCmd = packetQuery.value(0).toString();
				}
				
				QString ed2k, sizeStr;
				if(!mylistCmd.isEmpty())
				{
					QStringList params = mylistCmd.split("&");
					for(const QString& param : std::as_const(params))
					{
						if(param.contains("size="))
							sizeStr = param.mid(param.indexOf("size=") + 5).split("&").first();
						else if(param.contains("ed2k="))
							ed2k = param.mid(param.indexOf("ed2k=") + 5).split("&").first();
					}
				}
				
				// Insert or update file entry (so we can look up by ed2k later)
				if(!fid.isEmpty() && !ed2k.isEmpty() && !sizeStr.isEmpty())
				{
					QSqlQuery fileQuery(db);
					fileQuery.prepare("INSERT OR REPLACE INTO `file` (`fid`, `aid`, `eid`, `gid`, `size`, `ed2k`) VALUES (?, ?, ?, ?, ?, ?)");
					fileQuery.addBindValue(fid);
					fileQuery.addBindValue(aid);
					fileQuery.addBindValue(eid);
					fileQuery.addBindValue(gid);
					fileQuery.addBindValue(sizeStr);
					fileQuery.addBindValue(ed2k);
					
					if(fileQuery.exec())
					{
						LOG(QString("Stored file entry from 310 response - fid=%1, ed2k=%2").arg(fid).arg(ed2k));
					}
					else
					{
						LOG("Failed to store file entry from 310 response: " + fileQuery.lastError().text());
					}
				}
				
				// First, get existing values for local_file, playback_position, playback_duration, last_played
				int existingLocalFile = 0;
				int existingPlaybackPosition = 0;
				int existingPlaybackDuration = 0;
				int existingLastPlayed = 0;
				
				QSqlQuery existingQuery(db);
				existingQuery.prepare("SELECT `local_file`, `playback_position`, `playback_duration`, `last_played` FROM `mylist` WHERE `lid` = ?");
				existingQuery.addBindValue(lid);
				if(existingQuery.exec() && existingQuery.next())
				{
					existingLocalFile = existingQuery.value(0).toInt();
					existingPlaybackPosition = existingQuery.value(1).toInt();
					existingPlaybackDuration = existingQuery.value(2).toInt();
					existingLastPlayed = existingQuery.value(3).toInt();
				}
				
				// Insert or replace the mylist entry using prepared statement
				QSqlQuery insertQuery(db);
				insertQuery.prepare("INSERT OR REPLACE INTO `mylist` (`lid`, `fid`, `eid`, `aid`, `gid`, `date`, `state`, `viewed`, `viewdate`, `storage`, `source`, `other`, `filestate`, `local_file`, `playback_position`, `playback_duration`, `last_played`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
				insertQuery.addBindValue(lid);
				insertQuery.addBindValue(fid);
				insertQuery.addBindValue(eid);
				insertQuery.addBindValue(aid);
				insertQuery.addBindValue(gid);
				insertQuery.addBindValue(date);
				insertQuery.addBindValue(state);
				insertQuery.addBindValue(viewdate); // viewed - derived from viewdate (non-zero = viewed)
				insertQuery.addBindValue(viewdate);
				insertQuery.addBindValue(storage);
				insertQuery.addBindValue(source);
				insertQuery.addBindValue(other);
				insertQuery.addBindValue(filestate);
				insertQuery.addBindValue(existingLocalFile);
				insertQuery.addBindValue(existingPlaybackPosition);
				insertQuery.addBindValue(existingPlaybackDuration);
				insertQuery.addBindValue(existingLastPlayed);
				
				if(!insertQuery.exec())
				{
					LOG("Database insert error for 310 response: " + insertQuery.lastError().text());
				}
				else
				{
					LOG(QString("Stored mylist entry from 310 response - lid=%1, fid=%2, aid=%3").arg(lid).arg(fid).arg(aid));
				}
			}
			else
			{
				LOG(QString("310 response has unexpected field count: %1 (expected 12)").arg(fields.size()));
			}
		}
		
		// resend with tag and &edit=1 (to apply any changes from MYLISTADD command)
		QSqlQuery query(db);
		query.prepare("SELECT `str` FROM `packets` WHERE `tag` = ?");
		query.addBindValue(Tag);
		if(query.exec() && query.next())
		{
			QString originalStr = query.value(0).toString();
			QSqlQuery updateQuery(db);
			updateQuery.prepare("UPDATE `packets` SET `processed` = 0, `str` = ? WHERE `tag` = ?");
			updateQuery.addBindValue(originalStr + "&edit=1");
			updateQuery.addBindValue(Tag);
			updateQuery.exec();
		}
		emit notifyMylistAdd(Tag, 310);
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
			
			for(const QString& param : std::as_const(params))
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
		
		emit notifyMylistAdd(Tag, 311);
	}
	else if(ReplyID == "312"){ // 312 NO SUCH MYLIST ENTRY
		Logger::log("[AniDB Response] 312 NO SUCH MYLIST ENTRY - Tag: " + Tag, __FILE__, __LINE__);
		
		// Check if this was a MYLISTDEL command
		// Note: MYLISTDEL is implemented but not actively used (we use MYLISTADD with state=3 instead)
		// The notifyMylistDel signal is available for future use if MYLISTDEL is needed
		QSqlQuery query(db);
		query.prepare("SELECT str FROM packets WHERE tag = ?");
		query.addBindValue(Tag);
		if(query.exec() && query.next())
		{
			QString cmd = query.value(0).toString();
			if(cmd.startsWith("MYLISTDEL"))
			{
				// Extract lid from command - use shared static regex
				static const QRegularExpression lidRegex("lid=(\\d+)");
				QRegularExpressionMatch match = lidRegex.match(cmd);
				if(match.hasMatch())
				{
					int lid = match.captured(1).toInt();
					emit notifyMylistDel(Tag, lid, false);
				}
			}
		}
	}
	else if(ReplyID == "225"){ // 225 GROUP STATUS
		// Response format: gid|aid|state|name|shortname|lastepisode
		// State: 0=unknown, 1=ongoing, 2=stalled, 3=disbanded
		Logger::log("[AniDB Response] 225 GROUP STATUS - Tag: " + Tag, __FILE__, __LINE__);
		
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		if(token2.size() > 0)
		{
			QStringList fields = token2.first().split("|");
			if(fields.size() >= 5)
			{
				QString gid = fields[0];
				QString state = fields[2];
				QString name = fields[3];
				QString shortname = fields[4];
				
				// Store group information in database
				QSqlQuery query(db);
				query.prepare("INSERT OR REPLACE INTO `group` (`gid`, `name`, `shortname`, `status`) VALUES (?, ?, ?, ?)");
				query.addBindValue(gid.toInt());
				query.addBindValue(name);
				query.addBindValue(shortname);
				query.addBindValue(state.toInt());
				
				if(query.exec())
				{
					Logger::log(QString("[AniDB Response] 225 GROUP STATUS stored - GID: %1 Name: %2 Status: %3").arg(gid).arg(name).arg(state), __FILE__, __LINE__);
				}
				else
				{
					Logger::log(QString("[AniDB Error] Failed to store group status - GID: %1 Error: %2").arg(gid).arg(query.lastError().text()), __FILE__, __LINE__);
				}
			}
		}
	}
	else if(ReplyID == "211"){ // 211 MYLIST ENTRY DELETED
		Logger::log("[AniDB Response] 211 MYLIST ENTRY DELETED - Tag: " + Tag, __FILE__, __LINE__);
		
		// Parse the response to get the number of deleted entries
		// Format: 211 MYLIST ENTRY DELETED\n{int count}
		// Note: MYLISTDEL is implemented but not actively used (we use MYLISTADD with state=3 instead)
		// The notifyMylistDel signal is available for future use if MYLISTDEL is needed
		QStringList lines = Message.split("\n");
		int count = 0;
		if(lines.size() > 1)
		{
			count = lines.at(1).trimmed().toInt();
			Logger::log(QString("[AniDB Response] %1 mylist entry(ies) deleted").arg(count), __FILE__, __LINE__);
		}
		
		// Get the lid from the original command - use shared static regex
		static const QRegularExpression lidRegex("lid=(\\d+)");
		QSqlQuery query(db);
		query.prepare("SELECT str FROM packets WHERE tag = ?");
		query.addBindValue(Tag);
		if(query.exec() && query.next())
		{
			QString cmd = query.value(0).toString();
			QRegularExpressionMatch match = lidRegex.match(cmd);
			if(match.hasMatch())
			{
				int lid = match.captured(1).toInt();
				emit notifyMylistDel(Tag, lid, true);
			}
		}
		
		// Mark packet as processed
		QString q = QString("UPDATE `packets` SET `processed` = 1, `got_reply` = 1, `reply` = '%1' WHERE `tag` = '%2'").arg(ReplyID, Tag);
		QSqlQuery updateQuery(db);
		updateQuery.exec(q);
	}
    else if(ReplyID == "320"){ // 320 NO SUCH FILE
        emit notifyMylistAdd(Tag, 320);
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
/*		for(int i = 0; i < token2.size(); i++)
		{
			Logger::log("[AniDB Response] 291 NOTIFYLIST Entry " + QString::number(i+1) + " of " + QString::number(token2.size()) + ": " + token2[i], __FILE__, __LINE__);
        }*/
		
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
	else if(ReplyID == "297"){ // 297 CALENDAR
		// CALENDAR response: list of anime with episodes airing soon
		// Format: 297 CALENDAR
		// {int4 aid}|{int4 start time}|{str dateflags}
		// ... (multiple entries, one per line)
		Logger::log("[AniDB Response] 297 CALENDAR - Received calendar data", __FILE__, __LINE__);
		
		QStringList lines = Message.split("\n");
		lines.pop_front(); // Remove first line (status code line)
		
		int newAnimeCount = 0;
		int updatedAnimeCount = 0;
		QSqlDatabase db = QSqlDatabase::database();
		
		// Process each anime entry in the calendar
		for(const QString& line : lines)
		{
			if(line.trimmed().isEmpty())
				continue;
				
			QStringList parts = line.split("|");
			if(parts.size() >= 2)
			{
				int aid = parts[0].toInt();
				qint64 startTime = parts[1].toLongLong();  // Unix timestamp when episode airs
				QString dateflags = parts.size() >= 3 ? parts[2] : "";
				
				// Convert Unix timestamp to ISO date format (YYYY-MM-DDZ) using existing helper
				QString startdate = convertToISODate(QString::number(startTime));
				
				// Check if this anime is already in the anime table
				QSqlQuery checkQuery(db);
				checkQuery.prepare("SELECT COUNT(*) FROM anime WHERE aid = ?");
				checkQuery.bindValue(0, aid);
				
				bool animeExists = false;
				if(checkQuery.exec() && checkQuery.next())
				{
					animeExists = checkQuery.value(0).toInt() > 0;
				}
				
				if(animeExists)
				{
					// Update existing anime with startdate and dateflags from calendar
					// Use COALESCE to preserve existing richer data from full ANIME responses
					// while filling in calendar data only for empty fields
					QSqlQuery updateQuery(db);
					updateQuery.prepare("UPDATE anime SET "
						"startdate = COALESCE(NULLIF(:startdate, ''), startdate), "
						"dateflags = COALESCE(NULLIF(:dateflags, ''), dateflags) "
						"WHERE aid = :aid");
					updateQuery.bindValue(":startdate", startdate);
					updateQuery.bindValue(":dateflags", dateflags);
					updateQuery.bindValue(":aid", aid);
					
					if(updateQuery.exec())
					{
						if(updateQuery.numRowsAffected() > 0)
						{
							updatedAnimeCount++;
							Logger::log(QString("[AniDB Calendar] Updated anime: aid=%1 startdate=%2 dateflags=%3")
								.arg(aid).arg(startdate, dateflags), __FILE__, __LINE__);
						}
					}
					else
					{
						Logger::log(QString("[AniDB Calendar] Failed to update anime aid=%1: %2")
							.arg(aid).arg(updateQuery.lastError().text()), __FILE__, __LINE__);
					}
				}
				else
				{
					// Insert new anime entry with just aid, startdate, and dateflags
					QSqlQuery insertQuery(db);
					insertQuery.prepare("INSERT INTO anime (aid, startdate, dateflags) VALUES (:aid, :startdate, :dateflags)");
					insertQuery.bindValue(":aid", aid);
					insertQuery.bindValue(":startdate", startdate);
					insertQuery.bindValue(":dateflags", dateflags);
					
					if(insertQuery.exec())
					{
						newAnimeCount++;
						Logger::log(QString("[AniDB Calendar] New anime added: aid=%1 startdate=%2 dateflags=%3")
							.arg(aid).arg(startdate, dateflags), __FILE__, __LINE__);
					}
					else
					{
						Logger::log(QString("[AniDB Calendar] Failed to add anime aid=%1: %2")
							.arg(aid).arg(insertQuery.lastError().text()), __FILE__, __LINE__);
					}
				}
			}
		}
		
		if(newAnimeCount > 0 || updatedAnimeCount > 0)
		{
			Logger::log(QString("[AniDB Calendar] Processed calendar: %1 new anime added, %2 existing anime updated")
				.arg(newAnimeCount).arg(updatedAnimeCount), __FILE__, __LINE__);
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
        QStringList token2 = Message.split("-");
        token2.pop_front();
        bannedfor = token2.join("-").trimmed();
        Logger::log("[AniDB Error] 555 BANNED - Reason: " + bannedfor + " - All outgoing communication blocked until app restart", __FILE__, __LINE__);
        LOG("AniDBApi: Recv: 555 BANNED - " + bannedfor);
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
    waitingForReply.stopWaiting();
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

QString AniDBApi::MylistAddGeneric(int aid, QString epno, int viewed, int state, QString storage, QString other)
{
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	QString msg = buildMylistAddGenericCommand(aid, epno, viewed, state, storage, other);
	QString q;
	q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Logger::log("[AniDB MylistAddGeneric] Database insert error: " + query.lastError().text(), __FILE__, __LINE__);
		return "0";
	}
	return GetTag(msg);
}

QString AniDBApi::MylistDel(int lid)
{
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	QString msg = buildMylistDelCommand(lid);
	QString q;
	q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Logger::log("[AniDB MylistDel] Database insert error: " + query.lastError().text(), __FILE__, __LINE__);
		return "0";
	}
	Logger::log(QString("[AniDB MylistDel] Queued deletion for lid=%1").arg(lid), __FILE__, __LINE__);
	return GetTag(msg);
}

QString AniDBApi::File(qint64 size, QString ed2k)
{
	// Check if file already exists in database
	QSqlQuery checkQuery(db);
	checkQuery.prepare("SELECT fid, aid, eid, gid FROM `file` WHERE size = ? AND ed2k = ?");
	checkQuery.addBindValue(size);
	checkQuery.addBindValue(ed2k);
	
	unsigned int fmask = fAID | fEID | fGID | fLID | fOTHEREPS | fISDEPR | fSTATE | fSIZE | fED2K | fMD5 | fSHA1 |
				fCRC32 | fQUALITY | fSOURCE | fCODEC_AUDIO | fBITRATE_AUDIO | fCODEC_VIDEO | fBITRATE_VIDEO |
				fRESOLUTION | fFILETYPE | fLANG_DUB | fLANG_SUB | fLENGTH | fDESCRIPTION | fAIRDATE |
				fFILENAME;
	
	// Start with base amask - exclude anime name fields as they come from separate dump
	unsigned int amask = aEPISODE_TOTAL | aEPISODE_LAST | aANIME_YEAR | aANIME_TYPE | aANIME_RELATED_LIST |
				aANIME_RELATED_TYPE | aANIME_CATAGORY |
				aEPISODE_NUMBER | aEPISODE_NAME | aEPISODE_NAME_ROMAJI | aEPISODE_NAME_KANJI |
				aEPISODE_RATING | aEPISODE_VOTE_COUNT | aGROUP_NAME | aGROUP_NAME_SHORT |
				aDATE_AID_RECORD_UPDATED;
	
	if(checkQuery.exec() && checkQuery.next())
	{
		// File exists - we already have fid, check if we need to reduce mask further
		int fid = checkQuery.value(0).toInt();
		int aid = checkQuery.value(1).toInt();
		int eid = checkQuery.value(2).toInt();
		int gid = checkQuery.value(3).toInt();
		
		Logger::log(QString("[AniDB File] File already in database (fid=%1) - checking for missing data").arg(fid), __FILE__, __LINE__);
		
		// Check if anime data exists in database
		if(aid > 0)
		{
			QSqlQuery animeCheck(db);
			animeCheck.prepare("SELECT aid FROM `anime` WHERE aid = ?");
			animeCheck.addBindValue(aid);
			if(animeCheck.exec() && animeCheck.next())
			{
				// Anime data exists, remove anime fields from amask
				amask &= ~(aEPISODE_TOTAL | aEPISODE_LAST | aANIME_YEAR | aANIME_TYPE | 
						   aANIME_RELATED_LIST | aANIME_RELATED_TYPE | aANIME_CATAGORY | 
						   aDATE_AID_RECORD_UPDATED);
				Logger::log("[AniDB File] Anime data already in database - excluding from request", __FILE__, __LINE__);
			}
		}
		
		// Check if episode data exists in database
		if(eid > 0)
		{
			QSqlQuery episodeCheck(db);
			episodeCheck.prepare("SELECT eid FROM `episode` WHERE eid = ?");
			episodeCheck.addBindValue(eid);
			if(episodeCheck.exec() && episodeCheck.next())
			{
				// Episode data exists, remove episode fields from amask
				amask &= ~(aEPISODE_NUMBER | aEPISODE_NAME | aEPISODE_NAME_ROMAJI | 
						   aEPISODE_NAME_KANJI | aEPISODE_RATING | aEPISODE_VOTE_COUNT);
				Logger::log("[AniDB File] Episode data already in database - excluding from request", __FILE__, __LINE__);
			}
		}
		
		// Check if group data exists in database
		if(gid > 0)
		{
			QSqlQuery groupCheck(db);
			groupCheck.prepare("SELECT gid FROM `group` WHERE gid = ?");
			groupCheck.addBindValue(gid);
			if(groupCheck.exec() && groupCheck.next())
			{
				// Group data exists, remove group fields from amask
				amask &= ~(aGROUP_NAME | aGROUP_NAME_SHORT);
				Logger::log("[AniDB File] Group data already in database - excluding from request", __FILE__, __LINE__);
			}
		}
		
		// If all data exists, we don't need to request anything
		if(amask == 0)
		{
			Logger::log("[AniDB File] All data already in database - skipping API request", __FILE__, __LINE__);
			return GetTag(""); // Return empty tag to indicate no request was made
		}
	}
	
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
	if(query.exec(q))
	{
		Logger::log(QString("[AniDB API] Queued MYLIST packet for LID %1 with tag=%2")
			.arg(lid).arg(GetTag(msg)), __FILE__, __LINE__);
	}
	else
	{
		Logger::log(QString("[AniDB API] Failed to queue MYLIST packet for LID %1: %2")
			.arg(lid).arg(query.lastError().text()), __FILE__, __LINE__);
	}
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
	// Check which episode fields are already present in database FIRST
	// to avoid unnecessary Auth() calls
	QSqlQuery checkQuery(db);
	checkQuery.prepare("SELECT name, nameromaji, namekanji, rating, votecount, epno, last_checked FROM `episode` WHERE eid = ?");
	checkQuery.addBindValue(eid);
	
	if(checkQuery.exec() && checkQuery.next())
	{
		// Check if all important fields are populated
		bool hasName = !checkQuery.value(0).isNull() && !checkQuery.value(0).toString().isEmpty();
		bool hasNameRomaji = !checkQuery.value(1).isNull() && !checkQuery.value(1).toString().isEmpty();
		bool hasNameKanji = !checkQuery.value(2).isNull() && !checkQuery.value(2).toString().isEmpty();
		bool hasRating = !checkQuery.value(3).isNull();
		bool hasVoteCount = !checkQuery.value(4).isNull();
		bool hasEpno = !checkQuery.value(5).isNull() && !checkQuery.value(5).toString().isEmpty();
		qint64 lastChecked = checkQuery.value(6).toLongLong();
		
		// Track which fields are missing for debug output
		QStringList missingFields;
		if(!hasName && !hasNameRomaji) missingFields << "name";
		if(!hasNameKanji) missingFields << "namekanji";
		if(!hasRating) missingFields << "rating";
		if(!hasVoteCount) missingFields << "votecount";
		if(!hasEpno) missingFields << "epno";
		
		// Log missing fields that will be requested
		if(!missingFields.isEmpty())
		{
			Logger::log(QString("[AniDB Missing Data] Episode EID %1 missing fields: %2")
						.arg(eid).arg(missingFields.join(", ")), __FILE__, __LINE__);
		}
		
		// Check if we should skip this request based on last_checked timestamp
		qint64 currentTime = QDateTime::currentSecsSinceEpoch();
		qint64 oneWeekInSeconds = 7 * 24 * 60 * 60;
		
		if(lastChecked > 0 && (currentTime - lastChecked) < oneWeekInSeconds)
		{
			// Data was checked less than a week ago - skip request even though some fields may be missing
			Logger::log(QString("[AniDB Cache] Episode data was checked %1 seconds ago (EID=%2)")
						.arg(currentTime - lastChecked).arg(eid), __FILE__, __LINE__);
			Logger::log(QString("[AniDB Cache] Skipping request - data is less than 7 days old (EID=%1)").arg(eid), __FILE__, __LINE__);
			return GetTag("");
		}
		
		// If all critical fields are present, skip the request
		// Critical fields: at least one name field and epno
		if((hasName || hasNameRomaji) && hasEpno)
		{
			Logger::log(QString("[AniDB API] Episode data already in database (EID=%1) - skipping API request").arg(eid), __FILE__, __LINE__);
			return GetTag("");
		}
		else
		{
			Logger::log(QString("[AniDB API] Episode partially in database (EID=%1) - requesting missing data").arg(eid), __FILE__, __LINE__);
		}
	}
	
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
	
	// Update last_checked timestamp in database
	// Use INSERT OR IGNORE to create the row if it doesn't exist yet
	QSqlQuery insertQuery(db);
	insertQuery.prepare("INSERT OR IGNORE INTO `episode` (`eid`) VALUES (?)");
	insertQuery.addBindValue(eid);
	insertQuery.exec();
	
	QSqlQuery updateQuery(db);
	updateQuery.prepare("UPDATE `episode` SET `last_checked` = ? WHERE `eid` = ?");
	updateQuery.addBindValue(QDateTime::currentSecsSinceEpoch());
	updateQuery.addBindValue(eid);
	if(!updateQuery.exec())
	{
		Logger::log(QString("[AniDB Cache] Failed to update last_checked for EID %1").arg(eid), __FILE__, __LINE__);
	}
	else
	{
		Logger::log(QString("[AniDB Cache] Updated last_checked for EID %1").arg(eid), __FILE__, __LINE__);
	}
	
	return GetTag(msg);
}

QString AniDBApi::Anime(int aid)
{
	// Check which anime fields are already present in database FIRST
	// to avoid unnecessary Auth() calls
	QSqlQuery checkQuery(db);
	checkQuery.prepare("SELECT year, type, relaidlist, relaidtype, eps, startdate, enddate, picname, "
					   "url, rating, vote_count, temp_rating, temp_vote_count, avg_review_rating, "
					   "review_count, award_list, is_18_restricted, ann_id, allcinema_id, animenfo_id, "
					   "tag_name_list, tag_id_list, tag_weight_list, date_record_updated, character_id_list, "
					   "episodes, highest_episode, special_ep_count, specials_count, credits_count, "
					   "other_count, trailer_count, parody_count, dateflags, last_mask, last_checked "
					   "FROM `anime` WHERE aid = ?");
	checkQuery.addBindValue(aid);
	
	// Start with full mask (excluding name fields which come from separate dump)
	uint64_t amask = 
		// Byte 1
		ANIME_AID | ANIME_DATEFLAGS |
		ANIME_YEAR | ANIME_TYPE |
		ANIME_RELATED_AID_LIST | ANIME_RELATED_AID_TYPE |
		// Byte 2 - EXCLUDED: name fields come from separate dump
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
	
	// Log initial mask
	Mask initialMask(amask);
	Logger::log(QString("[AniDB Mask] Initial mask for AID %1: 0x%2").arg(aid).arg(initialMask.toString()), __FILE__, __LINE__);
	
	if(checkQuery.exec() && checkQuery.next())
	{
		// Anime exists - check which fields are populated and reduce mask
		Logger::log(QString("[AniDB Mask] Anime exists in database (AID=%1) - checking fields for mask reduction").arg(aid), __FILE__, __LINE__);
		
		// Track which fields are missing for debug output
		QStringList missingFields;
		
		// Check year field (index 0)
		if(!checkQuery.value(0).isNull() && !checkQuery.value(0).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing YEAR from mask (value: %1)").arg(checkQuery.value(0).toString()), __FILE__, __LINE__);
			amask &= ~ANIME_YEAR;
		}
		else
		{
			missingFields << "year";
		}
		
		// Check type field (index 1)
		if(!checkQuery.value(1).isNull() && !checkQuery.value(1).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing TYPE from mask (value: %1)").arg(checkQuery.value(1).toString()), __FILE__, __LINE__);
			amask &= ~ANIME_TYPE;
		}
		else
		{
			missingFields << "type";
		}
		
		// Check related anime lists (index 2)
		if(!checkQuery.value(2).isNull() && !checkQuery.value(2).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing RELATED_AID_LIST from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_RELATED_AID_LIST;
		}
		else
		{
			missingFields << "related_aid_list";
		}
		
		// Check related anime types (index 3)
		if(!checkQuery.value(3).isNull() && !checkQuery.value(3).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing RELATED_AID_TYPE from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_RELATED_AID_TYPE;
		}
		else
		{
			missingFields << "related_aid_type";
		}
		
		// Check eps count (index 4) - legacy field, kept for compatibility
		// Note: Individual episode fields are checked separately below
		if(!checkQuery.value(4).isNull() && checkQuery.value(4).toInt() > 0)
		{
			Logger::log(QString("[AniDB Mask] Found eps field (legacy, value: %1)").arg(checkQuery.value(4).toInt()), __FILE__, __LINE__);
			// Don't remove mask bits here - check actual fields below
		}
		
		// Check start date (index 5)
		if(!checkQuery.value(5).isNull() && !checkQuery.value(5).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing AIR_DATE from mask (value: %1)").arg(checkQuery.value(5).toString()), __FILE__, __LINE__);
			amask &= ~ANIME_AIR_DATE;
		}
		else
		{
			missingFields << "startdate";
		}
		
		// Check end date (index 6)
		if(!checkQuery.value(6).isNull() && !checkQuery.value(6).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing END_DATE from mask (value: %1)").arg(checkQuery.value(6).toString()), __FILE__, __LINE__);
			amask &= ~ANIME_END_DATE;
		}
		else
		{
			missingFields << "enddate";
		}
		
		// Check picname (index 7)
		if(!checkQuery.value(7).isNull() && !checkQuery.value(7).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing PICNAME from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_PICNAME;
		}
		else
		{
			missingFields << "picname";
		}
		
		// Check url (index 8)
		if(!checkQuery.value(8).isNull() && !checkQuery.value(8).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing URL from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_URL;
		}
		else
		{
			missingFields << "url";
		}
		
		// Check rating (index 9)
		if(!checkQuery.value(9).isNull() && !checkQuery.value(9).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing RATING from mask (value: %1)").arg(checkQuery.value(9).toString()), __FILE__, __LINE__);
			amask &= ~ANIME_RATING;
		}
		else
		{
			missingFields << "rating";
		}
		
		// Check vote_count (index 10)
		if(!checkQuery.value(10).isNull() && checkQuery.value(10).toInt() > 0)
		{
			Logger::log(QString("[AniDB Mask] Removing VOTE_COUNT from mask (value: %1)").arg(checkQuery.value(10).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_VOTE_COUNT;
		}
		else
		{
			missingFields << "vote_count";
		}
		
		// Check temp_rating (index 11)
		if(!checkQuery.value(11).isNull() && !checkQuery.value(11).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing TEMP_RATING from mask (value: %1)").arg(checkQuery.value(11).toString()), __FILE__, __LINE__);
			amask &= ~ANIME_TEMP_RATING;
		}
		else
		{
			missingFields << "temp_rating";
		}
		
		// Check temp_vote_count (index 12)
		if(!checkQuery.value(12).isNull() && checkQuery.value(12).toInt() > 0)
		{
			Logger::log(QString("[AniDB Mask] Removing TEMP_VOTE_COUNT from mask (value: %1)").arg(checkQuery.value(12).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_TEMP_VOTE_COUNT;
		}
		
		// Check avg_review_rating (index 13)
		if(!checkQuery.value(13).isNull() && !checkQuery.value(13).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing AVG_REVIEW_RATING from mask (value: %1)").arg(checkQuery.value(13).toString()), __FILE__, __LINE__);
			amask &= ~ANIME_AVG_REVIEW_RATING;
		}
		
		// Check review_count (index 14)
		if(!checkQuery.value(14).isNull() && checkQuery.value(14).toInt() > 0)
		{
			Logger::log(QString("[AniDB Mask] Removing REVIEW_COUNT from mask (value: %1)").arg(checkQuery.value(14).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_REVIEW_COUNT;
		}
		
		// Check award_list (index 15)
		if(!checkQuery.value(15).isNull() && !checkQuery.value(15).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing AWARD_LIST from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_AWARD_LIST;
		}
		
		// Check is_18_restricted (index 16)
		if(!checkQuery.value(16).isNull())
		{
			Logger::log(QString("[AniDB Mask] Removing IS_18_RESTRICTED from mask (value: %1)").arg(checkQuery.value(16).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_IS_18_RESTRICTED;
		}
		
		// Check ann_id (index 17)
		if(!checkQuery.value(17).isNull() && checkQuery.value(17).toInt() > 0)
		{
			Logger::log(QString("[AniDB Mask] Removing ANN_ID from mask (value: %1)").arg(checkQuery.value(17).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_ANN_ID;
		}
		
		// Check allcinema_id (index 18)
		if(!checkQuery.value(18).isNull() && checkQuery.value(18).toInt() > 0)
		{
			Logger::log(QString("[AniDB Mask] Removing ALLCINEMA_ID from mask (value: %1)").arg(checkQuery.value(18).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_ALLCINEMA_ID;
		}
		
		// Check animenfo_id (index 19)
		if(!checkQuery.value(19).isNull() && !checkQuery.value(19).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing ANIMENFO_ID from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_ANIMENFO_ID;
		}
		
		// Check tag_name_list (index 20)
		if(!checkQuery.value(20).isNull() && !checkQuery.value(20).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing TAG_NAME_LIST from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_TAG_NAME_LIST;
		}
		
		// Check tag_id_list (index 21)
		if(!checkQuery.value(21).isNull() && !checkQuery.value(21).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing TAG_ID_LIST from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_TAG_ID_LIST;
		}
		
		// Check tag_weight_list (index 22)
		if(!checkQuery.value(22).isNull() && !checkQuery.value(22).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing TAG_WEIGHT_LIST from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_TAG_WEIGHT_LIST;
		}
		
		// Check date_record_updated (index 23)
		if(!checkQuery.value(23).isNull() && checkQuery.value(23).toInt() > 0)
		{
			Logger::log(QString("[AniDB Mask] Removing DATE_RECORD_UPDATED from mask (value: %1)").arg(checkQuery.value(23).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_DATE_RECORD_UPDATED;
		}
		
		// Check character_id_list (index 24)
		if(!checkQuery.value(24).isNull() && !checkQuery.value(24).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing CHARACTER_ID_LIST from mask"), __FILE__, __LINE__);
			amask &= ~ANIME_CHARACTER_ID_LIST;
		}
		
		// Check episodes (index 25)
		if(!checkQuery.value(25).isNull() && checkQuery.value(25).toInt() >= 0)
		{
			Logger::log(QString("[AniDB Mask] Removing EPISODES from mask (value: %1)").arg(checkQuery.value(25).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_EPISODES;
		}
		
		// Check highest_episode (index 26)
		if(!checkQuery.value(26).isNull() && !checkQuery.value(26).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing HIGHEST_EPISODE from mask (value: %1)").arg(checkQuery.value(26).toString()), __FILE__, __LINE__);
			amask &= ~ANIME_HIGHEST_EPISODE;
		}
		
		// Check special_ep_count (index 27)
		if(!checkQuery.value(27).isNull() && checkQuery.value(27).toInt() >= 0)
		{
			Logger::log(QString("[AniDB Mask] Removing SPECIAL_EP_COUNT from mask (value: %1)").arg(checkQuery.value(27).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_SPECIAL_EP_COUNT;
		}
		
		// Check specials_count (index 28)
		if(!checkQuery.value(28).isNull() && checkQuery.value(28).toInt() >= 0)
		{
			Logger::log(QString("[AniDB Mask] Removing SPECIALS_COUNT from mask (value: %1)").arg(checkQuery.value(28).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_SPECIALS_COUNT;
		}
		
		// Check credits_count (index 29)
		if(!checkQuery.value(29).isNull() && checkQuery.value(29).toInt() >= 0)
		{
			Logger::log(QString("[AniDB Mask] Removing CREDITS_COUNT from mask (value: %1)").arg(checkQuery.value(29).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_CREDITS_COUNT;
		}
		
		// Check other_count (index 30)
		if(!checkQuery.value(30).isNull() && checkQuery.value(30).toInt() >= 0)
		{
			Logger::log(QString("[AniDB Mask] Removing OTHER_COUNT from mask (value: %1)").arg(checkQuery.value(30).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_OTHER_COUNT;
		}
		
		// Check trailer_count (index 31)
		if(!checkQuery.value(31).isNull() && checkQuery.value(31).toInt() >= 0)
		{
			Logger::log(QString("[AniDB Mask] Removing TRAILER_COUNT from mask (value: %1)").arg(checkQuery.value(31).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_TRAILER_COUNT;
		}
		
		// Check parody_count (index 32)
		if(!checkQuery.value(32).isNull() && checkQuery.value(32).toInt() >= 0)
		{
			Logger::log(QString("[AniDB Mask] Removing PARODY_COUNT from mask (value: %1)").arg(checkQuery.value(32).toInt()), __FILE__, __LINE__);
			amask &= ~ANIME_PARODY_COUNT;
		}
		
		// Check dateflags (index 33)
		if(!checkQuery.value(33).isNull() && !checkQuery.value(33).toString().isEmpty())
		{
			Logger::log(QString("[AniDB Mask] Removing DATEFLAGS from mask (value: %1)").arg(checkQuery.value(33).toString()), __FILE__, __LINE__);
			amask &= ~ANIME_DATEFLAGS;
		}
		
		// Log final mask after reduction
		Mask finalMask(amask);
		Logger::log(QString("[AniDB Mask] Final mask after reduction for AID %1: 0x%2").arg(aid).arg(finalMask.toString()), __FILE__, __LINE__);
		
		// Log missing fields that will be requested
		if(!missingFields.isEmpty())
		{
			Logger::log(QString("[AniDB Missing Data] Requesting missing fields for AID %1: %2")
						.arg(aid).arg(missingFields.join(", ")), __FILE__, __LINE__);
		}
		
		// Check if we should skip this request based on last_checked timestamp
		// Index 34 = last_mask, Index 35 = last_checked
		QString lastMaskStr = checkQuery.value(34).toString();
		qint64 lastChecked = checkQuery.value(35).toLongLong();
		qint64 currentTime = QDateTime::currentSecsSinceEpoch();
		qint64 oneWeekInSeconds = 7 * 24 * 60 * 60;
		
		if(!lastMaskStr.isEmpty() && lastChecked > 0 && (currentTime - lastChecked) < oneWeekInSeconds)
		{
			// Data was checked less than a week ago - skip request even though fields are missing
			Mask lastMask(lastMaskStr);
			Logger::log(QString("[AniDB Cache] Anime data was checked %1 seconds ago (last mask: 0x%2)")
						.arg(currentTime - lastChecked).arg(lastMask.toString()), __FILE__, __LINE__);
			Logger::log(QString("[AniDB Cache] Skipping request - data is less than 7 days old (AID=%1)").arg(aid), __FILE__, __LINE__);
			return GetTag("");
		}
		
		// If all fields are present, skip the request
		if(amask == 0)
		{
			Logger::log(QString("[AniDB API] All anime data present in database (AID=%1) - skipping API request").arg(aid), __FILE__, __LINE__);
			return GetTag("");
		}
	}
	else
	{
		// Anime doesn't exist in database yet
		Logger::log(QString("[AniDB Mask] Anime not found in database (AID=%1) - using full mask").arg(aid), __FILE__, __LINE__);
	}
	
	// Request anime information by anime ID
	{
		QMutexLocker animeRequestLocker(&s_animeRequestMutex);
		const qint64 now = QDateTime::currentSecsSinceEpoch();
		
		// Remove stale in-flight entries to avoid permanently blocking retries
		for (auto it = s_animeRequestInFlight.begin(); it != s_animeRequestInFlight.end(); ) {
			if ((now - it.value()) >= ANIME_REQUEST_INFLIGHT_TIMEOUT_SECS) {
				Logger::log(QString("[AniDB API] Expiring stale in-flight ANIME request guard for AID %1 (age=%2s)")
					.arg(it.key()).arg(now - it.value()), __FILE__, __LINE__);
				it = s_animeRequestInFlight.erase(it);
			} else {
				++it;
			}
		}
		
		if(s_animeRequestInFlight.contains(aid))
		{
			const qint64 age = now - s_animeRequestInFlight.value(aid);
			Logger::log(QString("[AniDB API] Duplicate ANIME request blocked for AID %1 (already in-flight, age=%2s, inFlightSize=%3)")
				.arg(aid).arg(age).arg(s_animeRequestInFlight.size()), __FILE__, __LINE__);
			return QString("DUPLICATE_ANIME_AID_%1").arg(aid);
		}
		s_animeRequestInFlight.insert(aid, now);
		Logger::log(QString("[AniDB API] Marked AID %1 as in-flight for ANIME request (inFlightSize=%2)")
			.arg(aid).arg(s_animeRequestInFlight.size()), __FILE__, __LINE__);
	}
	
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	
	Logger::log("[AniDB API] Requesting ANIME data for AID: " + QString::number(aid), __FILE__, __LINE__);
	
	// Build command with reduced mask
	Mask mask(amask);
	Logger::log(QString("[AniDB Mask] Sending ANIME request for AID %1 with mask: 0x%2").arg(aid).arg(mask.toString()), __FILE__, __LINE__);
	QString msg = QString("ANIME aid=%1&amask=%2").arg(aid).arg(mask.toString());
	
	// Read existing last_mask from database to combine with new request
	QSqlQuery readMaskQuery(db);
	readMaskQuery.prepare("SELECT `last_mask` FROM `anime` WHERE `aid` = ?");
	readMaskQuery.addBindValue(aid);
	uint64_t combinedMask = amask;
	if(readMaskQuery.exec() && readMaskQuery.next())
	{
		QString existingMaskStr = readMaskQuery.value(0).toString();
		if(!existingMaskStr.isEmpty())
		{
			Mask existingMask(existingMaskStr);
			// Combine masks using OR operation to track all data ever requested
			combinedMask = existingMask.getValue() | amask;
			Mask combined(combinedMask);
			Logger::log(QString("[AniDB Cache] Combining masks - existing: 0x%1, new: 0x%2, combined: 0x%3")
						.arg(existingMask.toString()).arg(mask.toString()).arg(combined.toString()), __FILE__, __LINE__);
		}
	}
	
	// Update last_mask and last_checked timestamp in database
	// Use INSERT OR IGNORE to create the row if it doesn't exist yet
	QSqlQuery insertQuery(db);
	insertQuery.prepare("INSERT OR IGNORE INTO `anime` (`aid`) VALUES (?)");
	insertQuery.addBindValue(aid);
	insertQuery.exec();
	
	QSqlQuery updateQuery(db);
	updateQuery.prepare("UPDATE `anime` SET `last_mask` = ?, `last_checked` = ? WHERE `aid` = ?");
	Mask combinedMaskObj(combinedMask);
	updateQuery.addBindValue(combinedMaskObj.toString());
	updateQuery.addBindValue(QDateTime::currentSecsSinceEpoch());
	updateQuery.addBindValue(aid);
	if(!updateQuery.exec())
	{
		Logger::log(QString("[AniDB Cache] Failed to update last_mask and last_checked for AID %1").arg(aid), __FILE__, __LINE__);
	}
	else
	{
		Logger::log(QString("[AniDB Cache] Updated last_mask (0x%1) and last_checked for AID %1").arg(combinedMaskObj.toString()).arg(aid), __FILE__, __LINE__);
	}
	
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	query.exec(q);
	return GetTag(msg);
}

QString AniDBApi::Calendar()
{
	// Check if logged in first
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
		return "0";
	}
	
	QString msg = buildCalendarCommand();
	QString q;
	q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Logger::log("[AniDB Calendar] Database insert error: " + query.lastError().text(), __FILE__, __LINE__);
		return "0";
	}
	return GetTag(msg);
}

/* === Command Builders === */
// These methods build formatted command strings for testing and reuse

QString AniDBApi::buildAuthCommand(QString username, QString password, int protover, QString client, int clientver, QString enc)
{
	return QString("AUTH user=%1&pass=%2&protover=%3&client=%4&clientver=%5&enc=%6&comp=1")
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

QString AniDBApi::buildMylistAddGenericCommand(int aid, QString epno, int viewed, int state, QString storage, QString other)
{
	QString msg = QString("MYLISTADD aid=%1&generic=1&epno=%2").arg(aid).arg(epno);
	if(viewed > 0 && viewed < 3)
	{
		msg += QString("&viewed=%1").arg(viewed-1);
	}
	if(storage.length() > 0)
	{
		msg += QString("&storage=%1").arg(QString(QUrl::toPercentEncoding(storage)));
	}
	if(other.length() > 0)
	{
		// Replace newlines with <br />
		QString escapedOther = other;
        escapedOther.replace("\n", "<br />");
        escapedOther = QString(escapedOther);
        // limit escapedOther to 90 characters to comply with AniDB limits
        if (escapedOther.length() > 90) {
            escapedOther = escapedOther.left(90);
        }
		msg += QString("&other=%1").arg(escapedOther);
	}
	msg += QString("&state=%1").arg(state);
	return msg;
}

QString AniDBApi::buildMylistDelCommand(int lid)
{
	return QString("MYLISTDEL lid=%1").arg(lid);
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
	// Build amask using 7-byte mask covering all available data fields
	// 
	// ANIME command uses byte-oriented mask (7 bytes):
	//   Byte 1: aid, dateflags, year, type, related lists
	//   Byte 2: name variations (romaji, kanji, english, other, short, synonym) - EXCLUDED (from separate dump)
	//   Byte 3: episodes, highest episode, special ep count, dates, url, picname
	//   Byte 4: ratings, reviews, awards, is 18+ restricted
	//   Byte 5: external IDs (ANN, allcinema, animenfo), tags, date updated
	//   Byte 6: character ID list
	//   Byte 7: episode type counts (specials, credits, other, trailer, parody)
	// 
	// Note: Per API spec, selecting unused/retired bits returns error 505
	// All enum values are now properly defined in 64-bit notation
	// Note: Anime name fields are excluded as they come from separate dump
	
	uint64_t amask = 
		// Byte 1
		ANIME_AID | ANIME_DATEFLAGS |
		ANIME_YEAR | ANIME_TYPE |
		ANIME_RELATED_AID_LIST | ANIME_RELATED_AID_TYPE |
		// Byte 2 - EXCLUDED: name fields come from separate dump
		// ANIME_ROMAJI_NAME | ANIME_KANJI_NAME | ANIME_ENGLISH_NAME |
		// ANIME_OTHER_NAME | ANIME_SHORT_NAME_LIST | ANIME_SYNONYM_LIST |
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
	
	Mask mask(amask);
	
	return QString("ANIME aid=%1&amask=%2").arg(aid).arg(mask.toString());
}

QString AniDBApi::buildCalendarCommand()
{
	// CALENDAR command - returns list of anime that have episodes airing soon
	// No parameters required - trailing space required for session/tag parameters
	return QString("CALENDAR ");
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

int AniDBApi::Send(QString str, QString /*msgtype*/, QString tag)
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
    waitingForReply.startWaiting();
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
		
		// Decompress if needed (comp=1 feature)
		QByteArray decompressedData = decompressIfNeeded(data);
		
//		result = codec->toUnicode(data);
//		result.toUtf8();
        result = QString::fromUtf8(decompressedData.data());
		LOG("AniDBApi: Recv: " + result);
		Logger::log(QString("[AniDB Recv] Datagram size: %1 bytes, Read: %2 bytes, Decompressed: %3 bytes, Result length: %4 chars")
			.arg(data.size()).arg(bytesRead).arg(decompressedData.size()).arg(result.length()), __FILE__, __LINE__);
		
		// Detect truncation: UDP MTU limit is typically 1400 bytes
		// Check ONLY the raw datagram size (before decompression), not the decompressed data
		// If raw datagram size is at/near 1400 bytes, the response is likely truncated
		if(bytesRead >= 1400 || data.size() >= 1400)
		{
			isTruncated = true;
			Logger::log(QString("[AniDB Recv] TRUNCATION DETECTED: Datagram at MTU limit (%1 bytes raw, %2 bytes decompressed), response is truncated")
				.arg(data.size()).arg(decompressedData.size()), __FILE__, __LINE__);
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
    if(waitingForReply.hasTimedOut(10000))
    {
        qint64 elapsed = waitingForReply.elapsedMs();
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
            waitingForReply.stopWaiting();
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
            waitingForReply.stopWaiting();
            currentTag = "";
        }
    }
    
    if(!waitingForReply.isWaiting())
    {
        // Check if banned - if so, stop all communication until app restart
        if(banned == true)
        {
            Logger::log("[AniDB Error] Client is BANNED - blocking all outgoing communication until app restart", __FILE__, __LINE__);
            packetsender->stop();
            return 0;
        }
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
		for (const int fid : fids)
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
		
		for(const QString& param : std::as_const(params))
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
					
					// Update status and binding_status in local_files table
					// status: 2 = in anidb, binding_status: 1 = bound_to_anime
					QSqlQuery statusQuery(db);
					statusQuery.prepare("UPDATE `local_files` SET `status` = 2, `binding_status` = 1 WHERE `id` = ?");
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
			LOG("Could not find mylist entry for tag=" + tag + " via file table join");
			
			// Fallback: try to find lid directly from mylist table by ed2k hash
			// This handles the case where the file was just stored from 310 response
			QSqlQuery recentQuery(db);
			recentQuery.prepare("SELECT m.lid FROM mylist m WHERE m.fid = (SELECT fid FROM file WHERE ed2k = ? LIMIT 1)");
			recentQuery.addBindValue(ed2k);
			if(recentQuery.exec() && recentQuery.next())
			{
				int lid = recentQuery.value(0).toInt();
				LOG(QString("Found lid=%1 via fallback query by ed2k").arg(lid));
				
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
						LOG(QString("Updated local_file for lid=%1 to local_file_id=%2 (path: %3) via fallback").arg(lid).arg(localFileId).arg(localPath));
						
						// Update status and binding_status in local_files table
						QSqlQuery statusQuery(db);
						statusQuery.prepare("UPDATE `local_files` SET `status` = 2, `binding_status` = 1 WHERE `id` = ?");
						statusQuery.addBindValue(localFileId);
						statusQuery.exec();
						
						return lid;
					}
				}
			}
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
				
				// Update status and binding_status in local_files table
				// status: 2 = in anidb, binding_status: 1 = bound_to_anime
				QSqlQuery statusQuery(db);
				statusQuery.prepare("UPDATE `local_files` SET `status` = 2, `binding_status` = 1 WHERE `id` = ?");
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

void AniDBApi::UpdateLocalFileBindingStatus(QString localPath, int bindingStatus)
{
	// Check if database is valid and open before using it
	if (!db.isValid() || !db.isOpen())
	{
		LOG("Database not available, cannot update local file binding status");
		return;
	}
	
	// Update the binding_status in local_files table
	// binding_status: 0=not_bound, 1=bound_to_anime, 2=not_anime
	QSqlQuery query(db);
	query.prepare("UPDATE `local_files` SET `binding_status` = ? WHERE `path` = ?");
	query.addBindValue(bindingStatus);
	query.addBindValue(localPath);
	
	if(query.exec())
	{
		LOG(QString("Updated local_files binding_status for path=%1 to binding_status=%2").arg(localPath).arg(bindingStatus));
	}
	else
	{
		LOG("Failed to update local_files binding_status: " + query.lastError().text());
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
	
	// Get file size
	QFileInfo fileInfo(localPath);
	qint64 fileSize = fileInfo.exists() ? fileInfo.size() : 0;
	
	// Update the ed2k_hash, file_size and status in local_files table
	// Status: 0=not hashed, 1=hashed but not checked by API, 2=in anidb, 3=not in anidb
	QSqlQuery query(db);
	query.prepare("UPDATE `local_files` SET `ed2k_hash` = ?, `file_size` = ?, `status` = ? WHERE `path` = ?");
	query.addBindValue(ed2kHash);
	query.addBindValue(fileSize);
	query.addBindValue(status);
	query.addBindValue(localPath);
	
	if(query.exec())
	{
		LOG(QString("Updated local_files hash, size and status for path=%1 to status=%2").arg(localPath).arg(status));
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
	query.prepare("UPDATE `local_files` SET `ed2k_hash` = ?, `file_size` = ?, `status` = ? WHERE `path` = ?");
	
	[[maybe_unused]] int successCount = 0;
	int failCount = 0;
	bool hasFailure = false;
	
	// Batch all updates in a single transaction
	for (const auto& pair : pathHashPairs)
	{
		// Get file size
		QFileInfo fileInfo(pair.first);
		qint64 fileSize = fileInfo.exists() ? fileInfo.size() : 0;
		
		query.addBindValue(pair.second); // ed2k hash
		query.addBindValue(fileSize);    // file size
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

QMap<QString, FileHashInfo> AniDBApi::batchGetLocalFileHashes(const QStringList& filePaths)
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
	
	QString queryStr = QString("SELECT `path`, `ed2k_hash`, `status`, `binding_status` FROM `local_files` WHERE `path` IN (%1)")
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
		info.setPath(query.value(0).toString());
		info.setHash(query.value(1).toString());
		info.setStatus(query.value(2).toInt());
		info.setBindingStatus(query.value(3).toInt());
		
		results[info.path()] = info;
	}
	qint64 processTime = processTimer.elapsed();
	LOG(QString("[TIMING] Query result processing: %1 ms [anidbapi.cpp]").arg(processTime));
	
	qint64 totalTime = overallTimer.elapsed();
	Logger::log(QString("[TIMING] batchGetLocalFileHashes TOTAL for %1 paths (found %2): %3 ms [anidbapi.cpp]")
	            .arg(filePaths.size()).arg(results.size()).arg(totalTime), __FILE__, __LINE__);
	
	return results;
}

QList<FileHashInfo> AniDBApi::getUnboundFiles()
{
	QList<FileHashInfo> results;
	
	// Check if database is valid and open
	if (!db.isValid() || !db.isOpen())
	{
		LOG("Database not available, cannot get unbound files");
		return results;
	}
	
	// Get files where binding_status is 0 (not_bound) and status is 3 (not in anidb)
	QSqlQuery query(db);
	query.prepare("SELECT `path`, `filename`, `ed2k_hash`, `status`, `binding_status` FROM `local_files` WHERE `binding_status` = 0 AND `status` = 3 AND `ed2k_hash` IS NOT NULL AND `ed2k_hash` != ''");
	
	if (!query.exec())
	{
		LOG(QString("Failed to get unbound files: %1").arg(query.lastError().text()));
		return results;
	}
	
	while (query.next())
	{
		FileHashInfo info;
		info.setPath(query.value(0).toString());
		// Use filename field if available, otherwise extract from path
		QString filename = query.value(1).toString();
		if (filename.isEmpty()) {
			QFileInfo fileInfo(info.path());
			filename = fileInfo.fileName();
		}
		info.setHash(query.value(2).toString());
		info.setStatus(query.value(3).toInt());
		info.setBindingStatus(query.value(4).toInt());
		
		results.append(info);
	}
	
	LOG(QString("Found %1 unbound files").arg(results.size()));
	return results;
}

QString AniDBApi::deleteFileFromMylist(int lid, bool deleteFromDisk)
{
	Logger::log(QString("[AniDB deleteFileFromMylist] Starting deletion for lid=%1, deleteFromDisk=%2")
	            .arg(lid).arg(deleteFromDisk), __FILE__, __LINE__);
	
	// Check if database is valid and open
	if (!db.isValid() || !db.isOpen())
	{
		Logger::log("[AniDB deleteFileFromMylist] Database not available", __FILE__, __LINE__);
		return QString();
	}
	
	// Get the file info from mylist and local_files tables
	QSqlQuery query(db);
	query.prepare("SELECT m.fid, m.aid, f.size, f.ed2k, lf.path "
	              "FROM mylist m "
	              "LEFT JOIN file f ON m.fid = f.fid "
	              "LEFT JOIN local_files lf ON m.local_file = lf.id "
	              "WHERE m.lid = ?");
	query.addBindValue(lid);
	
	if (!query.exec() || !query.next())
	{
		Logger::log(QString("[AniDB deleteFileFromMylist] Failed to find mylist entry for lid=%1: %2")
		            .arg(lid).arg(query.lastError().text()), __FILE__, __LINE__);
		return QString();
	}
	
	int fid = query.value(0).toInt();
	int aid = query.value(1).toInt();
	qint64 size = query.value(2).toLongLong();
	QString ed2k = query.value(3).toString();
	QString filePath = query.value(4).toString();
	
	Logger::log(QString("[AniDB deleteFileFromMylist] Found file: fid=%1, aid=%2, size=%3, path=%4")
	            .arg(fid).arg(aid).arg(size).arg(filePath), __FILE__, __LINE__);
	
	// Step 1: Delete the physical file from disk if requested
	if (deleteFromDisk && !filePath.isEmpty())
	{
		QFile file(filePath);
		if (file.exists())
		{
			bool deleted = false;
			
			// First attempt: try to remove normally
			if (file.remove())
			{
				Logger::log(QString("[AniDB deleteFileFromMylist] Deleted file from disk: %1").arg(filePath), __FILE__, __LINE__);
				deleted = true;
			}
			else
			{
				// If first attempt failed, try to remove read-only attribute and retry
				// This is common on Windows where downloaded files may be read-only
				QFile::Permissions originalPerms = file.permissions();
				bool hadReadOnly = !(originalPerms & QFile::WriteUser);
				
				if (hadReadOnly)
				{
					Logger::log(QString("[AniDB deleteFileFromMylist] File is read-only, attempting to remove read-only attribute: %1").arg(filePath), __FILE__, __LINE__);
					// Add write permission and retry
					if (file.setPermissions(originalPerms | QFile::WriteUser | QFile::WriteOwner))
					{
						if (file.remove())
						{
							Logger::log(QString("[AniDB deleteFileFromMylist] Deleted file from disk after removing read-only attribute: %1").arg(filePath), __FILE__, __LINE__);
							deleted = true;
						}
					}
				}
			}
			
			if (!deleted)
			{
				// Provide detailed error information for access denied errors
				QFileInfo fileInfo(filePath);
				QString errorDetails = file.errorString();
				QString permissions;
				if (fileInfo.exists()) {
					permissions = QString("readable=%1, writable=%2, executable=%3, isFile=%4, isDir=%5, size=%6")
						.arg(fileInfo.isReadable() ? "yes" : "no",
						     fileInfo.isWritable() ? "yes" : "no",
						     fileInfo.isExecutable() ? "yes" : "no",
						     fileInfo.isFile() ? "yes" : "no",
						     fileInfo.isDir() ? "yes" : "no",
						     QString::number(fileInfo.size()));
				} else {
					permissions = "file info unavailable";
				}
				
				// Check if file might be locked by another process
				QString lockHint;
				if (!fileInfo.isWritable()) {
					lockHint = " (file may be read-only or locked by another process like a video player or torrent client)";
				}
				
				Logger::log(QString("[AniDB deleteFileFromMylist] Failed to delete file from disk: %1 - Error: %2, Permissions: [%3]%4")
				            .arg(filePath, errorDetails, permissions, lockHint), __FILE__, __LINE__);
				// Skip all other steps if file deletion fails
				return QString();
			}
		}
		else
		{
			Logger::log(QString("[AniDB deleteFileFromMylist] File not found on disk (assuming already deleted): %1").arg(filePath), __FILE__, __LINE__);
			// Continue with database cleanup - file was probably already deleted externally
		}
	}
	
	// Step 2: Remove from local_files table
	if (!filePath.isEmpty())
	{
		QSqlQuery deleteLocalFile(db);
		deleteLocalFile.prepare("DELETE FROM local_files WHERE path = ?");
		deleteLocalFile.addBindValue(filePath);
		if (deleteLocalFile.exec())
		{
			Logger::log(QString("[AniDB deleteFileFromMylist] Removed from local_files: %1").arg(filePath), __FILE__, __LINE__);
		}
		else
		{
			Logger::log(QString("[AniDB deleteFileFromMylist] Failed to remove from local_files: %1")
			            .arg(deleteLocalFile.lastError().text()), __FILE__, __LINE__);
		}
	}
	
	// Step 3: Update local mylist table to mark as deleted (state=3)
	QSqlQuery updateMylist(db);
	updateMylist.prepare("UPDATE mylist SET state = 3, local_file = NULL WHERE lid = ?");
	updateMylist.addBindValue(lid);
	if (updateMylist.exec())
	{
		Logger::log(QString("[AniDB deleteFileFromMylist] Updated mylist state to deleted for lid=%1").arg(lid), __FILE__, __LINE__);
	}
	else
	{
		Logger::log(QString("[AniDB deleteFileFromMylist] Failed to update mylist state: %1")
		            .arg(updateMylist.lastError().text()), __FILE__, __LINE__);
	}
	
	// Step 4: Clear watch chunks for this lid
	QSqlQuery deleteChunks(db);
	deleteChunks.prepare("DELETE FROM watch_chunks WHERE lid = ?");
	deleteChunks.addBindValue(lid);
	deleteChunks.exec();
	
	// Step 5: Send MYLISTADD with state=3 to mark as deleted in AniDB API
	// state=3 means "deleted" in AniDB mylist state enum (0=unknown, 1=HDD, 2=CD/DVD, 3=deleted)
	// edit=true (1) means we're updating an existing mylist entry rather than creating a new one
	if (size > 0 && !ed2k.isEmpty())
	{
		QString tag = MylistAdd(size, ed2k, 0, 3, "", true);
		Logger::log(QString("[AniDB deleteFileFromMylist] Sent MYLISTADD with state=3 for lid=%1, tag=%2")
		            .arg(lid).arg(tag), __FILE__, __LINE__);
		return tag;
	}
	else
	{
		Logger::log(QString("[AniDB deleteFileFromMylist] Cannot update AniDB API - missing size or ed2k for lid=%1")
		            .arg(lid), __FILE__, __LINE__);
	}
	
	return QString();
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
	// Check if we should download anime titles
	// Download if: never downloaded before OR last update was more than 24 hours ago
	// Read from database to ensure we have the most up-to-date timestamp
	QSqlQuery query(db);
	query.exec("SELECT `value` FROM `settings` WHERE `name` = 'last_anime_titles_update'");
	
	if(!query.next())
	{
		// Never downloaded before
		return true;
	}
	
	QDateTime lastUpdate = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
	qint64 secondsSinceLastUpdate = lastUpdate.secsTo(QDateTime::currentDateTime());
	bool needsUpdate = secondsSinceLastUpdate > 86400; // 86400 seconds = 24 hours
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
	saveSetting("last_anime_titles_update", QString::number(lastAnimeTitlesUpdate.toSecsSinceEpoch()));
	
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
	for(const QString &line : std::as_const(lines))
	{
		// Skip comments and empty lines
		if(line.startsWith('#') || line.trimmed().isEmpty())
		{
			continue;
		}
		
		// Format: aid|type|language|title
		// Split only on the first 3 pipes to preserve any '|' characters in the title
		int firstPipe = line.indexOf('|');
		if(firstPipe == -1) continue;
		int secondPipe = line.indexOf('|', firstPipe + 1);
		if(secondPipe == -1) continue;
		int thirdPipe = line.indexOf('|', secondPipe + 1);
		if(thirdPipe == -1) continue;
		
		QString aid = line.mid(0, firstPipe).trimmed();
		QString type = line.mid(firstPipe + 1, secondPipe - firstPipe - 1).trimmed();
		QString language = line.mid(secondPipe + 1, thirdPipe - secondPipe - 1).trimmed();
		QString title = line.mid(thirdPipe + 1).trimmed();
		
		// Validate that we have the required fields (title can be empty, though unusual)
		if(!aid.isEmpty() && !type.isEmpty() && !language.isEmpty())
		{
			
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

bool AniDBApi::shouldCheckCalendar()
{
	// Check if we should update the calendar (once per day)
	if(lastCalendarCheck.isNull() || !lastCalendarCheck.isValid())
	{
		return true; // Never checked before
	}
	
	// Check if more than 24 hours have passed since last check
	qint64 secondsSinceLastCheck = lastCalendarCheck.secsTo(QDateTime::currentDateTime());
	return secondsSinceLastCheck > (24 * 60 * 60); // 24 hours
}

void AniDBApi::checkCalendarIfNeeded()
{
	// Check if calendar update is needed
	if(!shouldCheckCalendar())
	{
		qint64 secondsSinceLastCheck = lastCalendarCheck.secsTo(QDateTime::currentDateTime());
		qint64 hoursSinceLastCheck = secondsSinceLastCheck / 3600;
		Logger::log(QString("[AniDB Calendar] Calendar check not needed yet (last check was %1 hours ago)").arg(hoursSinceLastCheck), __FILE__, __LINE__);
		return;
	}
	
	// Only check if logged in
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Logger::log("[AniDB Calendar] Not logged in, skipping calendar check", __FILE__, __LINE__);
		return;
	}
	
	Logger::log("[AniDB Calendar] Performing calendar check for new anime", __FILE__, __LINE__);
	
	// Call the Calendar() method which will send the CALENDAR command
	QString tag = Calendar();
	
	if(tag != "0")
	{
		Logger::log("[AniDB Calendar] Calendar check requested, tag: " + tag, __FILE__, __LINE__);
		
		// Update last check time
		lastCalendarCheck = QDateTime::currentDateTime();
		saveSetting("last_calendar_check", QString::number(lastCalendarCheck.toSecsSinceEpoch()));
		Logger::log("[AniDB Calendar] Updated last calendar check time", __FILE__, __LINE__);
	}
	else
	{
		Logger::log("[AniDB Calendar] Failed to request calendar check", __FILE__, __LINE__);
	}
}

void AniDBApi::requestGroupStatus(int gid)
{
	// Request group status from AniDB
	if(gid <= 0)
	{
		Logger::log("[AniDB GroupStatus] Invalid GID provided", __FILE__, __LINE__);
		return;
	}
	
	// Only request if logged in
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Logger::log("[AniDB GroupStatus] Not logged in, skipping group status request", __FILE__, __LINE__);
		return;
	}
	
	Logger::log(QString("[AniDB GroupStatus] Requesting group status for GID: %1").arg(gid), __FILE__, __LINE__);
	
	// Send GROUPSTATUS command
	QString command = QString("GROUPSTATUS gid=%1").arg(gid);
	Send(command, "", "");
}

void AniDBApi::saveExportQueueState()
{
	// Save export queue state settings
	saveSetting("export_queued", isExportQueued ? "1" : "0");
	saveSetting("export_check_attempts", QString::number(notifyCheckAttempts));
	saveSetting("export_check_interval_ms", QString::number(notifyCheckIntervalMs));
	saveSetting("export_queued_timestamp", QString::number(exportQueuedTimestamp));
	
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
		// No pending export
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
 * Uses AniDBFileInfo class for type-safe parsing.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param fmask File mask indicating which fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return AniDBFileInfo object with parsed fields
 */
AniDBFileInfo AniDBApi::parseFileMask(const QStringList& tokens, unsigned int fmask, int& index)
{
	// Use AniDBFileInfo's factory method for type-safe parsing
	AniDBFileInfo fileInfo = AniDBFileInfo::fromApiResponse(tokens, fmask, index);
	
	// FID is always returned first in FILE responses (tokens[0])
	// It's handled separately in the calling code
	
	return fileInfo;
}

/**
 * Parse anime data from FILE command response using file_amask.
 * Processes mask bits in strict MSB to LSB order using a loop to ensure correctness.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amask Anime mask indicating which anime fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return AniDBAnimeInfo object with parsed anime fields
 */
AniDBAnimeInfo AniDBApi::parseFileAmaskAnimeData(const QStringList& tokens, unsigned int amask, int& index)
{
	AniDBAnimeInfo animeInfo;
	
	// Parse fields based on file_amask bits
	// Process from MSB to LSB using the defined masks
	
	if (amask & aEPISODE_TOTAL) animeInfo.setEptotal(tokens.value(index++));
	if (amask & aEPISODE_LAST) animeInfo.setEplast(tokens.value(index++));
	if (amask & aANIME_YEAR) animeInfo.setYear(tokens.value(index++));
	if (amask & aANIME_TYPE) animeInfo.setType(tokens.value(index++));
	if (amask & aANIME_RELATED_LIST) animeInfo.setRelatedAnimeIds(tokens.value(index++));
	if (amask & aANIME_RELATED_TYPE) animeInfo.setRelatedAnimeTypes(tokens.value(index++));
	if (amask & aANIME_CATAGORY) animeInfo.setCategory(tokens.value(index++));
	// Bit 24 reserved
	if (amask & aANIME_NAME_ROMAJI) animeInfo.setNameRomaji(tokens.value(index++));
	if (amask & aANIME_NAME_KANJI) animeInfo.setNameKanji(tokens.value(index++));
	if (amask & aANIME_NAME_ENGLISH) animeInfo.setNameEnglish(tokens.value(index++));
	if (amask & aANIME_NAME_OTHER) animeInfo.setNameOther(tokens.value(index++));
	if (amask & aANIME_NAME_SHORT) animeInfo.setNameShort(tokens.value(index++));
	if (amask & aANIME_SYNONYMS) animeInfo.setSynonyms(tokens.value(index++));
	// Bits 17-14 reserved
	
	return animeInfo;
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
/**
 * Parse episode data from FILE command response using file_amask.
 * Processes mask bits in strict MSB to LSB order.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amask Anime mask indicating which episode fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return AniDBEpisodeInfo object with parsed episode fields
 */
AniDBEpisodeInfo AniDBApi::parseFileAmaskEpisodeData(const QStringList& tokens, unsigned int amask, int& index)
{
	AniDBEpisodeInfo episodeInfo;
	
	// Parse fields based on file_amask bits (MSB to LSB order)
	if (amask & aEPISODE_NUMBER) episodeInfo.setEpisodeNumber(tokens.value(index++));
	if (amask & aEPISODE_NAME) episodeInfo.setName(tokens.value(index++));
	if (amask & aEPISODE_NAME_ROMAJI) episodeInfo.setNameRomaji(tokens.value(index++));
	if (amask & aEPISODE_NAME_KANJI) episodeInfo.setNameKanji(tokens.value(index++));
	if (amask & aEPISODE_RATING) episodeInfo.setRating(tokens.value(index++));
	if (amask & aEPISODE_VOTE_COUNT) episodeInfo.setVoteCount(tokens.value(index++).toInt());
	// Bits 9-8 reserved
	
	return episodeInfo;
}

/**
 * Parse group data from FILE command response using file_amask.
 * Processes mask bits in strict MSB to LSB order.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amask Anime mask indicating which group fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return AniDBGroupInfo object with parsed group fields
 */
AniDBGroupInfo AniDBApi::parseFileAmaskGroupData(const QStringList& tokens, unsigned int amask, int& index)
{
	AniDBGroupInfo groupInfo;
	
	// Parse fields based on file_amask bits (MSB to LSB order)
	if (amask & aGROUP_NAME) groupInfo.setGroupName(tokens.value(index++));
	if (amask & aGROUP_NAME_SHORT) groupInfo.setGroupShortName(tokens.value(index++));
	// Bits 5-1 reserved
	if (amask & aDATE_AID_RECORD_UPDATED) {
		// Skip this field - not stored
		index++;
	}
	
	return groupInfo;
}

/**
 * Parse anime data from ANIME command response using anime_amask.
 * Processes mask bits in strict MSB to LSB order using a loop to ensure correctness.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amask Anime mask indicating which anime fields are present
 * @param index Current index in tokens array (updated as fields are consumed)
 * @return AniDBAnimeInfo object with parsed anime fields
 */
AniDBAnimeInfo AniDBApi::parseMask(const QStringList& tokens, uint64_t amask, int& index)
{
	AniDBAnimeInfo::LegacyAnimeData data;
	
	// Note: The ANIME command response order is determined by the amask bits
	// Process from highest bit to lowest bit (MSB to LSB) in strict order
	// Byte 1 bits 7-0, then Byte 2 bits 7-0, then Byte 3 bits 7-0, etc.
	// This ensures correctness even if individual if-statements are reordered
	
	// Define all mask bits in MSB to LSB order (bit 7 of each byte first)
	struct MaskBit {
		uint64_t bit;
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
				Logger::log(QString("[AniDB parseMask] Skipping AID bit (already extracted by caller)"), __FILE__, __LINE__);
				continue;
			}
			
			// Check if we have a defined field for this bit
			auto it = bitMap.find(currentBit);
			if (it != bitMap.end())
			{
				// This is a defined field
				const MaskBit* maskBit = it->second;
				QString value = tokens.value(index);
				Logger::log(QString("[AniDB parseMask] Bit match: %1 (bit 0x%2) -> token[%3] = '%4'")
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
				Logger::log(QString("[AniDB parseMask] Retired/unused bit 0x%1 -> token[%2] = '%3' (skipped)")
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
			Logger::log(QString("[AniDB parseMask] Byte 5-7 field: %1 (bit 0x%2) -> token[%3] = '%4'")
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
	
	return AniDBAnimeInfo::fromLegacyStruct(data);
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
AniDBAnimeInfo AniDBApi::parseMaskFromString(const QStringList& tokens, const QString& amaskHexString, int& index)
{
	// Call the version with parsedMaskBytes
	QByteArray unusedBytes;
	return parseMaskFromString(tokens, amaskHexString, index, unusedBytes);
}

/**
 * Parse anime data from AniDB ANIME response using the mask hex string.
 * This version tracks which bits were successfully parsed for re-request logic.
 * 
 * @param tokens Pipe-delimited response tokens
 * @param amaskHexString The anime mask as a hex string (e.g., "fffffcfc")
 * @param index Current index in tokens array (updated as fields are consumed)
 * @param parsedMaskBytes Output: 7-byte array marking which bits were successfully parsed
 * @return AniDBAnimeInfo object with parsed anime fields
 */
AniDBAnimeInfo AniDBApi::parseMaskFromString(const QStringList& tokens, const QString& amaskHexString, int& index, QByteArray& parsedMaskBytes)
{
	// Use legacy struct internally (complex parsing logic)
	AniDBAnimeInfo::LegacyAnimeData data;
	
	// Parse the hex string into bytes
	QString paddedMask = amaskHexString.leftJustified(14, '0');
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
	
	// Initialize parsedMaskBytes to track what we successfully parse
	parsedMaskBytes.clear();
	for (int i = 0; i < 7; i++)
		parsedMaskBytes.append((char)0);
	
	Logger::log(QString("[AniDB parseMaskFromString] Mask string: %1 -> %2 bytes")
		.arg(amaskHexString)
		.arg(maskBytes.size()), __FILE__, __LINE__);
	
	// Define all mask bits in MSB to LSB order with their byte positions
	struct MaskBit {
		int byteIndex;
		unsigned char bitMask;
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
		{0, 0x02, nullptr,                    "RETIRED_BYTE1_BIT1"},
		{0, 0x01, nullptr,                    "RETIRED_BYTE1_BIT0"},
		
		// Byte 2 (byte 1 in array) - bits 7-0
		{1, 0x80, &data.nameromaji,           "ROMAJI_NAME"},
		{1, 0x40, &data.namekanji,            "KANJI_NAME"},
		{1, 0x20, &data.nameenglish,          "ENGLISH_NAME"},
		{1, 0x10, &data.nameother,            "OTHER_NAME"},
		{1, 0x08, &data.nameshort,            "SHORT_NAME_LIST"},
		{1, 0x04, &data.synonyms,             "SYNONYM_LIST"},
		{1, 0x02, nullptr,                    "RETIRED_BYTE2_BIT1"},
		{1, 0x01, nullptr,                    "RETIRED_BYTE2_BIT0"},
		
		// Byte 3 (byte 2 in array) - bits 7-0
		{2, 0x80, &data.episodes,             "EPISODES"},
		{2, 0x40, &data.highest_episode,      "HIGHEST_EPISODE"},
		{2, 0x20, &data.special_ep_count,     "SPECIAL_EP_COUNT"},
		{2, 0x10, &data.air_date,             "AIR_DATE"},
		{2, 0x08, &data.end_date,             "END_DATE"},
		{2, 0x04, &data.url,                  "URL"},
		{2, 0x02, &data.picname,              "PICNAME"},
		{2, 0x01, nullptr,                    "RETIRED_BYTE3_BIT0"},
		
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
		{4, 0x80, nullptr,                    "RETIRED_BYTE5_BIT7"},
		{4, 0x40, &data.ann_id,               "ANN_ID"},
		{4, 0x20, &data.allcinema_id,         "ALLCINEMA_ID"},
		{4, 0x10, &data.animenfo_id,          "ANIMENFO_ID"},
		{4, 0x08, &data.tag_name_list,        "TAG_NAME_LIST"},
		{4, 0x04, &data.tag_id_list,          "TAG_ID_LIST"},
		{4, 0x02, &data.tag_weight_list,      "TAG_WEIGHT_LIST"},
		{4, 0x01, &data.date_record_updated,  "DATE_RECORD_UPDATED"},
		
		// Byte 6 (byte 5 in array) - bits 7-0
		{5, 0x80, &data.character_id_list,    "CHARACTER_ID_LIST"},
		{5, 0x40, nullptr,                    "RETIRED_BYTE6_BIT6"},
		{5, 0x20, nullptr,                    "RETIRED_BYTE6_BIT5"},
		{5, 0x10, nullptr,                    "RETIRED_BYTE6_BIT4"},
		{5, 0x08, nullptr,                    "UNUSED_BYTE6_BIT3"},
		{5, 0x04, nullptr,                    "UNUSED_BYTE6_BIT2"},
		{5, 0x02, nullptr,                    "UNUSED_BYTE6_BIT1"},
		{5, 0x01, nullptr,                    "UNUSED_BYTE6_BIT0"},
		
		// Byte 7 (byte 6 in array) - bits 7-0
		{6, 0x80, &data.specials_count,       "SPECIALS_COUNT"},
		{6, 0x40, &data.credits_count,        "CREDITS_COUNT"},
		{6, 0x20, &data.other_count,          "OTHER_COUNT"},
		{6, 0x10, &data.trailer_count,        "TRAILER_COUNT"},
		{6, 0x08, &data.parody_count,         "PARODY_COUNT"},
		{6, 0x04, nullptr,                    "UNUSED_BYTE7_BIT2"},
		{6, 0x02, nullptr,                    "UNUSED_BYTE7_BIT1"},
		{6, 0x01, nullptr,                    "UNUSED_BYTE7_BIT0"}
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
				Logger::log(QString("[AniDB parseMaskFromString] Skipping AID bit (already extracted by caller)"), __FILE__, __LINE__);
				// Mark as parsed since AID is always available
				parsedMaskBytes[byteIdx] |= maskBits[i].bitMask;
				continue;
			}
			
			// Check if we have a token available
			if (index >= tokens.size())
			{
				// No more tokens available - this field was not received (truncation)
				Logger::log(QString("[AniDB parseMaskFromString] MISSING: %1 (byte %2, bit 0x%3) -> no token at index %4")
					.arg(maskBits[i].name)
					.arg(byteIdx + 1)
					.arg(maskBits[i].bitMask, 0, 16)
					.arg(index), __FILE__, __LINE__);
				break; // Stop processing - truncation point reached
			}
			
			QString value = tokens.value(index);
			
			if (maskBits[i].field != nullptr)
			{
				// This is a defined field - store it
				Logger::log(QString("[AniDB parseMaskFromString] Bit match: %1 (byte %2, bit 0x%3) -> token[%4] = '%5'")
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
				Logger::log(QString("[AniDB parseMaskFromString] Retired/unused: %1 (byte %2, bit 0x%3) -> token[%4] = '%5' (skipped)")
					.arg(maskBits[i].name)
					.arg(byteIdx + 1)
					.arg(maskBits[i].bitMask, 0, 16)
					.arg(index)
					.arg(value.left(80)), __FILE__, __LINE__);
			}
			
			// Mark this bit as successfully parsed
			parsedMaskBytes[byteIdx] |= maskBits[i].bitMask;
			
			// Always increment index for any bit set in the mask
			index++;
		}
	}
	
	// Set legacy fields for backward compatibility
	data.eptotal = data.episodes;
	data.eplast = data.highest_episode;
	
	return AniDBAnimeInfo::fromLegacyStruct(data);
}

/**
 * Calculate a reduced mask containing only the fields that were NOT successfully parsed.
 * This is used for re-requesting missing data after truncation.
 * 
 * @param originalMask Original mask hex string (e.g., "fffffcfc")
 * @param parsedMaskBytes 7-byte array marking which bits were successfully parsed
 * @return Reduced mask hex string containing only unparsed fields
 */
Mask AniDBApi::calculateReducedMask(const Mask& originalMask, const QByteArray& parsedMaskBytes)
{
	// Build parsed mask from byte array
	Mask parsedMask;
	for (int i = 0; i < parsedMaskBytes.size() && i < 7; i++)
	{
		parsedMask.setByte(i, (quint8)parsedMaskBytes[i]);
	}
	
	// Calculate reduced mask: original AND NOT parsed
	// This gives us the bits that were requested but not successfully parsed
	Mask reducedMask = originalMask & (~parsedMask);
	
	Logger::log(QString("[AniDB calculateReducedMask] Original: %1 -> Reduced: %2")
		.arg(originalMask.toString())
		.arg(reducedMask.toString()), __FILE__, __LINE__);
	
	return reducedMask;
}

/**
 * Store file data in the database.
 * Uses AniDBFileInfo type-safe fields directly.
 */
void AniDBApi::storeFileData(const AniDBFileInfo& fileInfo)
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
		.arg(fileInfo.fileId())
		.arg(fileInfo.animeId())
		.arg(fileInfo.episodeId())
		.arg(fileInfo.groupId())
		.arg(fileInfo.mylistId())
		.arg(QString(fileInfo.otherEpisodes()).replace("'", "''"))
		.arg(fileInfo.isDeprecated() ? "1" : "0")
		.arg(fileInfo.state())
		.arg(fileInfo.size())
		.arg(QString(fileInfo.ed2kHash()).replace("'", "''"))
		.arg(QString(fileInfo.md5Hash()).replace("'", "''"))
		.arg(QString(fileInfo.sha1Hash()).replace("'", "''"))
		.arg(QString(fileInfo.crc32()).replace("'", "''"))
		.arg(QString(fileInfo.quality()).replace("'", "''"))
		.arg(QString(fileInfo.source()).replace("'", "''"))
		.arg(QString(fileInfo.audioCodec()).replace("'", "''"))
		.arg(fileInfo.audioBitrate())
		.arg(QString(fileInfo.videoCodec()).replace("'", "''"))
		.arg(fileInfo.videoBitrate())
		.arg(QString(fileInfo.resolution()).replace("'", "''"))
		.arg(QString(fileInfo.fileType()).replace("'", "''"))
		.arg(fileInfo.audioLanguages().join("'").replace("'", "''"))
		.arg(fileInfo.subtitleLanguages().join("'").replace("'", "''"))
		.arg(fileInfo.length())
		.arg(QString(fileInfo.description()).replace("'", "''"))
		.arg(QString(fileInfo.airDate().toString("yyyy-MM-dd")).replace("'", "''"))
		.arg(QString(fileInfo.filename()).replace("'", "''"));
		
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		LOG("Database query error: " + query.lastError().text());
	}
}

/**
 * Convert date string to ISO format (YYYY-MM-DDZ) for database storage.
 * Enforces consistent date format at fundamental level.
 * 
 * Handles:
 * - Unix timestamps (e.g., "1759449600") -> converts to ISO format
 * - ISO dates (e.g., "2025-07-06Z") -> returns as-is
 * - Empty strings -> returns empty
 * 
 * @param dateStr Input date string (Unix timestamp or ISO format)
 * @return Date in YYYY-MM-DDZ format, or empty string if input is invalid/empty
 */
QString AniDBApi::convertToISODate(const QString& dateStr)
{
	if(dateStr.isEmpty())
		return QString();
	
	// Check if it's already in ISO format (YYYY-MM-DD with optional Z)
	static const QRegularExpression isoRegex("^\\d{4}-\\d{2}-\\d{2}Z?$");
	if(isoRegex.match(dateStr).hasMatch())
	{
		// Ensure it ends with Z
		if(dateStr.endsWith("Z"))
			return dateStr;
		else
			return dateStr + "Z";
	}
	
	// Check if it's a Unix timestamp (all digits)
	static const QRegularExpression timestampRegex("^\\d+$");
	if(timestampRegex.match(dateStr).hasMatch())
	{
		bool ok;
		qint64 timestamp = dateStr.toLongLong(&ok);
		if(ok && timestamp > 0)
		{
			QDateTime dt = QDateTime::fromSecsSinceEpoch(timestamp);
			return dt.toString("yyyy-MM-dd") + "Z";
		}
	}
	
	// Invalid format - return empty
	return QString();
}

/**
 * Decompress datagram if it's DEFLATE-compressed (comp=1 feature).
 * 
 * According to AniDB UDP API specification:
 * - If comp=1 is enabled in AUTH, server may send compressed datagrams
 * - Compressed datagrams always start with two zero bytes (0x00 0x00)
 * - Tags should never start with zero, so this is a reliable indicator
 * - Compression algorithm is DEFLATE (RFC 1951)
 * 
 * Implementation tries both zlib format (with header) and raw DEFLATE format
 * to handle potential variations in server compression implementation.
 * 
 * @param data Raw datagram data
 * @return Decompressed data if compressed, or original data if not compressed
 */
QByteArray AniDBApi::decompressIfNeeded(const QByteArray& data)
{
	// Check if data is compressed: first two bytes must be 0x00 0x00
	if(data.size() < 2)
		return data;  // Too small to be compressed
	
	if((unsigned char)data[0] != 0x00 || (unsigned char)data[1] != 0x00)
		return data;  // Not compressed - return as-is
	
	// Data is compressed - decompress using zlib DEFLATE
	Logger::log(QString("[AniDB Decompress] Detected compressed datagram (starts with 0x00 0x00), total size: %1 bytes")
		.arg(data.size()), __FILE__, __LINE__);
	
	// Skip the two leading zero bytes - actual compressed data starts at byte 2
	QByteArray compressedData = data.mid(2);
	
	// Log first few bytes of compressed data for debugging
	QString hexDump;
	for(int i = 0; i < qMin(16, compressedData.size()); i++)
	{
		hexDump += QString("%1 ").arg((unsigned char)compressedData[i], 2, 16, QChar('0'));
	}
	Logger::log(QString("[AniDB Decompress] First bytes of compressed data: %1").arg(hexDump), __FILE__, __LINE__);
	
	// Initialize zlib stream for DEFLATE decompression
	z_stream stream;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = compressedData.size();
	stream.next_in = (Bytef*)compressedData.data();
	
	// Try with zlib format first (windowBits = 15 for zlib wrapper)
	// AniDB documentation says DEFLATE, but implementation might vary
	int ret = inflateInit2(&stream, 15);
	bool tryAlternateFormat = false;
	
	if(ret != Z_OK)
	{
		Logger::log(QString("[AniDB Decompress] inflateInit2 (zlib format) failed with code %1, will try raw DEFLATE").arg(ret), __FILE__, __LINE__);
		tryAlternateFormat = true;
	}
	else
	{
		// Decompress in chunks
		QByteArray decompressed;
		decompressed.reserve(compressedData.size() * 4);
		
		const int CHUNK_SIZE = 4096;
		unsigned char outBuffer[CHUNK_SIZE];
		
		do
		{
			stream.avail_out = CHUNK_SIZE;
			stream.next_out = outBuffer;
			
			ret = inflate(&stream, Z_NO_FLUSH);
			
			if(ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
			{
				Logger::log(QString("[AniDB Decompress] inflate (zlib format) failed with code %1 after %2 bytes, will try raw DEFLATE")
					.arg(ret)
					.arg(stream.total_out), __FILE__, __LINE__);
				inflateEnd(&stream);
				tryAlternateFormat = true;
				break;
			}
			
			int have = CHUNK_SIZE - stream.avail_out;
			decompressed.append((char*)outBuffer, have);
			
			// Z_BUF_ERROR (-5) with no input remaining means decompression is complete
			// This can happen when compressed data doesn't have an explicit end marker
			if(ret == Z_BUF_ERROR && stream.avail_in == 0)
			{
				Logger::log(QString("[AniDB Decompress] inflate returned Z_BUF_ERROR with input exhausted - treating as completion"), __FILE__, __LINE__);
				ret = Z_STREAM_END;  // Treat as successful completion
				break;
			}
			
			// Safety check: if no output was produced and no more input, break to avoid infinite loop
			// But only if it's not Z_BUF_ERROR (which we handle above)
			if(have == 0 && stream.avail_in == 0 && ret != Z_BUF_ERROR)
			{
				Logger::log(QString("[AniDB Decompress] inflate (zlib format) stalled: no output, no input remaining (ret=%1)").arg(ret), __FILE__, __LINE__);
				break;
			}
			
		} while(ret != Z_STREAM_END);
		
		if(ret == Z_STREAM_END)
		{
			inflateEnd(&stream);
			Logger::log(QString("[AniDB Decompress] Successfully decompressed %1 bytes -> %2 bytes (ratio: %3x) using zlib format")
				.arg(compressedData.size())
				.arg(decompressed.size())
				.arg(QString::number((double)decompressed.size() / compressedData.size(), 'f', 2)), __FILE__, __LINE__);
			return decompressed;
		}
	}
	
	// Try raw DEFLATE format if zlib format failed
	if(tryAlternateFormat)
	{
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;
		stream.avail_in = compressedData.size();
		stream.next_in = (Bytef*)compressedData.data();
		
		ret = inflateInit2(&stream, -15);
		if(ret != Z_OK)
		{
			Logger::log(QString("[AniDB Decompress] ERROR: inflateInit2 (raw DEFLATE) also failed with code %1").arg(ret), __FILE__, __LINE__);
			return data;  // Return original data on error
		}
		
		// Decompress in chunks
		QByteArray decompressed;
		decompressed.reserve(compressedData.size() * 4);
		
		const int CHUNK_SIZE = 4096;
		unsigned char outBuffer[CHUNK_SIZE];
		
		do
		{
			stream.avail_out = CHUNK_SIZE;
			stream.next_out = outBuffer;
			
			ret = inflate(&stream, Z_NO_FLUSH);
			
			if(ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
			{
				Logger::log(QString("[AniDB Decompress] ERROR: inflate (raw DEFLATE) failed with code %1, bytes in: %2, bytes out: %3, total in: %4, total out: %5")
					.arg(ret)
					.arg(stream.avail_in)
					.arg(stream.avail_out)
					.arg(stream.total_in)
					.arg(stream.total_out), __FILE__, __LINE__);
				inflateEnd(&stream);
				return data;  // Return original data on error
			}
			
			int have = CHUNK_SIZE - stream.avail_out;
			decompressed.append((char*)outBuffer, have);
			
			// Z_BUF_ERROR (-5) with no input remaining means decompression is complete
			// This can happen when compressed data doesn't have an explicit end marker
			if(ret == Z_BUF_ERROR && stream.avail_in == 0)
			{
				Logger::log(QString("[AniDB Decompress] inflate returned Z_BUF_ERROR with input exhausted - treating as completion"), __FILE__, __LINE__);
				ret = Z_STREAM_END;  // Treat as successful completion
				break;
			}
			
			// Safety check: if no output was produced and no more input, break to avoid infinite loop
			// But only if it's not Z_BUF_ERROR (which we handle above)
			if(have == 0 && stream.avail_in == 0 && ret != Z_BUF_ERROR)
			{
				Logger::log(QString("[AniDB Decompress] inflate (raw DEFLATE) stalled: no output, no input remaining (ret=%1)").arg(ret), __FILE__, __LINE__);
				break;
			}
			
		} while(ret != Z_STREAM_END);
		
		inflateEnd(&stream);
		
		if(ret == Z_STREAM_END)
		{
			Logger::log(QString("[AniDB Decompress] Successfully decompressed %1 bytes -> %2 bytes (ratio: %3x) using raw DEFLATE")
				.arg(compressedData.size())
				.arg(decompressed.size())
				.arg(QString::number((double)decompressed.size() / compressedData.size(), 'f', 2)), __FILE__, __LINE__);
			
			return decompressed;
		}
		else
		{
			Logger::log(QString("[AniDB Decompress] ERROR: raw DEFLATE decompression incomplete (ret=%1)").arg(ret), __FILE__, __LINE__);
			return data;  // Return original data on error
		}
	}
	
	// Should not reach here
	Logger::log("[AniDB Decompress] ERROR: Unexpected code path reached", __FILE__, __LINE__);
	return data;
}

/**
 * Store anime data in the database.
 * Uses AniDBAnimeInfo type-safe fields directly.
 */
void AniDBApi::storeAnimeData(const AniDBAnimeInfo& animeInfo)
{
	if(!animeInfo.isValid())
		return;
	
	// Convert dates to ISO format for consistency
	QString startdate = convertToISODate(animeInfo.airDate());
	QString enddate = convertToISODate(animeInfo.endDate());
	
	// Use INSERT with ON CONFLICT DO UPDATE (UPSERT) to merge data from multiple responses
	// COALESCE preserves existing non-empty values when new value is empty
	QString q = QString("INSERT INTO `anime` "
		"(`aid`, `eptotal`, `eplast`, `year`, `type`, `relaidlist`, "
		"`relaidtype`, `category`, `nameromaji`, `namekanji`, `nameenglish`, "
		"`nameother`, `nameshort`, `synonyms`, `typename`, `startdate`, `enddate`, `picname`, "
		"`dateflags`, `episodes`, `highest_episode`, `special_ep_count`, `url`, "
		"`rating`, `vote_count`, `temp_rating`, `temp_vote_count`, `avg_review_rating`, `review_count`, "
		"`award_list`, `is_18_restricted`, `ann_id`, `allcinema_id`, `animenfo_id`, "
		"`tag_name_list`, `tag_id_list`, `tag_weight_list`, `date_record_updated`, "
		"`character_id_list`, `specials_count`, `credits_count`, `other_count`, `trailer_count`, `parody_count`) "
		"VALUES (:aid, :eptotal, :eplast, :year, :type, :relaidlist, "
		":relaidtype, :category, :nameromaji, :namekanji, :nameenglish, "
		":nameother, :nameshort, :synonyms, :typename, :startdate, :enddate, :picname, "
		":dateflags, :episodes, :highest_episode, :special_ep_count, :url, "
		":rating, :vote_count, :temp_rating, :temp_vote_count, :avg_review_rating, :review_count, "
		":award_list, :is_18_restricted, :ann_id, :allcinema_id, :animenfo_id, "
		":tag_name_list, :tag_id_list, :tag_weight_list, :date_record_updated, "
		":character_id_list, :specials_count, :credits_count, :other_count, :trailer_count, :parody_count) "
		"ON CONFLICT(`aid`) DO UPDATE SET "
		"`eptotal` = COALESCE(NULLIF(excluded.`eptotal`, ''), `anime`.`eptotal`), "
		"`eplast` = COALESCE(NULLIF(excluded.`eplast`, ''), `anime`.`eplast`), "
		"`year` = COALESCE(NULLIF(excluded.`year`, ''), `anime`.`year`), "
		"`type` = COALESCE(NULLIF(excluded.`type`, ''), `anime`.`type`), "
		"`relaidlist` = COALESCE(NULLIF(excluded.`relaidlist`, ''), `anime`.`relaidlist`), "
		"`relaidtype` = COALESCE(NULLIF(excluded.`relaidtype`, ''), `anime`.`relaidtype`), "
		"`category` = COALESCE(NULLIF(excluded.`category`, ''), `anime`.`category`), "
		"`nameromaji` = COALESCE(NULLIF(excluded.`nameromaji`, ''), `anime`.`nameromaji`), "
		"`namekanji` = COALESCE(NULLIF(excluded.`namekanji`, ''), `anime`.`namekanji`), "
		"`nameenglish` = COALESCE(NULLIF(excluded.`nameenglish`, ''), `anime`.`nameenglish`), "
		"`nameother` = COALESCE(NULLIF(excluded.`nameother`, ''), `anime`.`nameother`), "
		"`nameshort` = COALESCE(NULLIF(excluded.`nameshort`, ''), `anime`.`nameshort`), "
		"`synonyms` = COALESCE(NULLIF(excluded.`synonyms`, ''), `anime`.`synonyms`), "
		"`typename` = COALESCE(NULLIF(excluded.`typename`, ''), `anime`.`typename`), "
		"`startdate` = COALESCE(NULLIF(excluded.`startdate`, ''), `anime`.`startdate`), "
		"`enddate` = COALESCE(NULLIF(excluded.`enddate`, ''), `anime`.`enddate`), "
		"`picname` = COALESCE(NULLIF(excluded.`picname`, ''), `anime`.`picname`), "
		"`dateflags` = COALESCE(NULLIF(excluded.`dateflags`, ''), `anime`.`dateflags`), "
		"`episodes` = COALESCE(NULLIF(excluded.`episodes`, ''), `anime`.`episodes`), "
		"`highest_episode` = COALESCE(NULLIF(excluded.`highest_episode`, ''), `anime`.`highest_episode`), "
		"`special_ep_count` = COALESCE(NULLIF(excluded.`special_ep_count`, ''), `anime`.`special_ep_count`), "
		"`url` = COALESCE(NULLIF(excluded.`url`, ''), `anime`.`url`), "
		"`rating` = COALESCE(NULLIF(excluded.`rating`, ''), `anime`.`rating`), "
		"`vote_count` = COALESCE(NULLIF(excluded.`vote_count`, ''), `anime`.`vote_count`), "
		"`temp_rating` = COALESCE(NULLIF(excluded.`temp_rating`, ''), `anime`.`temp_rating`), "
		"`temp_vote_count` = COALESCE(NULLIF(excluded.`temp_vote_count`, ''), `anime`.`temp_vote_count`), "
		"`avg_review_rating` = COALESCE(NULLIF(excluded.`avg_review_rating`, ''), `anime`.`avg_review_rating`), "
		"`review_count` = COALESCE(NULLIF(excluded.`review_count`, ''), `anime`.`review_count`), "
		"`award_list` = COALESCE(NULLIF(excluded.`award_list`, ''), `anime`.`award_list`), "
		"`is_18_restricted` = COALESCE(NULLIF(excluded.`is_18_restricted`, ''), `anime`.`is_18_restricted`), "
		"`ann_id` = COALESCE(NULLIF(excluded.`ann_id`, ''), `anime`.`ann_id`), "
		"`allcinema_id` = COALESCE(NULLIF(excluded.`allcinema_id`, ''), `anime`.`allcinema_id`), "
		"`animenfo_id` = COALESCE(NULLIF(excluded.`animenfo_id`, ''), `anime`.`animenfo_id`), "
		"`tag_name_list` = COALESCE(NULLIF(excluded.`tag_name_list`, ''), `anime`.`tag_name_list`), "
		"`tag_id_list` = COALESCE(NULLIF(excluded.`tag_id_list`, ''), `anime`.`tag_id_list`), "
		"`tag_weight_list` = COALESCE(NULLIF(excluded.`tag_weight_list`, ''), `anime`.`tag_weight_list`), "
		"`date_record_updated` = COALESCE(NULLIF(excluded.`date_record_updated`, ''), `anime`.`date_record_updated`), "
		"`character_id_list` = COALESCE(NULLIF(excluded.`character_id_list`, ''), `anime`.`character_id_list`), "
		"`specials_count` = COALESCE(NULLIF(excluded.`specials_count`, ''), `anime`.`specials_count`), "
		"`credits_count` = COALESCE(NULLIF(excluded.`credits_count`, ''), `anime`.`credits_count`), "
		"`other_count` = COALESCE(NULLIF(excluded.`other_count`, ''), `anime`.`other_count`), "
		"`trailer_count` = COALESCE(NULLIF(excluded.`trailer_count`, ''), `anime`.`trailer_count`), "
		"`parody_count` = COALESCE(NULLIF(excluded.`parody_count`, ''), `anime`.`parody_count`)");	
	QSqlQuery query(db);
	query.prepare(q);
	
	query.bindValue(":aid", animeInfo.animeId());
	query.bindValue(":eptotal", animeInfo.eptotal());
	query.bindValue(":eplast", animeInfo.eplast());
	query.bindValue(":year", animeInfo.year());
	query.bindValue(":type", animeInfo.type());
	query.bindValue(":relaidlist", animeInfo.relatedAnimeIds());
	query.bindValue(":relaidtype", animeInfo.relatedAnimeTypes());
	query.bindValue(":category", animeInfo.category());
	query.bindValue(":nameromaji", animeInfo.nameRomaji());
	query.bindValue(":namekanji", animeInfo.nameKanji());
	query.bindValue(":nameenglish", animeInfo.nameEnglish());
	query.bindValue(":nameother", animeInfo.nameOther());
	query.bindValue(":nameshort", animeInfo.nameShort());
	query.bindValue(":synonyms", animeInfo.synonyms());
	query.bindValue(":typename", animeInfo.type());  // typename mirrors type
	query.bindValue(":startdate", startdate);
	query.bindValue(":enddate", enddate);
	query.bindValue(":picname", animeInfo.pictureName());
	
	// Bind fields from Byte 1-7 (all type-safe now)
	query.bindValue(":dateflags", animeInfo.dateFlags());
	query.bindValue(":episodes", animeInfo.episodeCount() > 0 ? animeInfo.episodeCount() : QVariant());
	query.bindValue(":highest_episode", animeInfo.highestEpisode());
	query.bindValue(":special_ep_count", animeInfo.specialEpisodeCount() > 0 ? animeInfo.specialEpisodeCount() : QVariant());
	query.bindValue(":url", animeInfo.url());
	query.bindValue(":rating", animeInfo.rating());
	query.bindValue(":vote_count", animeInfo.voteCount() > 0 ? animeInfo.voteCount() : QVariant());
	query.bindValue(":temp_rating", animeInfo.tempRating());
	query.bindValue(":temp_vote_count", animeInfo.tempVoteCount() > 0 ? animeInfo.tempVoteCount() : QVariant());
	query.bindValue(":avg_review_rating", animeInfo.avgReviewRating());
	query.bindValue(":review_count", animeInfo.reviewCount() > 0 ? animeInfo.reviewCount() : QVariant());
	query.bindValue(":award_list", animeInfo.awardList());
	query.bindValue(":is_18_restricted", animeInfo.is18Restricted() ? 1 : QVariant());
	query.bindValue(":ann_id", animeInfo.annId() > 0 ? animeInfo.annId() : QVariant());
	query.bindValue(":allcinema_id", animeInfo.allCinemaId() > 0 ? animeInfo.allCinemaId() : QVariant());
	query.bindValue(":animenfo_id", animeInfo.animeNfoId());
	query.bindValue(":tag_name_list", animeInfo.tagNameList());
	query.bindValue(":tag_id_list", animeInfo.tagIdList());
	query.bindValue(":tag_weight_list", animeInfo.tagWeightList());
	query.bindValue(":date_record_updated", animeInfo.dateRecordUpdated() > 0 ? animeInfo.dateRecordUpdated() : QVariant());
	query.bindValue(":character_id_list", animeInfo.characterIdList());
	query.bindValue(":specials_count", animeInfo.specialsCount() > 0 ? animeInfo.specialsCount() : QVariant());
	query.bindValue(":credits_count", animeInfo.creditsCount() > 0 ? animeInfo.creditsCount() : QVariant());
	query.bindValue(":other_count", animeInfo.otherCount() > 0 ? animeInfo.otherCount() : QVariant());
	query.bindValue(":trailer_count", animeInfo.trailerCount() > 0 ? animeInfo.trailerCount() : QVariant());
	query.bindValue(":parody_count", animeInfo.parodyCount() > 0 ? animeInfo.parodyCount() : QVariant());
	
	if(!query.exec())
	{
		LOG("Anime database query error: " + query.lastError().text());
	}
}

/**
 * Store episode data in the database.
 * Uses AniDBEpisodeInfo type-safe fields directly.
 */
void AniDBApi::storeEpisodeData(const AniDBEpisodeInfo& episodeInfo)
{
	if(!episodeInfo.isValid())
		return;
		
	QString q = QString("INSERT OR REPLACE INTO `episode` "
		"(`eid`, `name`, `nameromaji`, `namekanji`, `rating`, `votecount`, `epno`) "
		"VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7')")
		.arg(episodeInfo.episodeId())
		.arg(QString(episodeInfo.name()).replace("'", "''"))
		.arg(QString(episodeInfo.nameRomaji()).replace("'", "''"))
		.arg(QString(episodeInfo.nameKanji()).replace("'", "''"))
		.arg(QString(episodeInfo.rating()).replace("'", "''"))
		.arg(episodeInfo.voteCount())
		.arg(QString(episodeInfo.episodeNumber()).replace("'", "''"));
		
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		LOG("Episode database query error: " + query.lastError().text());
	}
}

/**
 * Store group data in the database.
 * Uses AniDBGroupInfo type-safe fields directly.
 */
void AniDBApi::storeGroupData(const AniDBGroupInfo& groupInfo)
{
	if(!groupInfo.isValid())
		return;
	if(!groupInfo.hasName())
		return;
		
	QString q = QString("INSERT OR REPLACE INTO `group` "
		"(`gid`, `name`, `shortname`) "
		"VALUES ('%1', '%2', '%3')")
		.arg(groupInfo.groupId())
		.arg(QString(groupInfo.groupName()).replace("'", "''"))
		.arg(QString(groupInfo.groupShortName()).replace("'", "''"));
		
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
	static const QRegularExpression fmaskRegex("fmask=([0-9a-fA-F]+)");
	QRegularExpressionMatch fmaskMatch = fmaskRegex.match(command);
	
	// Extract amask using regex
	static const QRegularExpression amaskRegex("amask=([0-9a-fA-F]+)");
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

/* === Duplicate Detection Implementation === */

/**
 * Get list of local_files IDs with the same ed2k_hash (duplicates).
 * Returns empty list if no duplicates found or if hash is NULL/empty.
 * Results are not ordered - caller should sort if needed.
 */
QList<int> AniDBApi::getDuplicateLocalFileIds(const QString& ed2k_hash)
{
	QList<int> fileIds;
	
	if (ed2k_hash.isEmpty()) {
		return fileIds;
	}
	
	QSqlQuery query(db);
	
	query.prepare("SELECT id FROM local_files WHERE ed2k_hash = ?");
	query.addBindValue(ed2k_hash);
	
	if (!query.exec()) {
		LOG(QString("Failed to get duplicate file IDs: %1").arg(query.lastError().text()));
		return fileIds;
	}
	
	while (query.next()) {
		fileIds.append(query.value(0).toInt());
	}
	
	return fileIds;
}

/**
 * Get list of all ed2k_hash values that have duplicates (appear more than once).
 * Only returns hashes for files that have been hashed (status > 0).
 */
QStringList AniDBApi::getAllDuplicateHashes()
{
	QStringList hashes;
	QSqlQuery query(db);
	
	// Find all ed2k_hash values that have duplicates (appear more than once)
	// Only consider files that have been hashed (ed2k_hash is not NULL or empty)
	QString queryStr = 
		"SELECT ed2k_hash, COUNT(*) as count "
		"FROM local_files "
		"WHERE ed2k_hash IS NOT NULL AND ed2k_hash != '' "
		"GROUP BY ed2k_hash "
		"HAVING count > 1";
	
	if (!query.exec(queryStr)) {
		LOG(QString("Failed to find duplicate hashes: %1").arg(query.lastError().text()));
		return hashes;
	}
	
	while (query.next()) {
		hashes.append(query.value(0).toString());
	}
	
	return hashes;
}
