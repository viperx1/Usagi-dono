# Technical Design Notes: Anime Tracking Implementation

## Quick Reference Guide

This document provides technical implementation details to complement the main feature plan. It includes code patterns, database migrations, API specifications, and implementation considerations.

## 1. Database Migration Strategy

### 1.1 Schema Version Management

Add a schema version table to track migrations:

```sql
CREATE TABLE schema_version (
    version INTEGER PRIMARY KEY,
    applied_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    description TEXT
);
```

### 1.2 Migration Order

**Migration 1: Watch Status Support**
```sql
-- Add watch status tracking
CREATE TABLE anime_status (
    aid INTEGER PRIMARY KEY,
    status TEXT CHECK(status IN ('watching', 'plan_to_watch', 'completed', 'on_hold', 'dropped', 'rewatching')),
    priority INTEGER DEFAULT 0,
    user_rating REAL,
    user_notes TEXT,
    date_started TIMESTAMP,
    date_completed TIMESTAMP,
    FOREIGN KEY (aid) REFERENCES anime(aid) ON DELETE CASCADE
);

-- Add index for common queries
CREATE INDEX idx_anime_status_status ON anime_status(status);
CREATE INDEX idx_anime_status_priority ON anime_status(priority DESC);
```

**Migration 2: Playback Tracking**
```sql
CREATE TABLE playback_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    eid INTEGER NOT NULL,
    fid INTEGER,
    file_path TEXT,  -- Store path in case fid changes
    last_position INTEGER DEFAULT 0,  -- seconds
    duration INTEGER,  -- total seconds
    last_played TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    play_count INTEGER DEFAULT 0,
    completion_percent REAL DEFAULT 0,
    FOREIGN KEY (eid) REFERENCES episode(eid) ON DELETE CASCADE,
    FOREIGN KEY (fid) REFERENCES file(fid) ON DELETE SET NULL
);

-- Ensure only one active playback record per episode
CREATE UNIQUE INDEX idx_playback_eid ON playback_history(eid);
CREATE INDEX idx_playback_last_played ON playback_history(last_played DESC);
```

**Migration 3: Torrent Management**
```sql
CREATE TABLE stored_torrents (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    info_hash TEXT UNIQUE NOT NULL,
    torrent_file BLOB,  -- Optional: store .torrent file
    magnet_link TEXT,
    aid INTEGER,
    eid INTEGER,  -- NULL for batch torrents
    name TEXT NOT NULL,
    size INTEGER,
    file_count INTEGER DEFAULT 1,
    tracker TEXT,
    added_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_used TIMESTAMP,
    use_count INTEGER DEFAULT 0,
    FOREIGN KEY (aid) REFERENCES anime(aid) ON DELETE CASCADE,
    FOREIGN KEY (eid) REFERENCES episode(eid) ON DELETE CASCADE
);

CREATE INDEX idx_torrents_aid ON stored_torrents(aid);
CREATE INDEX idx_torrents_eid ON stored_torrents(eid);
CREATE INDEX idx_torrents_hash ON stored_torrents(info_hash);

CREATE TABLE torrent_downloads (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    torrent_hash TEXT UNIQUE NOT NULL,
    stored_torrent_id INTEGER,
    aid INTEGER,
    eid INTEGER,
    torrent_name TEXT NOT NULL,
    download_path TEXT,
    status TEXT CHECK(status IN ('queued', 'downloading', 'paused', 'completed', 'error', 'stopped')) DEFAULT 'queued',
    progress REAL DEFAULT 0.0,  -- 0.0 to 1.0
    download_speed INTEGER DEFAULT 0,
    upload_speed INTEGER DEFAULT 0,
    eta INTEGER,  -- seconds
    error_message TEXT,
    added_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    completed_date TIMESTAMP,
    FOREIGN KEY (stored_torrent_id) REFERENCES stored_torrents(id) ON DELETE SET NULL,
    FOREIGN KEY (aid) REFERENCES anime(aid) ON DELETE CASCADE,
    FOREIGN KEY (eid) REFERENCES episode(eid) ON DELETE CASCADE
);

CREATE INDEX idx_downloads_status ON torrent_downloads(status);
CREATE INDEX idx_downloads_aid ON torrent_downloads(aid);
```

