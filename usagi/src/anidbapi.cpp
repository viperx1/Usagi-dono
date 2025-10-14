// API definition available at https://wiki.anidb.net/UDP_API_Definition
#include "anidbapi.h"

AniDBApi::AniDBApi(QString client_, int clientver_)
{
	protover = 3;
	client = client_; //"usagi";
	clientver = clientver_; //1;
	enc = "utf8";
	host = QHostInfo::fromName("api.anidb.net");
	if(!host.addresses().isEmpty())
	{
		anidbaddr.setAddress(host.addresses().first().toIPv4Address());
	}
	else
	{
		// Fallback to a default IP or leave uninitialized
		// DNS resolution failed, socket operations will be skipped
		qDebug()<<__FILE__<<__LINE__<<"DNS resolution for api.anidb.net failed";
	}
	anidbport = 9000;
	loggedin = 0;
	Socket = nullptr;

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

	if(db.open())
	{
		db.transaction();
//		Debug("AniDBApi: Database opened");
		query = QSqlQuery(db);
		query.exec("CREATE TABLE IF NOT EXISTS `mylist`(`lid` INTEGER PRIMARY KEY, `fid` INTEGER, `eid` INTEGER, `aid` INTEGER, `gid` INTEGER, `date` INTEGER, `state` INTEGER, `viewed` INTEGER, `viewdate` INTEGER, `storage` TEXT, `source` TEXT, `other` TEXT, `filestate` INTEGER)");
		query.exec("CREATE TABLE IF NOT EXISTS `anime`(`aid` INTEGER PRIMARY KEY, `eptotal` INTEGER, `eplast` INTEGER, `year` TEXT, `type` TEXT, `relaidlist` TEXT, `relaidtype` TEXT, `category` TEXT, `nameromaji` TEXT, `namekanji` TEXT, `nameenglish` TEXT, `nameother` TEXT, `nameshort` TEXT, `synonyms` TEXT);");
        query.exec("CREATE TABLE IF NOT EXISTS `file`(`fid` INTEGER PRIMARY KEY, `aid` INTEGER, `eid` INTEGER, `gid` INTEGER, `lid` INTEGER, `othereps` TEXT, `isdepr` INTEGER, `state` INTEGER, `size` BIGINT, `ed2k` TEXT, `md5` TEXT, `sha1` TEXT, `crc` TEXT, `quality` TEXT, `source` TEXT, `codec_audio` TEXT, `bitrate_audio` INTEGER, `codec_video` TEXT, `bitrate_video` INTEGER, `resolution` TEXT, `filetype` TEXT, `lang_dub` TEXT, `lang_sub` TEXT, `length` INTEGER, `description` TEXT, `airdate` INTEGER, `filename` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `episode`(`eid` INTEGER PRIMARY KEY, `name` TEXT, `nameromaji` TEXT, `namekanji` TEXT, `rating` INTEGER, `votecount` INTEGER);");
		query.exec("CREATE TABLE IF NOT EXISTS `group`(`gid` INTEGER PRIMARY KEY, `name` TEXT, `shortname` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `anime_titles`(`aid` INTEGER, `type` INTEGER, `language` TEXT, `title` TEXT, PRIMARY KEY(`aid`, `type`, `language`, `title`));");
		query.exec("CREATE TABLE IF NOT EXISTS `packets`(`tag` INTEGER PRIMARY KEY, `str` TEXT, `processed` BOOL DEFAULT 0, `sendtime` INTEGER, `got_reply` BOOL DEFAULT 0, `reply` TEXT);");
		query.exec("CREATE TABLE IF NOT EXISTS `settings`(`id` INTEGER PRIMARY KEY, `name` TEXT UNIQUE, `value` TEXT);");
		query.exec("UPDATE `packets` SET `processed` = 1 WHERE `processed` = 0;");
		query.exec("SELECT `name`, `value` FROM `settings` ORDER BY `name` ASC");
		db.commit();
	}
//	QStringList names = QStringList()<<"username"<<"password";
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
	}

	// Initialize network manager for anime titles download
	networkManager = new QNetworkAccessManager(this);
	connect(networkManager, &QNetworkAccessManager::finished, this, &AniDBApi::onAnimeTitlesDownloaded);

	packetsender = new QTimer();
	connect(packetsender, SIGNAL(timeout()), this, SLOT(SendPacket()));

	packetsender->setInterval(2100);
	packetsender->start();

	// Check and download anime titles if needed (automatically on startup)
	if(shouldUpdateAnimeTitles())
	{
		downloadAnimeTitles();
	}

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

    qDebug()<<__FILE__<<__LINE__<<Tag<<" "<<ReplyID;

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
        qDebug()<<__FILE__<<__LINE__<<"Parse message"<<"203";
		loggedin = 0;
        notifyLoggedOut(Tag, 203);
	}
	else if(ReplyID == "210"){ // 210 MYLIST ENTRY ADDED
		notifyMylistAdd(Tag, 210);
	}
	else if(ReplyID == "220"){ // 220 FILE
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		token2 = token2.first().split("|");
		// 1 = file id
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
					.arg(QString(token2.at(26)).replace("'", "''"));
		QSqlQuery query(db);
		query.exec(q);
		qDebug()<<query.lastError().text();
//		qDebug()<<q;
/*		while(!token2.isEmpty())
		{
			Debug("-- " + token2.first());
			token2.pop_front();
		}*/
	}
	else if(ReplyID == "221"){ // 221 MYLIST
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		token2 = token2.first().split("|");
		// Parse mylist entry: lid|fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
		if(token2.size() >= 12)
		{
			QString q = QString("INSERT OR REPLACE INTO `mylist` (`lid`, `fid`, `eid`, `aid`, `gid`, `date`, `state`, `viewed`, `viewdate`, `storage`, `source`, `other`, `filestate`) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10', '%11', '%12', '%13')")
						.arg(QString(token2.at(0)).replace("'", "''"))
						.arg(QString(token2.at(1)).replace("'", "''"))
						.arg(QString(token2.at(2)).replace("'", "''"))
						.arg(QString(token2.at(3)).replace("'", "''"))
						.arg(QString(token2.at(4)).replace("'", "''"))
						.arg(QString(token2.at(5)).replace("'", "''"))
						.arg(QString(token2.at(6)).replace("'", "''"))
						.arg(token2.size() > 7 ? QString(token2.at(7)).replace("'", "''") : "0")
						.arg(token2.size() > 8 ? QString(token2.at(8)).replace("'", "''") : "0")
						.arg(token2.size() > 9 ? QString(token2.at(9)).replace("'", "''") : "")
						.arg(token2.size() > 10 ? QString(token2.at(10)).replace("'", "''") : "")
						.arg(token2.size() > 11 ? QString(token2.at(11)).replace("'", "''") : "")
						.arg(token2.size() > 12 ? QString(token2.at(12)).replace("'", "''") : "0");
			QSqlQuery query(db);
			query.exec(q);
			qDebug()<<query.lastError().text();
		}
	}
	else if(ReplyID == "222"){ // 222 MYLISTSTATS
		// Response format: entries|watched|size|viewed size|viewed%|watched%|episodes watched
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		qDebug()<<__FILE__<<__LINE__<<"MYLISTSTATS:"<<token2.first();
	}
	else if(ReplyID == "223"){ // 223 WISHLIST
		// Parse wishlist response
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		qDebug()<<__FILE__<<__LINE__<<"WISHLIST:"<<token2.first();
	}
	else if(ReplyID == "310"){ // 310 FILE ALREADY IN MYLIST
		// resend with tag and &edit=1
		QString q;
		q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(Tag);
		QSqlQuery query(q);
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
		notifyMylistAdd(Tag, 311);
	}
	else if(ReplyID == "312"){ // 312 NO SUCH MYLIST ENTRY
		qDebug()<<__FILE__<<__LINE__<<"NO SUCH MYLIST ENTRY";
	}
    else if(ReplyID == "320"){ // 320 NO SUCH FILE
        QString q;
        notifyMylistAdd(Tag, 320);
        q = QString("DELETE from `packets` WHERE `tag` = '%1'").arg(Tag);
        qDebug()<<QString(q)<<QString(Tag);
        QSqlQuery query;
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
			QString title = parts[4];
			QString body = parts[5];
			
			qDebug()<<__FILE__<<__LINE__<<"Notification received:"<<nid<<title<<body;
			
			// Emit signal for notification
			emit notifyMessageReceived(nid, body);
			
			// Acknowledge the notification
			PushAck(nid);
		}
	}
	else if(ReplyID == "271"){ // 271 NOTIFYACK - NOTIFICATION ACKNOWLEDGED
		qDebug()<<__FILE__<<__LINE__<<"Notification acknowledged";
	}
	else if(ReplyID == "272"){ // 272 NO SUCH NOTIFICATION
		qDebug()<<__FILE__<<__LINE__<<"No such notification";
	}
	else if(ReplyID == "290"){ // 290 NOTIFYLIST
		// Parse notification list
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		qDebug()<<__FILE__<<__LINE__<<"NOTIFYLIST:"<<token2.first();
	}
	else if(ReplyID == "291"){ // 291 NOTIFYLIST ENTRY
		// Parse notification list entry
		QStringList token2 = Message.split("\n");
		token2.pop_front();
		qDebug()<<__FILE__<<__LINE__<<"NOTIFYLIST ENTRY:"<<token2.first();
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
		qDebug()<<__FILE__<<__LINE__<<"UNKNOWN COMMAND - check request format";
	}
	else if(ReplyID == "601"){ // 601 ANIDB OUT OF SERVICE - TRY AGAIN LATER
	}
    else
    {
        qDebug()<<__FILE__<<__LINE__<<"ParseMessage - not supported ReplyID ("<<ReplyID<<")";
    }
    waitingForReply.isWaiting = false;
	return ReplyID;
}

