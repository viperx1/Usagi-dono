#include "anidbapi.h"
#include "logger.h"

// Static flag to log only once
static bool s_settingsLogged = false;

// Helper method for saving settings to database
void AniDBApi::saveSetting(const QString& name, const QString& value)
{
	QSqlQuery query;
	// Use prepared statement with explicit column names (id is auto-increment PRIMARY KEY)
	query.prepare("INSERT OR REPLACE INTO `settings` (`name`, `value`) VALUES (?, ?)");
	query.addBindValue(name);
	query.addBindValue(value);
	query.exec();
}

void AniDBApi::setUsername(QString username)
{
	// Log only once when first settings method is called
	if (!s_settingsLogged) {
		LOG("AniDB settings system initialized [anidbapi_settings.cpp] - delegating to ApplicationSettings");
		s_settingsLogged = true;
	}
	
	if(username.length() > 0)
	{
		// Delegate to ApplicationSettings (which auto-saves)
		m_settings.setUsername(username);
		// Keep legacy field in sync for backward compatibility
		AniDBApi::username = username;
	}
}

void AniDBApi::setPassword(QString password)
{
	if(password.length() > 0)
	{
		// Delegate to ApplicationSettings (which auto-saves)
		m_settings.setPassword(password);
		// Keep legacy field in sync for backward compatibility
		AniDBApi::password = password;
	}
}

void AniDBApi::setLastDirectory(QString directory)
{
	if(directory.length() > 0)
	{
		// Delegate to ApplicationSettings (which auto-saves)
		m_settings.setLastDirectory(directory);
		// Keep legacy field in sync for backward compatibility
		AniDBApi::lastdirectory = directory;
	}
}

QString AniDBApi::getUsername()
{
	// Delegate to ApplicationSettings
	return m_settings.getUsername();
}

QString AniDBApi::getPassword()
{
	// Delegate to ApplicationSettings
	return m_settings.getPassword();
}

QString AniDBApi::getLastDirectory()
{
	// Delegate to ApplicationSettings
	return m_settings.getLastDirectory();
}

// Directory watcher settings
bool AniDBApi::getWatcherEnabled()
{
	// Delegate to ApplicationSettings
	return m_settings.getWatcherEnabled();
}

QString AniDBApi::getWatcherDirectory()
{
	// Delegate to ApplicationSettings
	return m_settings.getWatcherDirectory();
}

bool AniDBApi::getWatcherAutoStart()
{
	// Delegate to ApplicationSettings
	return m_settings.getWatcherAutoStart();
}

void AniDBApi::setWatcherEnabled(bool enabled)
{
	// Delegate to ApplicationSettings (which auto-saves)
	m_settings.setWatcherEnabled(enabled);
	// Keep legacy field in sync for backward compatibility
	AniDBApi::watcherEnabled = enabled;
}

void AniDBApi::setWatcherDirectory(QString directory)
{
	// Delegate to ApplicationSettings (which auto-saves)
	m_settings.setWatcherDirectory(directory);
	// Keep legacy field in sync for backward compatibility
	AniDBApi::watcherDirectory = directory;
}

void AniDBApi::setWatcherAutoStart(bool autoStart)
{
	// Delegate to ApplicationSettings (which auto-saves)
	m_settings.setWatcherAutoStart(autoStart);
	// Keep legacy field in sync for backward compatibility
	AniDBApi::watcherAutoStart = autoStart;
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

// File marking preferences
QString AniDBApi::getPreferredAudioLanguages()
{
	return AniDBApi::preferredAudioLanguages;
}

void AniDBApi::setPreferredAudioLanguages(const QString& languages)
{
	AniDBApi::preferredAudioLanguages = languages;
	saveSetting("preferredAudioLanguages", languages);
}

QString AniDBApi::getPreferredSubtitleLanguages()
{
	return AniDBApi::preferredSubtitleLanguages;
}

void AniDBApi::setPreferredSubtitleLanguages(const QString& languages)
{
	AniDBApi::preferredSubtitleLanguages = languages;
	saveSetting("preferredSubtitleLanguages", languages);
}

bool AniDBApi::getPreferHighestVersion()
{
	return AniDBApi::preferHighestVersion;
}

void AniDBApi::setPreferHighestVersion(bool prefer)
{
	AniDBApi::preferHighestVersion = prefer;
	saveSetting("preferHighestVersion", prefer ? "1" : "0");
}

bool AniDBApi::getPreferHighestQuality()
{
	return AniDBApi::preferHighestQuality;
}

void AniDBApi::setPreferHighestQuality(bool prefer)
{
	AniDBApi::preferHighestQuality = prefer;
	saveSetting("preferHighestQuality", prefer ? "1" : "0");
}

double AniDBApi::getPreferredBitrate()
{
	return AniDBApi::preferredBitrate;
}

void AniDBApi::setPreferredBitrate(double bitrate)
{
	AniDBApi::preferredBitrate = bitrate;
	saveSetting("preferredBitrate", QString::number(bitrate, 'f', 2));
}

QString AniDBApi::getPreferredResolution()
{
	return AniDBApi::preferredResolution;
}

void AniDBApi::setPreferredResolution(const QString& resolution)
{
	AniDBApi::preferredResolution = resolution;
	saveSetting("preferredResolution", resolution);
}

// Hasher filter settings
QString AniDBApi::getHasherFilterMasks()
{
	// Delegate to ApplicationSettings
	return m_settings.getHasherFilterMasks();
}

void AniDBApi::setHasherFilterMasks(const QString& masks)
{
	// Delegate to ApplicationSettings (which auto-saves)
	m_settings.setHasherFilterMasks(masks);
	// Keep legacy field in sync for backward compatibility
	AniDBApi::hasherFilterMasks = masks;
}
