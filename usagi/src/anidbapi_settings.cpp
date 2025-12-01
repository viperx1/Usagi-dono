#include "anidbapi.h"
#include "logger.h"

// Static flag to log only once
static bool s_settingsLogged = false;

// Helper method for saving settings to database
void AniDBApi::saveSetting(const QString& name, const QString& value)
{
	QSqlQuery query;
	QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, '%1', '%2');").arg(name).arg(value);
	query.exec(q);
}

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
		saveSetting("username", username);
	}
}

void AniDBApi::setPassword(QString password)
{
	if(password.length() > 0)
	{
		AniDBApi::password = password;
		saveSetting("password", password);
	}
}

void AniDBApi::setLastDirectory(QString directory)
{
	if(directory.length() > 0)
	{
		AniDBApi::lastdirectory = directory;
		saveSetting("lastdirectory", directory);
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
	saveSetting("watcherEnabled", enabled ? "1" : "0");
}

void AniDBApi::setWatcherDirectory(QString directory)
{
	AniDBApi::watcherDirectory = directory;
	saveSetting("watcherDirectory", directory);
}

void AniDBApi::setWatcherAutoStart(bool autoStart)
{
	AniDBApi::watcherAutoStart = autoStart;
	saveSetting("watcherAutoStart", autoStart ? "1" : "0");
}

// Auto-fetch settings
bool AniDBApi::getAutoFetchEnabled()
{
	return AniDBApi::autoFetchEnabled;
}

void AniDBApi::setAutoFetchEnabled(bool enabled)
{
	AniDBApi::autoFetchEnabled = enabled;
	saveSetting("autoFetchEnabled", enabled ? "1" : "0");
}

// Tray settings
bool AniDBApi::getTrayMinimizeToTray()
{
	return AniDBApi::trayMinimizeToTray;
}

void AniDBApi::setTrayMinimizeToTray(bool enabled)
{
	AniDBApi::trayMinimizeToTray = enabled;
	saveSetting("trayMinimizeToTray", enabled ? "1" : "0");
}

bool AniDBApi::getTrayCloseToTray()
{
	return AniDBApi::trayCloseToTray;
}

void AniDBApi::setTrayCloseToTray(bool enabled)
{
	AniDBApi::trayCloseToTray = enabled;
	saveSetting("trayCloseToTray", enabled ? "1" : "0");
}

bool AniDBApi::getTrayStartMinimized()
{
	return AniDBApi::trayStartMinimized;
}

void AniDBApi::setTrayStartMinimized(bool enabled)
{
	AniDBApi::trayStartMinimized = enabled;
	saveSetting("trayStartMinimized", enabled ? "1" : "0");
}

// Auto-start settings
bool AniDBApi::getAutoStartEnabled()
{
	return AniDBApi::autoStartEnabled;
}

void AniDBApi::setAutoStartEnabled(bool enabled)
{
	AniDBApi::autoStartEnabled = enabled;
	saveSetting("autoStartEnabled", enabled ? "1" : "0");
}

// Filter bar visibility settings
bool AniDBApi::getFilterBarVisible()
{
	return AniDBApi::filterBarVisible;
}

void AniDBApi::setFilterBarVisible(bool visible)
{
	AniDBApi::filterBarVisible = visible;
	saveSetting("filterBarVisible", visible ? "1" : "0");
}
