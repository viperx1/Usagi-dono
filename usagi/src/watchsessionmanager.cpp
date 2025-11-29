#include "watchsessionmanager.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStorageInfo>
#include <QCoreApplication>
#include <QRegularExpression>
#include <algorithm>

WatchSessionManager::WatchSessionManager(QObject *parent)
    : QObject(parent)
    , m_aheadBuffer(DEFAULT_AHEAD_BUFFER)
    , m_thresholdType(DeletionThresholdType::FixedGB)
    , m_thresholdValue(DEFAULT_THRESHOLD_VALUE)
    , m_autoMarkDeletionEnabled(false)
    , m_initialScanComplete(false)
{
    LOG("[WatchSessionManager] Initializing...");
    ensureTablesExist();
    loadSettings();
    loadFromDatabase();
    LOG("[WatchSessionManager] Initialization complete");
}

WatchSessionManager::~WatchSessionManager()
{
    LOG("[WatchSessionManager] Shutting down, saving session data...");
    saveToDatabase();
}

void WatchSessionManager::ensureTablesExist()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[WatchSessionManager] Database not open, cannot create tables");
        return;
    }
    
    QSqlQuery q(db);
    
    // Create watch_sessions table
    q.exec("CREATE TABLE IF NOT EXISTS watch_sessions ("
           "aid INTEGER PRIMARY KEY, "
           "start_aid INTEGER, "
           "current_episode INTEGER, "
           "is_active INTEGER DEFAULT 0"
           ")");
    
    // Create session_watched_episodes table for tracking watched episodes per session
    q.exec("CREATE TABLE IF NOT EXISTS session_watched_episodes ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "aid INTEGER NOT NULL, "
           "episode_number INTEGER NOT NULL, "
           "UNIQUE(aid, episode_number)"
           ")");
    q.exec("CREATE INDEX IF NOT EXISTS idx_session_watched_aid ON session_watched_episodes(aid)");
    
    // Create file_marks table for manual/auto file markings
    q.exec("CREATE TABLE IF NOT EXISTS file_marks ("
           "lid INTEGER PRIMARY KEY, "
           "mark_type INTEGER DEFAULT 0, "
           "mark_score INTEGER DEFAULT 0"
           ")");
}

void WatchSessionManager::loadSettings()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    QSqlQuery q(db);
    
    // Load ahead buffer
    q.prepare("SELECT value FROM settings WHERE name = 'session_ahead_buffer'");
    if (q.exec() && q.next()) {
        m_aheadBuffer = q.value(0).toInt();
    }
    
    // Load threshold type
    q.prepare("SELECT value FROM settings WHERE name = 'deletion_threshold_type'");
    if (q.exec() && q.next()) {
        m_thresholdType = static_cast<DeletionThresholdType>(q.value(0).toInt());
    }
    
    // Load threshold value
    q.prepare("SELECT value FROM settings WHERE name = 'deletion_threshold_value'");
    if (q.exec() && q.next()) {
        m_thresholdValue = q.value(0).toDouble();
    }
    
    // Load auto-mark enabled
    q.prepare("SELECT value FROM settings WHERE name = 'auto_mark_deletion_enabled'");
    if (q.exec() && q.next()) {
        m_autoMarkDeletionEnabled = q.value(0).toInt() != 0;
    }
    
    // Load watched path (defaults to empty, which means use directory watcher path)
    q.prepare("SELECT value FROM settings WHERE name = 'session_watched_path'");
    if (q.exec() && q.next()) {
        m_watchedPath = q.value(0).toString();
    }
}

void WatchSessionManager::saveSettings()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    QSqlQuery q(db);
    
    // Save ahead buffer
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('session_ahead_buffer', ?)");
    q.addBindValue(m_aheadBuffer);
    q.exec();
    
    // Save threshold type
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('deletion_threshold_type', ?)");
    q.addBindValue(static_cast<int>(m_thresholdType));
    q.exec();
    
    // Save threshold value
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('deletion_threshold_value', ?)");
    q.addBindValue(m_thresholdValue);
    q.exec();
    
    // Save auto-mark enabled
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('auto_mark_deletion_enabled', ?)");
    q.addBindValue(m_autoMarkDeletionEnabled ? 1 : 0);
    q.exec();
    
    // Save watched path
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('session_watched_path', ?)");
    q.addBindValue(m_watchedPath);
    q.exec();
}

