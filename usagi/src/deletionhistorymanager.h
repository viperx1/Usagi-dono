#ifndef DELETIONHISTORYMANAGER_H
#define DELETIONHISTORYMANAGER_H

#include <QObject>
#include <QList>
#include "deletionhistoryentry.h"

/**
 * @brief Records and queries the full deletion history (every file ever
 *        auto-deleted or user-deleted).
 *
 * Maximum 5000 entries; oldest entries pruned when limit exceeded.
 */
class DeletionHistoryManager : public QObject
{
    Q_OBJECT

public:
    static constexpr int MAX_ENTRIES = 5000;

    explicit DeletionHistoryManager(QObject *parent = nullptr);

    void ensureTablesExist();

    // ── Record ──
    void recordDeletion(int lid, int aid, int eid,
                        const QString &filePath, const QString &animeName,
                        const QString &episodeLabel, qint64 fileSize,
                        int tier, const QString &reason,
                        double learnedScore, const QString &deletionType,
                        qint64 spaceBefore, qint64 spaceAfter,
                        int replacedByLid = -1);

    // ── Query ──
    QList<DeletionHistoryEntry> allEntries(int limit = 100, int offset = 0) const;
    QList<DeletionHistoryEntry> entriesForAnime(int aid) const;
    QList<DeletionHistoryEntry> entriesByType(const QString &type) const;

    // ── Statistics ──
    qint64 totalSpaceFreed() const;
    int totalDeletions() const;

signals:
    void entryAdded(int historyId);

private:
    void pruneOldest();
    DeletionHistoryEntry rowToEntry(const class QSqlQuery &q) const;
};

#endif // DELETIONHISTORYMANAGER_H
