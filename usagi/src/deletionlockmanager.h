#ifndef DELETIONLOCKMANAGER_H
#define DELETIONLOCKMANAGER_H

#include <QObject>
#include <QList>
#include <QSet>
#include "deletionlock.h"

/**
 * @brief CRUD operations on the deletion_locks table and
 *        propagation of the denormalized mylist.deletion_locked column.
 *
 * Lock semantics (from design doc):
 *   - Anime lock  → protects highest-rated file per episode for the anime.
 *   - Episode lock → protects highest-rated file for that episode.
 *   - deletion_locked column: 0 = not locked, 1 = episode lock, 2 = anime lock.
 */
class DeletionLockManager : public QObject
{
    Q_OBJECT

public:
    explicit DeletionLockManager(QObject *parent = nullptr);

    // ── Table setup ──
    void ensureTablesExist();

    // ── Anime-level locks ──
    void lockAnime(int aid);
    void unlockAnime(int aid);
    bool isAnimeLocked(int aid) const;

    // ── Episode-level locks ──
    void lockEpisode(int eid);
    void unlockEpisode(int eid);
    bool isEpisodeLocked(int eid) const;

    // ── File-level query (checks both anime and episode locks) ──
    bool isFileLocked(int lid) const;

    // ── Bulk queries ──
    QList<DeletionLock> allLocks() const;
    int lockedAnimeCount() const;
    int lockedEpisodeCount() const;

    // ── Cached ID sets (rebuilt on any lock change) ──
    const QSet<int> &lockedAnimeIds()   const { return m_lockedAnimeIds; }
    const QSet<int> &lockedEpisodeIds() const { return m_lockedEpisodeIds; }

    /// Refresh the in-memory caches from the database.
    void reloadCaches();

signals:
    void lockChanged(int aid, int eid, bool locked);

private:
    void propagateToMylist(int aid, int eid, int lockValue);
    void recalculateMylistLocksForAnime(int aid);
    void recalculateMylistLocksForEpisode(int eid);

    QSet<int> m_lockedAnimeIds;
    QSet<int> m_lockedEpisodeIds;
};

#endif // DELETIONLOCKMANAGER_H