void WatchSessionManager::loadFromDatabase()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[WatchSessionManager] Database not open, cannot load sessions");
        return;
    }
    
    LOG("[WatchSessionManager] Loading session data from database...");
    
    QSqlQuery q(db);
    
    // Load active sessions
    q.exec("SELECT aid, start_aid, current_episode, is_active FROM watch_sessions");
    while (q.next()) {
        SessionInfo session;
        session.aid = q.value(0).toInt();
        session.startAid = q.value(1).toInt();
        session.currentEpisode = q.value(2).toInt();
        session.isActive = q.value(3).toInt() != 0;
        
        m_sessions[session.aid] = session;
    }
    
    // Load watched episodes for each session
    int activeCount = 0;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        QSqlQuery epQuery(db);
        epQuery.prepare("SELECT episode_number FROM session_watched_episodes WHERE aid = ?");
        epQuery.addBindValue(it.key());
        if (epQuery.exec()) {
            while (epQuery.next()) {
                it.value().watchedEpisodes.insert(epQuery.value(0).toInt());
            }
        }
        if (it.value().isActive) {
            activeCount++;
            LOG(QString("[WatchSessionManager] Loaded active session: aid=%1, startAid=%2, currentEpisode=%3, watchedCount=%4")
                .arg(it.value().aid).arg(it.value().startAid)
                .arg(it.value().currentEpisode).arg(it.value().watchedEpisodes.size()));
        }
    }
    
    // Load file marks
    int downloadCount = 0;
    int deletionCount = 0;
    q.exec("SELECT lid, mark_type, mark_score FROM file_marks");
    while (q.next()) {
        FileMarkInfo info;
        info.lid = q.value(0).toInt();
        info.markType = static_cast<FileMarkType>(q.value(1).toInt());
        info.markScore = q.value(2).toInt();
        m_fileMarks[info.lid] = info;
        
        if (info.markType == FileMarkType::ForDownload) {
            downloadCount++;
        } else if (info.markType == FileMarkType::ForDeletion) {
            deletionCount++;
        }
    }
    
    LOG(QString("[WatchSessionManager] Loaded %1 sessions (%2 active) and %3 file marks (%4 for download, %5 for deletion)")
        .arg(m_sessions.size()).arg(activeCount).arg(m_fileMarks.size())
        .arg(downloadCount).arg(deletionCount));
}

void WatchSessionManager::saveToDatabase()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[WatchSessionManager] Database not open, cannot save sessions");
        return;
    }
    
    QSqlQuery q(db);
    
    // Save sessions
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        const SessionInfo& session = it.value();
        
        q.prepare("INSERT OR REPLACE INTO watch_sessions (aid, start_aid, current_episode, is_active) "
                  "VALUES (?, ?, ?, ?)");
        q.addBindValue(session.aid);
        q.addBindValue(session.startAid);
        q.addBindValue(session.currentEpisode);
        q.addBindValue(session.isActive ? 1 : 0);
        q.exec();
        
        // Save watched episodes
        q.prepare("DELETE FROM session_watched_episodes WHERE aid = ?");
        q.addBindValue(session.aid);
        q.exec();
        
        for (int ep : session.watchedEpisodes) {
            q.prepare("INSERT INTO session_watched_episodes (aid, episode_number) VALUES (?, ?)");
            q.addBindValue(session.aid);
            q.addBindValue(ep);
            q.exec();
        }
    }
    
    // Save file marks
    for (auto it = m_fileMarks.constBegin(); it != m_fileMarks.constEnd(); ++it) {
        const FileMarkInfo& info = it.value();
        q.prepare("INSERT OR REPLACE INTO file_marks (lid, mark_type, mark_score) VALUES (?, ?, ?)");
        q.addBindValue(info.lid);
        q.addBindValue(static_cast<int>(info.markType));
        q.addBindValue(info.markScore);
        q.exec();
    }
    
    saveSettings();
    
    LOG("[WatchSessionManager] Saved session data to database");
}

// ========== Session Management ==========

bool WatchSessionManager::startSession(int aid, int startEpisode)
{
    // Find the original prequel for this anime
    int originalAid = getOriginalPrequel(aid);
    
    SessionInfo session;
    session.aid = aid;
    session.startAid = originalAid;
    session.currentEpisode = startEpisode > 0 ? startEpisode : 1;
    session.isActive = true;
    
    m_sessions[aid] = session;
    
    LOG(QString("[WatchSessionManager] Started session for aid=%1 (original=%2) at episode %3")
        .arg(aid).arg(originalAid).arg(session.currentEpisode));
    
    emit sessionStateChanged(aid, true);
    
    // Auto-mark files for download based on new session
    autoMarkFilesForDownload();
    
    return true;
}