**Migration 4: Episode Release Tracking**
```sql
CREATE TABLE episode_releases (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    aid INTEGER NOT NULL,
    eid INTEGER NOT NULL,
    episode_number INTEGER NOT NULL,
    air_date TIMESTAMP,
    notified BOOLEAN DEFAULT 0,
    download_requested BOOLEAN DEFAULT 0,
    FOREIGN KEY (aid) REFERENCES anime(aid) ON DELETE CASCADE,
    FOREIGN KEY (eid) REFERENCES episode(eid) ON DELETE CASCADE,
    UNIQUE(aid, eid)
);

CREATE INDEX idx_releases_air_date ON episode_releases(air_date);
CREATE INDEX idx_releases_notified ON episode_releases(notified, air_date);
```

**Migration 5: File Path Tracking**
```sql
-- Add local file paths to mylist/file table
ALTER TABLE file ADD COLUMN local_path TEXT;
ALTER TABLE mylist ADD COLUMN local_path TEXT;

CREATE INDEX idx_file_local_path ON file(local_path);
CREATE INDEX idx_mylist_local_path ON mylist(local_path);
```

### 1.3 Migration Application Code Pattern

```cpp
// In AniDBApi constructor, after opening database
void AniDBApi::applyMigrations() {
    int currentVersion = getSchemaVersion();
    
    if (currentVersion < 1) {
        applyMigration1();
        setSchemaVersion(1, "Add anime_status table");
    }
    if (currentVersion < 2) {
        applyMigration2();
        setSchemaVersion(2, "Add playback_history table");
    }
    // ... etc
}

int AniDBApi::getSchemaVersion() {
    QSqlQuery query(db);
    if (!query.exec("SELECT MAX(version) FROM schema_version")) {
        // Table doesn't exist, this is version 0
        query.exec("CREATE TABLE schema_version (version INTEGER PRIMARY KEY, applied_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP, description TEXT)");
        return 0;
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

void AniDBApi::setSchemaVersion(int version, const QString& description) {
    QSqlQuery query(db);
    query.prepare("INSERT INTO schema_version (version, description) VALUES (?, ?)");
    query.addBindValue(version);
    query.addBindValue(description);
    query.exec();
}
```

## 2. Code Architecture Patterns

### 2.1 Manager Classes

**PlaybackManager.h**
```cpp
#ifndef PLAYBACKMANAGER_H
#define PLAYBACKMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QSqlDatabase>

class PlaybackManager : public QObject {
    Q_OBJECT
    
public:
    explicit PlaybackManager(QSqlDatabase& db, QObject *parent = nullptr);
    ~PlaybackManager();
    
    // Main playback control
    bool playEpisode(int eid, const QString& filePath);
    bool playNextEpisode(int aid);
    void stopPlayback();
    void pausePlayback();
    void resumePlayback();
    
    // Resume functionality
    bool hasResumePoint(int eid) const;
    int getResumePosition(int eid) const;
    
    // Playback state
    bool isPlaying() const;
    int getCurrentEpisode() const;
    QString getCurrentFilePath() const;
    
    // Configuration
    void setPlayerPath(const QString& path);
    void setPlayerArguments(const QStringList& args);
    void setAutoMarkWatchedThreshold(double threshold); // 0.0 to 1.0
    
signals:
    void playbackStarted(int eid, const QString& filePath);
    void playbackEnded(int eid, double completionPercent);
    void playbackError(const QString& error);
    void episodeCompleted(int eid); // Emitted when > threshold
    void readyForNext(int aid, int nextEid); // Suggest next episode
    
private slots:
    void onPlayerFinished(int exitCode, QProcess::ExitStatus status);
    void onPlayerError(QProcess::ProcessError error);
    void updatePlaybackPosition(); // Periodic check if player supports it
    
private:
    QSqlDatabase& m_db;
    QProcess* m_playerProcess;
    QTimer* m_positionTimer;
    
    QString m_playerPath;
    QStringList m_playerArgs;
    double m_autoMarkThreshold;
    
    int m_currentEid;
    QString m_currentFilePath;
    qint64 m_playbackStartTime;
    
    void savePlaybackHistory(int eid, int position, int duration);
    void markAsWatched(int eid);
    int findNextEpisode(int aid, int currentEid);
};

#endif // PLAYBACKMANAGER_H
```

