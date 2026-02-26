#include "watchsessionmanager.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStorageInfo>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QFileInfo>
#include <algorithm>

// Static regex for episode number extraction (shared across functions)
const QRegularExpression WatchSessionManager::s_epnoNumericRegex(R"(\d+)");

WatchSessionManager::WatchSessionManager(QObject *parent)
    : QObject(parent)
    , m_aheadBuffer(DEFAULT_AHEAD_BUFFER)
    , m_thresholdType(DeletionThresholdType::FixedGB)
    , m_thresholdValue(DEFAULT_THRESHOLD_VALUE)
    , m_autoMarkDeletionEnabled(false)
    , m_enableActualDeletion(false)          // Default: disabled for safety
    , m_forceDeletePermissions(false)        // Default: disabled for safety
    , m_initialScanComplete(false)
{
    ensureTablesExist();
    loadSettings();
    loadFromDatabase();
}

WatchSessionManager::~WatchSessionManager()
{
    saveToDatabase();
}

void WatchSessionManager::ensureTablesExist()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
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
    
    // Note: file_marks table removed - all marks are calculated on demand and kept in memory only
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
    
    // Load actual deletion enabled
    q.prepare("SELECT value FROM settings WHERE name = 'enable_actual_deletion'");
    if (q.exec() && q.next()) {
        m_enableActualDeletion = q.value(0).toInt() != 0;
    }
    
    // Load force delete permissions
    q.prepare("SELECT value FROM settings WHERE name = 'force_delete_permissions'");
    if (q.exec() && q.next()) {
        m_forceDeletePermissions = q.value(0).toInt() != 0;
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
    
    // Save actual deletion enabled
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('enable_actual_deletion', ?)");
    q.addBindValue(m_enableActualDeletion ? 1 : 0);
    q.exec();
    
    // Save force delete permissions
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('force_delete_permissions', ?)");
    q.addBindValue(m_forceDeletePermissions ? 1 : 0);
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
        return;
    }
    
    QSqlQuery q(db);
    
    // Load active sessions
    q.exec("SELECT aid, start_aid, current_episode, is_active FROM watch_sessions");
    while (q.next()) {
        SessionInfo session;
        session.setAid(q.value(0).toInt());
        session.setStartAid(q.value(1).toInt());
        session.setCurrentEpisode(q.value(2).toInt());
        session.setActive(q.value(3).toInt() != 0);
        
        m_sessions[session.aid()] = session;
    }
    
    // Load watched episodes for each session
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        QSqlQuery epQuery(db);
        epQuery.prepare("SELECT episode_number FROM session_watched_episodes WHERE aid = ?");
        epQuery.addBindValue(it.key());
        if (epQuery.exec()) {
            while (epQuery.next()) {
                it.value().markEpisodeWatched(epQuery.value(0).toInt());
            }
        }
    }
    
    // Note: file_marks are no longer persisted - they are calculated on demand and kept in memory only
}

void WatchSessionManager::saveToDatabase()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    // Start transaction for atomic save operation
    if (!db.transaction()) {
        LOG(QString("ERROR: Failed to start transaction: %1").arg(db.lastError().text()));
        // Continue without transaction for backwards compatibility
    }
    
    QSqlQuery q(db);
    
    // Save sessions
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        const SessionInfo& session = it.value();
        
        q.prepare("INSERT OR REPLACE INTO watch_sessions (aid, start_aid, current_episode, is_active) "
                  "VALUES (?, ?, ?, ?)");
        q.addBindValue(session.aid());
        q.addBindValue(session.startAid());
        q.addBindValue(session.currentEpisode());
        q.addBindValue(session.isActive() ? 1 : 0);
        q.exec();
        
        // Save watched episodes
        q.prepare("DELETE FROM session_watched_episodes WHERE aid = ?");
        q.addBindValue(session.aid());
        q.exec();
        
        const auto& watchedEps = session.watchedEpisodes();
        for (int ep : watchedEps) {
            q.prepare("INSERT INTO session_watched_episodes (aid, episode_number) VALUES (?, ?)");
            q.addBindValue(session.aid());
            q.addBindValue(ep);
            q.exec();
        }
    }
    
    // Note: file_marks are no longer persisted to database - they are kept in memory only
    
    // Commit transaction
    if (!db.commit()) {
        LOG(QString("ERROR: Failed to commit transaction: %1").arg(db.lastError().text()));
        db.rollback();
        return;
    }
    
    saveSettings();
}

// ========== Session Management ==========