bool WatchSessionManager::startSessionFromFile(int lid)
{
    int aid = getAnimeIdForFile(lid);
    if (aid <= 0) {
        LOG(QString("[WatchSessionManager] Cannot start session: unknown anime for lid=%1").arg(lid));
        return false;
    }
    
    int episodeNumber = getEpisodeNumber(lid);
    if (episodeNumber <= 0) {
        episodeNumber = 1;
    }
    
    return startSession(aid, episodeNumber);
}

void WatchSessionManager::endSession(int aid)
{
    if (m_sessions.contains(aid)) {
        m_sessions[aid].isActive = false;
        LOG(QString("[WatchSessionManager] Ended session for aid=%1").arg(aid));
        emit sessionStateChanged(aid, false);
    }
}

bool WatchSessionManager::hasActiveSession(int aid) const
{
    return m_sessions.contains(aid) && m_sessions[aid].isActive;
}

int WatchSessionManager::getCurrentSessionEpisode(int aid) const
{
    if (m_sessions.contains(aid)) {
        return m_sessions[aid].currentEpisode;
    }
    return 0;
}

void WatchSessionManager::markEpisodeWatched(int aid, int episodeNumber)
{
    if (!m_sessions.contains(aid)) {
        // Start a new session if none exists
        startSession(aid, 1);
    }
    
    SessionInfo& session = m_sessions[aid];
    session.watchedEpisodes.insert(episodeNumber);
    
    // Advance current episode if this was the current one
    if (episodeNumber >= session.currentEpisode) {
        session.currentEpisode = episodeNumber + 1;
    }
    
    LOG(QString("[WatchSessionManager] Marked episode %1 as watched for aid=%2, next=%3")
        .arg(episodeNumber).arg(aid).arg(session.currentEpisode));
    
    // Trigger auto-marking updates
    if (m_autoMarkDeletionEnabled) {
        autoMarkFilesForDeletion();
    }
    autoMarkFilesForDownload();
}

int WatchSessionManager::getOriginalPrequel(int aid) const
{
    loadAnimeRelations(aid);
    
    int currentAid = aid;
    QSet<int> visited;
    
    // Follow prequel chain until we find the first anime
    while (!visited.contains(currentAid)) {
        visited.insert(currentAid);
        
        if (!m_relationsCache.contains(currentAid)) {
            loadAnimeRelations(currentAid);
        }
        
        if (!m_relationsCache.contains(currentAid)) {
            break;
        }
        
        // Look for prequel relation
        int prequelAid = findPrequelAid(currentAid, "prequel");
        if (prequelAid > 0 && !visited.contains(prequelAid)) {
            currentAid = prequelAid;
        } else {
            break;
        }
    }
    
    return currentAid;
}

