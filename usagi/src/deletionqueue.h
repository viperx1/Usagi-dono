#ifndef DELETIONQUEUE_H
#define DELETIONQUEUE_H

#include <QObject>
#include <QList>
#include <QPair>
#include <QTimer>
#include "deletioncandidate.h"

class HybridDeletionClassifier;
class DeletionLockManager;
class FactorWeightLearner;

/**
 * @brief Maintains the ranked list of deletion candidates and locked files.
 *
 * rebuild() classifies ALL local files and populates two disjoint lists:
 *   m_candidates  – deletable files (T0-T3), sorted by tier + score
 *   m_lockedFiles – locked files shown for visibility
 *
 * scheduleRebuild() coalesces rapid requests into a single deferred rebuild.
 */
class DeletionQueue : public QObject
{
    Q_OBJECT

public:
    explicit DeletionQueue(HybridDeletionClassifier &classifier,
                           DeletionLockManager &lockManager,
                           FactorWeightLearner &learner,
                           QObject *parent = nullptr);

    /// Re-classify every local file immediately.
    void rebuild();

    /// Schedule a deferred rebuild, coalescing rapid successive calls.
    void scheduleRebuild();

    /// Top candidate (nullptr if empty).
    const DeletionCandidate *next() const;

    /// Combined list: candidates + locked files. Locked entries have locked=true.
    QList<DeletionCandidate> allCandidates() const;

    /// Only unlocked candidates.
    const QList<DeletionCandidate> &candidates() const { return m_candidates; }

    /// Only locked files.
    const QList<DeletionCandidate> &lockedFiles() const { return m_lockedFiles; }

    /// Number of files classified as protected (not shown in candidates or locked lists).
    int protectedCount() const { return m_protectedCount; }

    /// Total number of local files classified in the last rebuild.
    int totalClassified() const { return m_totalClassified; }

    /// True if the top candidate requires user input (A vs B).
    bool needsUserChoice() const;

    /// The A vs B pair (top two Tier-3 candidates).
    QPair<DeletionCandidate, DeletionCandidate> getAvsBPair() const;

    // ── Lock actions (delegates + schedules rebuild) ──
    void lockAnime(int aid);
    void unlockAnime(int aid);
    void lockEpisode(int eid);
    void unlockEpisode(int eid);

    /// Process an A vs B user choice (deletes file + updates weights).
    void recordChoice(int keptLid, int deletedLid);

    /// Debounce interval in milliseconds for scheduleRebuild().
    static constexpr int DEBOUNCE_MS = 500;

signals:
    void queueRebuilt();
    void choiceNeeded();

private:
    QList<DeletionCandidate> m_candidates;
    QList<DeletionCandidate> m_lockedFiles;
    int m_protectedCount  = 0;
    int m_totalClassified = 0;
    HybridDeletionClassifier &m_classifier;
    DeletionLockManager &m_lockManager;
    FactorWeightLearner &m_learner;
    QTimer m_rebuildTimer;
};

#endif // DELETIONQUEUE_H