bool WatchSessionManager::startSession(int aid, int startEpisode)
{
    // Find the original prequel for this anime
    int originalAid = getOriginalPrequel(aid);
    
    SessionInfo session;
    session.setAid(aid);
    session.setStartAid(originalAid);
    session.setCurrentEpisode(startEpisode > 0 ? startEpisode : 1);
    session.setActive(true);
    
    m_sessions[aid] = session;
    
    emit sessionStateChanged(aid, true);
    
    // Auto-mark files for download based on new session
    autoMarkFilesForDownload();
    
    return true;
}

bool WatchSessionManager::startSessionFromFile(int lid)
{
    int aid = getAnimeIdForFile(lid);
    if (aid <= 0) {
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
        m_sessions[aid].setActive(false);
        emit sessionStateChanged(aid, false);
    }
}

bool WatchSessionManager::hasActiveSession(int aid) const
{
    return m_sessions.contains(aid) && m_sessions[aid].isActive();
}

int WatchSessionManager::getCurrentSessionEpisode(int aid) const
{
    if (m_sessions.contains(aid)) {
        return m_sessions[aid].currentEpisode();
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
    session.markEpisodeWatched(episodeNumber);
    
    // Advance current episode if this was the current one
    if (episodeNumber >= session.currentEpisode()) {
        session.setCurrentEpisode(episodeNumber + 1);
    }
    
    // Trigger auto-marking updates
    if (m_autoMarkDeletionEnabled) {
        autoMarkFilesForDeletion();
    }
    autoMarkFilesForDownload();
}

int WatchSessionManager::getOriginalPrequel(int aid) const
{
    // Check cache first
    if (m_prequelCache.contains(aid)) {
        return m_prequelCache[aid];
    }
    
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
    
    // Cache result for all visited AIDs (they all have the same original prequel)
    for (int visitedAid : visited) {
        m_prequelCache[visitedAid] = currentAid;
    }
    
    return currentAid;
}

QList<int> WatchSessionManager::getSeriesChain(int aid) const
{
    // Check cache first
    if (m_seriesChainCache.contains(aid)) {
        return m_seriesChainCache[aid];
    }
    
    QList<int> chain;
    QSet<int> visited;
    
    // Start from original prequel
    int currentAid = getOriginalPrequel(aid);
    
    // Follow sequel chain
    while (currentAid > 0 && !visited.contains(currentAid)) {
        chain.append(currentAid);
        visited.insert(currentAid);
        
        loadAnimeRelations(currentAid);
        
        // Look for sequel (type code 1 or string "sequel")
        int sequelAid = 0;
        if (m_relationsCache.contains(currentAid)) {
            for (const auto& rel : m_relationsCache[currentAid]) {
                int relCode = rel.second.toInt();
                if (relCode == RELATION_SEQUEL || rel.second.contains("sequel", Qt::CaseInsensitive)) {
                    sequelAid = rel.first;
                    break;
                }
            }
        }
        
        currentAid = sequelAid;
    }
    
    // Cache result for all AIDs in the chain (they share the same chain)
    for (int chainAid : chain) {
        m_seriesChainCache[chainAid] = chain;
    }
    
    return chain;
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
    q.prepare("SELECT relaidlist, relaidtype FROM anime WHERE aid = ?");
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
    
    // Map string relation type to numeric code for comparison
    int targetCode = -1;
    if (relationType.toLower() == "prequel") {
        targetCode = RELATION_PREQUEL;
    } else if (relationType.toLower() == "sequel") {
        targetCode = RELATION_SEQUEL;
    }
    
    const QList<QPair<int, QString>>& relations = m_relationsCache[aid];
    for (const auto& rel : relations) {
        // Check both numeric code and string match (for backwards compatibility with tests)
        int relCode = rel.second.toInt();
        if (relCode == targetCode || rel.second.contains(relationType, Qt::CaseInsensitive)) {
            return rel.first;
        }
    }
    
    return 0;
}

// ========== File Marking ==========

std::tuple<int, int, int> WatchSessionManager::findActiveSessionInSeriesChain(int aid) const
{
    // Use the public getSeriesChain method to build the chain
    QList<int> chain = getSeriesChain(aid);
    
    // Find if any anime in the chain has an active session
    for (int chainAid : chain) {
        if (hasActiveSession(chainAid)) {
            // Calculate offset for the requested anime
            int offsetForRequestedAnime = 0;
            for (int i = 0; i < chain.size(); i++) {
                if (chain[i] == aid) {
                    break;
                }
                offsetForRequestedAnime += getTotalEpisodesForAnime(chain[i]);
            }
            
            // Calculate offset for the session anime
            int offsetForSessionAnime = 0;
            for (int i = 0; i < chain.size(); i++) {
                if (chain[i] == chainAid) {
                    break;
                }
                offsetForSessionAnime += getTotalEpisodesForAnime(chain[i]);
            }
            
            return std::make_tuple(chainAid, offsetForRequestedAnime, offsetForSessionAnime);
        }
    }
    
    return std::make_tuple(0, 0, 0);  // No active session found
}

int WatchSessionManager::getTotalEpisodesForAnime(int aid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return DEFAULT_EPISODE_COUNT;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT COALESCE(eptotal, episodes, 0) FROM anime WHERE aid = ?");
    q.addBindValue(aid);
    
    if (q.exec() && q.next()) {
        int total = q.value(0).toInt();
        return total > 0 ? total : DEFAULT_EPISODE_COUNT;
    }
    
    return DEFAULT_EPISODE_COUNT;
}

bool WatchSessionManager::isDeletionNeeded() const
{
    if (!m_autoMarkDeletionEnabled) {
        return false;
    }
    
    // Get available space on the watched drive (or application directory if not set)
    QString pathToMonitor = m_watchedPath.isEmpty() ? QCoreApplication::applicationDirPath() : m_watchedPath;
    QStorageInfo storage(pathToMonitor);
    qint64 availableBytes = storage.bytesAvailable();
    qint64 totalBytes = storage.bytesTotal();
    
    double availableGB = availableBytes / (1024.0 * 1024.0 * 1024.0);
    double totalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
    
    double threshold = 0;
    if (m_thresholdType == DeletionThresholdType::FixedGB) {
        threshold = m_thresholdValue;
    } else {
        // Percentage threshold type: use totalGB to calculate threshold
        threshold = (m_thresholdValue / 100.0) * totalGB;
    }
    
    // Return true if below threshold
    return availableGB < threshold;
}

bool WatchSessionManager::deleteFile(int lid, bool deleteFromDisk)
{
    LOG(QString("[WatchSessionManager] deleteFile called for lid=%1, deleteFromDisk=%2").arg(lid).arg(deleteFromDisk));
    
    // Get the aid for this file before deletion
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[WatchSessionManager] Database not open, cannot delete file");
        return false;
    }
    
    // Query aid for the file
    QSqlQuery aidQuery(db);
    aidQuery.prepare("SELECT aid FROM mylist WHERE lid = ?");
    aidQuery.addBindValue(lid);
    int aid = 0;
    if (aidQuery.exec() && aidQuery.next()) {
        aid = aidQuery.value(0).toInt();
    }
    
    // Emit signal to request file deletion (Window will handle it with API access)
    // Window will call onFileDeletionResult() when the operation completes
    emit deleteFileRequested(lid, deleteFromDisk);
    
    LOG(QString("[WatchSessionManager] File deletion requested for lid=%1, aid=%2").arg(lid).arg(aid));
    return true;
}

