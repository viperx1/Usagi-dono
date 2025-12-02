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
        session.aid = q.value(0).toInt();
        session.startAid = q.value(1).toInt();
        session.currentEpisode = q.value(2).toInt();
        session.isActive = q.value(3).toInt() != 0;
        
        m_sessions[session.aid] = session;
    }
    
    // Load watched episodes for each session
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        QSqlQuery epQuery(db);
        epQuery.prepare("SELECT episode_number FROM session_watched_episodes WHERE aid = ?");
        epQuery.addBindValue(it.key());
        if (epQuery.exec()) {
            while (epQuery.next()) {
                it.value().watchedEpisodes.insert(epQuery.value(0).toInt());
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
    session.aid = aid;
    session.startAid = originalAid;
    session.currentEpisode = startEpisode > 0 ? startEpisode : 1;
    session.isActive = true;
    
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
        m_sessions[aid].isActive = false;
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

QList<int> WatchSessionManager::getSeriesChain(int aid) const
{
    // Check series chain cache first
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
    
    // Apply file revision penalty - older revisions are more deletable
    // Only applies when there are multiple local files for the same episode
    int fileCount = getFileCountForEpisode(lid);
    if (fileCount > 1) {
        // Count how many local files for this episode have a higher version
        int higherVersionCount = getHigherVersionFileCount(lid);
        
        // Apply penalty per local file with higher version
        if (higherVersionCount > 0) {
            score += higherVersionCount * SCORE_OLDER_REVISION;  // Negative, more eligible for deletion
        }
    }
    
    // Check language preferences (if settings are enabled)
    bool hasAudioMatch = matchesPreferredAudioLanguage(lid);
    bool hasSubtitleMatch = matchesPreferredSubtitleLanguage(lid);
    
    if (hasAudioMatch) {
        score += SCORE_PREFERRED_AUDIO;  // Bonus for matching preferred audio
    } else {
        // Only apply penalty if we have audio language info but it doesn't match
        QString audioLang = getFileAudioLanguage(lid);
        if (!audioLang.isEmpty()) {
            score += SCORE_NOT_PREFERRED_AUDIO;  // Penalty for not matching
        }
    }
    
    if (hasSubtitleMatch) {
        score += SCORE_PREFERRED_SUBTITLE;  // Bonus for matching preferred subtitles
    } else {
        // Only apply penalty if we have subtitle language info but it doesn't match
        QString subLang = getFileSubtitleLanguage(lid);
        if (!subLang.isEmpty()) {
            score += SCORE_NOT_PREFERRED_SUBTITLE;  // Penalty for not matching
        }
    }
    
    // Check quality and resolution (if preference is enabled)
    // Reuse existing database connection instead of creating a new one
    if (db.isOpen()) {
        QSqlQuery q2(db);
        q2.prepare("SELECT value FROM settings WHERE name = 'preferHighestQuality'");
        if (q2.exec() && q2.next() && q2.value(0).toString() == "1") {
            QString quality = getFileQuality(lid);
            QString resolution = getFileResolution(lid);
            
            int qualityScore = getQualityScore(quality);
            int resolutionScore = getResolutionScore(resolution);
            
            // Compare with thresholds - files above threshold get bonus, below get penalty
            if (qualityScore >= QUALITY_HIGH_THRESHOLD && resolutionScore >= RESOLUTION_HIGH_THRESHOLD) {
                score += SCORE_HIGHER_QUALITY;  // High quality/resolution
            } else if (qualityScore < QUALITY_LOW_THRESHOLD || resolutionScore < RESOLUTION_LOW_THRESHOLD) {
                score += SCORE_LOWER_QUALITY;  // Low quality/resolution
            }
        }
    }
    
    // Check anime rating
    int rating = getFileRating(lid);
    if (rating >= RATING_HIGH_THRESHOLD) {
        score += SCORE_HIGH_RATING;  // Highly rated anime - keep longer
    } else if (rating > 0 && rating < RATING_LOW_THRESHOLD) {
        score += SCORE_LOW_RATING;  // Poorly rated anime - more eligible for deletion
    }
    
    // Calculate distance-based scoring across the series chain
    // Distance is the primary factor - episodes far from current position are more deletable
    if (aid > 0) {
        // Find active session across the series chain
        // Returns (sessionAid, episodeOffsetForRequestedAnime, sessionEpisodeOffset)
        auto sessionInfo = findActiveSessionInSeriesChain(aid);
        int sessionAid = std::get<0>(sessionInfo);
        int episodeOffset = std::get<1>(sessionInfo);  // Cumulative episode offset for this anime in the chain
        int sessionOffset = std::get<2>(sessionInfo);  // Cumulative episode offset for session anime
        
        if (sessionAid > 0) {
            // Active session is a factor (not the primary reason for scoring)
            score += SCORE_ACTIVE_SESSION;
            
            int currentEp = getCurrentSessionEpisode(sessionAid);
            
            // Calculate total episode position in the series chain
            // episodeOffset is the sum of all episodes in prequels before this anime
            int totalEpisodePosition = episodeOffset + episodeNumber;
            int currentTotalPosition = sessionOffset + currentEp;
            
            int distance = totalEpisodePosition - currentTotalPosition;
            
            // Use global ahead buffer (applies to all anime)
            int aheadBuffer = m_aheadBuffer;
            
            // Episodes in the ahead buffer get bonus protection
            if (distance >= 0 && distance <= aheadBuffer) {
                score += SCORE_IN_AHEAD_BUFFER;
            }
            
            // Distance penalty/bonus - episodes far from current are more deletable
            // This is the primary scoring factor
            score += distance * SCORE_DISTANCE_FACTOR;
            
            // Already watched in session gets penalty (more deletable)
            if (m_sessions.contains(aid) && m_sessions[aid].watchedEpisodes.contains(episodeNumber)) {
                score += SCORE_ALREADY_WATCHED;
            }
        }
    }
    
    return score;
}

std::tuple<int, int, int> WatchSessionManager::findActiveSessionInSeriesChain(int aid) const
{
    // Get the series chain starting from the original prequel
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
    
    // Remove from file marks
    m_fileMarks.remove(lid);
    
    // Emit signal to request file deletion (Window will handle it with API access)
    // Window will call onFileDeletionResult() when the operation completes
    emit deleteFileRequested(lid, deleteFromDisk);
    
    // Emit fileMarkChanged immediately since we've removed from marks
    emit fileMarkChanged(lid, FileMarkType::None);
    
    LOG(QString("[WatchSessionManager] File deletion requested for lid=%1, aid=%2").arg(lid).arg(aid));
    return true;
}

void WatchSessionManager::deleteMarkedFiles(bool deleteFromDisk)
{
    QList<int> filesToDelete = getFilesForDeletion();
    
    LOG(QString("[WatchSessionManager] deleteMarkedFiles: %1 files to delete").arg(filesToDelete.size()));
    
    for (int lid : filesToDelete) {
        deleteFile(lid, deleteFromDisk);
    }
}

void WatchSessionManager::onFileDeletionResult(int lid, int aid, bool success)
{
    if (success) {
        LOG(QString("[WatchSessionManager] File deletion succeeded for lid=%1, aid=%2").arg(lid).arg(aid));
        emit fileDeleted(lid, aid);
    } else {
        LOG(QString("[WatchSessionManager] File deletion failed for lid=%1, aid=%2 - not emitting fileDeleted").arg(lid).arg(aid));
    }
}

void WatchSessionManager::autoMarkFilesForDeletion()
{
    if (!m_autoMarkDeletionEnabled) {
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
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
    
    // Track which lids were updated
    QSet<int> updatedLids;
    
    // If we're above threshold, clear deletion marks
    if (availableGB >= threshold) {
        for (auto it = m_fileMarks.begin(); it != m_fileMarks.end(); ++it) {
            if (it.value().markType == FileMarkType::ForDeletion) {
                it.value().markType = FileMarkType::None;
                updatedLids.insert(it.key());
                emit fileMarkChanged(it.key(), FileMarkType::None);
            }
        }
        if (!updatedLids.isEmpty()) {
            emit markingsUpdated(updatedLids);
        }
        return;
    }
    
    // Calculate how many bytes we need to free
    qint64 spaceToFreeBytes = static_cast<qint64>((threshold - availableGB) * 1024.0 * 1024.0 * 1024.0);
    
    // First, clear all existing deletion marks before calculating new ones
    for (auto it = m_fileMarks.begin(); it != m_fileMarks.end(); ++it) {
        if (it.value().markType == FileMarkType::ForDeletion) {
            it.value().markType = FileMarkType::None;
            emit fileMarkChanged(it.key(), FileMarkType::None);
        }
    }
    
    // Get mylist entries with local paths, file sizes, and anime names
    // Use COALESCE with multiple fallbacks for anime name
    QSqlQuery q(db);
    bool querySuccess = q.exec(
        "SELECT m.lid, lf.path, COALESCE(f.size, 0) as file_size, "
        "COALESCE(NULLIF(a.nameromaji, ''), NULLIF(a.nameenglish, ''), NULLIF(a.namekanji, ''), '') as anime_name FROM mylist m "
        "JOIN local_files lf ON m.local_file = lf.id "
        "LEFT JOIN file f ON m.fid = f.fid "
        "LEFT JOIN anime a ON m.aid = a.aid "
        "WHERE lf.path IS NOT NULL AND lf.path != ''");
    
    if (!querySuccess) {
        return;
    }
    
    // Structure to hold candidate info: lid, score, size, path, anime_name
    struct CandidateInfo {
        int lid;
        int score;
        qint64 size;
        QString path;
        QString animeName;
    };
    
    QList<CandidateInfo> candidates;
    while (q.next()) {
        CandidateInfo info;
        info.lid = q.value(0).toInt();
        info.path = q.value(1).toString();
        info.size = q.value(2).toLongLong();
        info.animeName = q.value(3).toString();
        info.score = calculateMarkScore(info.lid);
        candidates.append(info);
    }
    
    // Sort by score (lowest first - most eligible for deletion)
    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateInfo& a, const CandidateInfo& b) {
                  return a.score < b.score;
              });
    
    // Mark files for deletion until we've accumulated enough space to free
    qint64 accumulatedSpace = 0;
    
    for (const auto& candidate : candidates) {
        // Stop if we've already marked enough files to free the required space
        if (accumulatedSpace >= spaceToFreeBytes) {
            break;
        }
        
        FileMarkInfo& markInfo = m_fileMarks[candidate.lid];
        markInfo.lid = candidate.lid;
        markInfo.markType = FileMarkType::ForDeletion;
        markInfo.markScore = candidate.score;
        
        accumulatedSpace += candidate.size;
        
        // Extract file name from path
        QString fileName = QFileInfo(candidate.path).fileName();
        
        LOG(QString("[WatchSessionManager] Marked file for deletion: lid=%1, score=%2, size=%3 bytes, accumulated=%4 bytes, anime=%5, file=%6")
            .arg(candidate.lid)
            .arg(candidate.score)
            .arg(candidate.size)
            .arg(accumulatedSpace)
            .arg(candidate.animeName, fileName));
        
        updatedLids.insert(candidate.lid);
        emit fileMarkChanged(candidate.lid, FileMarkType::ForDeletion);
    }
    
    if (!updatedLids.isEmpty()) {
        emit markingsUpdated(updatedLids);
        
        // Actually delete the marked files if deletion is enabled
        if (m_enableActualDeletion) {
            deleteMarkedFiles(true);
        }
    }
}

void WatchSessionManager::autoMarkFilesForDownload()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    // Track which lids were updated
    QSet<int> updatedLids;
    
    // For each active session, mark files in the ahead buffer for download
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        const SessionInfo& session = it.value();
        
        if (!session.isActive) {
            continue;
        }
        
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
                        
                        updatedLids.insert(lid);
                        emit fileMarkChanged(lid, FileMarkType::ForDownload);
                    }
                }
            }
        }
    }
    
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
            session.aid = aid;
            
            // Get the original prequel, falling back to current aid if not found
            int startAid = getOriginalPrequel(aid);
            session.startAid = (startAid > 0) ? startAid : aid;
            
            session.currentEpisode = 1;
            session.isActive = true;
            
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
    if (m_autoMarkDeletionEnabled) {
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
    QString audioLang = getFileAudioLanguage(lid);
    if (audioLang.isEmpty()) {
        return false;  // No audio language info
    }
    
    // Get preferred languages from global settings
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT value FROM settings WHERE name = 'preferredAudioLanguages'");
    if (!q.exec() || !q.next()) {
        return false;  // No preference set
    }
    
    QString preferredLangs = q.value(0).toString().toLower();
    QStringList langList = preferredLangs.split(',', Qt::SkipEmptyParts);
    
    // Normalize and check if file's audio language matches any preferred language
    QString normalizedAudioLang = audioLang.toLower().trimmed();
    for (const QString& lang : langList) {
        if (normalizedAudioLang.contains(lang.trimmed())) {
            return true;
        }
    }
    
    return false;
}

