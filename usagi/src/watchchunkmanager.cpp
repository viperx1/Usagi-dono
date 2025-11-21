#include "watchchunkmanager.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariant>
#include <cmath>

WatchChunkManager::WatchChunkManager(QObject *parent)
    : QObject(parent)
{
}

WatchChunkManager::~WatchChunkManager()
{
}

void WatchChunkManager::recordChunk(int lid, int chunkIndex)
{
    if (lid <= 0 || chunkIndex < 0) {
        return;
    }
    
    // Load chunks from database if not cached
    if (!m_cachedChunks.contains(lid)) {
        loadChunksFromDatabase(lid);
    }
    
    // Check if chunk is already recorded
    if (m_cachedChunks[lid].contains(chunkIndex)) {
        return; // Already recorded, no need to save again
    }
    
    // Add to cache
    m_cachedChunks[lid].insert(chunkIndex);
    
    // Save to database
    saveChunkToDatabase(lid, chunkIndex);
    
    LOG(QString("Recorded watch chunk: LID %1, chunk %2").arg(lid).arg(chunkIndex));
}

void WatchChunkManager::clearChunks(int lid)
{
    if (lid <= 0) {
        return;
    }
    
    // Clear from cache
    m_cachedChunks.remove(lid);
    
    // Clear from database
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("Cannot clear chunks: Database not open");
        return;
    }
    
    QSqlQuery q(db);
    q.prepare("DELETE FROM watch_chunks WHERE lid = ?");
    q.addBindValue(lid);
    
    if (!q.exec()) {
        LOG(QString("Error clearing watch chunks: %1").arg(q.lastError().text()));
    } else {
        LOG(QString("Cleared watch chunks for LID %1").arg(lid));
    }
}

QSet<int> WatchChunkManager::getWatchedChunks(int lid) const
{
    if (lid <= 0) {
        return QSet<int>();
    }
    
    // Load from database if not cached
    if (!m_cachedChunks.contains(lid)) {
        const_cast<WatchChunkManager*>(this)->loadChunksFromDatabase(lid);
    }
    
    return m_cachedChunks.value(lid);
}

double WatchChunkManager::calculateWatchPercentage(int lid, int durationSeconds) const
{
    if (lid <= 0 || durationSeconds <= 0) {
        return 0.0;
    }
    
    QSet<int> chunks = getWatchedChunks(lid);
    int totalChunks = getTotalChunks(durationSeconds);
    
    if (totalChunks <= 0) {
        return 0.0;
    }
    
    return (chunks.size() * 100.0) / totalChunks;
}

bool WatchChunkManager::shouldMarkAsWatched(int lid, int durationSeconds) const
{
    if (lid <= 0 || durationSeconds <= 0) {
        return false;
    }
    
    // Check minimum duration requirement (at least 5 minutes)
    if (durationSeconds < MIN_WATCH_TIME_SECONDS) {
        // For very short files, just check if any chunks are watched
        return !getWatchedChunks(lid).isEmpty();
    }
    
    // Calculate watch percentage
    double percentage = calculateWatchPercentage(lid, durationSeconds);
    
    // Check if percentage meets threshold
    return percentage >= MIN_WATCH_PERCENTAGE;
}

void WatchChunkManager::updateLocalWatchedStatus(int lid, bool watched)
{
    if (lid <= 0) {
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("Cannot update local watched status: Database not open");
        return;
    }
    
    QSqlQuery q(db);
    q.prepare("UPDATE mylist SET local_watched = ? WHERE lid = ?");
    q.addBindValue(watched ? 1 : 0);
    q.addBindValue(lid);
    
    if (!q.exec()) {
        LOG(QString("Error updating local watched status: %1").arg(q.lastError().text()));
    } else {
        LOG(QString("Updated local watched status for LID %1: %2")
            .arg(lid).arg(watched ? "watched" : "not watched"));
        
        if (watched) {
            emit fileMarkedAsWatched(lid);
        }
    }
}

bool WatchChunkManager::getLocalWatchedStatus(int lid) const
{
    if (lid <= 0) {
        return false;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare("SELECT local_watched FROM mylist WHERE lid = ?");
    q.addBindValue(lid);
    
    if (q.exec() && q.next()) {
        return q.value(0).toInt() == 1;
    }
    
    return false;
}

void WatchChunkManager::loadChunksFromDatabase(int lid)
{
    if (lid <= 0) {
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("Cannot load chunks: Database not open");
        return;
    }
    
    QSet<int> chunks;
    
    QSqlQuery q(db);
    q.prepare("SELECT chunk_index FROM watch_chunks WHERE lid = ?");
    q.addBindValue(lid);
    
    if (q.exec()) {
        while (q.next()) {
            chunks.insert(q.value(0).toInt());
        }
    } else {
        LOG(QString("Error loading watch chunks: %1").arg(q.lastError().text()));
    }
    
    m_cachedChunks[lid] = chunks;
    LOG(QString("Loaded %1 chunks from database for LID %2").arg(chunks.size()).arg(lid));
}

void WatchChunkManager::saveChunkToDatabase(int lid, int chunkIndex)
{
    if (lid <= 0 || chunkIndex < 0) {
        return;
    }
    
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("Cannot save chunk: Database not open");
        return;
    }
    
    QSqlQuery q(db);
    q.prepare("INSERT OR IGNORE INTO watch_chunks (lid, chunk_index, watched_at) VALUES (?, ?, ?)");
    q.addBindValue(lid);
    q.addBindValue(chunkIndex);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    
    if (!q.exec()) {
        LOG(QString("Error saving watch chunk: %1").arg(q.lastError().text()));
    }
}

int WatchChunkManager::getTotalChunks(int durationSeconds) const
{
    if (durationSeconds <= 0) {
        return 0;
    }
    
    // Calculate total number of chunks, rounding up
    return static_cast<int>(std::ceil(static_cast<double>(durationSeconds) / CHUNK_SIZE_SECONDS));
}

// Include the MOC-generated file (only needed if AUTOMOC doesn't handle it)
#include "moc_watchchunkmanager.cpp"