void WatchSessionManager::cleanupMissingFileStatus(int lid)
{
    LOG(QString("[WatchSessionManager] cleanupMissingFileStatus called for lid=%1").arg(lid));
    
    // Get the aid for this file before cleanup
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[WatchSessionManager] Database not open, cannot cleanup file status");
        return;
    }
    
    // Query aid for the file
    QSqlQuery aidQuery(db);
    aidQuery.prepare("SELECT aid FROM mylist WHERE lid = ?");
    aidQuery.addBindValue(lid);
    int aid = 0;
    if (aidQuery.exec() && aidQuery.next()) {
        aid = aidQuery.value(0).toInt();
    }
    
    // Emit signal to request file status cleanup (Window will handle it with API access)
    // deleteFromDisk=false because the file is already gone from disk
    // This will update local database and AniDB API to reflect the file's absence
    emit deleteFileRequested(lid, false);
    
    LOG(QString("[WatchSessionManager] File status cleanup requested for lid=%1, aid=%2").arg(lid).arg(aid));
}

void WatchSessionManager::onFileDeletionResult(int lid, int aid, bool success)
{
    LOG(QString("[WatchSessionManager] onFileDeletionResult lid=%1, aid=%2, success=%3, failedCount=%4")
        .arg(lid).arg(aid).arg(success).arg(m_failedDeletions.size()));

    if (success) {
        LOG(QString("[WatchSessionManager] File deletion succeeded for lid=%1, aid=%2").arg(lid).arg(aid));
        m_failedDeletions.remove(lid);
        emit fileDeleted(lid, aid);
        
        // Request next deletion cycle via DeletionQueue (handled by Window)
        if (m_enableActualDeletion && isDeletionNeeded()) {
            LOG("[WatchSessionManager] Space still below threshold after deletion, requesting next deletion cycle");
            emit deletionCycleRequested();
        } else {
            LOG(QString("[WatchSessionManager] No further deletion needed: enableActualDeletion=%1, deletionNeeded=%2")
                .arg(m_enableActualDeletion).arg(isDeletionNeeded()));
        }
    } else {
        LOG(QString("[WatchSessionManager] File deletion failed for lid=%1, aid=%2").arg(lid).arg(aid));
        m_failedDeletions.insert(lid);
        
        // Request next deletion cycle even after failure so DeletionQueue picks a different candidate
        if (m_enableActualDeletion && isDeletionNeeded()) {
            emit deletionCycleRequested();
        }
    }
    LOG(QString("[WatchSessionManager] onFileDeletionResult completed for lid=%1").arg(lid));
}