void WatchSessionManager::loadAnimeRelations(int aid) const
{
    if (m_relationsCache.contains(aid)) {
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT related_aid_list, related_aid_type FROM anime WHERE aid = ?");
    q.addBindValue(aid);
    
    if (!q.exec() || !q.next()) {
        return;
    }
    
    QString relatedAids = q.value(0).toString();
    QString relatedTypes = q.value(1).toString();
    
    QList<QPair<int, QString>> relations;
    
    if (!relatedAids.isEmpty() && !relatedTypes.isEmpty()) {
        QStringList aidList = relatedAids.split("'", Qt::SkipEmptyParts);
        QStringList typeList = relatedTypes.split("'", Qt::SkipEmptyParts);
        
        int count = qMin(aidList.size(), typeList.size());
        for (int i = 0; i < count; i++) {
            int relAid = aidList[i].toInt();
            QString relType = typeList[i].toLower();
            if (relAid > 0) {
                relations.append(qMakePair(relAid, relType));
            }
        }
    }
    
    m_relationsCache[aid] = relations;
}

int WatchSessionManager::findPrequelAid(int aid, const QString& relationType) const
{
    if (!m_relationsCache.contains(aid)) {
        return 0;
    }
    
    const QList<QPair<int, QString>>& relations = m_relationsCache[aid];
    for (const auto& rel : relations) {
        if (rel.second.contains(relationType, Qt::CaseInsensitive)) {
            return rel.first;
        }
    }
    
    return 0;
}

QList<int> WatchSessionManager::getSeriesChain(int aid) const
{
    QList<int> chain;
    QSet<int> visited;
    
    // Start from original prequel
    int currentAid = getOriginalPrequel(aid);
    
    // Follow sequel chain
    while (currentAid > 0 && !visited.contains(currentAid)) {
        chain.append(currentAid);
        visited.insert(currentAid);
        
        loadAnimeRelations(currentAid);
        
        // Look for sequel
        int sequelAid = 0;
        if (m_relationsCache.contains(currentAid)) {
            for (const auto& rel : m_relationsCache[currentAid]) {
                if (rel.second.contains("sequel", Qt::CaseInsensitive)) {
                    sequelAid = rel.first;
                    break;
                }
            }
        }
        
        currentAid = sequelAid;
    }
    
    return chain;
}

// ========== File Marking ==========

int WatchSessionManager::calculateMarkScore(int lid) const
{
    // Base score - all files start with this
    int score = 50;  // Middle-of-the-road score
    
    int aid = getAnimeIdForFile(lid);
    int episodeNumber = getEpisodeNumber(lid);
    
    // Check if card is hidden - hidden cards are more eligible for deletion
    if (aid > 0 && isCardHidden(aid)) {
        score += SCORE_HIDDEN_CARD;  // Negative, makes it more eligible for deletion
    }
    
    // Check if file has been watched (from mylist viewed status)
    // Watched files are more eligible for deletion than unwatched
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("SELECT viewed, local_watched FROM mylist WHERE lid = ?");
        q.addBindValue(lid);
        if (q.exec() && q.next()) {
            int viewed = q.value(0).toInt();
            int localWatched = q.value(1).toInt();
            
            // Files that have been watched (either server or locally) are more deletable
            if (viewed > 0 || localWatched > 0) {
                score += SCORE_ALREADY_WATCHED;  // Negative, more eligible for deletion
            } else {
                score += SCORE_NOT_WATCHED;  // Positive, less eligible for deletion
            }
        }
    }
    
    // Files with active sessions get significant protection
    if (aid > 0 && hasActiveSession(aid)) {
        score += SCORE_ACTIVE_SESSION;  // Large positive, protect from deletion
        
        int currentEp = getCurrentSessionEpisode(aid);
        int distance = episodeNumber - currentEp;
        
        // Use global ahead buffer (applies to all anime)
        int aheadBuffer = m_aheadBuffer;
        
        // Episodes in the ahead buffer get bonus protection
        if (distance >= 0 && distance <= aheadBuffer) {
            score += SCORE_IN_AHEAD_BUFFER;
        }
        
        // Distance penalty/bonus - episodes behind current are more deletable
        score += distance * SCORE_DISTANCE_FACTOR;
        
        // Already watched in session gets penalty (more deletable)
        if (m_sessions.contains(aid) && m_sessions[aid].watchedEpisodes.contains(episodeNumber)) {
            score += SCORE_ALREADY_WATCHED;
        }
    }
    
    return score;
}

FileMarkType WatchSessionManager::getFileMarkType(int lid) const
{
    if (m_fileMarks.contains(lid)) {
        return m_fileMarks[lid].markType;
    }
    return FileMarkType::None;
}

void WatchSessionManager::setFileMarkType(int lid, FileMarkType markType)
{
    FileMarkInfo& info = m_fileMarks[lid];
    info.lid = lid;
    info.markType = markType;
    info.markScore = calculateMarkScore(lid);
    
    emit fileMarkChanged(lid, markType);
    
    QString markTypeName;
    switch (markType) {
        case FileMarkType::None: markTypeName = "None"; break;
        case FileMarkType::ForDownload: markTypeName = "ForDownload"; break;
        case FileMarkType::ForDeletion: markTypeName = "ForDeletion"; break;
    }
    
    LOG(QString("[WatchSessionManager] Set file mark: lid=%1, type=%2, score=%3")
        .arg(lid).arg(markTypeName).arg(info.markScore));
}

FileMarkInfo WatchSessionManager::getFileMarkInfo(int lid) const
{
    if (m_fileMarks.contains(lid)) {
        FileMarkInfo info = m_fileMarks[lid];
        info.markScore = calculateMarkScore(lid);
        return info;
    }
    
    FileMarkInfo info;
    info.lid = lid;
    info.aid = getAnimeIdForFile(lid);
    info.markScore = calculateMarkScore(lid);
    info.isInActiveSession = info.aid > 0 && hasActiveSession(info.aid);
    
    return info;
}

