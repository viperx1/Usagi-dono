#include "deletionlockmanager.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

DeletionLockManager::DeletionLockManager(QObject *parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Table setup
// ---------------------------------------------------------------------------

void DeletionLockManager::ensureTablesExist()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("DeletionLockManager: database not open");
        return;
    }
    QSqlQuery q(db);

    q.exec("CREATE TABLE IF NOT EXISTS deletion_locks ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "aid INTEGER,"
           "eid INTEGER,"
           "locked_at INTEGER,"
           "CHECK ((aid IS NOT NULL AND eid IS NULL) OR (aid IS NULL AND eid IS NOT NULL)),"
           "UNIQUE(aid, eid)"
           ")");
    q.exec("CREATE INDEX IF NOT EXISTS idx_deletion_locks_aid ON deletion_locks(aid)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_deletion_locks_eid ON deletion_locks(eid)");

    // Add the denormalized cache column to mylist (safe to call repeatedly).
    q.exec("ALTER TABLE mylist ADD COLUMN deletion_locked INTEGER DEFAULT 0");
    // The ALTER will fail silently if the column already exists.

    reloadCaches();
    LOG("DeletionLockManager: tables ensured");
}

// ---------------------------------------------------------------------------
// Anime-level locks
// ---------------------------------------------------------------------------

void DeletionLockManager::lockAnime(int aid)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("INSERT OR IGNORE INTO deletion_locks (aid, eid, locked_at) VALUES (:aid, NULL, :ts)");
    q.bindValue(":aid", aid);
    q.bindValue(":ts", QDateTime::currentSecsSinceEpoch());
    q.exec();

    propagateToMylist(aid, -1, 2);
    m_lockedAnimeIds.insert(aid);
    LOG(QString("DeletionLockManager: locked anime %1").arg(aid));
    emit lockChanged(aid, -1, true);
}

void DeletionLockManager::unlockAnime(int aid)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("DELETE FROM deletion_locks WHERE aid = :aid AND eid IS NULL");
    q.bindValue(":aid", aid);
    q.exec();

    m_lockedAnimeIds.remove(aid);
    recalculateMylistLocksForAnime(aid);
    LOG(QString("DeletionLockManager: unlocked anime %1").arg(aid));
    emit lockChanged(aid, -1, false);
}

bool DeletionLockManager::isAnimeLocked(int aid) const
{
    return m_lockedAnimeIds.contains(aid);
}

// ---------------------------------------------------------------------------
// Episode-level locks
// ---------------------------------------------------------------------------

void DeletionLockManager::lockEpisode(int eid)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("INSERT OR IGNORE INTO deletion_locks (aid, eid, locked_at) VALUES (NULL, :eid, :ts)");
    q.bindValue(":eid", eid);
    q.bindValue(":ts", QDateTime::currentSecsSinceEpoch());
    q.exec();

    // Only set episode lock if not already covered by anime lock.
    q.prepare("UPDATE mylist SET deletion_locked = 1 WHERE eid = :eid AND deletion_locked < 1");
    q.bindValue(":eid", eid);
    q.exec();

    m_lockedEpisodeIds.insert(eid);
    LOG(QString("DeletionLockManager: locked episode %1").arg(eid));
    emit lockChanged(-1, eid, true);
}

void DeletionLockManager::unlockEpisode(int eid)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("DELETE FROM deletion_locks WHERE eid = :eid AND aid IS NULL");
    q.bindValue(":eid", eid);
    q.exec();

    m_lockedEpisodeIds.remove(eid);
    recalculateMylistLocksForEpisode(eid);
    LOG(QString("DeletionLockManager: unlocked episode %1").arg(eid));
    emit lockChanged(-1, eid, false);
}

bool DeletionLockManager::isEpisodeLocked(int eid) const
{
    return m_lockedEpisodeIds.contains(eid);
}

// ---------------------------------------------------------------------------
// File-level query
// ---------------------------------------------------------------------------

bool DeletionLockManager::isFileLocked(int lid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT deletion_locked FROM mylist WHERE lid = :lid");
    q.bindValue(":lid", lid);
    if (q.exec() && q.next()) {
        return q.value(0).toInt() > 0;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Bulk queries
// ---------------------------------------------------------------------------

QList<DeletionLock> DeletionLockManager::allLocks() const
{
    QList<DeletionLock> result;
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec("SELECT id, aid, eid, locked_at FROM deletion_locks");
    while (q.next()) {
        DeletionLock lock;
        lock.id       = q.value(0).toInt();
        lock.aid      = q.value(1).isNull() ? -1 : q.value(1).toInt();
        lock.eid      = q.value(2).isNull() ? -1 : q.value(2).toInt();
        lock.lockedAt = q.value(3).toLongLong();
        result.append(lock);
    }
    return result;
}

int DeletionLockManager::lockedAnimeCount() const
{
    return m_lockedAnimeIds.size();
}

int DeletionLockManager::lockedEpisodeCount() const
{
    return m_lockedEpisodeIds.size();
}

// ---------------------------------------------------------------------------
// Cache reload
// ---------------------------------------------------------------------------

void DeletionLockManager::reloadCaches()
{
    m_lockedAnimeIds.clear();
    m_lockedEpisodeIds.clear();

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec("SELECT aid FROM deletion_locks WHERE aid IS NOT NULL");
    while (q.next()) {
        m_lockedAnimeIds.insert(q.value(0).toInt());
    }
    q.exec("SELECT eid FROM deletion_locks WHERE eid IS NOT NULL");
    while (q.next()) {
        m_lockedEpisodeIds.insert(q.value(0).toInt());
    }
}

// ---------------------------------------------------------------------------
// Propagation helpers
// ---------------------------------------------------------------------------

void DeletionLockManager::propagateToMylist(int aid, int /*eid*/, int lockValue)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    if (aid > 0) {
        q.prepare("UPDATE mylist SET deletion_locked = :val WHERE aid = :aid");
        q.bindValue(":val", lockValue);
        q.bindValue(":aid", aid);
        q.exec();
    }
}

void DeletionLockManager::recalculateMylistLocksForAnime(int aid)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // Reset all rows for this anime to 0 first.
    q.prepare("UPDATE mylist SET deletion_locked = 0 WHERE aid = :aid");
    q.bindValue(":aid", aid);
    q.exec();

    // Re-apply any remaining episode-level locks for episodes of this anime.
    q.prepare("UPDATE mylist SET deletion_locked = 1 "
              "WHERE aid = :aid AND eid IN (SELECT eid FROM deletion_locks WHERE eid IS NOT NULL)");
    q.bindValue(":aid", aid);
    q.exec();
}

void DeletionLockManager::recalculateMylistLocksForEpisode(int eid)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // Check if the episode's anime still has an anime-level lock.
    q.prepare("SELECT m.aid FROM mylist m "
              "JOIN deletion_locks dl ON dl.aid = m.aid AND dl.eid IS NULL "
              "WHERE m.eid = :eid LIMIT 1");
    q.bindValue(":eid", eid);
    if (q.exec() && q.next()) {
        // Anime-level lock still covers this episode → value stays 2.
        return;
    }

    // No anime lock — reset to 0.
    q.prepare("UPDATE mylist SET deletion_locked = 0 WHERE eid = :eid");
    q.bindValue(":eid", eid);
    q.exec();
}
