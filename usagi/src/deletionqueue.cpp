#include "deletionqueue.h"
#include "hybriddeletionclassifier.h"
#include "deletionlockmanager.h"
#include "factorweightlearner.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
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
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(DEBOUNCE_MS);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &DeletionQueue::rebuild);
}

// ---------------------------------------------------------------------------
// rebuild
// ---------------------------------------------------------------------------

void DeletionQueue::rebuild()
{
    m_rebuildTimer.stop();

    LOG(QString("DeletionQueue::rebuild() entered, thread=%1")
        .arg((quintptr)QThread::currentThreadId()));

    m_candidates.clear();
    m_lockedFiles.clear();

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid()) {
        LOG(QString("DeletionQueue::rebuild() ERROR: default database connection is INVALID"));
        return;
    }
    if (!db.isOpen()) {
        LOG(QString("DeletionQueue::rebuild() ERROR: default database connection is NOT OPEN"));
        return;
    }

    LOG(QString("DeletionQueue::rebuild() db connection='%1' driver='%2'")
        .arg(db.connectionName(), db.driverName()));

    QSqlQuery q(db);
    // Fetch all local files with non-null paths (i.e. files that exist on disk)
    if (!q.exec("SELECT m.lid FROM mylist m "
           "JOIN local_files lf ON lf.id = m.local_file "
           "WHERE lf.path IS NOT NULL AND m.state != 3")) {
        LOG(QString("DeletionQueue::rebuild() ERROR: query failed: %1").arg(q.lastError().text()));
        return;
    }
    QList<int> lids;
    while (q.next()) {
        lids.append(q.value(0).toInt());
    }

    LOG(QString("DeletionQueue: found %1 local file(s) for classification").arg(lids.size()));

    m_protectedCount  = 0;
    m_totalClassified = lids.size();
    int classifiedCount = 0;
    int lastReportedLid = 0;
    for (int lid : lids) {
        lastReportedLid = lid;
        DeletionCandidate c = m_classifier.classify(lid);
        if (c.locked) {
            m_lockedFiles.append(c);
        } else if (c.tier != DeletionTier::PROTECTED) {
            m_candidates.append(c);
        } else {
            ++m_protectedCount;
        }
        ++classifiedCount;
        if (classifiedCount % 50 == 0) {
            LOG(QString("DeletionQueue: classified %1/%2 (last lid=%3, candidates=%4, locked=%5, protected=%6)")
                .arg(classifiedCount).arg(lids.size()).arg(lid)
                .arg(m_candidates.size()).arg(m_lockedFiles.size()).arg(m_protectedCount));
        }
    }
    if (classifiedCount % 50 != 0) {
        LOG(QString("DeletionQueue: classified %1/%2 (last lid=%3, candidates=%4, locked=%5, protected=%6)")
            .arg(classifiedCount).arg(lids.size()).arg(lastReportedLid)
            .arg(m_candidates.size()).arg(m_lockedFiles.size()).arg(m_protectedCount));
    }

    std::sort(m_candidates.begin(), m_candidates.end());
    std::sort(m_lockedFiles.begin(), m_lockedFiles.end());

    LOG(QString("DeletionQueue: rebuilt — %1 candidates, %2 locked, %3 protected")
        .arg(m_candidates.size()).arg(m_lockedFiles.size()).arg(m_protectedCount));

    LOG(QString("DeletionQueue::rebuild() completed"));
    emit queueRebuilt();

    if (needsUserChoice()) {
        emit choiceNeeded();
    }
}

// ---------------------------------------------------------------------------
// scheduleRebuild  (debounced)
// ---------------------------------------------------------------------------

void DeletionQueue::scheduleRebuild()
{
    m_rebuildTimer.start();
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
    scheduleRebuild();
}

void DeletionQueue::unlockAnime(int aid)
{
    m_lockManager.unlockAnime(aid);
    scheduleRebuild();
}

void DeletionQueue::lockEpisode(int eid)
{
    m_lockManager.lockEpisode(eid);
    scheduleRebuild();
}

void DeletionQueue::unlockEpisode(int eid)
{
    m_lockManager.unlockEpisode(eid);
    scheduleRebuild();
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
    scheduleRebuild();
}
