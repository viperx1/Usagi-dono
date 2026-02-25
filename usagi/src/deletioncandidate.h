#ifndef DELETIONCANDIDATE_H
#define DELETIONCANDIDATE_H

#include <QMap>
#include <QString>

/**
 * @brief Tier constants for the hybrid deletion classifier.
 *
 * Lower numeric value = higher deletion priority (deleted first).
 * PROTECTED is a sentinel that keeps the file from being deleted.
 */
namespace DeletionTier {
    constexpr int HIDDEN_ANIME              = 0;
    constexpr int SUPERSEDED_REVISION       = 1;
    constexpr int LOW_QUALITY_DUPLICATE     = 2;
    constexpr int LANGUAGE_MISMATCH         = 3;
    constexpr int LEARNED_PREFERENCE        = 4;
    constexpr int PROTECTED                 = 999;
}

/**
 * @brief Value type holding the classification result for a single file.
 */
struct DeletionCandidate {
    int lid             = -1;
    int aid             = -1;
    int eid             = -1;
    int tier            = DeletionTier::PROTECTED;
    double learnedScore = 0.0;   ///< From factor weights (tier 3 only; 0.0 for procedural)
    QMap<QString, double> factorValues;  ///< Normalized factor values for this file
    QString reason;              ///< Human-readable reason with actual values
    QString filePath;
    QString animeName;
    QString episodeLabel;        ///< e.g. "Ep 30 - Title"
    int replacementLid  = -1;    ///< Lid of the better alternative (tiers 0-2)
    QString replacementPath;     ///< File path of the replacement
    qint64 fileSize     = 0;     ///< Size of the file in bytes (captured before deletion)
    bool gapProtected = false;
    bool locked       = false;   ///< True if anime or episode is locked

    /// Comparison: sort by tier ascending, then score ascending (lowest = deleted first).
    bool operator<(const DeletionCandidate &other) const {
        if (tier != other.tier) return tier < other.tier;
        return learnedScore < other.learnedScore;
    }
};

#endif // DELETIONCANDIDATE_H