void WatchSessionManager::autoMarkFilesForDeletion()
{
    if (!m_enableActualDeletion) {
        return;
    }
    
    // Emit signal so Window can use DeletionQueue to pick the best candidate
    if (isDeletionNeeded()) {
        LOG("[WatchSessionManager] Space below threshold, requesting deletion cycle via DeletionQueue");
        emit deletionCycleRequested();
    }
}

void WatchSessionManager::autoMarkFilesForDownload()
{
    // Removed: Marking system has been eliminated
    // Download management now handled elsewhere
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
        saveSettings();
        
        // Trigger space check with new path only after initial scan is complete
        // (before that, the mylist data may not be loaded yet)
        if (m_autoMarkDeletionEnabled && m_initialScanComplete) {
            autoMarkFilesForDeletion();
        }
    }
}

bool WatchSessionManager::isActualDeletionEnabled() const
{
    return m_enableActualDeletion;
}

void WatchSessionManager::setActualDeletionEnabled(bool enabled)
{
    m_enableActualDeletion = enabled;
    saveSettings();
}

bool WatchSessionManager::isForceDeletePermissionsEnabled() const
{
    return m_forceDeletePermissions;
}

void WatchSessionManager::setForceDeletePermissionsEnabled(bool enabled)
{
    m_forceDeletePermissions = enabled;
    saveSettings();
}

void WatchSessionManager::autoStartSessionsForExistingAnime()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    // Find all unique anime IDs that have local files (via mylist -> local_files join)
    // Ensure local_file is not NULL to avoid matching records without valid local file references
    QSqlQuery q(db);
    bool querySuccess = q.exec(
        "SELECT DISTINCT m.aid FROM mylist m "
        "JOIN local_files lf ON m.local_file = lf.id "
        "WHERE m.local_file IS NOT NULL AND lf.path IS NOT NULL AND lf.path != '' AND m.aid > 0");
    
    if (!querySuccess) {
        return;
    }
    
    while (q.next()) {
        int aid = q.value(0).toInt();
        
        if (!hasActiveSession(aid)) {
            // Start session at episode 1 for anime without an active session
            SessionInfo session;
            session.setAid(aid);
            
            // Get the original prequel, falling back to current aid if not found
            int startAid = getOriginalPrequel(aid);
            session.setStartAid((startAid > 0) ? startAid : aid);
            
            session.setCurrentEpisode(1);
            session.setActive(true);
            
            m_sessions[aid] = session;
            
            emit sessionStateChanged(aid, true);
        }
    }
}

void WatchSessionManager::performInitialScan()
{
    // Mark that initial scan is complete - this enables space checks on path changes
    m_initialScanComplete = true;
    
    // Auto-start sessions for anime that have local files but no active session
    // This ensures WatchSessionManager works for existing anime collections
    autoStartSessionsForExistingAnime();
    
    // Scan for files that should be marked for download based on active sessions
    // (this will emit markingsUpdated with the set of updated lids)
    autoMarkFilesForDownload();
    
    // Scan for files that should be marked for deletion if auto-deletion is enabled
    // (this will emit markingsUpdated with the set of updated lids)
    if (m_enableActualDeletion) {
        autoMarkFilesForDeletion();
    }
    
    // Save the marks to database
    saveToDatabase();
    
    // Note: markingsUpdated signal is emitted by autoMarkFilesForDownload/autoMarkFilesForDeletion
}