QList<int> WatchSessionManager::getFilesForDeletion() const
{
    QList<QPair<int, int>> scoredFiles; // (lid, score)
    
    for (auto it = m_fileMarks.constBegin(); it != m_fileMarks.constEnd(); ++it) {
        if (it.value().markType == FileMarkType::ForDeletion) {
            scoredFiles.append(qMakePair(it.key(), calculateMarkScore(it.key())));
        }
    }
    
    // Sort by score (ascending - lowest score = delete first)
    std::sort(scoredFiles.begin(), scoredFiles.end(), 
              [](const QPair<int, int>& a, const QPair<int, int>& b) {
                  return a.second < b.second;
              });
    
    QList<int> result;
    for (const auto& pair : scoredFiles) {
        result.append(pair.first);
    }
    
    return result;
}

QList<int> WatchSessionManager::getFilesForDownload() const
{
    QList<int> result;
    
    for (auto it = m_fileMarks.constBegin(); it != m_fileMarks.constEnd(); ++it) {
        if (it.value().markType == FileMarkType::ForDownload) {
            result.append(it.key());
        }
    }
    
    return result;
}

void WatchSessionManager::autoMarkFilesForDeletion()
{
    if (!m_autoMarkDeletionEnabled) {
        LOG("[WatchSessionManager] autoMarkFilesForDeletion: auto-mark deletion is disabled, skipping");
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[WatchSessionManager] autoMarkFilesForDeletion: database not open, cannot process");
        return;
    }
    
    // Get available space on the watched drive (or application directory if not set)
    QString pathToMonitor = m_watchedPath.isEmpty() ? QCoreApplication::applicationDirPath() : m_watchedPath;
    QStorageInfo storage(pathToMonitor);
    qint64 availableBytes = storage.bytesAvailable();
    qint64 totalBytes = storage.bytesTotal();
    
    double availableGB = availableBytes / (1024.0 * 1024.0 * 1024.0);
    double totalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
    // Ensure usedGB is non-negative (could be negative if values are invalid)
    double usedGB = (totalGB > availableGB && totalGB > 0.0 && availableGB >= 0.0) 
                    ? (totalGB - availableGB) : 0.0;
    
    // Log drive information
    LOG(QString("[WatchSessionManager] Monitored path: %1").arg(pathToMonitor));
    LOG(QString("[WatchSessionManager] Watched drive info - Path: %1, Name: %2, FileSystem: %3")
        .arg(storage.rootPath(), storage.displayName(), QString::fromUtf8(storage.fileSystemType())));
    LOG(QString("[WatchSessionManager] Drive space - Total: %1 GB, Used: %2 GB, Available: %3 GB (%4% free)")
        .arg(totalGB, 0, 'f', 2).arg(usedGB, 0, 'f', 2).arg(availableGB, 0, 'f', 2)
        .arg(totalGB > 0.0 ? (availableGB / totalGB * 100.0) : 0.0, 0, 'f', 1));
    
    double threshold = 0;
    if (m_thresholdType == DeletionThresholdType::FixedGB) {
        threshold = m_thresholdValue;
        LOG(QString("[WatchSessionManager] Deletion threshold: %1 GB (fixed)")
            .arg(threshold, 0, 'f', 2));
    } else {
        // Percentage threshold type: use totalGB to calculate threshold
        threshold = (m_thresholdValue / 100.0) * totalGB;
        LOG(QString("[WatchSessionManager] Deletion threshold: %1 GB (%2% of total)")
            .arg(threshold, 0, 'f', 2).arg(m_thresholdValue, 0, 'f', 1));
    }
    
    // Track which lids were updated
    QSet<int> updatedLids;
    
    // If we're above threshold, clear deletion marks
    if (availableGB >= threshold) {
        LOG(QString("[WatchSessionManager] Space check: %1 GB available >= %2 GB threshold, clearing deletion marks")
            .arg(availableGB, 0, 'f', 2).arg(threshold, 0, 'f', 2));
        int clearedCount = 0;
        for (auto it = m_fileMarks.begin(); it != m_fileMarks.end(); ++it) {
            if (it.value().markType == FileMarkType::ForDeletion) {
                it.value().markType = FileMarkType::None;
                updatedLids.insert(it.key());
                emit fileMarkChanged(it.key(), FileMarkType::None);
                clearedCount++;
            }
        }
        if (clearedCount > 0) {
            LOG(QString("[WatchSessionManager] Cleared deletion marks from %1 files").arg(clearedCount));
        }
        if (!updatedLids.isEmpty()) {
            emit markingsUpdated(updatedLids);
        }
        return;
    }
    
    // Calculate how many bytes we need to free
    qint64 spaceToFreeBytes = static_cast<qint64>((threshold - availableGB) * 1024.0 * 1024.0 * 1024.0);
    
    LOG(QString("[WatchSessionManager] Low space detected: %1 GB available < %2 GB threshold, need to free %3 GB (%4 bytes)")
        .arg(availableGB, 0, 'f', 2).arg(threshold, 0, 'f', 2).arg(threshold - availableGB, 0, 'f', 2).arg(spaceToFreeBytes));
    
    // First, clear all existing deletion marks before calculating new ones
    // This ensures we only have the minimum required files marked for deletion
    int clearedCount = 0;
    for (auto it = m_fileMarks.begin(); it != m_fileMarks.end(); ++it) {
        if (it.value().markType == FileMarkType::ForDeletion) {
            it.value().markType = FileMarkType::None;
            updatedLids.insert(it.key());
            emit fileMarkChanged(it.key(), FileMarkType::None);
            clearedCount++;
        }
    }
    if (clearedCount > 0) {
        LOG(QString("[WatchSessionManager] Cleared %1 existing deletion marks before recalculating").arg(clearedCount));
    }
    
    // Get all files with local paths that could be candidates for deletion
    // Join mylist with local_files and file tables to get lid, path, and file size
    QSqlQuery q(db);
    
    // First, log the count of files in local_files table
    q.exec("SELECT COUNT(*) FROM local_files");
    int totalLocalFiles = 0;
    if (q.next()) {
        totalLocalFiles = q.value(0).toInt();
    }
    LOG(QString("[WatchSessionManager] Total rows in local_files table: %1").arg(totalLocalFiles));
    
    // Count files with valid path
    q.exec("SELECT COUNT(*) FROM local_files WHERE path IS NOT NULL AND path != ''");
    int filesWithPath = 0;
    if (q.next()) {
        filesWithPath = q.value(0).toInt();
    }
    LOG(QString("[WatchSessionManager] Files with valid path: %1").arg(filesWithPath));
    
    // Count mylist entries with local_file reference
    q.exec("SELECT COUNT(*) FROM mylist WHERE local_file IS NOT NULL");
    int mylistWithLocalFile = 0;
    if (q.next()) {
        mylistWithLocalFile = q.value(0).toInt();
    }
    LOG(QString("[WatchSessionManager] Mylist entries with local_file reference: %1").arg(mylistWithLocalFile));
    
    // Get mylist entries with local paths and file sizes
    // Join mylist -> local_files (for path) and mylist -> file (for size)
    bool querySuccess = q.exec(
        "SELECT m.lid, lf.path, COALESCE(f.size, 0) as file_size FROM mylist m "
        "JOIN local_files lf ON m.local_file = lf.id "
        "LEFT JOIN file f ON m.fid = f.fid "
        "WHERE lf.path IS NOT NULL AND lf.path != ''");
    
    if (!querySuccess) {
        LOG(QString("[WatchSessionManager] SQL query failed: %1").arg(q.lastError().text()));
        return;
    }
    
    // Structure to hold candidate info: lid, score, size, path
    struct CandidateInfo {
        int lid;
        int score;
        qint64 size;
        QString path;
    };
    
    QList<CandidateInfo> candidates;
    int processedCount = 0;
    while (q.next()) {
        CandidateInfo info;
        info.lid = q.value(0).toInt();
        info.path = q.value(1).toString();
        info.size = q.value(2).toLongLong();
        info.score = calculateMarkScore(info.lid);
        candidates.append(info);
        processedCount++;
        
        // Log first few candidates for debugging
        if (processedCount <= 5) {
            LOG(QString("[WatchSessionManager] Candidate file: lid=%1, path=%2, score=%3, size=%4 bytes")
                .arg(info.lid).arg(info.path).arg(info.score).arg(info.size));
        }
    }
    
    if (processedCount > 5) {
        LOG(QString("[WatchSessionManager] ... and %1 more candidates").arg(processedCount - 5));
    }
    
    LOG(QString("[WatchSessionManager] Found %1 files as deletion candidates").arg(candidates.size()));
    
    // Sort by score (lowest first - most eligible for deletion)
    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateInfo& a, const CandidateInfo& b) {
                  return a.score < b.score;
              });
    
    // Mark files for deletion until we've accumulated enough space to free
    qint64 accumulatedSpace = 0;
    int markedCount = 0;
    
    for (const auto& candidate : candidates) {
        // Stop if we've already marked enough files to free the required space
        if (accumulatedSpace >= spaceToFreeBytes) {
            LOG(QString("[WatchSessionManager] Reached deletion target: %1 bytes accumulated >= %2 bytes needed")
                .arg(accumulatedSpace).arg(spaceToFreeBytes));
            break;
        }
        
        FileMarkInfo& markInfo = m_fileMarks[candidate.lid];
        markInfo.lid = candidate.lid;
        markInfo.markType = FileMarkType::ForDeletion;
        markInfo.markScore = candidate.score;
        
        accumulatedSpace += candidate.size;
        markedCount++;
        
        LOG(QString("[WatchSessionManager] Marked file for deletion: lid=%1, score=%2, size=%3 bytes, accumulated=%4 bytes")
            .arg(candidate.lid).arg(candidate.score).arg(candidate.size).arg(accumulatedSpace));
        
        updatedLids.insert(candidate.lid);
        emit fileMarkChanged(candidate.lid, FileMarkType::ForDeletion);
    }
    
    if (!updatedLids.isEmpty()) {
        LOG(QString("[WatchSessionManager] Total files marked for deletion: %1 (would free %2 GB)")
            .arg(updatedLids.size()).arg(accumulatedSpace / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2));
        emit markingsUpdated(updatedLids);
    } else {
        LOG("[WatchSessionManager] No files marked for deletion (no candidates or no space needed)");
    }
}