**TorrentManager.h**
```cpp
#ifndef TORRENTMANAGER_H
#define TORRENTMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QSqlDatabase>
#include <QJsonObject>

class TorrentManager : public QObject {
    Q_OBJECT
    
public:
    explicit TorrentManager(QSqlDatabase& db, QObject *parent = nullptr);
    ~TorrentManager();
    
    // Configuration
    void setQBittorrentHost(const QString& host, int port);
    void setCredentials(const QString& username, const QString& password);
    bool testConnection();
    
    // Torrent operations
    QString addTorrent(const QString& magnetLink, int aid = 0, int eid = 0);
    QString addTorrentFile(const QByteArray& torrentData, int aid = 0, int eid = 0);
    bool pauseTorrent(const QString& hash);
    bool resumeTorrent(const QString& hash);
    bool deleteTorrent(const QString& hash, bool deleteFiles = false);
    
    // Status queries
    QJsonObject getTorrentInfo(const QString& hash);
    QList<QJsonObject> getAllTorrents();
    double getProgress(const QString& hash);
    QString getStatus(const QString& hash);
    
    // Storage
    bool storeTorrent(const QString& hash, const QByteArray& torrentFile, int aid, int eid);
    QByteArray getStoredTorrent(const QString& hash);
    
    // Auto-refresh
    void setAutoRefreshInterval(int seconds);
    void startAutoRefresh();
    void stopAutoRefresh();
    
signals:
    void torrentAdded(const QString& hash, const QString& name);
    void downloadProgress(const QString& hash, double progress);
    void downloadCompleted(const QString& hash, const QString& savePath);
    void downloadError(const QString& hash, const QString& error);
    void statusChanged(const QString& hash, const QString& status);
    void connectionError(const QString& error);
    
private slots:
    void onLoginReply();
    void onAddTorrentReply();
    void onTorrentInfoReply();
    void onDeleteTorrentReply();
    void refreshAllTorrents();
    
private:
    QSqlDatabase& m_db;
    QNetworkAccessManager* m_netManager;
    QTimer* m_refreshTimer;
    
    QString m_host;
    int m_port;
    QString m_username;
    QString m_password;
    QString m_sid; // Session ID
    
    QString m_baseUrl;
    
    bool login();
    void makeRequest(const QString& endpoint, 
                     const QMap<QString, QString>& params,
                     const QObject* receiver,
                     const char* slot);
    void updateDatabaseStatus(const QString& hash, const QJsonObject& info);
};

#endif // TORRENTMANAGER_H
```

### 2.2 MyList Enhanced Widget

**MyListWidget.h**
```cpp
#ifndef MYLISTWIDGET_H
#define MYLISTWIDGET_H

#include <QTreeWidget>
#include <QMenu>
#include <QSqlDatabase>

class MyListWidget : public QTreeWidget {
    Q_OBJECT
    
public:
    explicit MyListWidget(QSqlDatabase& db, QWidget *parent = nullptr);
    
    // Data operations
    void loadMyList();
    void filterByStatus(const QString& status);
    void sortBy(const QString& criteria);
    void search(const QString& query);
    
    // Get current selection
    int getSelectedAnimeId() const;
    int getSelectedEpisodeId() const;
    bool isAnimeSelected() const;
    bool isEpisodeSelected() const;
    
public slots:
    void refresh();
    void onAnimeUpdated(int aid);
    void onEpisodeWatched(int eid);
    void onDownloadProgress(const QString& hash, double progress);
    
signals:
    void playEpisodeRequested(int eid, const QString& filePath);
    void playNextEpisodeRequested(int aid);
    void downloadEpisodeRequested(int eid);
    void animeDetailsRequested(int aid);
    void episodeDetailsRequested(int eid);
    void watchStatusChanged(int aid, const QString& status);
    
protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    
private slots:
    void onPlayEpisode();
    void onPlayNextEpisode();
    void onMarkWatched();
    void onMarkUnwatched();
    void onDownloadEpisode();
    void onViewAnimeDetails();
    void onViewEpisodeDetails();
    void onOpenFileLocation();
    void onChangeWatchStatus();
    void onRefreshFromAniDB();
    
private:
    QSqlDatabase& m_db;
    QMenu* m_animeContextMenu;
    QMenu* m_episodeContextMenu;
    QString m_currentFilter;
    QString m_currentSort;
    
    void buildContextMenus();
    void loadAnimeList(const QString& status = QString());
    void loadEpisodesForAnime(QTreeWidgetItem* animeItem, int aid);
    QIcon getStatusIcon(const QString& status);
    QString formatEpisodeTitle(int episodeNum, const QString& title, bool watched);
    bool hasLocalFile(int eid) const;
    QString getLocalFilePath(int eid) const;
    int getNextUnwatchedEpisode(int aid) const;
    void updateAnimeProgress(QTreeWidgetItem* item, int aid);
};

#endif // MYLISTWIDGET_H
```

### 2.3 Settings Management Pattern

