#ifndef APPLICATIONSETTINGS_H
#define APPLICATIONSETTINGS_H

#include <QString>
#include <QSqlDatabase>

/**
 * @brief Manages all application settings with proper encapsulation
 * 
 * This class follows the Single Responsibility Principle by focusing solely
 * on settings management. It provides:
 * - Type-safe access to all application settings
 * - Centralized persistence logic
 * - Clear grouping of related settings
 * - Easy testability and mockability
 * 
 * Design improvements over scattered settings in AniDBApi:
 * - All settings in one place (Cohesion)
 * - Clear public interface (Encapsulation)
 * - Database access encapsulated (Internal implementation detail)
 * - Easy to extend with new settings (Open/Closed Principle)
 */
class ApplicationSettings
{
public:
    /**
     * @brief Authentication credentials
     */
    struct AuthSettings {
        QString username;
        QString password;
        
        AuthSettings() = default;
    };
    
    /**
     * @brief Directory watcher configuration
     */
    struct WatcherSettings {
        bool enabled;
        QString directory;
        bool autoStart;
        
        WatcherSettings() : enabled(false), autoStart(false) {}
    };
    
    /**
     * @brief System tray behavior settings
     */
    struct TraySettings {
        bool minimizeToTray;
        bool closeToTray;
        bool startMinimized;
        
        TraySettings() : minimizeToTray(false), closeToTray(false), startMinimized(false) {}
    };
    
    /**
     * @brief File marking preferences for quality selection
     */
    struct FilePreferences {
        QString preferredAudioLanguages;      // Comma-separated (e.g., "japanese,english")
        QString preferredSubtitleLanguages;   // Comma-separated (e.g., "english,none")
        bool preferHighestVersion;
        bool preferHighestQuality;
        double preferredBitrate;              // Baseline bitrate in Mbps for 1080p
        QString preferredResolution;          // e.g., "1080p", "1440p", "4K"
        
        FilePreferences() 
            : preferHighestVersion(false)
            , preferHighestQuality(false)
            , preferredBitrate(3.5)
            , preferredResolution("1080p") {}
    };
    
    /**
     * @brief Hasher filter configuration
     */
    struct HasherSettings {
        QString filterMasks;  // Comma-separated file masks to ignore (e.g., "*.!qB,*.tmp")
        
        HasherSettings() = default;
    };
    
    /**
     * @brief UI preferences
     */
    struct UISettings {
        bool filterBarVisible;
        QString lastDirectory;
        
        UISettings() : filterBarVisible(true) {}
    };
    
    /**
     * @brief Construct settings manager with database connection
     * @param database Database to use for persistence (if invalid, settings won't persist)
     */
    explicit ApplicationSettings(QSqlDatabase database = QSqlDatabase());
    
    // Destructor
    ~ApplicationSettings() = default;
    
    // Prevent copying (settings should be singleton-like)
    ApplicationSettings(const ApplicationSettings&) = delete;
    ApplicationSettings& operator=(const ApplicationSettings&) = delete;
    
    // Allow moving
    ApplicationSettings(ApplicationSettings&&) = default;
    ApplicationSettings& operator=(ApplicationSettings&&) = default;
    
    // === Load/Save ===
    
    /**
     * @brief Load all settings from database
     */
    void load();
    
    /**
     * @brief Save all settings to database
     */
    void save();
    
    // === Authentication ===
    
    const AuthSettings& auth() const { return m_auth; }
    AuthSettings& auth() { return m_auth; }
    
    QString getUsername() const { return m_auth.username; }
    void setUsername(const QString& username);
    
    QString getPassword() const { return m_auth.password; }
    void setPassword(const QString& password);
    
    // === Directory Watcher ===
    
    const WatcherSettings& watcher() const { return m_watcher; }
    WatcherSettings& watcher() { return m_watcher; }
    
    bool getWatcherEnabled() const { return m_watcher.enabled; }
    void setWatcherEnabled(bool enabled);
    
    QString getWatcherDirectory() const { return m_watcher.directory; }
    void setWatcherDirectory(const QString& directory);
    
    bool getWatcherAutoStart() const { return m_watcher.autoStart; }
    void setWatcherAutoStart(bool autoStart);
    
    // === Auto-fetch ===
    
    bool getAutoFetchEnabled() const { return m_autoFetchEnabled; }
    void setAutoFetchEnabled(bool enabled);
    
    // === Tray ===
    
    const TraySettings& tray() const { return m_tray; }
    TraySettings& tray() { return m_tray; }
    
    bool getTrayMinimizeToTray() const { return m_tray.minimizeToTray; }
    void setTrayMinimizeToTray(bool enabled);
    
    bool getTrayCloseToTray() const { return m_tray.closeToTray; }
    void setTrayCloseToTray(bool enabled);
    
    bool getTrayStartMinimized() const { return m_tray.startMinimized; }
    void setTrayStartMinimized(bool enabled);
    
    // === Auto-start ===
    
    bool getAutoStartEnabled() const { return m_autoStartEnabled; }
    void setAutoStartEnabled(bool enabled);
    
    // === UI ===
    
    const UISettings& ui() const { return m_ui; }
    UISettings& ui() { return m_ui; }
    
    bool getFilterBarVisible() const { return m_ui.filterBarVisible; }
    void setFilterBarVisible(bool visible);
    
    QString getLastDirectory() const { return m_ui.lastDirectory; }
    void setLastDirectory(const QString& directory);
    
    // === File Preferences ===
    
    const FilePreferences& filePreferences() const { return m_filePrefs; }
    FilePreferences& filePreferences() { return m_filePrefs; }
    
    QString getPreferredAudioLanguages() const { return m_filePrefs.preferredAudioLanguages; }
    void setPreferredAudioLanguages(const QString& languages);
    
    QString getPreferredSubtitleLanguages() const { return m_filePrefs.preferredSubtitleLanguages; }
    void setPreferredSubtitleLanguages(const QString& languages);
    
    bool getPreferHighestVersion() const { return m_filePrefs.preferHighestVersion; }
    void setPreferHighestVersion(bool prefer);
    
    bool getPreferHighestQuality() const { return m_filePrefs.preferHighestQuality; }
    void setPreferHighestQuality(bool prefer);
    
    double getPreferredBitrate() const { return m_filePrefs.preferredBitrate; }
    void setPreferredBitrate(double bitrate);
    
    QString getPreferredResolution() const { return m_filePrefs.preferredResolution; }
    void setPreferredResolution(const QString& resolution);
    
    // === Hasher ===
    
    const HasherSettings& hasher() const { return m_hasher; }
    HasherSettings& hasher() { return m_hasher; }
    
    QString getHasherFilterMasks() const { return m_hasher.filterMasks; }
    void setHasherFilterMasks(const QString& masks);
    
private:
    // Helper method for saving individual settings to database
    void saveSetting(const QString& name, const QString& value);
    
    // Database connection for persistence
    QSqlDatabase m_database;
    
    // Settings groups
    AuthSettings m_auth;
    WatcherSettings m_watcher;
    TraySettings m_tray;
    UISettings m_ui;
    FilePreferences m_filePrefs;
    HasherSettings m_hasher;
    
    // Individual settings not in groups
    bool m_autoFetchEnabled;
    bool m_autoStartEnabled;
};

#endif // APPLICATIONSETTINGS_H
