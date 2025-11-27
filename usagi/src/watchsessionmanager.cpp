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
}

void WatchSessionManager::loadFromDatabase()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("[WatchSessionManager] Database not open, cannot load sessions");
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
    
    // Load file marks
    q.exec("SELECT lid, mark_type, mark_score FROM file_marks");
    while (q.next()) {
        FileMarkInfo info;
        info.lid = q.value(0).toInt();
        info.markType = static_cast<FileMarkType>(q.value(1).toInt());
        info.markScore = q.value(2).toInt();
        m_fileMarks[info.lid] = info;
    }
    
    LOG(QString("[WatchSessionManager] Loaded %1 sessions and %2 file marks")
        .arg(m_sessions.size()).arg(m_fileMarks.size()));
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
    int score = 0;
    
    int aid = getAnimeIdForFile(lid);
    if (aid <= 0) {
        return score;
    }
    
    int episodeNumber = getEpisodeNumber(lid);
    
    // Check if card is hidden
    if (isCardHidden(aid)) {
        score += SCORE_HIDDEN_CARD;
    }
    
    // Check session status
    if (hasActiveSession(aid)) {
        score += SCORE_ACTIVE_SESSION;
        
        int currentEp = getCurrentSessionEpisode(aid);
        int distance = episodeNumber - currentEp;
        
        // Episodes in the ahead buffer get bonus
        if (distance >= 0 && distance <= m_aheadBuffer) {
            score += SCORE_IN_AHEAD_BUFFER;
        }
        
        // Distance penalty/bonus
        score += distance * SCORE_DISTANCE_FACTOR;
        
        // Already watched in session gets penalty
        if (m_sessions.contains(aid) && m_sessions[aid].watchedEpisodes.contains(episodeNumber)) {
            score += SCORE_ALREADY_WATCHED;
        } else {
            score += SCORE_NOT_WATCHED;
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
    
    LOG(QString("[WatchSessionManager] Set file mark for lid=%1 to type=%2")
        .arg(lid).arg(static_cast<int>(markType)));
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
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    // Get available space on the drive where the application is installed
    QStorageInfo storage(QCoreApplication::applicationDirPath());
    qint64 availableBytes = storage.bytesAvailable();
    qint64 totalBytes = storage.bytesTotal();
    
    double availableGB = availableBytes / (1024.0 * 1024.0 * 1024.0);
    double totalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
    double availablePercent = (totalBytes > 0) ? (100.0 * availableBytes / totalBytes) : 0;
    
    double threshold = 0;
    if (m_thresholdType == DeletionThresholdType::FixedGB) {
        threshold = m_thresholdValue;
    } else {
        threshold = (m_thresholdValue / 100.0) * totalGB;
    }
    
    // If we're above threshold, clear deletion marks
    if (availableGB >= threshold) {
        for (auto it = m_fileMarks.begin(); it != m_fileMarks.end(); ++it) {
            if (it.value().markType == FileMarkType::ForDeletion) {
                it.value().markType = FileMarkType::None;
                emit fileMarkChanged(it.key(), FileMarkType::None);
            }
        }
        return;
    }
    
    LOG(QString("[WatchSessionManager] Low space detected: %1 GB available, threshold: %2 GB")
        .arg(availableGB, 0, 'f', 2).arg(threshold, 0, 'f', 2));
    
    // Get all watched files that could be candidates for deletion
    QSqlQuery q(db);
    q.exec("SELECT m.lid, m.aid FROM mylist m "
           "JOIN local_files lf ON m.lid = lf.lid "
           "WHERE m.local_watched = 1 AND lf.local_path IS NOT NULL AND lf.local_path != ''");
    
    QList<QPair<int, int>> candidates; // (lid, score)
    while (q.next()) {
        int lid = q.value(0).toInt();
        int score = calculateMarkScore(lid);
        candidates.append(qMakePair(lid, score));
    }
    
    // Sort by score (lowest first - most eligible for deletion)
    std::sort(candidates.begin(), candidates.end(),
              [](const QPair<int, int>& a, const QPair<int, int>& b) {
                  return a.second < b.second;
              });
    
    // Mark files for deletion until we would free enough space
    for (const auto& candidate : candidates) {
        FileMarkInfo& info = m_fileMarks[candidate.first];
        info.lid = candidate.first;
        info.markType = FileMarkType::ForDeletion;
        info.markScore = candidate.second;
        
        emit fileMarkChanged(candidate.first, FileMarkType::ForDeletion);
    }
    
    emit markingsUpdated();
}

void WatchSessionManager::autoMarkFilesForDownload()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    // For each active session, mark files in the ahead buffer for download
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        const SessionInfo& session = it.value();
        
        if (!session.isActive) {
            continue;
        }
        
        // Find files for episodes in the ahead buffer
        for (int ep = session.currentEpisode; ep <= session.currentEpisode + m_aheadBuffer; ep++) {
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
                        
                        emit fileMarkChanged(lid, FileMarkType::ForDownload);
                    }
                }
            }
        }
    }
    
    emit markingsUpdated();
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
    
    if (enabled) {
        autoMarkFilesForDeletion();
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
        QRegularExpression regex("(\\d+)");
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