Extend existing settings system:

```cpp
// In anidbapi.h, add methods:
class AniDBApi : public ed2k {
    // ... existing code ...
    
public:
    // Playback settings
    QString getPlayerPath() const;
    void setPlayerPath(const QString& path);
    QStringList getPlayerArgs() const;
    void setPlayerArgs(const QStringList& args);
    double getAutoMarkWatchedThreshold() const;
    void setAutoMarkWatchedThreshold(double threshold);
    bool getPromptNextEpisode() const;
    void setPromptNextEpisode(bool enabled);
    
    // qBittorrent settings
    bool getQBittorrentEnabled() const;
    void setQBittorrentEnabled(bool enabled);
    QString getQBittorrentHost() const;
    void setQBittorrentHost(const QString& host);
    int getQBittorrentPort() const;
    void setQBittorrentPort(int port);
    QString getQBittorrentUsername() const;
    void setQBittorrentUsername(const QString& username);
    QString getQBittorrentPassword() const;
    void setQBittorrentPassword(const QString& password);
    
    // File organization settings
    bool getAutoOrganize() const;
    void setAutoOrganize(bool enabled);
    QString getFolderTemplate() const;
    void setFolderTemplate(const QString& templ);
    QString getDefaultAnimeFolder() const;
    void setDefaultAnimeFolder(const QString& path);
    
private:
    QString getSetting(const QString& name, const QString& defaultValue = QString()) const;
    void setSetting(const QString& name, const QString& value);
};
```

## 3. qBittorrent Web API Integration

### 3.1 API Endpoints

**Authentication:**
```
POST /api/v2/auth/login
  username=<username>&password=<password>
→ Returns: Set-Cookie: SID=<session_id>
```

**Add Torrent:**
```
POST /api/v2/torrents/add
  Cookie: SID=<session_id>
  Body (multipart/form-data):
    torrents: <torrent_file> OR urls: <magnet_link>
    savepath: <download_path>
    category: <category>
→ Returns: "Ok." or error message
```

**Get Torrent List:**
```
GET /api/v2/torrents/info
  Cookie: SID=<session_id>
  ?filter=all&category=anime
→ Returns: JSON array of torrent objects
```

**Get Torrent Properties:**
```
GET /api/v2/torrents/properties
  Cookie: SID=<session_id>
  ?hash=<torrent_hash>
→ Returns: JSON object with detailed properties
```

**Control Operations:**
```
POST /api/v2/torrents/pause
  Cookie: SID=<session_id>
  hashes=<hash1>|<hash2>|...

POST /api/v2/torrents/resume
POST /api/v2/torrents/delete
  Cookie: SID=<session_id>
  hashes=<hash>
  deleteFiles=true|false
```

### 3.2 Implementation Example