void WatchSessionManager::autoMarkFilesForDownload()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[WatchSessionManager] autoMarkFilesForDownload: database not open, cannot process");
        return;
    }
    
    LOG(QString("[WatchSessionManager] autoMarkFilesForDownload: scanning %1 sessions with ahead buffer of %2 episodes")
        .arg(m_sessions.size()).arg(m_aheadBuffer));
    
    // Track which lids were updated
    QSet<int> updatedLids;
    int activeSessionCount = 0;
    
    // For each active session, mark files in the ahead buffer for download
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        const SessionInfo& session = it.value();
        
        if (!session.isActive) {
            continue;
        }
        
        activeSessionCount++;
        LOG(QString("[WatchSessionManager] Processing active session: aid=%1, currentEpisode=%2, startAid=%3")
            .arg(session.aid).arg(session.currentEpisode).arg(session.startAid));
        
        // Use global ahead buffer setting (applies to all anime)
        int aheadBuffer = m_aheadBuffer;
        
        // Find files for episodes in the ahead buffer
        for (int ep = session.currentEpisode; ep <= session.currentEpisode + aheadBuffer; ep++) {
            // Query for files that match this episode and don't have a local file
            QSqlQuery q(db);
            q.prepare("SELECT m.lid FROM mylist m "
                      "JOIN episode e ON m.eid = e.eid "
                      "LEFT JOIN local_files lf ON m.lid = lf.lid "
                      "WHERE m.aid = ? AND e.epno LIKE ? "
                      "AND (lf.local_path IS NULL OR lf.local_path = '')");
            q.addBindValue(session.aid);
            q.addBindValue(QString::number(ep) + "%"); // Match episode number
            
            if (q.exec()) {
                while (q.next()) {
                    int lid = q.value(0).toInt();
                    
                    // Only mark if not already marked for something else
                    if (!m_fileMarks.contains(lid) || 
                        m_fileMarks[lid].markType == FileMarkType::None) {
                        FileMarkInfo& info = m_fileMarks[lid];
                        info.lid = lid;
                        info.markType = FileMarkType::ForDownload;
                        info.markScore = calculateMarkScore(lid);
                        
                        LOG(QString("[WatchSessionManager] Marked file for download: lid=%1, aid=%2, episode=%3, score=%4")
                            .arg(lid).arg(session.aid).arg(ep).arg(info.markScore));
                        
                        updatedLids.insert(lid);
                        emit fileMarkChanged(lid, FileMarkType::ForDownload);
                    }
                }
            }
        }
    }
    
    LOG(QString("[WatchSessionManager] autoMarkFilesForDownload complete: %1 active sessions processed, %2 files marked for download")
        .arg(activeSessionCount).arg(updatedLids.size()));
    
    if (!updatedLids.isEmpty()) {
        emit markingsUpdated(updatedLids);
    }
}

