#include "playbackmanager.h"
#include "watchchunkmanager.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QFileInfo>

PlaybackManager::PlaybackManager(QObject *parent)
    : QObject(parent)
    , m_tracking(false)
    , m_currentLid(0)
    , m_lastPosition(0)
    , m_lastDuration(0)
    , m_failCount(0)
    , m_saveCounter(0)
    , m_completionSaved(false)
    , m_mpcStatusUrl(QString("http://localhost:%1/status.html").arg(MPC_WEB_PORT))
{
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(STATUS_POLL_INTERVAL_MS);
    connect(m_statusTimer, &QTimer::timeout, this, &PlaybackManager::checkPlaybackStatus);
    
    m_networkManager = new QNetworkAccessManager(this);
    
    // Initialize watch chunk manager
    m_watchChunkManager = new WatchChunkManager(this);
    
    // Forward fileMarkedAsWatched signal from chunk manager
    connect(m_watchChunkManager, &WatchChunkManager::fileMarkedAsWatched,
            this, &PlaybackManager::fileMarkedAsLocallyWatched);
}

PlaybackManager::~PlaybackManager()
{
    stopTracking();
}

bool PlaybackManager::startPlayback(const QString &filePath, int lid, int resumePosition)
{
    // Stop tracking any currently playing file first
    if (m_tracking) {
        stopTracking();
    }
    
    // Check if file exists
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        LOG(QString("Playback error: File does not exist: %1").arg(filePath));
        return false;
    }
    
    // Get media player path from settings
    QString playerPath = getMediaPlayerPath();
    if (playerPath.isEmpty()) {
        LOG("Playback error: Media player path not configured");
        return false;
    }
    
    // Check if player executable exists
    QFileInfo playerInfo(playerPath);
    if (!playerInfo.exists()) {
        LOG(QString("Playback error: Media player not found at: %1").arg(playerPath));
        return false;
    }
    
    // Build command arguments
    QStringList arguments;
    arguments << filePath;
    arguments << "/play";
    arguments << "/close"; // Close after playback ends
    
    // Start the player process (detached, so it continues after app closes)
    bool started = QProcess::startDetached(playerPath, arguments);
    
    if (!started) {
        LOG(QString("Playback error: Failed to start media player: %1").arg(playerPath));
        return false;
    }
    
    LOG(QString("Started playback: %1 (LID: %2)").arg(filePath).arg(lid));
    
    // Start tracking
    m_tracking = true;
    m_currentLid = lid;
    m_currentFilePath = filePath;
    m_lastPosition = resumePosition;
    m_lastDuration = 0;
    m_failCount = 0;          // Reset fail count
    m_saveCounter = 0;        // Reset save counter
    m_completionSaved = false; // Reset completion flag
    
    // Emit state change signal to indicate playback started
    emit playbackStateChanged(lid, true);
    
    // Start status polling after a short delay to let player start
    // Use QTimer with single-shot mode instead of QTimer::singleShot to avoid chrono overload
    // The chrono overload is not available in static Qt 6.10 builds with LLVM MinGW
    QTimer *startupTimer = new QTimer(this);
    startupTimer->setSingleShot(true);
    connect(startupTimer, &QTimer::timeout, this, [this]() {
        m_statusTimer->start();
        emit playbackStateChanged(m_currentLid, true);
    });
    connect(startupTimer, &QTimer::timeout, startupTimer, &QTimer::deleteLater);
    startupTimer->start(PLAYER_STARTUP_DELAY_MS);
    
    return true;
}

void PlaybackManager::stopTracking()
{
    if (m_tracking) {
        int lid = m_currentLid;  // Save lid before clearing
        LOG(QString("Stopping playback tracking for LID %1 (position: %2s, duration: %3s)")
            .arg(m_currentLid).arg(m_lastPosition).arg(m_lastDuration));
        
        m_statusTimer->stop();
        
        // Save final position only if not already saved at completion
        // (to avoid overwriting completion position with earlier position)
        if (m_lastDuration > 0 && !m_completionSaved) {
            savePlaybackPosition(m_lastPosition, m_lastDuration);
            emit playbackStopped(lid, m_lastPosition);
        } else if (m_completionSaved) {
            LOG("Completion position already saved - skipping redundant save");
            emit playbackStopped(lid, m_lastDuration);
        } else {
            LOG("No duration recorded - playback data not saved");
            emit playbackStopped(lid, 0);
        }
        
        m_tracking = false;
        m_currentLid = 0;
        m_currentFilePath.clear();
        m_lastPosition = 0;
        m_lastDuration = 0;
        m_failCount = 0;
        m_saveCounter = 0;
        m_completionSaved = false;
        
        // Emit state change signal
        emit playbackStateChanged(lid, false);
    }
}

void PlaybackManager::checkPlaybackStatus()
{
    if (!m_tracking) {
        return;
    }
    
    // Request status from MPC-HC web interface
    QUrl url(m_mpcStatusUrl);
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &PlaybackManager::handleStatusReply);
}