```cpp
bool TorrentManager::login() {
    QNetworkRequest request(QUrl(m_baseUrl + "/api/v2/auth/login"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, 
                      "application/x-www-form-urlencoded");
    
    QByteArray postData;
    postData.append("username=" + QUrl::toPercentEncoding(m_username));
    postData.append("&password=" + QUrl::toPercentEncoding(m_password));
    
    QNetworkReply* reply = m_netManager->post(request, postData);
    
    // Wait for response (use signal/slot in real implementation)
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (reply->error() == QNetworkReply::NoError) {
        QString response = reply->readAll();
        if (response == "Ok.") {
            // Extract SID from cookies
            QList<QNetworkCookie> cookies = 
                reply->header(QNetworkRequest::SetCookieHeader)
                     .value<QList<QNetworkCookie>>();
            for (const QNetworkCookie& cookie : cookies) {
                if (cookie.name() == "SID") {
                    m_sid = cookie.value();
                    return true;
                }
            }
        }
    }
    
    emit connectionError("Login failed: " + reply->errorString());
    reply->deleteLater();
    return false;
}

QString TorrentManager::addTorrent(const QString& magnetLink, int aid, int eid) {
    if (m_sid.isEmpty() && !login()) {
        return QString();
    }
    
    QNetworkRequest request(QUrl(m_baseUrl + "/api/v2/torrents/add"));
    
    // Add session cookie
    QString cookieHeader = "SID=" + m_sid;
    request.setRawHeader("Cookie", cookieHeader.toUtf8());
    
    // Build multipart form data
    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    
    QHttpPart urlsPart;
    urlsPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"urls\""));
    urlsPart.setBody(magnetLink.toUtf8());
    multiPart->append(urlsPart);
    
    if (!m_defaultDownloadPath.isEmpty()) {
        QHttpPart pathPart;
        pathPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"savepath\""));
        pathPart.setBody(m_defaultDownloadPath.toUtf8());
        multiPart->append(pathPart);
    }
    
    QHttpPart categoryPart;
    categoryPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"category\""));
    categoryPart.setBody("anime");
    multiPart->append(categoryPart);
    
    QNetworkReply* reply = m_netManager->post(request, multiPart);
    multiPart->setParent(reply); // Delete multiPart with reply
    
    // Connect to slot for async handling
    connect(reply, &QNetworkReply::finished, this, &TorrentManager::onAddTorrentReply);
    
    // Extract hash from magnet link
    QString hash = extractHashFromMagnet(magnetLink);
    
    // Store in database
    if (!hash.isEmpty()) {
        QSqlQuery query(m_db);
        query.prepare("INSERT INTO torrent_downloads (torrent_hash, aid, eid, torrent_name, status) "
                     "VALUES (?, ?, ?, ?, 'queued')");
        query.addBindValue(hash);
        query.addBindValue(aid > 0 ? aid : QVariant());
        query.addBindValue(eid > 0 ? eid : QVariant());
        query.addBindValue(magnetLink);
        query.exec();
    }
    
    return hash;
}

void TorrentManager::refreshAllTorrents() {
    if (m_sid.isEmpty() && !login()) {
        return;
    }
    
    QNetworkRequest request(QUrl(m_baseUrl + "/api/v2/torrents/info?filter=all"));
    request.setRawHeader("Cookie", ("SID=" + m_sid).toUtf8());
    
    QNetworkReply* reply = m_netManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray torrents = doc.array();
            
            for (const QJsonValue& value : torrents) {
                QJsonObject torrent = value.toObject();
                QString hash = torrent["hash"].toString();
                double progress = torrent["progress"].toDouble();
                QString state = torrent["state"].toString();
                
                // Update database
                updateDatabaseStatus(hash, torrent);
                
                // Emit signals
                emit downloadProgress(hash, progress);
                
                if (state == "uploading" || state == "pausedUP") {
                    // Download completed
                    QString savePath = torrent["save_path"].toString();
                    emit downloadCompleted(hash, savePath);
                }
            }
        }
        reply->deleteLater();
    });
}
```

## 4. File Organization Patterns

### 4.1 Template System

**Template Variables:**
- `{anime_name}` - Romaji name
- `{anime_name_english}` - English name
- `{anime_name_kanji}` - Kanji name
- `{year}` - Year
- `{season}` - Season number (if applicable)
- `{episode}` - Episode number
- `{episode:02d}` - Zero-padded episode (e.g., 03)
- `{episode_title}` - Episode title
- `{group}` - Fansub group name
- `{quality}` - Quality (1080p, 720p, etc.)
- `{ext}` - File extension

**Example Templates:**
```
{anime_name}/{anime_name} - {episode:02d} - {episode_title}.{ext}
→ Cowboy Bebop/Cowboy Bebop - 01 - Asteroid Blues.mkv

{anime_name}/Season {season}/{anime_name} - S{season:02d}E{episode:02d}.{ext}
→ Attack on Titan/Season 1/Attack on Titan - S01E01.mkv

Anime/{year}/{anime_name}/[{group}] {anime_name} - {episode:02d} [{quality}].{ext}
→ Anime/2024/Frieren/[SubsPlease] Frieren - 01 [1080p].mkv
```

### 4.2 Implementation

