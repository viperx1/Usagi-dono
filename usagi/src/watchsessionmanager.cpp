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

int WatchSessionManager::calculateDeletionScore(int lid) const
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
    
    // Check quality (if preference is enabled)
    // Quality is the file.quality field from AniDB (e.g., "very high", "high", "medium", "low")
    // Reuse existing database connection instead of creating a new one
    if (db.isOpen()) {
        QSqlQuery q2(db);
        q2.prepare("SELECT value FROM settings WHERE name = 'preferHighestQuality'");
        if (q2.exec() && q2.next() && q2.value(0).toString() == "1") {
            QString quality = getFileQuality(lid);
            int qualityScore = getQualityScore(quality);
            
            // Compare with thresholds - files above threshold get bonus, below get penalty
            if (qualityScore >= QUALITY_HIGH_THRESHOLD) {
                score += SCORE_HIGHER_QUALITY;  // High quality
            } else if (qualityScore < QUALITY_LOW_THRESHOLD) {
                score += SCORE_LOWER_QUALITY;  // Low quality
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
    
    // Check group status
    int gid = getFileGroupId(lid);
    if (gid > 0) {
        int groupStatus = getGroupStatus(gid);
        if (groupStatus == 1) {  // Ongoing
            score += SCORE_ACTIVE_GROUP;  // Active groups are preferred
        } else if (groupStatus == 2) {  // Stalled
            score += SCORE_STALLED_GROUP;  // Stalled groups get penalty
        } else if (groupStatus == 3) {  // Disbanded
            score += SCORE_DISBANDED_GROUP;  // Disbanded groups get larger penalty
        }
    }
    
    // Check bitrate distance from expected (only applies when multiple files exist)
    int bitrate = getFileBitrate(lid);
    QString resolution = getFileResolution(lid);
    QString codec = getFileCodec(lid);
    if (bitrate > 0 && !resolution.isEmpty() && fileCount > 1) {
        double bitrateScore = calculateBitrateScore(bitrate, resolution, codec, fileCount);
        score += static_cast<int>(bitrateScore);
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
            if (m_sessions.contains(aid) && m_sessions[aid].isEpisodeWatched(episodeNumber)) {
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
        return m_fileMarks[lid].markType();
    }
    return FileMarkType::None;
}

void WatchSessionManager::setFileMarkType(int lid, FileMarkType markType)
{
    FileMarkInfo& info = m_fileMarks[lid];
    info.setLid(lid);
    info.setMarkType(markType);
    info.setMarkScore(calculateDeletionScore(lid));
    
    emit fileMarkChanged(lid, markType);
}

FileMarkInfo WatchSessionManager::getFileMarkInfo(int lid) const
{
    if (m_fileMarks.contains(lid)) {
        FileMarkInfo info = m_fileMarks[lid];
        info.setMarkScore(calculateDeletionScore(lid));
        return info;
    }
    
    FileMarkInfo info;
    info.setLid(lid);
    info.setAid(getAnimeIdForFile(lid));
    info.setMarkScore(calculateDeletionScore(lid));
    info.setIsInActiveSession(info.aid() > 0 && hasActiveSession(info.aid()));
    
    return info;
}

QList<int> WatchSessionManager::getFilesForDeletion() const
{
    QList<QPair<int, int>> scoredFiles; // (lid, score)
    
    for (auto it = m_fileMarks.constBegin(); it != m_fileMarks.constEnd(); ++it) {
        if (it.value().markType() == FileMarkType::ForDeletion) {
            scoredFiles.append(qMakePair(it.key(), calculateDeletionScore(it.key())));
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
        if (it.value().markType() == FileMarkType::ForDownload) {
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
    
    // Remove from file marks if present
    m_fileMarks.remove(lid);
    
    // Emit signal to request file status cleanup (Window will handle it with API access)
    // deleteFromDisk=false because the file is already gone from disk
    // This will update local database and AniDB API to reflect the file's absence
    emit deleteFileRequested(lid, false);
    
    // Emit fileMarkChanged immediately since we've removed from marks
    emit fileMarkChanged(lid, FileMarkType::None);
    
    LOG(QString("[WatchSessionManager] File status cleanup requested for lid=%1, aid=%2").arg(lid).arg(aid));
}

bool WatchSessionManager::deleteNextMarkedFile(bool deleteFromDisk)
{
    QList<int> filesToDelete = getFilesForDeletion();
    
    if (filesToDelete.isEmpty()) {
        LOG("[WatchSessionManager] deleteNextMarkedFile: No files marked for deletion");
        return false;
    }
    
    // Find the first file that can be safely deleted (doesn't create a gap)
    // Process files in score order (lowest score = highest deletion priority)
    // Pass empty set because we evaluate gaps against current database state,
    // not against a hypothetical batch deletion state
    QSet<int> emptyDeletedSet;
    
    for (int lid : filesToDelete) {
        // Check if deleting this file would create a gap
        // Pass empty set since we're checking current state, not batch state
        if (wouldCreateGap(lid, emptyDeletedSet)) {
            LOG(QString("[WatchSessionManager] deleteNextMarkedFile: Skipping lid=%1 - would create gap in series")
                .arg(lid));
            continue;
        }
        
        // Found a safe file to delete
        LOG(QString("[WatchSessionManager] deleteNextMarkedFile: Deleting lid=%1 (1 of %2 marked files)")
            .arg(lid).arg(filesToDelete.size()));
        
        // Delete the file (this will trigger deleteFileRequested signal)
        deleteFile(lid, deleteFromDisk);
        return true;
    }
    
    // No files can be safely deleted without creating gaps
    LOG(QString("[WatchSessionManager] deleteNextMarkedFile: Cannot delete any files - all %1 marked files would create gaps")
        .arg(filesToDelete.size()));
    return false;
}

void WatchSessionManager::onFileDeletionResult(int lid, int aid, bool success)
{
    if (success) {
        LOG(QString("[WatchSessionManager] File deletion succeeded for lid=%1, aid=%2").arg(lid).arg(aid));
        emit fileDeleted(lid, aid);
    } else {
        LOG(QString("[WatchSessionManager] File deletion failed for lid=%1, aid=%2 - attempting next file").arg(lid).arg(aid));
        
        // If deletion failed (e.g., file locked by another application),
        // unmark this file and try the next one automatically
        if (m_fileMarks.contains(lid)) {
            m_fileMarks[lid].setMarkType(FileMarkType::None);
            emit fileMarkChanged(lid, FileMarkType::None);
        }
        
        // Try the next file if auto-deletion is enabled
        if (m_enableActualDeletion) {
            bool deletedNext = deleteNextMarkedFile(true);
            if (!deletedNext) {
                LOG("[WatchSessionManager] No more files available for deletion after failure");
            }
        }
    }
    
    // Note: On success, we do NOT automatically process the next file.
    // The caller must explicitly call deleteNextMarkedFile() again after receiving
    // API confirmation to ensure proper sequencing and gap protection.
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
            if (it.value().markType() == FileMarkType::ForDeletion) {
                it.value().setMarkType(FileMarkType::None);
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
        if (it.value().markType() == FileMarkType::ForDeletion) {
            it.value().setMarkType(FileMarkType::None);
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
        
        // Verify the file actually exists on disk before considering it for deletion
        // This prevents marking stale database entries where files have been deleted externally
        QFileInfo fileInfo(info.path);
        if (!fileInfo.exists()) {
            LOG(QString("[WatchSessionManager] File marked in database but missing on disk: %1 (lid=%2) - cleaning up status")
                .arg(info.path).arg(info.lid));
            // Clean up the database status for this missing file (both locally and in API)
            cleanupMissingFileStatus(info.lid);
            continue;
        }
        
        // Verify it's actually a file (not a directory)
        if (!fileInfo.isFile()) {
            LOG(QString("[WatchSessionManager] Non-file entry found: %1 (lid=%2) - cleaning up status")
                .arg(info.path).arg(info.lid));
            // Clean up the database status for this invalid entry
            cleanupMissingFileStatus(info.lid);
            continue;
        }
        
        info.score = calculateDeletionScore(info.lid);
        candidates.append(info);
    }
    
    // Sort by score (lowest first - most eligible for deletion)
    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateInfo& a, const CandidateInfo& b) {
                  return a.score < b.score;
              });
    
    // Mark files for deletion until we've accumulated enough space to free
    // Using "Approach B: Gap as Condition" with "Restart After Each Deletion"
    // This ensures we always process the lowest-scored available file and maintain score ordering
    qint64 accumulatedSpace = 0;
    QSet<int> deletedEpisodeIds;  // Track fully deleted episodes for gap detection
    
    size_t index = 0;
    while (accumulatedSpace < spaceToFreeBytes && index < candidates.size()) {
        const auto& candidate = candidates[index];
        
        // CONSTRAINT 1: Gap prevention - skip files that would create gaps
        if (wouldCreateGap(candidate.lid, deletedEpisodeIds)) {
            LOG(QString("[WatchSessionManager] Skipping lid=%1 (score=%2, anime=%3): would create gap in series")
                .arg(candidate.lid).arg(candidate.score).arg(candidate.animeName));
            index++;
            continue;
        }
        
        // CONSTRAINT 2: Last file protection - additional check for last file of episode
        if (isLastFileForEpisode(candidate.lid)) {
            // This is the last file for its episode
            // Double-check gap condition with current deletion state
            if (wouldCreateGap(candidate.lid, deletedEpisodeIds)) {
                LOG(QString("[WatchSessionManager] Skipping lid=%1 (score=%2, anime=%3): last file that would create gap")
                    .arg(candidate.lid).arg(candidate.score).arg(candidate.animeName));
                index++;
                continue;
            }
        }
        
        // Mark this file for deletion
        FileMarkInfo& markInfo = m_fileMarks[candidate.lid];
        markInfo.setLid(candidate.lid);
        markInfo.setMarkType(FileMarkType::ForDeletion);
        markInfo.setMarkScore(candidate.score);
        
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
        
        // Track which episodes are fully deleted for gap detection
        if (isLastFileForEpisode(candidate.lid)) {
            int episodeId = getEpisodeIdForFile(candidate.lid);
            if (episodeId > 0) {
                deletedEpisodeIds.insert(episodeId);
            }
        }
        
        // CRITICAL: Restart from beginning after each deletion
        // This ensures we always check newly-available endpoints first
        // and strictly maintain score ordering
        index = 0;
    }
    
    // Log if quota not met due to series continuity constraints
    if (accumulatedSpace < spaceToFreeBytes) {
        LOG(QString("[WatchSessionManager] Could not meet space quota (%1 MB freed of %2 MB needed) - "
                    "remaining files protected by series continuity")
            .arg(accumulatedSpace / 1024.0 / 1024.0, 0, 'f', 2)
            .arg(spaceToFreeBytes / 1024.0 / 1024.0, 0, 'f', 2));
    }
    
    if (!updatedLids.isEmpty()) {
        emit markingsUpdated(updatedLids);
        
        // Actually delete the next file with highest priority (lowest score) if deletion is enabled
        // Note: Only one file is deleted at a time. The caller must wait for
        // API confirmation before calling deleteNextMarkedFile() again.
        if (m_enableActualDeletion) {
            deleteNextMarkedFile(true);
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
        
        if (!session.isActive()) {
            continue;
        }
        
        // Use global ahead buffer setting (applies to all anime)
        int aheadBuffer = m_aheadBuffer;
        
        // Find files for episodes in the ahead buffer
        for (int ep = session.currentEpisode(); ep <= session.currentEpisode() + aheadBuffer; ep++) {
            // Query for files that match this episode and don't have a local file
            QSqlQuery q(db);
            q.prepare("SELECT m.lid FROM mylist m "
                      "JOIN episode e ON m.eid = e.eid "
                      "LEFT JOIN local_files lf ON m.lid = lf.lid "
                      "WHERE m.aid = ? AND e.epno LIKE ? "
                      "AND (lf.local_path IS NULL OR lf.local_path = '')");
            q.addBindValue(session.aid());
            q.addBindValue(QString::number(ep) + "%"); // Match episode number
            
            if (q.exec()) {
                while (q.next()) {
                    int lid = q.value(0).toInt();
                    
                    // Only mark if not already marked for something else
                    if (!m_fileMarks.contains(lid) || 
                        m_fileMarks[lid].markType() == FileMarkType::None) {
                        FileMarkInfo& info = m_fileMarks[lid];
                        info.setLid(lid);
                        info.setMarkType(FileMarkType::ForDownload);
                        info.setMarkScore(calculateDeletionScore(lid));
                        
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
    // Use word boundary matching to avoid false positives (e.g., 'eng' in 'bengali')
    QString normalizedAudioLang = audioLang.toLower().trimmed();
    for (const QString& lang : langList) {
        QString trimmedLang = lang.trimmed();
        // Check for exact match or word boundary match
        if (normalizedAudioLang == trimmedLang || 
            normalizedAudioLang.startsWith(trimmedLang + " ") ||
            normalizedAudioLang.endsWith(" " + trimmedLang) ||
            normalizedAudioLang.contains(" " + trimmedLang + " ")) {
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
    // Use word boundary matching to avoid false positives (e.g., 'eng' in 'bengali')
    QString normalizedSubLang = subLang.toLower().trimmed();
    for (const QString& lang : langList) {
        QString trimmedLang = lang.trimmed();
        // Check for exact match or word boundary match
        if (normalizedSubLang == trimmedLang || 
            normalizedSubLang.startsWith(trimmedLang + " ") ||
            normalizedSubLang.endsWith(" " + trimmedLang) ||
            normalizedSubLang.contains(" " + trimmedLang + " ")) {
            return true;
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

double WatchSessionManager::calculateBitrateScore(double actualBitrate, const QString& resolution, const QString& codec, int fileCount) const
{
    // Only apply bitrate penalty when there are multiple files
    if (fileCount <= 1) {
        return 0.0;
    }
    
    // Convert actualBitrate from Kbps to Mbps
    double actualBitrateMbps = actualBitrate / 1000.0;
    
    // Calculate expected bitrate based on resolution and codec
    double expectedBitrate = calculateExpectedBitrate(resolution, codec);
    
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
        penalty = SCORE_BITRATE_FAR;
    } else if (percentDiff > 30.0) {
        penalty = SCORE_BITRATE_MODERATE;
    } else if (percentDiff > 10.0) {
        penalty = SCORE_BITRATE_CLOSE;
    }
    
    // Add codec tier bonus/penalty
    QString codecLower = codec.toLower().trimmed();
    
    // Tier 1 (Excellent): AV1, H.265/HEVC
    if (codecLower.contains("hevc") || codecLower.contains("h265") || 
        codecLower.contains("h.265") || codecLower.contains("x265") ||
        codecLower.contains("av1") || codecLower.contains("av01")) {
        penalty += SCORE_MODERN_CODEC;  // +10 bonus
    }
    // Tier 3 (Acceptable): MPEG-4, XviD, DivX
    else if (codecLower.contains("xvid") || codecLower.contains("divx") || 
             codecLower.contains("mpeg4")) {
        penalty += SCORE_OLD_CODEC;  // -15 penalty
    }
    // Tier 4 (Poor): MPEG-2, H.263
    else if (codecLower.contains("mpeg2") || codecLower.contains("mpeg-2") ||
             codecLower.contains("h263") || codecLower.contains("h.263")) {
        penalty += SCORE_ANCIENT_CODEC;  // -30 penalty
    }
    // Tier 2 (Good): H.264/AVC, VP9 - no bonus/penalty (neutral)
    
    return penalty;
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
        return false;  // Can't parse episode number
    }
    int epno = match.captured(0).toInt();
    
    // IMPORTANT: Check if there are other files for this same episode
    // If there are other files, deleting this one won't remove the episode entirely,
    // so it cannot create a gap
    QSqlQuery fileCountQuery(db);
    fileCountQuery.prepare(
        "SELECT COUNT(*) FROM mylist m "
        "JOIN local_files lf ON m.local_file = lf.id "
        "WHERE m.eid = ? AND lf.path IS NOT NULL AND lf.path != ''"
    );
    fileCountQuery.addBindValue(eid);
    
    if (fileCountQuery.exec() && fileCountQuery.next()) {
        int fileCount = fileCountQuery.value(0).toInt();
        if (fileCount > 1) {
            // There are other files for this episode, so deleting this file
            // won't remove the episode and thus cannot create a gap
            return false;
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
