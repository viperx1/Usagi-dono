#include "deletionqueue.h"
#include "hybriddeletionclassifier.h"
#include "deletionlockmanager.h"
#include "factorweightlearner.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <algorithm>

DeletionQueue::DeletionQueue(HybridDeletionClassifier &classifier,
                             DeletionLockManager &lockManager,
                             FactorWeightLearner &learner,
                             QObject *parent)
    : QObject(parent)
    , m_classifier(classifier)
    , m_lockManager(lockManager)
    , m_learner(learner)
{
}

// ---------------------------------------------------------------------------
// rebuild
// ---------------------------------------------------------------------------

void DeletionQueue::rebuild()
{
    m_candidates.clear();
    m_lockedFiles.clear();

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    // Fetch all local files with non-null paths (i.e. files that exist on disk)
    q.exec("SELECT m.lid FROM mylist m "
           "JOIN local_files lf ON lf.id = m.local_file "
           "WHERE lf.path IS NOT NULL AND m.state != 3");
    QList<int> lids;
    while (q.next()) {
        lids.append(q.value(0).toInt());
    }

    for (int lid : lids) {
        DeletionCandidate c = m_classifier.classify(lid);
        if (c.locked) {
            m_lockedFiles.append(c);
        } else if (c.tier != DeletionTier::PROTECTED) {
            m_candidates.append(c);
        }
    }

    std::sort(m_candidates.begin(), m_candidates.end());
    std::sort(m_lockedFiles.begin(), m_lockedFiles.end());

    LOG(QString("DeletionQueue: rebuilt — %1 candidates, %2 locked")
        .arg(m_candidates.size()).arg(m_lockedFiles.size()));
    emit queueRebuilt();

    if (needsUserChoice()) {
        emit choiceNeeded();
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const DeletionCandidate *DeletionQueue::next() const
{
    return m_candidates.isEmpty() ? nullptr : &m_candidates.first();
}

QList<DeletionCandidate> DeletionQueue::allCandidates() const
{
    QList<DeletionCandidate> combined;
    combined.reserve(m_candidates.size() + m_lockedFiles.size());
    combined.append(m_candidates);
    combined.append(m_lockedFiles);
    return combined;
}

bool DeletionQueue::needsUserChoice() const
{
    if (m_candidates.isEmpty()) return false;
    const auto &top = m_candidates.first();
    // Procedural tiers don't need user choice
    if (top.tier < DeletionTier::LEARNED_PREFERENCE) return false;
    // Not enough training — always ask
    if (!m_learner.isTrained()) return true;
    // Only one Tier-3 candidate — show single confirmation
    int tier3Count = 0;
    for (const auto &c : m_candidates) {
        if (c.tier == DeletionTier::LEARNED_PREFERENCE) ++tier3Count;
        if (tier3Count >= 2) break;
    }
    if (tier3Count < 2) return true;
    // Check confidence
    return m_learner.scoreDifference(
               m_candidates[0].factorValues,
               m_candidates[1].factorValues) < FactorWeightLearner::CONFIDENCE_THRESHOLD;
}

QPair<DeletionCandidate, DeletionCandidate> DeletionQueue::getAvsBPair() const
{
    if (m_candidates.size() >= 2) {
        return qMakePair(m_candidates[0], m_candidates[1]);
    }
    if (m_candidates.size() == 1) {
        DeletionCandidate empty;
        return qMakePair(m_candidates[0], empty);
    }
    return {};
}

// ---------------------------------------------------------------------------
// Lock actions
// ---------------------------------------------------------------------------

void DeletionQueue::lockAnime(int aid)
{
    m_lockManager.lockAnime(aid);
    rebuild();
}

void DeletionQueue::unlockAnime(int aid)
{
    m_lockManager.unlockAnime(aid);
    rebuild();
}

void DeletionQueue::lockEpisode(int eid)
{
    m_lockManager.lockEpisode(eid);
    rebuild();
}

void DeletionQueue::unlockEpisode(int eid)
{
    m_lockManager.unlockEpisode(eid);
    rebuild();
}

// ---------------------------------------------------------------------------
// A vs B choice
// ---------------------------------------------------------------------------

void DeletionQueue::recordChoice(int keptLid, int deletedLid)
{
    // Find factor values for both files
    QMap<QString, double> keptFactors   = m_classifier.normalizeFactors(keptLid);
    QMap<QString, double> deletedFactors = m_classifier.normalizeFactors(deletedLid);

    m_learner.recordChoice(keptLid, deletedLid, keptFactors, deletedFactors);
    rebuild();
}