```cpp
class FileOrganizer : public QObject {
    Q_OBJECT
    
public:
    QString applyTemplate(const QString& templateStr, 
                          int aid, int eid, 
                          const QString& originalPath);
    
    bool organizeFile(const QString& sourcePath, 
                      int aid, int eid);
    
private:
    QString getAnimeInfo(int aid, const QString& field);
    QString getEpisodeInfo(int eid, const QString& field);
    QString sanitizeFilename(const QString& name);
    QString extractGroup(const QString& originalFilename);
    QString extractQuality(const QString& originalFilename);
};

QString FileOrganizer::applyTemplate(const QString& templateStr, 
                                     int aid, int eid, 
                                     const QString& originalPath) {
    QString result = templateStr;
    QFileInfo fileInfo(originalPath);
    
    // Replace all template variables
    result.replace("{anime_name}", getAnimeInfo(aid, "nameromaji"));
    result.replace("{anime_name_english}", getAnimeInfo(aid, "nameenglish"));
    result.replace("{anime_name_kanji}", getAnimeInfo(aid, "namekanji"));
    result.replace("{year}", getAnimeInfo(aid, "year"));
    result.replace("{episode_title}", getEpisodeInfo(eid, "name"));
    result.replace("{ext}", fileInfo.suffix());
    
    // Episode number with zero padding
    QString episodeNum = getEpisodeInfo(eid, "episode_number");
    result.replace(QRegularExpression("\\{episode:(\\d+)d\\}"), 
                   episodeNum.rightJustified(/* extracted width */, '0'));
    result.replace("{episode}", episodeNum);
    
    // Extract from original filename
    result.replace("{group}", extractGroup(fileInfo.fileName()));
    result.replace("{quality}", extractQuality(fileInfo.fileName()));
    
    // Sanitize
    return sanitizeFilename(result);
}

QString FileOrganizer::sanitizeFilename(const QString& name) {
    QString sanitized = name;
    
    // Remove/replace invalid characters
    QRegularExpression invalidChars("[<>:\"|?*]");
    sanitized.replace(invalidChars, "_");
    
    // Handle Windows reserved names
    QStringList reserved = {"CON", "PRN", "AUX", "NUL", 
                           "COM1", "COM2", "LPT1", "LPT2"};
    for (const QString& res : reserved) {
        if (sanitized.startsWith(res + ".", Qt::CaseInsensitive)) {
            sanitized.prepend("_");
        }
    }
    
    // Trim whitespace
    sanitized = sanitized.trimmed();
    
    return sanitized;
}
```

## 5. Episode/Anime Matching Algorithm

### 5.1 Filename Parsing

```cpp
struct ParsedFilename {
    QString animeName;
    int episodeNumber;
    int seasonNumber;
    QString quality;
    QString group;
    QString extension;
    double confidence; // 0.0 to 1.0
};

class FilenameParser {
public:
    ParsedFilename parse(const QString& filename);
    
private:
    // Common patterns
    // [Group] Anime Name - 01 [1080p].mkv
    // Anime.Name.S01E01.1080p.WEB.x264-GROUP.mkv
    // Anime Name - 01 - Episode Title [1080p][Group].mkv
    
    QList<QRegularExpression> m_patterns;
    QMap<QString, QString> m_qualityAliases; // BD → Blu-ray, etc.
};

ParsedFilename FilenameParser::parse(const QString& filename) {
    ParsedFilename result;
    result.confidence = 0.0;
    
    // Try multiple patterns
    QList<QRegularExpression> patterns = {
        // [Group] Name - Episode [Quality]
        QRegularExpression("\\[([^\\]]+)\\]\\s*(.+?)\\s*-\\s*(\\d+)\\s*\\[([^\\]]+)\\]"),
        
        // Name.S01E01.Quality
        QRegularExpression("(.+?)[.\\s]+S(\\d+)E(\\d+)[.\\s]+([^.]+)"),
        
        // Name - Episode - Title
        QRegularExpression("(.+?)\\s*-\\s*(\\d+)\\s*-\\s*(.+?)\\.[^.]+$"),
        
        // Name Episode
        QRegularExpression("(.+?)\\s+(\\d{1,3})\\s*\\.[^.]+$")
    };
    
    for (const QRegularExpression& pattern : patterns) {
        QRegularExpressionMatch match = pattern.match(filename);
        if (match.hasMatch()) {
            // Extract matched groups based on pattern
            // ... populate result struct
            result.confidence = 0.8; // Adjust based on pattern reliability
            break;
        }
    }
    
    return result;
}
```

### 5.2 Fuzzy Matching