void WatchSessionManager::onNewAnimeAdded(int aid)
{
    // Auto-start session for brand new anime added to mylist
    if (aid > 0 && !hasActiveSession(aid)) {
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

int WatchSessionManager::getFileVersion(int lid) const
{
    // Get file version from state bits (AniDB state field)
    // State field bit encoding (from AniDB UDP API):
    //   Bit 0 (1): FILE_CRCOK
    //   Bit 1 (2): FILE_CRCERR
    //   Bit 2 (4): FILE_ISV2 - file is version 2
    //   Bit 3 (8): FILE_ISV3 - file is version 3
    //   Bit 4 (16): FILE_ISV4 - file is version 4
    //   Bit 5 (32): FILE_ISV5 - file is version 5
    //   Bit 6 (64): FILE_UNC - uncensored
    //   Bit 7 (128): FILE_CEN - censored
    // If no version bits are set, the file is version 1
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 1;  // Default to version 1
    }
    
    QSqlQuery q(db);
    // Join mylist to file table to get state field
    q.prepare("SELECT f.state FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        int state = q.value(0).toInt();
        
        // Extract version from state bits 2-5
        // Check version flags in priority order (v5 > v4 > v3 > v2)
        int version = 1;  // Default to version 1
        if (state & 32) {      // Bit 5: FILE_ISV5
            version = 5;
        } else if (state & 16) { // Bit 4: FILE_ISV4
            version = 4;
        } else if (state & 8) {  // Bit 3: FILE_ISV3
            version = 3;
        } else if (state & 4) {  // Bit 2: FILE_ISV2
            version = 2;
        }
        // else version = 1 (no version bits set means version 1)
        
        return version;
    }
    
    return 1;  // Default to version 1
}

int WatchSessionManager::getFileCountForEpisode(int lid) const
{
    // Count how many files exist for the same episode (same eid)
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 1;
    }
    
    QSqlQuery q(db);
    // Count mylist entries with same eid as this lid, that have local files
    q.prepare(
        "SELECT COUNT(*) FROM mylist m "
        "JOIN local_files lf ON m.local_file = lf.id "
        "WHERE m.eid = (SELECT eid FROM mylist WHERE lid = ?) "
        "AND lf.path IS NOT NULL AND lf.path != ''"
    );
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    
    return 1;  // Default to 1 file
}

int WatchSessionManager::getHigherVersionFileCount(int lid) const
{
    // Count how many local files for the same episode have a higher version than this file
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 0;
    }
    
    // First get this file's version
    int myVersion = getFileVersion(lid);
    
    QSqlQuery q(db);
    // Count local files with same eid that have a higher version
    // Version is encoded in state bits 2-5 as flags:
    //   Bit 2 (4): v2, Bit 3 (8): v3, Bit 4 (16): v4, Bit 5 (32): v5
    //   No bits set = v1
    // We need to extract version from state and compare
    q.prepare(
        "SELECT COUNT(*) FROM mylist m "
        "JOIN file f ON m.fid = f.fid "
        "JOIN local_files lf ON m.local_file = lf.id "
        "WHERE m.eid = (SELECT eid FROM mylist WHERE lid = ?) "
        "AND m.lid != ? "
        "AND lf.path IS NOT NULL AND lf.path != '' "
        "AND CASE "
        "  WHEN (f.state & 32) THEN 5 "  // Bit 5: v5
        "  WHEN (f.state & 16) THEN 4 "  // Bit 4: v4
        "  WHEN (f.state & 8) THEN 3 "   // Bit 3: v3
        "  WHEN (f.state & 4) THEN 2 "   // Bit 2: v2
        "  ELSE 1 "                       // No version bits: v1
        "END > ?"
    );
    q.addBindValue(lid);  // Same episode
    q.addBindValue(lid);  // Exclude self
    q.addBindValue(myVersion);  // Files with higher version
    
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    
    return 0;
}

