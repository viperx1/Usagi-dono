// API definition available at https://wiki.anidb.net/UDP_API_Definition
#include "anidbapi.h"

AniDBApi::AniDBApi(QString client_, int clientver_)
{
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Constructor started");
	protover = 3;
	client = client_; //"usagi";
	clientver = clientver_; //1;
	enc = "utf8";
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Starting DNS lookup for api.anidb.net (this may block)");
	host = QHostInfo::fromName("api.anidb.net");
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] DNS lookup completed");
	if(!host.addresses().isEmpty())
	{
		anidbaddr.setAddress(host.addresses().first().toIPv4Address());
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] DNS resolved successfully to " + host.addresses().first().toString());
	}
	else
	{
		// Fallback to a default IP or leave uninitialized
		// DNS resolution failed, socket operations will be skipped
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Error] DNS resolution for api.anidb.net failed");
	}
	anidbport = 9000;
	loggedin = 0;
	Socket = nullptr;

	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Setting up database connection");
	// Check if default database connection already exists (e.g., in tests)
	if(QSqlDatabase::contains(QSqlDatabase::defaultConnection))
	{
		db = QSqlDatabase::database();
	}
	else
	{
		db = QSqlDatabase::addDatabase("QSQLITE");
	}
	db.setDatabaseName("usagi.sqlite");
	QSqlQuery query;

    aes_key = "8fsd789f7sd7f6sd78695g35345g34gf4";

	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Opening database");
	if(db.open())
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Database opened, starting transaction");
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
		query.exec("CREATE TABLE IF NOT EXISTS `packets`(`tag` INTEGER PRIMARY KEY, `str` TEXT, `processed` BOOL DEFAULT 0, `sendtime` INTEGER, `got_reply` BOOL DEFAULT 0, `reply` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `settings`(`id` INTEGER PRIMARY KEY, `name` TEXT UNIQUE, `value` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `notifications`(`nid` INTEGER PRIMARY KEY, `type` TEXT, `from_user_id` INTEGER, `from_user_name` TEXT, `date` INTEGER, `message_type` INTEGER, `title` TEXT, `body` TEXT, `received_at` INTEGER, `acknowledged` BOOL DEFAULT 0);");
		query.exec("UPDATE `packets` SET `processed` = 1 WHERE `processed` = 0;");
		
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Committing database transaction");
		db.commit();
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Database transaction committed");
	}
//	QStringList names = QStringList()<<"username"<<"password";
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Reading settings from database");
	
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
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Initializing network manager");
	networkManager = new QNetworkAccessManager(this);
	connect(networkManager, &QNetworkAccessManager::finished, this, &AniDBApi::onAnimeTitlesDownloaded);

	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Setting up packet sender timer");
	packetsender = new QTimer();
	connect(packetsender, SIGNAL(timeout()), this, SLOT(SendPacket()));

	packetsender->setInterval(2100);
	packetsender->start();
	
	// Initialize notification checking timer
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Setting up notification check timer");
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
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Checking if anime titles need update");
	if(shouldUpdateAnimeTitles())
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Starting anime titles download");
		downloadAnimeTitles();
	}
	else
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Anime titles are up to date, skipping download");
	}

	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Init] Constructor completed successfully");

//	codec = QTextCodec::codecForName("UTF-8");
}

AniDBApi::~AniDBApi()
{
}

int AniDBApi::CreateSocket()
{
	if(Socket != nullptr)
	{
		Debug("AniDBApi: Socket already created");
		return 1;
	}
 	Socket = new QUdpSocket;
 	if(!Socket->bind(QHostAddress::Any, 3962))
 	{
		Debug("AniDBApi: Can't bind socket");
		return 0;
 	}
 	else
 	{
		if(Socket->isValid())
	 	{
			Debug("AniDBApi: UDP socket created");
		}
		else
		{
			Debug("AniDBApi: ERROR: failed to create UDP socket");
			Debug("AniDBApi: " + Socket->errorString());
			return 0;
		}
	}
	Socket->connectToHost(anidbaddr, anidbport);
/*	if(Socket->error())
	{
		Debug("AniDBApi: " + Socket->errorString());
		return 0;
	}*/
	connect(Socket, SIGNAL(readyRead()), this, SLOT(Recv()));
	return 1;
}

