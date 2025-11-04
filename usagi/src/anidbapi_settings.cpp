#include "anidbapi.h"
#include "logger.h"

// Static flag to log only once
static bool s_settingsLogged = false;

void AniDBApi::setUsername(QString username)
{
	// Log only once when first settings method is called
	if (!s_settingsLogged) {
		LOG("AniDB settings system initialized [anidbapi_settings.cpp]");
		s_settingsLogged = true;
	}
	
	if(username.length() > 0)
	{
		AniDBApi::username = username;
		QSqlQuery query;
		QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'username', '%1');").arg(username);
		query.exec(q);
//		Debug(AniDBApi::username);
	}
}
void AniDBApi::setPassword(QString password)
{
	if(password.length() > 0)
	{
		AniDBApi::password = password;
		QSqlQuery query;
        QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'password', '%1');").arg(password);
		query.exec(q);
//		Debug(AniDBApi::password);
	}
}

void AniDBApi::setLastDirectory(QString directory)
{
	if(directory.length() > 0)
	{
		AniDBApi::lastdirectory = directory;
		QSqlQuery query;
		QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'lastdirectory', '%1');").arg(directory);
		query.exec(q);
//		Debug(AniDBApi::lastDirectory);
	}
}

QString AniDBApi::getUsername()
{
	return AniDBApi::username;
}

QString AniDBApi::getPassword()
{
	return AniDBApi::password;
}

QString AniDBApi::getLastDirectory()
{
	return AniDBApi::lastdirectory;
}

// Directory watcher settings
bool AniDBApi::getWatcherEnabled()
{
	return AniDBApi::watcherEnabled;
}

QString AniDBApi::getWatcherDirectory()
{
	return AniDBApi::watcherDirectory;
}

bool AniDBApi::getWatcherAutoStart()
{
	return AniDBApi::watcherAutoStart;
}

void AniDBApi::setWatcherEnabled(bool enabled)
{
	AniDBApi::watcherEnabled = enabled;
	QSqlQuery query;
	QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'watcherEnabled', '%1');").arg(enabled ? "1" : "0");
	query.exec(q);
}

void AniDBApi::setWatcherDirectory(QString directory)
{
	AniDBApi::watcherDirectory = directory;
	QSqlQuery query;
	QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'watcherDirectory', '%1');").arg(directory);
	query.exec(q);
}

void AniDBApi::setWatcherAutoStart(bool autoStart)
{
	AniDBApi::watcherAutoStart = autoStart;
	QSqlQuery query;
	QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'watcherAutoStart', '%1');").arg(autoStart ? "1" : "0");
	query.exec(q);
}