```cpp
class AnimeMatcher {
public:
    QList<int> findMatchingAnime(const ParsedFilename& parsed, 
                                 double minConfidence = 0.7);
    
private:
    double calculateSimilarity(const QString& str1, const QString& str2);
    double levenshteinDistance(const QString& s1, const QString& s2);
};

QList<int> AnimeMatcher::findMatchingAnime(const ParsedFilename& parsed, 
                                           double minConfidence) {
    QList<int> matches;
    
    // Query database for potential matches
    QSqlQuery query;
    query.prepare("SELECT aid, nameromaji, nameenglish, nameshort, synonyms "
                 "FROM anime");
    query.exec();
    
    while (query.next()) {
        int aid = query.value(0).toInt();
        QStringList names = {
            query.value(1).toString(), // romaji
            query.value(2).toString(), // english
            query.value(3).toString()  // short
        };
        
        // Add synonyms
        QString synonyms = query.value(4).toString();
        names.append(synonyms.split("|", Qt::SkipEmptyParts));
        
        // Calculate best match
        double bestSimilarity = 0.0;
        for (const QString& name : names) {
            double similarity = calculateSimilarity(parsed.animeName, name);
            bestSimilarity = qMax(bestSimilarity, similarity);
        }
        
        if (bestSimilarity >= minConfidence) {
            matches.append(aid);
        }
    }
    
    return matches;
}

double AnimeMatcher::calculateSimilarity(const QString& str1, const QString& str2) {
    // Normalize strings
    QString s1 = str1.toLower().simplified();
    QString s2 = str2.toLower().simplified();
    
    // Exact match
    if (s1 == s2) return 1.0;
    
    // Levenshtein distance
    double distance = levenshteinDistance(s1, s2);
    double maxLen = qMax(s1.length(), s2.length());
    double similarity = 1.0 - (distance / maxLen);
    
    return similarity;
}
```

## 6. UI Implementation Details

### 6.1 Custom Tree Item with Progress

```cpp
class AnimeTreeItem : public QTreeWidgetItem {
public:
    AnimeTreeItem(QTreeWidget* parent, int aid)
        : QTreeWidgetItem(parent), m_aid(aid) {}
    
    void updateProgress(int watched, int total) {
        m_watched = watched;
        m_total = total;
        
        // Update display
        QString progressText = QString("EP %1/%2").arg(watched).arg(total);
        setText(1, progressText);
        
        // Color based on completion
        if (watched == total) {
            setForeground(0, QBrush(Qt::darkGreen));
        } else if (watched > 0) {
            setForeground(0, QBrush(Qt::darkBlue));
        }
    }
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option, 
               const QModelIndex& index) override {
        // Custom painting for progress bar
        if (index.column() == 1 && m_total > 0) {
            double progress = static_cast<double>(m_watched) / m_total;
            // Draw progress bar
            // ...
        }
        QTreeWidgetItem::paint(painter, option, index);
    }
    
private:
    int m_aid;
    int m_watched = 0;
    int m_total = 0;
};
```

### 6.2 Episode Status Icons

```cpp
QIcon MyListWidget::getEpisodeStatusIcon(int eid) {
    // Check various states
    bool watched = isEpisodeWatched(eid);
    bool hasFile = hasLocalFile(eid);
    bool downloading = isDownloading(eid);
    bool nextEpisode = isNextEpisode(eid);
    
    if (nextEpisode && hasFile) {
        return QIcon(":/icons/play_next.png");
    } else if (watched) {
        return QIcon(":/icons/check.png");
    } else if (downloading) {
        return QIcon(":/icons/download.png");
    } else if (hasFile) {
        return QIcon(":/icons/file.png");
    } else {
        return QIcon(":/icons/missing.png");
    }
}
```

## 7. Performance Considerations

### 7.1 Database Indexing

Ensure indexes on frequently queried columns:
```sql
-- Episode queries by anime
CREATE INDEX idx_file_aid_eid ON file(aid, eid);
CREATE INDEX idx_mylist_aid_eid ON mylist(aid, eid);

-- Watch status queries
CREATE INDEX idx_mylist_viewed ON mylist(viewed);

-- Date-based queries
CREATE INDEX idx_mylist_viewdate ON mylist(viewdate DESC);
CREATE INDEX idx_episode_releases_airdate ON episode_releases(air_date);
```

### 7.2 Lazy Loading

Load episodes only when anime node is expanded:
```cpp
void MyListWidget::onItemExpanded(QTreeWidgetItem* item) {
    if (isAnimeItem(item) && item->childCount() == 0) {
        int aid = getAnimeId(item);
        loadEpisodesForAnime(item, aid);
    }
}
```

### 7.3 Caching

Cache frequently accessed data:
```cpp
class DataCache {
public:
    struct AnimeInfo {
        int aid;
        QString name;
        int eptotal;
        QDateTime cached;
    };
    
    QMap<int, AnimeInfo> m_animeCache;
    QMap<int, QString> m_episodeCache;
    
    AnimeInfo getAnime(int aid) {
        if (m_animeCache.contains(aid)) {
            AnimeInfo& info = m_animeCache[aid];
            if (info.cached.secsTo(QDateTime::currentDateTime()) < 300) {
                return info; // Use cached if < 5 minutes old
            }
        }
        
        // Query database
        // ...
    }
};
```

## 8. Error Handling Patterns

### 8.1 Graceful Degradation