QString AniDBApi::ParseMessage(QString Message, QString ReplyTo, QString ReplyToMsg)
{
	if(Message.length() == 0)
	{
        Debug("AniDBApi: ParseMessage: Message empty");
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
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] Tagless response detected - Tag: " + Tag + " ReplyID: " + ReplyID);
		}
		else
		{
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] Tag: " + Tag + " ReplyID: " + ReplyID);
		}
	}
	else
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] Tag: " + Tag + " ReplyID: " + ReplyID);
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
        Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 203 LOGGED OUT - Tag: " + Tag);
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
				
				// Insert into mylist table
				q = QString("INSERT OR REPLACE INTO `mylist` "
					"(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) "
					"VALUES (%1, %2, %3, %4, %5, %6, %7, '%8')")
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
					Debug("Failed to insert mylist entry: " + insertQuery.lastError().text());
				}
				else
				{
					Debug(QString("Successfully added mylist entry - lid=%1, fid=%2").arg(lid).arg(fid));
				}
			}
			else
			{
				Debug("Could not find file info for size=" + size + " ed2k=" + ed2k);
			}
		}
		
		notifyMylistAdd(Tag, 210);
	}
	else if(ReplyID == "217"){ // 217 EXPORT QUEUED
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 217 EXPORT QUEUED - Tag: " + Tag);
		// Export has been queued and will be generated by AniDB
		// When ready, a notification will be sent with the download link
		
		// Start periodic notification checking
		isExportQueued = true;
		notifyCheckAttempts = 0;
		notifyCheckIntervalMs = 60000; // Start with 1 minute
		exportQueuedTimestamp = QDateTime::currentSecsSinceEpoch();
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		notifyCheckTimer->start();
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Started periodic notification checking (every 1 minute initially)");
		
		// Save state to persist across restarts
		saveExportQueueState();
		
		emit notifyExportQueued(Tag);
	}
	else if(ReplyID == "218"){ // 218 EXPORT CANCELLED
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 218 EXPORT CANCELLED - Tag: " + Tag);
		// Reply to cancel=1 parameter - export was cancelled
	}
	else if(ReplyID == "317"){ // 317 EXPORT NO SUCH TEMPLATE
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 317 EXPORT NO SUCH TEMPLATE - Tag: " + Tag);
		// The requested template name does not exist
		// Valid templates: xml-plain-cs, xml, csv, json, anidb
		emit notifyExportNoSuchTemplate(Tag);
	}
	else if(ReplyID == "318"){ // 318 EXPORT ALREADY IN QUEUE
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 318 EXPORT ALREADY IN QUEUE - Tag: " + Tag);
		// An export is already queued - cannot queue another until current one completes
		// Client should wait for notification when current export is ready
		emit notifyExportAlreadyInQueue(Tag);
	}
	else if(ReplyID == "319"){ // 319 EXPORT NO EXPORT QUEUED OR IS PROCESSING
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 319 EXPORT NO EXPORT QUEUED OR IS PROCESSING - Tag: " + Tag);
		// Reply to cancel=1 when there's no export to cancel
		// No export is currently queued or being processed
	}
	else if(ReplyID == "220"){ // 220 FILE
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		token2 = token2.first().split("|");
		
		// Parse file data (indices 0-26)
		QString q = QString("INSERT OR REPLACE INTO `file` (`fid`, `aid`, `eid`, `gid`, `lid`, `othereps`, `isdepr`, `state`, `size`, `ed2k`, `md5`, `sha1`, `crc`, `quality`, `source`, `codec_audio`, `bitrate_audio`, `codec_video`, `bitrate_video`, `resolution`, `filetype`, `lang_dub`, `lang_sub`, `length`, `description`, `airdate`, `filename`) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10', '%11', '%12', '%13', '%14', '%15', '%16', '%17', '%18', '%19', '%20', '%21', '%22', '%23', '%24', '%25', '%26', '%27')")
					.arg(QString(token2.at(0)).replace("'", "''"))
					.arg(QString(token2.at(1)).replace("'", "''"))
					.arg(QString(token2.at(2)).replace("'", "''"))
					.arg(QString(token2.at(3)).replace("'", "''"))
					.arg(QString(token2.at(4)).replace("'", "''"))
					.arg(QString(token2.at(5)).replace("'", "''"))
					.arg(QString(token2.at(6)).replace("'", "''"))
					.arg(QString(token2.at(7)).replace("'", "''"))
					.arg(QString(token2.at(8)).replace("'", "''"))
					.arg(QString(token2.at(9)).replace("'", "''"))
					.arg(QString(token2.at(10)).replace("'", "''"))
					.arg(QString(token2.at(11)).replace("'", "''"))
					.arg(QString(token2.at(12)).replace("'", "''"))
					.arg(QString(token2.at(13)).replace("'", "''"))
					.arg(QString(token2.at(14)).replace("'", "''"))
					.arg(QString(token2.at(15)).replace("'", "''"))
					.arg(QString(token2.at(16)).replace("'", "''"))
					.arg(QString(token2.at(17)).replace("'", "''"))
					.arg(QString(token2.at(18)).replace("'", "''"))
					.arg(QString(token2.at(19)).replace("'", "''"))
					.arg(QString(token2.at(20)).replace("'", "''"))
					.arg(QString(token2.at(21)).replace("'", "''"))
					.arg(QString(token2.at(22)).replace("'", "''"))
					.arg(QString(token2.at(23)).replace("'", "''"))
					.arg(QString(token2.at(24)).replace("'", "''"))
					.arg(QString(token2.at(25)).replace("'", "''"))
					.arg(QString(token2.at(26)).replace("'", "''"))
					.arg(token2.size() > 27 ? QString(token2.at(27)).replace("'", "''") : "");
		QSqlQuery query(db);
		if(!query.exec(q))
		{
			Debug("Database query error: " + query.lastError().text());
		}
		
		// Parse anime/episode data if available (indices 27+)
		// Anime data fields based on amask: eptotal|eplast|year|type|relaidlist|relaidtype|category|
		//   nameromaji|namekanji|nameenglish|nameother|nameshort|synonyms|
		//   epno|epname|epnameromaji|epnamekanji|eprating|epvotecount|groupname|groupshortname|dateaidrecordupdated
		if(token2.size() > 27)
		{
			QString aid = token2.size() > 1 ? token2.at(1) : "";
			QString eid = token2.size() > 2 ? token2.at(2) : "";
			
			// Anime data starts at index 27
			QString eptotal = token2.size() > 27 ? token2.at(27) : "";
			QString eplast = token2.size() > 28 ? token2.at(28) : "";
			QString year = token2.size() > 29 ? token2.at(29) : "";
			QString type = token2.size() > 30 ? token2.at(30) : "";
			QString relaidlist = token2.size() > 31 ? token2.at(31) : "";
			QString relaidtype = token2.size() > 32 ? token2.at(32) : "";
			QString category = token2.size() > 33 ? token2.at(33) : "";
			QString nameromaji = token2.size() > 34 ? token2.at(34) : "";
			QString namekanji = token2.size() > 35 ? token2.at(35) : "";
			QString nameenglish = token2.size() > 36 ? token2.at(36) : "";
			QString nameother = token2.size() > 37 ? token2.at(37) : "";
			QString nameshort = token2.size() > 38 ? token2.at(38) : "";
			QString synonyms = token2.size() > 39 ? token2.at(39) : "";
			QString epno = token2.size() > 40 ? token2.at(40) : "";  // Episode number!
			QString epname = token2.size() > 41 ? token2.at(41) : "";
			QString epnameromaji = token2.size() > 42 ? token2.at(42) : "";
			QString epnamekanji = token2.size() > 43 ? token2.at(43) : "";
			QString eprating = token2.size() > 44 ? token2.at(44) : "";
			QString epvotecount = token2.size() > 45 ? token2.at(45) : "";
			
			// Store anime data
			if(!aid.isEmpty())
			{
				QString q_anime = QString("INSERT OR REPLACE INTO `anime` (`aid`, `eptotal`, `eplast`, `year`, `type`, `relaidlist`, `relaidtype`, `category`, `nameromaji`, `namekanji`, `nameenglish`, `nameother`, `nameshort`, `synonyms`) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10', '%11', '%12', '%13', '%14')")
					.arg(QString(aid).replace("'", "''"))
					.arg(QString(eptotal).replace("'", "''"))
					.arg(QString(eplast).replace("'", "''"))
					.arg(QString(year).replace("'", "''"))
					.arg(QString(type).replace("'", "''"))
					.arg(QString(relaidlist).replace("'", "''"))
					.arg(QString(relaidtype).replace("'", "''"))
					.arg(QString(category).replace("'", "''"))
					.arg(QString(nameromaji).replace("'", "''"))
					.arg(QString(namekanji).replace("'", "''"))
					.arg(QString(nameenglish).replace("'", "''"))
					.arg(QString(nameother).replace("'", "''"))
					.arg(QString(nameshort).replace("'", "''"))
					.arg(QString(synonyms).replace("'", "''"));
				if(!query.exec(q_anime))
				{
					Debug("Anime database query error: " + query.lastError().text());
				}
			}
			
			// Store episode data including episode number
			if(!eid.isEmpty())
			{
				QString q_episode = QString("INSERT OR REPLACE INTO `episode` (`eid`, `name`, `nameromaji`, `namekanji`, `rating`, `votecount`, `epno`) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7')")
					.arg(QString(eid).replace("'", "''"))
					.arg(QString(epname).replace("'", "''"))
					.arg(QString(epnameromaji).replace("'", "''"))
					.arg(QString(epnamekanji).replace("'", "''"))
					.arg(QString(eprating).replace("'", "''"))
					.arg(QString(epvotecount).replace("'", "''"))
					.arg(QString(epno).replace("'", "''"));  // Store episode number!
				if(!query.exec(q_episode))
				{
					Debug("Episode database query error: " + query.lastError().text());
				}
			}
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
		// Parse mylist entry: fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
		// Note: lid is NOT included in the response - it's extracted from the query command
		if(token2.size() >= 11 && !lid.isEmpty())
		{
			q = QString("INSERT OR REPLACE INTO `mylist` (`lid`, `fid`, `eid`, `aid`, `gid`, `date`, `state`, `viewed`, `viewdate`, `storage`, `source`, `other`, `filestate`) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10', '%11', '%12', '%13')")
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
				Debug("Database query error: " + insertQuery.lastError().text());
			}
			else
			{
				Debug(QString("Successfully stored mylist entry - lid=%1, fid=%2").arg(lid).arg(QString(token2.at(0))));
			}
		}
		else if(lid.isEmpty())
		{
			Debug("Could not extract lid from MYLIST command");
		}
	}
	else if(ReplyID == "222"){ // 222 MYLISTSTATS
		// Response format: entries|watched|size|viewed size|viewed%|watched%|episodes watched
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 222 MYLISTSTATS - Tag: " + Tag + " Data: " + token2.first());
	}
	else if(ReplyID == "223"){ // 223 WISHLIST
		// Parse wishlist response
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 223 WISHLIST - Tag: " + Tag + " Data: " + token2.first());
	}
	else if(ReplyID == "240"){ // 240 EPISODE
		// Response format: eid|aid|length|rating|votes|epno|eng|romaji|kanji|aired|type
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		token2 = token2.first().split("|");
		
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
				Debug("Episode database query error: " + query.lastError().text());
			}
			else
			{
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 240 EPISODE stored - EID: " + eid + " AID: " + aid + " EPNO: " + epno + " Name: " + epname);
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
				
				// Update mylist table
				q = QString("INSERT OR REPLACE INTO `mylist` "
					"(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) "
					"VALUES (%1, %2, %3, %4, %5, %6, %7, '%8')")
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
					Debug("Failed to update mylist entry: " + insertQuery.lastError().text());
				}
				else
				{
					Debug(QString("Successfully updated mylist entry - lid=%1, fid=%2").arg(lid).arg(fid));
				}
			}
			else
			{
				Debug("Could not find file info for size=" + size + " ed2k=" + ed2k);
			}
		}
		
		notifyMylistAdd(Tag, 311);
	}
	else if(ReplyID == "312"){ // 312 NO SUCH MYLIST ENTRY
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 312 NO SUCH MYLIST ENTRY - Tag: " + Tag);
	}
    else if(ReplyID == "320"){ // 320 NO SUCH FILE
        notifyMylistAdd(Tag, 320);
        // Mark packet as processed and received reply instead of deleting
        QString q = QString("UPDATE `packets` SET `processed` = 1, `got_reply` = 1, `reply` = '%1' WHERE `tag` = '%2'").arg(ReplyID).arg(Tag);
        Debug("Database update query: " + q + " Tag: " + Tag);
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
			
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 270 NOTIFICATION - NID: " + QString::number(nid) + " Title: " + title + " Body: " + body);
			
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
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Database] Error storing notification: " + query.lastError().text());
			}
			
			// Check if this is an export notification (contains .tgz link)
			if(body.contains(".tgz", Qt::CaseInsensitive) && isExportQueued)
			{
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Export notification received, stopping periodic checks");
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
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 271 NOTIFICATION ACKNOWLEDGED - Tag: " + Tag);
	}
	else if(ReplyID == "272"){ // 272 NO SUCH NOTIFICATION
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 272 NO SUCH NOTIFICATION - Tag: " + Tag);
	}
	else if(ReplyID == "290"){ // 290 NOTIFYLIST
		// Parse notification list - show all entries
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 290 NOTIFYLIST - Tag: " + Tag + " Entry count: " + QString::number(token2.size()));
		
		// Log all notification entries
		for(int i = 0; i < token2.size(); i++)
		{
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 290 NOTIFYLIST Entry " + QString::number(i+1) + " of " + QString::number(token2.size()) + ": " + token2[i]);
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
		
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 290 NOTIFYLIST - Total messages: " + QString::number(messageNids.size()) + ", New messages: " + QString::number(newNids.size()));
		
		// Fetch only new message notifications to search for export link
		// The export notification is most likely recent, but not guaranteed to be the very last one
		const int maxNotificationsToFetch = 10;
		int notificationsToFetch = qMin(newNids.size(), maxNotificationsToFetch);
		
		if(notificationsToFetch > 0)
		{
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 290 NOTIFYLIST - Fetching " + QString::number(notificationsToFetch) + " new message notifications");
			emit notifyCheckStarting(notificationsToFetch);
			for(int i = 0; i < notificationsToFetch; i++)
			{
				// Fetch from the end of the list (most recent first)
				QString nid = newNids[newNids.size() - 1 - i];
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 290 NOTIFYLIST - Fetching new message notification " + QString::number(i+1) + " of " + QString::number(notificationsToFetch) + ": " + nid);
				NotifyGet(nid.toInt());
			}
		}
		else if(messageNids.size() > 0)
		{
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 290 NOTIFYLIST - No new notifications to fetch, all are already in database");
		}
	}
	else if(ReplyID == "291"){ // 291 NOTIFYLIST ENTRY
		// Parse notification list - can contain single entry (pagination) or full list
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 291 NOTIFYLIST - Tag: " + Tag + " Entry count: " + QString::number(token2.size()));
		
		// Log all notification entries
		for(int i = 0; i < token2.size(); i++)
		{
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 291 NOTIFYLIST Entry " + QString::number(i+1) + " of " + QString::number(token2.size()) + ": " + token2[i]);
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
		
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 291 NOTIFYLIST - Total messages: " + QString::number(messageNids.size()) + ", New messages: " + QString::number(newNids.size()));
		
		// Fetch only new message notifications to search for export link
		// The export notification is most likely recent, but not guaranteed to be the very last one
		const int maxNotificationsToFetch = 10;
		int notificationsToFetch = qMin(newNids.size(), maxNotificationsToFetch);
		
		if(notificationsToFetch > 0)
		{
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 291 NOTIFYLIST - Fetching " + QString::number(notificationsToFetch) + " new message notifications");
			emit notifyCheckStarting(notificationsToFetch);
			for(int i = 0; i < notificationsToFetch; i++)
			{
				// Fetch from the end of the list (most recent first)
				QString nid = newNids[newNids.size() - 1 - i];
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 291 NOTIFYLIST - Fetching new message notification " + QString::number(i+1) + " of " + QString::number(notificationsToFetch) + ": " + nid);
				NotifyGet(nid.toInt());
			}
		}
		else if(messageNids.size() > 0)
		{
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 291 NOTIFYLIST - No new notifications to fetch, all are already in database");
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
			
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 292 NOTIFYGET - ID: " + QString::number(id) + " Title: " + title + " Body: " + body);
			
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
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Database] Error storing notification: " + query.lastError().text());
			}
			
			// Check if this is an export notification (contains .tgz link)
			if(body.contains(".tgz", Qt::CaseInsensitive) && isExportQueued)
			{
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Export notification received, stopping periodic checks");
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
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 292 NOTIFYGET - Invalid format, parts count: " + QString::number(parts.size()));
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
			
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 293 NOTIFYGET - RelID: " + QString::number(relid) + " Type: " + QString::number(type) + " Count: " + QString::number(count) + " Name: " + relidname + " FIDs: " + fids);
			
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
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Database] Error storing file notification: " + query.lastError().text());
			}
			
			// Note: For N-type notifications, we don't emit notifyMessageReceived as these are file notifications
			// They would need different handling for new file notifications
			
			// Note: PUSHACK is only for PUSH notifications (code 270), not for fetched notifications via NOTIFYGET
		}
		else
		{
			Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 293 NOTIFYGET - Invalid format, parts count: " + QString::number(parts.size()));
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
		Debug("AniDBApi: Client banned: "+ bannedfor);
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
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Error] 598 UNKNOWN COMMAND - Tag: " + Tag + " - check request format");
	}
	else if(ReplyID == "601"){ // 601 ANIDB OUT OF SERVICE - TRY AGAIN LATER
	}
	else if(ReplyID == "702"){ // 702 NO SUCH PACKET PENDING
		// This occurs when trying to PUSHACK a notification that wasn't sent via PUSH
		// PUSHACK is only for notifications received via code 270 (PUSH), not for notifications fetched via NOTIFYGET
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Response] 702 NO SUCH PACKET PENDING - Tag: " + Tag);
	}
    else
    {
        Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Error] ParseMessage - UNSUPPORTED ReplyID: " + ReplyID + " Tag: " + Tag);
    }
    waitingForReply.isWaiting = false;
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
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Auth] Database query error: " + query.lastError().text());
	}

