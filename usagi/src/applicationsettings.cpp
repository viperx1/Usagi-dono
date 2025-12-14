#include "applicationsettings.h"
#include "logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

ApplicationSettings::ApplicationSettings(QSqlDatabase database)
    : m_database(database)
    , m_autoFetchEnabled(false)
    , m_autoStartEnabled(false)
{
    // Constructor initializes with default values
    // Call load() to read from database
}

void ApplicationSettings::load()
{
    if (!m_database.isValid() || !m_database.isOpen()) {
        Logger::log("[Settings] Database not available, using defaults", __FILE__, __LINE__);
        return;
    }
    
    QSqlQuery query(m_database);
    query.exec("SELECT `name`, `value` FROM `settings`");
    
    while (query.next()) {
        QString name = query.value(0).toString();
        QString value = query.value(1).toString();
        
        // Authentication
        if (name == "username") {
            m_auth.username = value;
        }
        else if (name == "password") {
            m_auth.password = value;
        }
        // Directory watcher
        else if (name == "watcherEnabled") {
            m_watcher.enabled = (value == "1");
        }
        else if (name == "watcherDirectory") {
            m_watcher.directory = value;
        }
        else if (name == "watcherAutoStart") {
            m_watcher.autoStart = (value == "1");
        }
        // Auto-fetch
        else if (name == "autoFetchEnabled") {
            m_autoFetchEnabled = (value == "1");
        }
        // Tray
        else if (name == "trayMinimizeToTray") {
            m_tray.minimizeToTray = (value == "1");
        }
        else if (name == "trayCloseToTray") {
            m_tray.closeToTray = (value == "1");
        }
        else if (name == "trayStartMinimized") {
            m_tray.startMinimized = (value == "1");
        }
        // Auto-start
        else if (name == "autoStartEnabled") {
            m_autoStartEnabled = (value == "1");
        }
        // UI
        else if (name == "filterBarVisible") {
            m_ui.filterBarVisible = (value == "1");
        }
        else if (name == "lastDirectory") {
            m_ui.lastDirectory = value;
        }
        // File preferences
        else if (name == "preferredAudioLanguages") {
            m_filePrefs.preferredAudioLanguages = value;
        }
        else if (name == "preferredSubtitleLanguages") {
            m_filePrefs.preferredSubtitleLanguages = value;
        }
        else if (name == "preferHighestVersion") {
            m_filePrefs.preferHighestVersion = (value == "1");
        }
        else if (name == "preferHighestQuality") {
            m_filePrefs.preferHighestQuality = (value == "1");
        }
        else if (name == "preferredBitrate") {
            m_filePrefs.preferredBitrate = value.toDouble();
        }
        else if (name == "preferredResolution") {
            m_filePrefs.preferredResolution = value;
        }
        // Hasher
        else if (name == "hasherFilterMasks") {
            m_hasher.filterMasks = value;
        }
    }
}

void ApplicationSettings::save()
{
    if (!m_database.isValid() || !m_database.isOpen()) {
        Logger::log("[Settings] Database not available, cannot save settings", __FILE__, __LINE__);
        return;
    }
    
    Logger::log("[Settings] Saving application settings to database", __FILE__, __LINE__);
    
    // Save all settings using helper method
    // Authentication
    saveSetting("username", m_auth.username);
    saveSetting("password", m_auth.password);
    
    // Directory watcher
    saveSetting("watcherEnabled", m_watcher.enabled ? "1" : "0");
    saveSetting("watcherDirectory", m_watcher.directory);
    saveSetting("watcherAutoStart", m_watcher.autoStart ? "1" : "0");
    
    // Auto-fetch
    saveSetting("autoFetchEnabled", m_autoFetchEnabled ? "1" : "0");
    
    // Tray
    saveSetting("trayMinimizeToTray", m_tray.minimizeToTray ? "1" : "0");
    saveSetting("trayCloseToTray", m_tray.closeToTray ? "1" : "0");
    saveSetting("trayStartMinimized", m_tray.startMinimized ? "1" : "0");
    
    // Auto-start
    saveSetting("autoStartEnabled", m_autoStartEnabled ? "1" : "0");
    
    // UI
    saveSetting("filterBarVisible", m_ui.filterBarVisible ? "1" : "0");
    saveSetting("lastDirectory", m_ui.lastDirectory);
    
    // File preferences
    saveSetting("preferredAudioLanguages", m_filePrefs.preferredAudioLanguages);
    saveSetting("preferredSubtitleLanguages", m_filePrefs.preferredSubtitleLanguages);
    saveSetting("preferHighestVersion", m_filePrefs.preferHighestVersion ? "1" : "0");
    saveSetting("preferHighestQuality", m_filePrefs.preferHighestQuality ? "1" : "0");
    saveSetting("preferredBitrate", QString::number(m_filePrefs.preferredBitrate));
    saveSetting("preferredResolution", m_filePrefs.preferredResolution);
    
    // Hasher
    saveSetting("hasherFilterMasks", m_hasher.filterMasks);
    
    Logger::log("[Settings] Application settings saved successfully", __FILE__, __LINE__);
}