// Helper methods for new marking criteria
bool WatchSessionManager::matchesPreferredAudioLanguage(int lid) const
{
    LOG(QString("[WatchSessionManager] matchesPreferredAudioLanguage() enter: lid=%1").arg(lid));
    QString audioLang = getFileAudioLanguage(lid);
    if (audioLang.isEmpty()) {
        LOG(QString("[WatchSessionManager] matchesPreferredAudioLanguage: lid=%1 audioLang is empty, returning false").arg(lid));
        return false;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG(QString("[WatchSessionManager] matchesPreferredAudioLanguage: lid=%1 db not open, returning false").arg(lid));
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT value FROM settings WHERE name = 'preferredAudioLanguages'");
    if (!q.exec() || !q.next()) {
        return false;
    }
    
    QString preferredLangs = q.value(0).toString().toLower();
    QStringList prefList = preferredLangs.split(',', Qt::SkipEmptyParts);
    
    // lang_dub uses ' as delimiter (e.g. "japanese'english")
    QStringList fileLangs = audioLang.toLower().trimmed().split('\'', Qt::SkipEmptyParts);
    for (const QString &fileLang : fileLangs) {
        QString trimmedFileLang = fileLang.trimmed();
        for (const QString &pref : prefList) {
            if (trimmedFileLang == pref.trimmed()) {
                return true;
            }
        }
    }
    
    return false;
}

bool WatchSessionManager::matchesPreferredSubtitleLanguage(int lid) const
{
    LOG(QString("[WatchSessionManager] matchesPreferredSubtitleLanguage() enter: lid=%1").arg(lid));
    QString subLang = getFileSubtitleLanguage(lid);
    if (subLang.isEmpty()) {
        LOG(QString("[WatchSessionManager] matchesPreferredSubtitleLanguage: lid=%1 subLang is empty, returning false").arg(lid));
        return false;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG(QString("[WatchSessionManager] matchesPreferredSubtitleLanguage: lid=%1 db not open, returning false").arg(lid));
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT value FROM settings WHERE name = 'preferredSubtitleLanguages'");
    if (!q.exec() || !q.next()) {
        return false;
    }
    
    QString preferredLangs = q.value(0).toString().toLower();
    QStringList prefList = preferredLangs.split(',', Qt::SkipEmptyParts);
    
    // lang_sub uses ' as delimiter (e.g. "english'japanese")
    QStringList fileLangs = subLang.toLower().trimmed().split('\'', Qt::SkipEmptyParts);
    for (const QString &fileLang : fileLangs) {
        QString trimmedFileLang = fileLang.trimmed();
        for (const QString &pref : prefList) {
            if (trimmedFileLang == pref.trimmed()) {
                return true;
            }
        }
    }
    
    return false;
}

int WatchSessionManager::getQualityScore(const QString& quality) const
{
    // Convert AniDB quality string to numeric score
    // AniDB quality values: "very high", "high", "medium", "low", "very low", "corrupted", "eyecancer"
    // Higher score = better quality (less likely to delete)
    QString q = quality.toLower().trimmed();
    
    if (q == "very high") return 100;
    if (q == "high") return 80;
    if (q == "medium") return 60;
    if (q == "low") return 40;
    if (q == "very low") return 20;
    if (q == "corrupted" || q == "eyecancer") return 10;
    
    return 50;  // Unknown/default quality
}

QString WatchSessionManager::getFileQuality(int lid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return QString();
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT f.quality FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    
    return QString();
}

QString WatchSessionManager::getFileAudioLanguage(int lid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG(QString("[WatchSessionManager] getFileAudioLanguage: lid=%1 db not open").arg(lid));
        return QString();
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT f.lang_dub FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        QString result = q.value(0).toString();
        LOG(QString("[WatchSessionManager] getFileAudioLanguage: lid=%1 result='%2'").arg(lid).arg(result));
        return result;
    }
    
    LOG(QString("[WatchSessionManager] getFileAudioLanguage: lid=%1 query failed or no result: %2")
        .arg(lid).arg(q.lastError().text()));
    return QString();
}

QString WatchSessionManager::getFileSubtitleLanguage(int lid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG(QString("[WatchSessionManager] getFileSubtitleLanguage: lid=%1 db not open").arg(lid));
        return QString();
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT f.lang_sub FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        QString result = q.value(0).toString();
        LOG(QString("[WatchSessionManager] getFileSubtitleLanguage: lid=%1 result='%2'").arg(lid).arg(result));
        return result;
    }
    
    LOG(QString("[WatchSessionManager] getFileSubtitleLanguage: lid=%1 query failed or no result: %2")
        .arg(lid).arg(q.lastError().text()));
    return QString();
}

int WatchSessionManager::getFileRating(int lid) const
{
    // Get anime rating for this file (0-1000 scale, where 800+ is excellent)
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return RATING_HIGH_THRESHOLD;  // Treat as high rating if DB unavailable
    }
    
    QSqlQuery q(db);
    // Extract numeric rating from anime.rating field (format: "8.23" -> 823)
    q.prepare("SELECT a.rating FROM mylist m JOIN anime a ON m.aid = a.aid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        QString ratingStr = q.value(0).toString();
        if (!ratingStr.isEmpty()) {
            // Convert "8.23" to 823 using qRound for predictable rounding
            double rating = ratingStr.toDouble() * 100.0;
            int ratingValue = qRound(rating);
            // Treat zero or invalid rating as high rating to preserve content
            // Zero occurs when: (1) rating field is explicitly "0" or "0.00",
            // (2) toDouble() returns 0.0 (e.g., for non-numeric strings)
            // In both cases, assume the anime is worth keeping (optimistic approach)
            return (ratingValue == 0) ? RATING_HIGH_THRESHOLD : ratingValue;
        }
    }
    
    return RATING_HIGH_THRESHOLD;  // No rating available - treat as high rating
}

