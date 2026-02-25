#ifndef HYBRIDDELETIONCLASSIFIER_H
#define HYBRIDDELETIONCLASSIFIER_H

#include <QObject>
#include "deletioncandidate.h"

class DeletionLockManager;
class FactorWeightLearner;
class WatchSessionManager;

/**
 * @brief Assigns a tier + score to a file using procedural rules
 *        (tiers 0-2) and learned weights (tier 3).
 *
 * Absolute protections (lock, gap) are checked first.
 */
class HybridDeletionClassifier : public QObject
{
    Q_OBJECT

public:
    explicit HybridDeletionClassifier(const DeletionLockManager &lockManager,
                                      const FactorWeightLearner &learner,
                                      const WatchSessionManager &sessionManager,
                                      QObject *parent = nullptr);

    /**
     * @brief Classify a single file.
     * @param lid  MyList ID of the file.
     * @return     DeletionCandidate with tier and score filled in.
     */
    DeletionCandidate classify(int lid) const;

    /**
     * @brief Compute normalized learnable factors for a file.
     */
    QMap<QString, double> normalizeFactors(int lid) const;

private:
    bool hasNewerLocalRevision(int lid) const;
    DeletionCandidate classifyTier0(int lid) const;
    DeletionCandidate classifyTier1(int lid) const;
    DeletionCandidate classifyTier2(int lid) const;
    DeletionCandidate classifyTier3(int lid) const;
    bool isEligibleForDeletion(int lid) const;

    const DeletionLockManager &m_lockManager;
    const FactorWeightLearner &m_learner;
    const WatchSessionManager &m_sessionManager;
};

#endif // HYBRIDDELETIONCLASSIFIER_H