// ========== Settings ==========

int WatchSessionManager::getAheadBuffer() const
{
    return m_aheadBuffer;
}

void WatchSessionManager::setAheadBuffer(int episodes)
{
    m_aheadBuffer = episodes;
    saveSettings();
    autoMarkFilesForDownload();
}

DeletionThresholdType WatchSessionManager::getDeletionThresholdType() const
{
    return m_thresholdType;
}

void WatchSessionManager::setDeletionThresholdType(DeletionThresholdType type)
{
    m_thresholdType = type;
    saveSettings();
}

double WatchSessionManager::getDeletionThresholdValue() const
{
    return m_thresholdValue;
}

void WatchSessionManager::setDeletionThresholdValue(double value)
{
    m_thresholdValue = value;
    saveSettings();
}

bool WatchSessionManager::isAutoMarkDeletionEnabled() const
{
    return m_autoMarkDeletionEnabled;
}

void WatchSessionManager::setAutoMarkDeletionEnabled(bool enabled)
{
    m_autoMarkDeletionEnabled = enabled;
    saveSettings();
    
    // Only trigger auto-mark if initial scan is complete (mylist data is loaded)
    if (enabled && m_initialScanComplete) {
        autoMarkFilesForDeletion();
    }
}

QString WatchSessionManager::getWatchedPath() const
{
    return m_watchedPath;
}