```cpp
bool PlaybackManager::playEpisode(int eid, const QString& filePath) {
    // Verify file exists
    if (!QFile::exists(filePath)) {
        emit playbackError("File not found: " + filePath);
        
        // Offer alternatives
        QString message = "File not found. Would you like to:\n"
                         "1. Download this episode\n"
                         "2. Choose another file\n"
                         "3. Remove from mylist";
        // Show dialog
        return false;
    }
    
    // Verify player exists
    if (!QFile::exists(m_playerPath)) {
        emit playbackError("Video player not found: " + m_playerPath);
        // Prompt to configure player
        return false;
    }
    
    // Try to play
    try {
        m_playerProcess->start(m_playerPath, QStringList() << filePath);
        if (!m_playerProcess->waitForStarted(3000)) {
            emit playbackError("Failed to start player");
            return false;
        }
    } catch (...) {
        emit playbackError("Unexpected error launching player");
        return false;
    }
    
    return true;
}
```

### 8.2 Connection Recovery

```cpp
void TorrentManager::makeRequest(const QString& endpoint, ...) {
    // Retry logic
    int retries = 3;
    while (retries > 0) {
        QNetworkReply* reply = m_netManager->get(request);
        
        // Wait for reply
        // ...
        
        if (reply->error() == QNetworkReply::NoError) {
            return; // Success
        } else if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
            // Re-login and retry
            if (login()) {
                retries--;
                continue;
            }
        }
        
        retries--;
    }
    
    emit connectionError("Failed after 3 retries");
}
```

## 9. Testing Considerations

### 9.1 Unit Tests

```cpp
// Test filename parsing
void TestFilenameParser::testBasicParsing() {
    FilenameParser parser;
    
    QString filename = "[SubsPlease] Anime Name - 05 [1080p].mkv";
    ParsedFilename result = parser.parse(filename);
    
    QCOMPARE(result.group, "SubsPlease");
    QCOMPARE(result.animeName, "Anime Name");
    QCOMPARE(result.episodeNumber, 5);
    QCOMPARE(result.quality, "1080p");
    QVERIFY(result.confidence > 0.7);
}

// Test template application
void TestFileOrganizer::testTemplateApply() {
    FileOrganizer organizer;
    
    QString template = "{anime_name}/Episode {episode:02d}.{ext}";
    QString result = organizer.applyTemplate(template, 123, 456, "test.mkv");
    
    // Verify expected output based on test database
    QCOMPARE(result, "Test Anime/Episode 05.mkv");
}
```

### 9.2 Integration Tests

```cpp
// Test qBittorrent connection
void TestTorrentManager::testConnection() {
    TorrentManager manager(db);
    manager.setQBittorrentHost("localhost", 8080);
    manager.setCredentials("admin", "adminpass");
    
    bool connected = manager.testConnection();
    QVERIFY(connected);
}

// Test playback workflow
void TestPlaybackManager::testPlayEpisode() {
    PlaybackManager manager(db);
    manager.setPlayerPath("/usr/bin/vlc");
    
    QSignalSpy spy(&manager, &PlaybackManager::playbackStarted);
    bool started = manager.playEpisode(123, "/path/to/test.mkv");
    
    QVERIFY(started);
    QCOMPARE(spy.count(), 1);
}
```

## 10. Migration Path for Existing Users

### 10.1 Data Migration

```cpp
void migrateExistingData() {
    // Migrate existing mylist entries to new schema
    QSqlQuery query(db);
    
    // Set default watch status for existing anime
    query.exec("INSERT INTO anime_status (aid, status) "
              "SELECT DISTINCT aid, 'watching' FROM mylist "
              "WHERE aid NOT IN (SELECT aid FROM anime_status)");
    
    // Initialize playback history for watched episodes
    query.exec("INSERT INTO playback_history (eid, completion_percent, play_count) "
              "SELECT eid, 1.0, 1 FROM mylist "
              "WHERE viewed = 1");
    
    // Migrate file paths if storage field contains paths
    query.exec("UPDATE mylist SET local_path = storage "
              "WHERE storage LIKE '/%' OR storage LIKE '_:%'");
}
```

### 10.2 Backward Compatibility

Maintain compatibility with existing functionality while adding new features:
```cpp
// Keep existing hasher functionality
// Add new features without breaking old workflows
// Provide migration wizard on first launch of new version
```

---

**Document Version:** 1.0
**Companion to:** FEATURE_PLAN_ANIME_TRACKING.md
**Date:** 2024
**Status:** Technical Reference