//	Send(msg, "AUTH", "xxx");

	return 0;
}

QString AniDBApi::Logout()
{
	QString msg = buildLogoutCommand();
    Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB API] Sending LOGOUT command");
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
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB MylistAdd] Database insert error: " + query.lastError().text());
		return "0";
	}

/*	q = QString("SELECT `tag` FROM `packets` WHERE `str` = '%1' AND `processed` = 0").arg(msg);
	query.exec(q);
	Debug("Database query error: " + query.lastError().text());

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
	Debug(msg);
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB File] Database insert error: " + query.lastError().text());
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
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB API] Requesting MYLISTEXPORT with template: " + template_name);
	
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
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB API] Requesting EPISODE data for EID: " + QString::number(eid));
	QString msg = buildEpisodeCommand(eid);
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
	if(Socket == nullptr)
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Error] Socket not initialized, cannot send");
		return 0;
	}
	QString a;
	if(SID.length() > 0)
	{
		a = QString("%1&s=%2").arg(str).arg(SID);
	}
	Debug("AniDBApi: Send: " + (a.length() > 0 ? a : a = str));

	a = QString("%1&tag=%2").arg(a).arg(tag);
    Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Send] Command: " + a);
//    QByteArray bytes = a.toUtf8();
//	const char *ptr = bytes.data();
//	Socket->write(ptr);
    Socket->write(a.toUtf8().constData());
    waitingForReply.isWaiting = true;
    waitingForReply.start.start();

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
	while(Socket->hasPendingDatagrams())
	{
		data.resize(Socket->pendingDatagramSize());
		Socket->readDatagram(data.data(), data.size());
//		result = codec->toUnicode(data);
//		result.toUtf8();
        result = QString::fromUtf8(data.data());
		Debug("AniDBApi: Recv: " + result);
    }
	if(result.length() > 0)
	{
		ParseMessage(result,"", lastSentPacket);
		return 1;
	}
	return 0;
}

void AniDBApi::Debug(QString msg)
{
	// Use qDebug instead of QMessageBox for non-blocking output
	// QMessageBox blocks and is not suitable for automated tests
	qDebug() << "AniDBApi:" << msg;
	// QMessageBox(QMessageBox::NoIcon, "", msg).exec();
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
                Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Queue] Sending query - Tag: " + tag + " Command: " + str);
                if(!LoggedIn() && !str.contains("AUTH"))
                {
                    Auth();
                    return 0;
                }
                Send(str,"", tag);
                Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Sent] Command: " + lastSentPacket);
            }
        }
    }
	Recv();
    if(waitingForReply.isWaiting == true && waitingForReply.start.elapsed() > 10000)
    {
        Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Timeout] Waited for reply for more than 10 seconds - Elapsed: " + QString::number(waitingForReply.start.elapsed()) + " ms");
    }
	return 0;
}

unsigned long AniDBApi::LocalIdentify(int size, QString ed2khash)
{
	std::bitset<2> ret;
	QString q = QString("SELECT `fid` FROM `file` WHERE `size` = '%1' AND `ed2k` = '%2'").arg(size).arg(ed2khash);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB LocalIdentify] Database query error: " + query.lastError().text());
		return ret.to_ulong();
	}
	
	int fid = 0;
	if(query.next())
	{
		fid = query.value(0).toInt();
		if(fid > 0)
		{
			ret[0] = 1;
		}
	}
	
	q = QString("SELECT `lid` FROM `mylist` WHERE `fid` = '%1'").arg(fid);
	if(!query.exec(q))
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB LocalIdentify] Database query error: " + query.lastError().text());
		return ret.to_ulong();
	}
	
	if(query.next())
	{
		if(query.value(0).toInt() > 0)
		{
			ret[1] = 1;
		}
	}
//	}
	return ret.to_ulong();
}

void AniDBApi::UpdateFile(int size, QString ed2khash, int viewed, int state, QString storage)
{
	QString q = QString("SELECT `fid`,`lid` FROM `file` WHERE `size` = %1 AND `ed2k` = %2").arg(size).arg(ed2khash);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB UpdateFile] Database query error: " + query.lastError().text());
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
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB UpdateFile] Database update error: " + query.lastError().text());
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

void AniDBApi::UpdateLocalPath(QString tag, QString localPath)
{
	// Get the original MYLISTADD command from packets table using the tag
	QString q = QString("SELECT `str` FROM `packets` WHERE `tag` = '%1'").arg(tag);
	QSqlQuery query(db);
	
	if(query.exec(q) && query.next())
	{
		QString mylistAddCmd = query.value(0).toString();
		
		// Parse size and ed2k from the MYLISTADD command
		QStringList params = mylistAddCmd.split("&");
		QString size, ed2k;
		
		for(const QString& param : params)
		{
			if(param.contains("size="))
				size = param.mid(param.indexOf("size=") + 5).split("&").first();
			else if(param.contains("ed2k="))
				ed2k = param.mid(param.indexOf("ed2k=") + 5).split("&").first();
		}
		
		// Find the lid using the file info
		q = QString("SELECT m.lid FROM mylist m "
					"INNER JOIN file f ON m.fid = f.fid "
					"WHERE f.size = '%1' AND f.ed2k = '%2'")
			.arg(size).arg(ed2k);
		QSqlQuery lidQuery(db);
		
		if(lidQuery.exec(q) && lidQuery.next())
		{
			QString lid = lidQuery.value(0).toString();
			
			// Get the local_file id from local_files table
			q = QString("SELECT id FROM local_files WHERE path = '%1'")
				.arg(QString(localPath).replace("'", "''"));
			QSqlQuery localFileQuery(db);
			
			if(localFileQuery.exec(q) && localFileQuery.next())
			{
				QString localFileId = localFileQuery.value(0).toString();
				
				// Update the local_file reference in mylist table
				q = QString("UPDATE `mylist` SET `local_file` = %1 WHERE `lid` = %2")
					.arg(localFileId)
					.arg(lid);
				QSqlQuery updateQuery(db);
				
				if(updateQuery.exec(q))
				{
					Debug(QString("Updated local_file for lid=%1 to local_file_id=%2 (path: %3)").arg(lid).arg(localFileId).arg(localPath));
					
					// Update status in local_files table to 2 (in anidb)
					q = QString("UPDATE `local_files` SET `status` = 2 WHERE `id` = %1").arg(localFileId);
					QSqlQuery statusQuery(db);
					statusQuery.exec(q);
				}
				else
				{
					Debug("Failed to update local_file: " + updateQuery.lastError().text());
				}
			}
			else
			{
				Debug("Could not find local_file entry for path=" + localPath);
			}
		}
		else
		{
			Debug("Could not find mylist entry for tag=" + tag);
		}
	}
	else
	{
		Debug("Could not find packet for tag=" + tag);
	}
}

void AniDBApi::UpdateLocalFileStatus(QString localPath, int status)
{
	// Update the status in local_files table
	QString q = QString("UPDATE `local_files` SET `status` = %1 WHERE `path` = '%2'")
		.arg(status)
		.arg(QString(localPath).replace("'", "''"));
	QSqlQuery query(db);
	
	if(query.exec(q))
	{
		Debug(QString("Updated local_files status for path=%1 to status=%2").arg(localPath).arg(status));
	}
	else
	{
		Debug("Failed to update local_files status: " + query.lastError().text());
	}
}

void AniDBApi::updateLocalFileHash(QString localPath, QString ed2kHash, int status)
{
	// Update the ed2k_hash and status in local_files table
	// Status: 0=not hashed, 1=hashed but not checked by API, 2=in anidb, 3=not in anidb
	QSqlQuery query(db);
	query.prepare("UPDATE `local_files` SET `ed2k_hash` = ?, `status` = ? WHERE `path` = ?");
	query.addBindValue(ed2kHash);
	query.addBindValue(status);
	query.addBindValue(localPath);
	
	if(query.exec())
	{
		Debug(QString("Updated local_files hash and status for path=%1 to status=%2").arg(localPath).arg(status));
	}
	else
	{
		Debug("Failed to update local_files hash and status: " + query.lastError().text());
	}
}

QString AniDBApi::GetTag(QString str)
{
	QString q = QString("SELECT `tag` FROM `packets` WHERE `str` = '%1' AND `processed` = '0' ORDER BY `tag` ASC LIMIT 1").arg(str);
	QSqlQuery query(db);
	if(!query.exec(q))
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB GetTag] Database query error: " + query.lastError().text());
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
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Checking if anime titles need update");
	// Check if we should download anime titles
	// Download if: never downloaded before OR last update was more than 24 hours ago
	// Read from database to ensure we have the most up-to-date timestamp
	QSqlQuery query(db);
	query.exec("SELECT `value` FROM `settings` WHERE `name` = 'last_anime_titles_update'");
	
	if(!query.next())
	{
		// Never downloaded before
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] No previous download found, download needed");
		return true;
	}
	
	QDateTime lastUpdate = QDateTime::fromSecsSinceEpoch(query.value(0).toLongLong());
	qint64 secondsSinceLastUpdate = lastUpdate.secsTo(QDateTime::currentDateTime());
	bool needsUpdate = secondsSinceLastUpdate > 86400; // 86400 seconds = 24 hours
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Last update was " + QString::number(secondsSinceLastUpdate) + " seconds ago, needs update: " + (needsUpdate ? "yes" : "no"));
	return needsUpdate;
}

void AniDBApi::downloadAnimeTitles()
{
	Debug("Downloading anime titles from AniDB...");
	
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
	
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Download callback triggered");
	if(reply->error() != QNetworkReply::NoError)
	{
		Debug(QString("Failed to download anime titles: %1").arg(reply->errorString()));
		reply->deleteLater();
		return;
	}
	
	QByteArray compressedData = reply->readAll();
	reply->deleteLater();
	
	Debug(QString("Downloaded %1 bytes of compressed anime titles data").arg(compressedData.size()));
	
	// The file is in gzip format, which uses deflate compression
	// We need to decompress it properly using zlib
	QByteArray decompressedData;
	
	// Check if it's gzip format (starts with 0x1f 0x8b)
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Starting decompression");
	if(compressedData.size() >= 2 && 
	   (unsigned char)compressedData[0] == 0x1f && 
	   (unsigned char)compressedData[1] == 0x8b)
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Detected gzip format, using zlib decompression");
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
			Debug(QString("Failed to initialize gzip decompression: %1").arg(ret));
			return;
		}
		
		// Allocate output buffer - start with 4MB which should handle most anime titles files
		const int CHUNK = 4 * 1024 * 1024;
		unsigned char* out = new unsigned char[CHUNK];
		
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Starting inflate operation (this may take a moment)");
		// Decompress
		do {
			stream.avail_out = CHUNK;
			stream.next_out = out;
			
			ret = inflate(&stream, Z_NO_FLUSH);
			if(ret == Z_STREAM_ERROR || ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
			{
				Debug(QString("Gzip decompression failed: %1").arg(ret));
				delete[] out;
				inflateEnd(&stream);
				return;
			}
			
			int have = CHUNK - stream.avail_out;
			decompressedData.append((char*)out, have);
		} while(ret != Z_STREAM_END);
		
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Decompression completed successfully");
		// Clean up
		delete[] out;
		inflateEnd(&stream);
	}
	else
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Not gzip format, trying qUncompress");
		// Try direct decompression with qUncompress (for zlib format)
		decompressedData = qUncompress(compressedData);
	}
	
	if(decompressedData.isEmpty())
	{
		Debug("Failed to decompress anime titles data. Will retry on next startup.");
		return;
	}
	
	Debug(QString("Decompressed to %1 bytes").arg(decompressedData.size()));
	
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Starting to parse and store titles");
	parseAndStoreAnimeTitles(decompressedData);
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Finished parsing and storing titles");
	
	// Update last download timestamp
	lastAnimeTitlesUpdate = QDateTime::currentDateTime();
	QSqlQuery query(db);
	QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'last_anime_titles_update', '%1');")
				.arg(lastAnimeTitlesUpdate.toSecsSinceEpoch());
	query.exec(q);
	
	Debug(QString("Anime titles updated successfully at %1").arg(lastAnimeTitlesUpdate.toString()));
}

void AniDBApi::parseAndStoreAnimeTitles(const QByteArray &data)
{
	if(data.isEmpty())
	{
		Debug("No data to parse for anime titles");
		return;
	}
	
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Starting to parse anime titles data (" + QString::number(data.size()) + " bytes)");
	QString content = QString::fromUtf8(data);
	QStringList lines = content.split('\n', Qt::SkipEmptyParts);
	
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Starting database transaction for " + QString::number(lines.size()) + " lines");
	db.transaction();
	
	// Clear old titles
	QSqlQuery query(db);
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Clearing old anime titles from database");
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
				Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Processing progress: " + QString::number(count) + " titles inserted");
			}
		}
	}
	
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Committing database transaction with " + QString::number(count) + " titles");
	db.commit();
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Anime Titles] Parsed and stored " + QString::number(count) + " anime titles successfully");
}

void AniDBApi::checkForNotifications()
{
	if(!isExportQueued)
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] No export queued, stopping notification checks");
		notifyCheckTimer->stop();
		return;
	}
	
	// Check if we've exceeded 48 hours since export was queued
	qint64 elapsedSeconds = QDateTime::currentSecsSinceEpoch() - exportQueuedTimestamp;
	qint64 elapsedHours = elapsedSeconds / 3600;
	if(elapsedSeconds > 48 * 3600)  // 48 hours = 172800 seconds
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Stopping notification checks after 48 hours");
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
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Periodic notification check (attempt " + QString::number(notifyCheckAttempts) + ", interval: " + QString::number(intervalMinutes) + " minutes, elapsed: " + QString::number(elapsedHours) + " hours)");
	
	// Check for new notifications by requesting NOTIFYLIST
	if(SID.length() > 0 && LoginStatus() > 0)
	{
		// Request notification list to check for export ready notification
		QString msg = buildNotifyListCommand();
		QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
		QSqlQuery query(db);
		query.exec(q);
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Requesting NOTIFYLIST to check for export notification");
		
		// Increase check interval by 1 minute after each successful check attempt
		// Cap at 60 minutes to avoid very long waits
		notifyCheckIntervalMs += 60000;
		if(notifyCheckIntervalMs > 3600000)  // Cap at 60 minutes
		{
			notifyCheckIntervalMs = 3600000;
		}
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		int nextIntervalMinutes = notifyCheckIntervalMs / 60000;
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Next check will be in " + QString::number(nextIntervalMinutes) + " minutes");
	}
	else
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Not logged in, skipping notification check");
		// Don't increase interval when not logged in - keep trying at the same interval
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		int nextIntervalMinutes = notifyCheckIntervalMs / 60000;
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Will retry in " + QString::number(nextIntervalMinutes) + " minutes after login");
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
	
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Saved export queue state to database");
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
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Loaded export queue state from database - queued since " + QDateTime::fromSecsSinceEpoch(exportQueuedTimestamp).toString());
		
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
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] No pending export found in database");
	}
}

void AniDBApi::checkForExistingExport()
{
	if(!isExportQueued)
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] No export queued, skipping check for existing export");
		return;
	}
	
	// Check if we've exceeded 48 hours since export was queued
	qint64 elapsedSeconds = QDateTime::currentSecsSinceEpoch() - exportQueuedTimestamp;
	if(elapsedSeconds > 48 * 3600)  // 48 hours
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Export queue expired (>48 hours), clearing state");
		isExportQueued = false;
		notifyCheckAttempts = 0;
		notifyCheckIntervalMs = 60000;
		exportQueuedTimestamp = 0;
		saveExportQueueState();
		return;
	}
	
	Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Checking for existing export notification on startup");
	
	// Check if we're logged in
	if(SID.length() > 0 && LoginStatus() > 0)
	{
		// Request notification list to check if export is already ready
		QString msg = buildNotifyListCommand();
		QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
		QSqlQuery query(db);
		query.exec(q);
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Requested NOTIFYLIST to check for existing export");
		
		// Resume periodic checking with the saved interval
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		notifyCheckTimer->start();
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Resumed periodic notification checking");
	}
	else
	{
		Debug(QString(__FILE__) + " " + QString::number(__LINE__) + " [AniDB Export] Not logged in yet, will check after login");
		// The check will be triggered again after login via the timer
		notifyCheckTimer->setInterval(notifyCheckIntervalMs);
		notifyCheckTimer->start();
	}
}
