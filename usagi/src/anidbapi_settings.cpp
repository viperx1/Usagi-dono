#include "anidbapi.h"
#include "logger.h"
#include <QRegularExpression>
#include <cmath>

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

/**
 * Calculate expected bitrate for a given resolution based on baseline 1080p bitrate.
 * Uses the formula: bitrate = base_bitrate × (resolution_megapixels / 2.07)
 * where 2.07 is the megapixel count of 1080p (1920×1080).
 * 
 * @param resolution Resolution string (e.g., "1080p", "1920x1080", "4K")
 * @return Expected bitrate in Mbps
 */
double AniDBApi::calculateExpectedBitrate(const QString& resolution) const
{
	// Parse resolution to get megapixels
	double megapixels = 0.0;
	QString resLower = resolution.toLower();
	
	// Common named resolutions
	if (resLower.contains("480p") || resLower.contains("480")) {
		megapixels = 0.92;  // 720×480 or 854×480
	} else if (resLower.contains("720p") || resLower.contains("720")) {
		megapixels = 0.92;  // 1280×720
	} else if (resLower.contains("1080p") || resLower.contains("1080")) {
		megapixels = 2.07;  // 1920×1080
	} else if (resLower.contains("1440p") || resLower.contains("1440") || resLower.contains("2k")) {
		megapixels = 3.69;  // 2560×1440
	} else if (resLower.contains("2160p") || resLower.contains("2160") || resLower.contains("4k")) {
		megapixels = 8.29;  // 3840×2160
	} else if (resLower.contains("4320p") || resLower.contains("4320") || resLower.contains("8k")) {
		megapixels = 33.18;  // 7680×4320
	} else {
		// Try to parse as WxH format
		QRegularExpression widthHeightRegex(R"((\d+)\s*[x×]\s*(\d+))");
		QRegularExpressionMatch match = widthHeightRegex.match(resolution);
		if (match.hasMatch()) {
			int width = match.captured(1).toInt();
			int height = match.captured(2).toInt();
			megapixels = (width * height) / 1000000.0;
		} else {
			// Default to 1080p if unable to parse
			megapixels = 2.07;
		}
	}
	
	// Calculate expected bitrate using the formula
	// bitrate = base_bitrate × (resolution_megapixels / 2.07)
	double expectedBitrate = preferredBitrate * (megapixels / 2.07);
	
	return expectedBitrate;
}

/**
 * Calculate bitrate score for a file based on how close it is to the expected bitrate.
 * Only applies penalty when there are multiple files (fileCount > 1).
 * 
 * @param actualBitrate Actual bitrate of the file in Kbps
 * @param resolution Resolution of the file
 * @param fileCount Number of files for this episode (penalty only applies if > 1)
 * @return Score adjustment (negative for penalty, 0 for no adjustment)
 */
double AniDBApi::calculateBitrateScore(double actualBitrate, const QString& resolution, int fileCount) const
{
	// Only apply bitrate penalty when there are multiple files
	if (fileCount <= 1) {
		return 0.0;
	}
	
	// Convert actualBitrate from Kbps to Mbps
	double actualBitrateMbps = actualBitrate / 1000.0;
	
	// Calculate expected bitrate based on resolution
	double expectedBitrate = calculateExpectedBitrate(resolution);
	
	// Calculate percentage difference from expected
	double percentDiff = 0.0;
	if (expectedBitrate > 0) {
		percentDiff = std::abs((actualBitrateMbps - expectedBitrate) / expectedBitrate) * 100.0;
	}
	
	// Apply penalty based on percentage difference
	// 0-10% difference: no penalty
	// 10-30% difference: -10 penalty
	// 30-50% difference: -25 penalty
	// 50%+ difference: -40 penalty
	double penalty = 0.0;
	if (percentDiff > 50.0) {
		penalty = -40.0;
	} else if (percentDiff > 30.0) {
		penalty = -25.0;
	} else if (percentDiff > 10.0) {
		penalty = -10.0;
	}
	
	return penalty;
}