QString AniDBApi::Auth()
{
	QString msg;
	msg = QString("AUTH user=%1&pass=%2&protover=%3&client=%4&clientver=%5&enc=%6").arg(AniDBApi::username).arg(AniDBApi::password).arg(AniDBApi::protover).arg(AniDBApi::client).arg(AniDBApi::clientver).arg(AniDBApi::enc);
	QString q;
	q = QString("INSERT OR REPLACE INTO `packets` (`tag`, `str`) VALUES ('0', '%1');").arg(msg);
	QSqlQuery query(q);
	qDebug()<<"auth"<<query.lastError().text();

//	Send(msg, "AUTH", "xxx");

	return 0;
}

QString AniDBApi::Logout()
{
	QString msg;
	msg = QString("LOGOUT ");
    qDebug()<<__FILE__<<__LINE__<<msg;
    Send(msg, "LOGOUT", "0");

	return 0;
}

QString AniDBApi::MylistAdd(qint64 size, QString ed2khash, int viewed, int state, QString storage, bool edit)
{
	if(SID.length() == 0 || LoginStatus() == 0)
	{
		Auth();
	}
	QString msg;
	msg = QString("MYLISTADD size=%1&ed2k=%2").arg(size).arg(ed2khash);
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
	QString q;
	q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query;
	query.exec(q);

/*	q = QString("SELECT `tag` FROM `packets` WHERE `str` = '%1' AND `processed` = 0").arg(msg);
	query.exec(q);
	qDebug()<<query.lastError().text();

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
	QString msg;
	msg = QString("FILE size=%1&ed2k=%2&fmask=%3&amask=%4").arg(size).arg(ed2k).arg(fmask, 8, 16, QChar('0')).arg(amask, 8, 16, QChar('0'));
	Debug(msg);
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query(q);
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
		msg = QString("MYLIST lid=%1").arg(lid);
	}
	else
	{
		// Query all mylist entries - we'll need to do this iteratively or use MYLISTSTATS first
		msg = QString("MYLISTSTATS ");
	}
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query;
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
	QString msg = QString("PUSHACK nid=%1").arg(nid);
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query;
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
	QString msg = QString("NOTIFYLIST");
	QString q = QString("INSERT INTO `packets` (`str`) VALUES ('%1');").arg(msg);
	QSqlQuery query;
	query.exec(q);
	return GetTag(msg);
}

QString AniDBApi::GetSID()
{
	return AniDBApi::SID;
}

int AniDBApi::Send(QString str, QString msgtype, QString tag)
{
	if(Socket == nullptr)
	{
		qDebug()<<__FILE__<<__LINE__<<"Socket not initialized, cannot send";
		return 0;
	}
	QString a;
	if(SID.length() > 0)
	{
		a = QString("%1&s=%2").arg(str).arg(SID);
	}
	Debug("AniDBApi: Send: " + (a.length() > 0 ? a : a = str));

	a = QString("%1&tag=%2").arg(a).arg(tag);
    qDebug()<<__FILE__<<__LINE__<<a;
//    QByteArray bytes = a.toUtf8();
//	const char *ptr = bytes.data();
//	Socket->write(ptr);
    Socket->write(a.toUtf8().constData());
    waitingForReply.isWaiting = true;
    waitingForReply.start.start();

	lastSentPacket = a;

	QSqlQuery query;
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
        QSqlQuery query(q);

        if(query.isSelect())
        {
            if(query.next())
            {
                tag = query.value(0).toString();
                str = query.value(1).toString();
                qDebug()<<__FILE__<<__LINE__<<"got query to send:"<<tag<<str;
                if(!LoggedIn() && !str.contains("AUTH"))
                {
                    Auth();
                    return 0;
                }
                Send(str,"", tag);
                qDebug()<<__FILE__<<__LINE__<<str;
            }
        }
    }
	Recv();
    if(waitingForReply.isWaiting == true && waitingForReply.start.elapsed() > 10000)
    {
        qDebug()<<__FILE__<<__LINE__<<"waited for reply for more than 10 seconds="<<waitingForReply.start.elapsed();
    }
	return 0;
}

unsigned long AniDBApi::LocalIdentify(int size, QString ed2khash)
{
	std::bitset<2> ret;
	QString q = QString("SELECT `fid` FROM `file` WHERE `size` = '%1' AND `ed2k` = '%2'").arg(size).arg(ed2khash);
	QSqlQuery query(db);
	query.exec(q);
	query.next();
	if(query.value(0).toInt() > 0)
	{
		ret[0] = 1;
	}
	q = QString("SELECT `lid` FROM `mylist` WHERE `fid` = '%1'").arg(query.value(0).toInt());
	query.exec(q);
	query.next();
	if(query.value(0).toInt() > 0)
	{
		ret[1] = 1;
	}
//	}
	return ret.to_ulong();
}

void AniDBApi::UpdateFile(int size, QString ed2khash, int viewed, int state, QString storage)
{
	QString q = QString("SELECT `fid`,`lid` FROM `file` WHERE `size` = %1 AND `ed2k` = %2").arg(size).arg(ed2khash);
	QSqlQuery query(q);
	if(query.size() > 0)
	{
		if(query.value(0).toInt() > 0)
		{
			q = QString("UPDATE `mylist` SET `viewed` = '%1', `state` = '%2', `storage` = '%3' WHERE `lid` = %4").arg(viewed).arg(state).arg(storage).arg(query.value(0).toString());
			query.exec(q);
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

QString AniDBApi::GetTag(QString str)
{
	QString q = QString("SELECT `tag` FROM `packets` WHERE `str` = '%1' AND `processed` = '0' ORDER BY `tag` ASC LIMIT 1").arg(str);
	QSqlQuery query(db);
	query.exec(q);
	query.next();
	return (query.isValid())?query.value(0).toString():"0";
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
	return secondsSinceLastUpdate > 86400; // 86400 seconds = 24 hours
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
	if(compressedData.size() >= 2 && 
	   (unsigned char)compressedData[0] == 0x1f && 
	   (unsigned char)compressedData[1] == 0x8b)
	{
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
		
		// Clean up
		delete[] out;
		inflateEnd(&stream);
	}
	else
	{
		// Try direct decompression with qUncompress (for zlib format)
		decompressedData = qUncompress(compressedData);
	}
	
	if(decompressedData.isEmpty())
	{
		Debug("Failed to decompress anime titles data. Will retry on next startup.");
		return;
	}
	
	Debug(QString("Decompressed to %1 bytes").arg(decompressedData.size()));
	
	parseAndStoreAnimeTitles(decompressedData);
	
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
	
	QString content = QString::fromUtf8(data);
	QStringList lines = content.split('\n', Qt::SkipEmptyParts);
	
	db.transaction();
	
	// Clear old titles
	QSqlQuery query(db);
	query.exec("DELETE FROM `anime_titles`");
	
	int count = 0;
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
		}
	}
	
	db.commit();
	Debug(QString("Parsed and stored %1 anime titles").arg(count));
}