int WatchSessionManager::getFileGroupId(int lid) const
{
    // Get group ID for this file
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 0;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT f.gid FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    
    return 0;  // No group ID available
}

int WatchSessionManager::getGroupStatus(int gid) const
{
    // Get group status from database
    // Status values: 0=unknown, 1=ongoing, 2=stalled, 3=disbanded
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 0;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT status FROM `group` WHERE gid = ?");
    q.addBindValue(gid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    
    return 0;  // Unknown status
}

int WatchSessionManager::getFileBitrate(int lid) const
{
    // Get video bitrate for this file (in Kbps)
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 0;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT f.bitrate_video FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    
    return 0;
}

QString WatchSessionManager::getFileResolution(int lid) const
{
    // Get resolution for this file
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return QString();
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT f.resolution FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    
    return QString();
}

QString WatchSessionManager::getFileCodec(int lid) const
{
    // Get video codec for this file
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return QString();
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT f.codec_video FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    
    return QString();
}

double WatchSessionManager::getCodecEfficiency(const QString& codec) const
{
    QString codecLower = codec.toLower().trimmed();
    
    // H.265/HEVC family - 50% of H.264 bitrate for same quality
    if (codecLower.contains("hevc") || codecLower.contains("h265") || 
        codecLower.contains("h.265") || codecLower.contains("x265")) {
        return 0.5;
    }
    
    // AV1 family - 35% of H.264 bitrate for same quality
    if (codecLower.contains("av1") || codecLower.contains("av01")) {
        return 0.35;
    }
    
    // VP9 - 60% of H.264 bitrate for same quality
    if (codecLower.contains("vp9") || codecLower.contains("vp09")) {
        return 0.6;
    }
    
    // H.264/AVC family (baseline) - 100% reference bitrate
    if (codecLower.contains("avc") || codecLower.contains("h264") || 
        codecLower.contains("h.264") || codecLower.contains("x264")) {
        return 1.0;
    }
    
    // Older/inefficient codecs - 150% of H.264 bitrate needed
    if (codecLower.contains("xvid") || codecLower.contains("divx") || 
        codecLower.contains("mpeg4") || codecLower.contains("h263")) {
        return 1.5;
    }
    
    // Very old codecs - 200% of H.264 bitrate needed
    if (codecLower.contains("mpeg2") || codecLower.contains("mpeg-2")) {
        return 2.0;
    }
    
    // Unknown codec: assume H.264 efficiency
    return 1.0;
}

double WatchSessionManager::calculateExpectedBitrate(const QString& resolution, const QString& codec) const
{
    // Get baseline bitrate from settings (in Mbps, default 3.5 for H.264 at 1080p)
    double baselineBitrate = 3.5;
    
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("SELECT value FROM settings WHERE name = 'preferredBitrate'");
        if (q.exec() && q.next()) {
            baselineBitrate = q.value(0).toDouble();
            if (baselineBitrate <= 0) {
                baselineBitrate = 3.5;  // Fallback to default
            }
        }
    }
    
    // Parse resolution to get megapixels
    double megapixels = 0.0;
    QString resLower = resolution.toLower();
    
    // Common named resolutions
    if (resLower.contains("480p") || resLower.contains("480")) {
        megapixels = 0.41;  // 854×480
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
        static const QRegularExpression widthHeightRegex(R"((\d+)\s*[x×]\s*(\d+))");
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
    double resolutionScaled = baselineBitrate * (megapixels / 2.07);
    
    // Apply codec efficiency multiplier
    double codecEfficiency = getCodecEfficiency(codec);
    double expectedBitrate = resolutionScaled * codecEfficiency;
    
    return expectedBitrate;
}

bool WatchSessionManager::isLastFileForEpisode(int lid) const
{
    // Check if this is the only remaining file for its episode
    int fileCount = getFileCountForEpisode(lid);
    return fileCount == 1;
}

int WatchSessionManager::getEpisodeIdForFile(int lid) const
{
    // Get a unique episode identifier (aid + episode number)
    // Used for gap tracking across deletions
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 0;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT m.aid, e.epno FROM mylist m "
              "JOIN episode e ON m.eid = e.eid "
              "WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        int aid = q.value(0).toInt();
        QString epnoStr = q.value(1).toString();
        
        // Parse episode number from epno string (format: "1", "2", "S1", etc.)
        // Extract just the numeric part using shared static regex
        QRegularExpressionMatch match = s_epnoNumericRegex.match(epnoStr);
        if (match.hasMatch()) {
            int epno = match.captured(0).toInt();
            // Create a unique ID combining aid and episode number
            return aid * EPISODE_ID_MULTIPLIER + epno;
        }
    }
    
    return 0;
}