void WatchSessionManager::setWatchedPath(const QString& path)
{
    if (m_watchedPath != path) {
        m_watchedPath = path;
        LOG(QString("[WatchSessionManager] Watched path set to: %1").arg(path.isEmpty() ? "(application directory)" : path));
        saveSettings();
        
        // Trigger space check with new path only after initial scan is complete
        // (before that, the mylist data may not be loaded yet)
        if (m_autoMarkDeletionEnabled && m_initialScanComplete) {
            autoMarkFilesForDeletion();
        }
    }
}

void WatchSessionManager::performInitialScan()
{
    LOG("[WatchSessionManager] Performing initial file scan...");
    
    // Mark that initial scan is complete - this enables space checks on path changes
    m_initialScanComplete = true;
    
    // Note: We do NOT auto-start sessions for existing anime here.
    // Sessions are only auto-started for BRAND NEW anime when they are added to mylist.
    // This preserves user control over which anime have active sessions.
    
    // Scan for files that should be marked for download based on active sessions
    // (this will emit markingsUpdated with the set of updated lids)
    autoMarkFilesForDownload();
    
    // Scan for files that should be marked for deletion if auto-deletion is enabled
    // (this will emit markingsUpdated with the set of updated lids)
    if (m_autoMarkDeletionEnabled) {
        autoMarkFilesForDeletion();
    }
    
    // Log the results
    int downloadCount = 0;
    int deletionCount = 0;
    for (auto it = m_fileMarks.constBegin(); it != m_fileMarks.constEnd(); ++it) {
        if (it.value().markType == FileMarkType::ForDownload) {
            downloadCount++;
        } else if (it.value().markType == FileMarkType::ForDeletion) {
            deletionCount++;
        }
    }
    
    LOG(QString("[WatchSessionManager] Initial scan complete: %1 files marked for download, %2 files marked for deletion")
        .arg(downloadCount).arg(deletionCount));
    
    // Save the marks to database
    saveToDatabase();
    
    // Note: markingsUpdated signal is emitted by autoMarkFilesForDownload/autoMarkFilesForDeletion
}

void WatchSessionManager::onNewAnimeAdded(int aid)
{
    // Auto-start session for brand new anime added to mylist
    if (aid > 0 && !hasActiveSession(aid)) {
        LOG(QString("[WatchSessionManager] New anime aid=%1 added to mylist, auto-starting session").arg(aid));
        startSession(aid, 1);
        
        // Mark files for download based on the new session
        // (this will emit markingsUpdated with the set of updated lids)
        autoMarkFilesForDownload();
        saveToDatabase();
        // Note: markingsUpdated signal is emitted by autoMarkFilesForDownload
    }
}

// ========== Helper Methods ==========

int WatchSessionManager::getEpisodeNumber(int lid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 0;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT e.epno FROM mylist m JOIN episode e ON m.eid = e.eid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        // Parse episode number from epno string (could be "1", "S1", "C1", etc.)
        QString epno = q.value(0).toString();
        
        // Try to extract numeric part
        static const QRegularExpression regex("(\\d+)");
        QRegularExpressionMatch match = regex.match(epno);
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
    }
    
    return 0;
}

int WatchSessionManager::getAnimeIdForFile(int lid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 0;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT aid FROM mylist WHERE lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    
    return 0;
}

bool WatchSessionManager::isCardHidden(int aid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT is_hidden FROM anime WHERE aid = ?");
    q.addBindValue(aid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toInt() != 0;
    }
    
    return false;
}
