#ifndef DELETIONLOCK_H
#define DELETIONLOCK_H

#include <QtTypes>

/**
 * @brief Value type representing a deletion lock on an anime or episode.
 *
 * A lock protects the highest-rated file for the locked anime/episode
 * from automatic deletion.  Lower-rated duplicates remain eligible.
 *
 * Invariant: exactly one of aid/eid is positive; the other is -1.
 */
struct DeletionLock {
    int id   = -1;          ///< deletion_locks.id  (-1 if not yet persisted)
    int aid  = -1;          ///< anime ID, or -1 for an episode-level lock
    int eid  = -1;          ///< episode ID, or -1 for an anime-level lock
    qint64 lockedAt = 0;    ///< Unix timestamp

    bool isAnimeLock()   const { return aid > 0 && eid < 0; }
    bool isEpisodeLock() const { return eid > 0 && aid < 0; }
};

#endif // DELETIONLOCK_H
