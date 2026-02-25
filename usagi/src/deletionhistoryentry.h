#ifndef DELETIONHISTORYENTRY_H
#define DELETIONHISTORYENTRY_H

#include <QString>
#include <QtTypes>

/**
 * @brief Value type for a single row in the deletion_history table.
 */
struct DeletionHistoryEntry {
    int id              = -1;
    int lid             = -1;
    int aid             = -1;
    int eid             = -1;
    int replacedByLid   = -1;   ///< lid of the file that replaced this one (procedural); -1 if N/A
    QString filePath;
    QString animeName;
    QString episodeLabel;
    qint64 fileSize     = 0;
    int tier            = -1;
    QString reason;
    double learnedScore = -1.0; ///< -1.0 if not applicable (procedural tiers)
    QString deletionType;       ///< "procedural", "learned_auto", "user_avsb", "manual"
    qint64 spaceBefore  = 0;
    qint64 spaceAfter   = 0;
    qint64 deletedAt    = 0;    ///< Unix timestamp
};

#endif // DELETIONHISTORYENTRY_H