void PlaybackManager::handleStatusReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        LOG("handleStatusReply: Reply is null");
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        // Player might have closed or web interface is not enabled
        // Check if we should stop tracking (after several failed attempts)
        m_failCount++;
        
        LOG(QString("Failed to connect to MPC-HC web interface (attempt %1/%2): %3")
            .arg(m_failCount).arg(MAX_CONSECUTIVE_FAILURES).arg(reply->errorString()));
        
        if (m_failCount > MAX_CONSECUTIVE_FAILURES) {
            LOG("Playback tracking stopped: Player closed or web interface not responding");
            stopTracking();
        }
        
        reply->deleteLater();
        return;
    }
    
    // Reset fail count on success
    m_failCount = 0;
    
    // Parse the status response
    // Format: OnStatus("filename", "state", position_ms, "position_str", duration_ms, "duration_str", muted, volume, "filepath")
    // Example: OnStatus("file.mkv", "Playing", 5298, "00:00:05", 1455038, "00:24:15", 0, 100, "D:\path\file.mkv")
    // We extract: state (capture 1), position_ms (capture 2), duration_ms (capture 3)
    QString response = QString::fromUtf8(reply->readAll());
    
    // Use regex to extract position and duration
    // Pattern matches: OnStatus("file", "state", pos_ms, "pos_str", dur_ms, "dur_str"...)
    // The first field is a quoted filename (match \"...\"), then state, position, etc.
    static const QRegularExpression regex("OnStatus\\(\"[^\"]*\",\\s*\"([^\"]+)\",\\s*(\\d+),\\s*\"[^\"]+\",\\s*(\\d+),\\s*\"[^\"]+\"");
    QRegularExpressionMatch match = regex.match(response);
    
    if (match.hasMatch()) {
        QString state = match.captured(1);
        int positionMs = match.captured(2).toInt();
        int durationMs = match.captured(3).toInt();
        
        int position = positionMs / 1000; // Convert to seconds
        int duration = durationMs / 1000;
        
        // Update position if changed
        if (position != m_lastPosition || duration != m_lastDuration) {
            m_lastPosition = position;
            m_lastDuration = duration;
            
            // Record watched chunk (1-minute chunks)
            if (duration > 0 && position > 0) {
                int chunkIndex = position / WatchChunkManager::getChunkSizeSeconds();
                m_watchChunkManager->recordChunk(m_currentLid, chunkIndex);
                
                // Check if file should be marked as locally watched based on chunks
                if (!m_watchChunkManager->getLocalWatchedStatus(m_currentLid)) {
                    if (m_watchChunkManager->shouldMarkAsWatched(m_currentLid, duration)) {
                        m_watchChunkManager->updateLocalWatchedStatus(m_currentLid, true);
                        LOG(QString("File marked as locally watched based on chunks: LID %1").arg(m_currentLid));
                    }
                }
            }
            
            // Save to database periodically
            m_saveCounter++;
            
            if (m_saveCounter >= SAVE_INTERVAL_SECONDS) {
                savePlaybackPosition(position, duration);
                emit playbackPositionUpdated(m_currentLid, position, duration);
                m_saveCounter = 0;
            }
        }
        
        // Check if playback completed
        if (duration > 0 && position >= duration - COMPLETION_THRESHOLD_SECONDS) {
            LOG(QString("Playback completed: LID %1").arg(m_currentLid));
            
            // Mark as fully watched
            savePlaybackPosition(duration, duration);
            m_completionSaved = true;  // Mark that we've saved completion position
            emit playbackCompleted(m_currentLid);
            
            // Update viewed and local_watched status in mylist
            QSqlDatabase db = QSqlDatabase::database();
            if (db.isOpen()) {
                qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
                QSqlQuery q(db);
                q.prepare("UPDATE mylist SET viewed = 1, local_watched = 1, viewdate = ? WHERE lid = ?");
                q.addBindValue(currentTimestamp);
                q.addBindValue(m_currentLid);
                
                if (!q.exec()) {
                    LOG(QString("Error updating file watch status for lid=%1: %2")
                        .arg(m_currentLid).arg(q.lastError().text()));
                } else {
                    // Mark episode as watched at episode level (persists across file replacements)
                    q.prepare("SELECT eid FROM mylist WHERE lid = ?");
                    q.addBindValue(m_currentLid);
                    if (q.exec() && q.next()) {
                        int eid = q.value(0).toInt();
                        if (eid > 0) {
                            q.prepare("INSERT OR REPLACE INTO watched_episodes (eid, watched_at) VALUES (?, ?)");
                            q.addBindValue(eid);
                            q.addBindValue(currentTimestamp);
                            if (!q.exec()) {
                                LOG(QString("Error marking episode eid=%1 as watched at episode level: %2")
                                    .arg(eid).arg(q.lastError().text()));
                            }
                        }
                    }
                }
            }
            
            stopTracking();
        }
    } else {
        LOG(QString("Failed to parse MPC-HC status response (length %1). Response: %2").arg(response.length()).arg(response));
        LOG(QString("Regex pattern used: %1").arg(regex.pattern()));
    }
    
    reply->deleteLater();
}

void PlaybackManager::savePlaybackPosition(int position, int duration)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("Cannot save playback position: Database not open");
        return;
    }
    
    QSqlQuery q(db);
    q.prepare("UPDATE mylist SET playback_position = ?, playback_duration = ?, last_played = ? WHERE lid = ?");
    q.addBindValue(position);
    q.addBindValue(duration);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(m_currentLid);
    
    if (!q.exec()) {
        LOG(QString("Error saving playback position: %1").arg(q.lastError().text()));
    } else {
        LOG(QString("Saved playback position: LID %1, position %2/%3s")
            .arg(m_currentLid).arg(position).arg(duration));
    }
}

QString PlaybackManager::getDefaultMediaPlayerPath()
{
    return "C:\\Program Files (x86)\\K-Lite Codec Pack\\MPC-HC64\\mpc-hc64_nvo.exe";
}

QString PlaybackManager::getMediaPlayerPath()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return getDefaultMediaPlayerPath();
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT value FROM settings WHERE name = 'media_player_path'");
    
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    
    // Return default path if not set
    return getDefaultMediaPlayerPath();
}

void PlaybackManager::setMediaPlayerPath(const QString &path)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('media_player_path', ?)");
    q.addBindValue(path);
    
    if (!q.exec()) {
        LOG(QString("Error saving media player path: %1").arg(q.lastError().text()));
    }
}