void ApplicationSettings::saveSetting(const QString& name, const QString& value)
{
    if (!m_database.isValid() || !m_database.isOpen()) {
        return;
    }
    
    QSqlQuery query(m_database);
    query.prepare("INSERT OR REPLACE INTO `settings`(`name`, `value`) VALUES (?, ?)");
    query.addBindValue(name);
    query.addBindValue(value);
    
    if (!query.exec()) {
        Logger::log(QString("[Settings] Failed to save setting %1: %2")
                   .arg(name).arg(query.lastError().text()), __FILE__, __LINE__);
    }
}

// === Setters with auto-save ===

void ApplicationSettings::setUsername(const QString& username)
{
    m_auth.username = username;
    saveSetting("username", username);
}

void ApplicationSettings::setPassword(const QString& password)
{
    m_auth.password = password;
    saveSetting("password", password);
}

void ApplicationSettings::setWatcherEnabled(bool enabled)
{
    m_watcher.enabled = enabled;
    saveSetting("watcherEnabled", enabled ? "1" : "0");
}

void ApplicationSettings::setWatcherDirectory(const QString& directory)
{
    m_watcher.directory = directory;
    saveSetting("watcherDirectory", directory);
}

void ApplicationSettings::setWatcherAutoStart(bool autoStart)
{
    m_watcher.autoStart = autoStart;
    saveSetting("watcherAutoStart", autoStart ? "1" : "0");
}

void ApplicationSettings::setAutoFetchEnabled(bool enabled)
{
    m_autoFetchEnabled = enabled;
    saveSetting("autoFetchEnabled", enabled ? "1" : "0");
}

void ApplicationSettings::setTrayMinimizeToTray(bool enabled)
{
    m_tray.minimizeToTray = enabled;
    saveSetting("trayMinimizeToTray", enabled ? "1" : "0");
}

void ApplicationSettings::setTrayCloseToTray(bool enabled)
{
    m_tray.closeToTray = enabled;
    saveSetting("trayCloseToTray", enabled ? "1" : "0");
}

void ApplicationSettings::setTrayStartMinimized(bool enabled)
{
    m_tray.startMinimized = enabled;
    saveSetting("trayStartMinimized", enabled ? "1" : "0");
}

void ApplicationSettings::setAutoStartEnabled(bool enabled)
{
    m_autoStartEnabled = enabled;
    saveSetting("autoStartEnabled", enabled ? "1" : "0");
}

void ApplicationSettings::setFilterBarVisible(bool visible)
{
    m_ui.filterBarVisible = visible;
    saveSetting("filterBarVisible", visible ? "1" : "0");
}

void ApplicationSettings::setLastDirectory(const QString& directory)
{
    m_ui.lastDirectory = directory;
    saveSetting("lastDirectory", directory);
}

void ApplicationSettings::setPreferredAudioLanguages(const QString& languages)
{
    m_filePrefs.preferredAudioLanguages = languages;
    saveSetting("preferredAudioLanguages", languages);
}

void ApplicationSettings::setPreferredSubtitleLanguages(const QString& languages)
{
    m_filePrefs.preferredSubtitleLanguages = languages;
    saveSetting("preferredSubtitleLanguages", languages);
}

void ApplicationSettings::setPreferHighestVersion(bool prefer)
{
    m_filePrefs.preferHighestVersion = prefer;
    saveSetting("preferHighestVersion", prefer ? "1" : "0");
}

void ApplicationSettings::setPreferHighestQuality(bool prefer)
{
    m_filePrefs.preferHighestQuality = prefer;
    saveSetting("preferHighestQuality", prefer ? "1" : "0");
}

void ApplicationSettings::setPreferredBitrate(double bitrate)
{
    m_filePrefs.preferredBitrate = bitrate;
    saveSetting("preferredBitrate", QString::number(bitrate));
}

void ApplicationSettings::setPreferredResolution(const QString& resolution)
{
    m_filePrefs.preferredResolution = resolution;
    saveSetting("preferredResolution", resolution);
}

void ApplicationSettings::setHasherFilterMasks(const QString& masks)
{
    m_hasher.filterMasks = masks;
    saveSetting("hasherFilterMasks", masks);
}