bool WatchSessionManager::matchesPreferredSubtitleLanguage(int lid) const
{
    QString subLang = getFileSubtitleLanguage(lid);
    if (subLang.isEmpty()) {
        return false;  // No subtitle language info
    }
    
    // Get preferred languages from global settings
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT value FROM settings WHERE name = 'preferredSubtitleLanguages'");
    if (!q.exec() || !q.next()) {
        return false;  // No preference set
    }
    
    QString preferredLangs = q.value(0).toString().toLower();
    QStringList langList = preferredLangs.split(',', Qt::SkipEmptyParts);
    
    // Normalize and check if file's subtitle language matches any preferred language
    QString normalizedSubLang = subLang.toLower().trimmed();
    for (const QString& lang : langList) {
        if (normalizedSubLang.contains(lang.trimmed())) {
            return true;
        }
    }
    
    return false;
}

int WatchSessionManager::getQualityScore(const QString& quality) const
{
    // Convert quality string to numeric score
    // Higher score = better quality (less likely to delete)
    QString q = quality.toLower();
    
    if (q.contains("blu-ray") || q.contains("bluray")) return 100;
    if (q.contains("dvd")) return 70;
    if (q.contains("hdtv")) return 60;
    if (q.contains("web")) return 50;
    if (q.contains("tv")) return 40;
    if (q.contains("vhs")) return 20;
    
    return 30;  // Unknown/default quality
}