bool WatchSessionManager::wouldCreateGap(int lid, const QSet<int>& deletedEpisodes) const
{
    // Get anime ID and episode number for this file
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return false;  // Can't determine, assume no gap
    }
    
    // Note: We also retrieve m.eid here (in addition to aid and epno) to use later
    // for checking if multiple files exist for the same episode
    QSqlQuery q(db);
    q.prepare("SELECT m.aid, e.epno, m.eid FROM mylist m "
              "JOIN episode e ON m.eid = e.eid "
              "WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (!q.exec() || !q.next()) {
        return false;  // Can't determine, assume no gap
    }
    
    int aid = q.value(0).toInt();
    QString epnoStr = q.value(1).toString();
    int eid = q.value(2).toInt();
    
    // Parse episode number from epno string using shared static regex
    QRegularExpressionMatch match = s_epnoNumericRegex.match(epnoStr);
    if (!match.hasMatch()) {
        // Be conservative: if we can't determine episode continuity, avoid deleting.
        return true;
    }
    int epno = match.captured(0).toInt();
    
    // IMPORTANT: Check if there are other files for this same episode
    // If there are other files, deleting this one won't remove the episode entirely,
    // so it cannot create a gap
    QSqlQuery fileCountQuery(db);
    fileCountQuery.prepare(
        "SELECT m.lid, lf.path FROM mylist m "
        "JOIN local_files lf ON m.local_file = lf.id "
        "WHERE m.eid = ? AND m.lid != ? AND lf.path IS NOT NULL AND lf.path != ''"
    );
    fileCountQuery.addBindValue(eid);
    fileCountQuery.addBindValue(lid);
    
    if (fileCountQuery.exec()) {
        while (fileCountQuery.next()) {
            QFileInfo otherFile(fileCountQuery.value(1).toString());
            if (otherFile.exists() && otherFile.isFile()) {
                // Another real file exists for this episode, so deleting this file
                // won't remove the episode and thus cannot create a gap
                return false;
            }
        }
    }
    
    // Check if this file, when deleted, would leave episodes on both sides
    // creating a gap in the series
    
    // Create episode ID for this episode
    int thisEpisodeId = aid * EPISODE_ID_MULTIPLIER + epno;
    
    // Check if this episode is already marked as deleted
    if (deletedEpisodes.contains(thisEpisodeId)) {
        return false;  // Already being deleted, not creating new gap
    }
    
    // Query for all episodes of this anime that have local files
    // Note: We don't use ORDER BY e.epno here because:
    // 1. It would do string sorting which is incorrect for multi-digit episodes ("10" < "2")
    // 2. We extract episode numbers numerically and process them in code anyway
    // 3. This avoids the performance cost of sorting when we don't need it
    QSqlQuery q2(db);
    q2.prepare("SELECT DISTINCT e.epno FROM mylist m "
               "JOIN episode e ON m.eid = e.eid "
               "JOIN local_files lf ON m.local_file = lf.id "
               "WHERE m.aid = ? AND lf.path IS NOT NULL AND lf.path != ''");
    q2.addBindValue(aid);
    
    if (!q2.exec()) {
        return false;  // Query failed, assume no gap
    }
    
    // Build list of existing episode numbers (excluding those marked for deletion)
    QList<int> existingEpisodes;
    while (q2.next()) {
        QString existingEpnoStr = q2.value(0).toString();
        QRegularExpressionMatch existingMatch = s_epnoNumericRegex.match(existingEpnoStr);
        if (existingMatch.hasMatch()) {
            int existingEpno = existingMatch.captured(0).toInt();
            int existingEpisodeId = aid * EPISODE_ID_MULTIPLIER + existingEpno;
            
            // Skip episodes already marked for deletion
            if (deletedEpisodes.contains(existingEpisodeId)) {
                continue;
            }
            
            existingEpisodes.append(existingEpno);
        }
    }
    
    // If this is the only episode, deleting it won't create a gap
    if (existingEpisodes.size() <= 1) {
        return false;
    }
    
    // Check if there are episodes both before and after this one
    bool hasEpisodeBefore = false;
    bool hasEpisodeAfter = false;
    
    for (int existingEpno : existingEpisodes) {
        if (existingEpno == epno) {
            continue;  // Skip this episode itself
        }
        
        if (existingEpno < epno) {
            hasEpisodeBefore = true;
        }
        if (existingEpno > epno) {
            hasEpisodeAfter = true;
        }
    }
    
    // If there are episodes on both sides, deleting this would create a gap
    return hasEpisodeBefore && hasEpisodeAfter;
}