int WatchSessionManager::getResolutionScore(const QString& resolution) const
{
    // Convert resolution string to numeric score
    // Higher score = better resolution (less likely to delete)
    QString res = resolution.toLower();
    
    if (res.contains("3840") || res.contains("2160") || res.contains("4k")) return 100;
    if (res.contains("2560") || res.contains("1440")) return 85;
    if (res.contains("1920") || res.contains("1080")) return 80;
    if (res.contains("1280") || res.contains("720")) return 60;
    if (res.contains("854") || res.contains("480")) return 40;
    if (res.contains("640") || res.contains("360")) return 30;
    if (res.contains("426") || res.contains("240")) return 20;
    
    return 50;  // Unknown/default resolution
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

QString WatchSessionManager::getFileResolution(int lid) const
{
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

QString WatchSessionManager::getFileAudioLanguage(int lid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return QString();
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT f.lang_dub FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    
    return QString();
}

QString WatchSessionManager::getFileSubtitleLanguage(int lid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return QString();
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT f.lang_sub FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    
    return QString();
}

int WatchSessionManager::getFileRating(int lid) const
{
    // Get anime rating for this file (0-1000 scale, where 800+ is excellent)
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 0;
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
            return qRound(rating);
        }
    }
    
    return 0;  // No rating available
}

