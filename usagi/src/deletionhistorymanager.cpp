#include "deletionhistorymanager.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

DeletionHistoryManager::DeletionHistoryManager(QObject *parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Table setup
// ---------------------------------------------------------------------------

void DeletionHistoryManager::ensureTablesExist()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("DeletionHistoryManager: database not open");
        return;
    }
    QSqlQuery q(db);

    q.exec("CREATE TABLE IF NOT EXISTS deletion_history ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "lid INTEGER,"
           "aid INTEGER,"
           "eid INTEGER,"
           "replaced_by_lid INTEGER,"
           "file_path TEXT,"
           "anime_name TEXT,"
           "episode_label TEXT,"
           "file_size INTEGER,"
           "tier INTEGER,"
           "reason TEXT,"
           "learned_score REAL,"
           "deletion_type TEXT,"
           "space_before INTEGER,"
           "space_after INTEGER,"
           "deleted_at INTEGER"
           ")");
    q.exec("CREATE INDEX IF NOT EXISTS idx_deletion_history_time ON deletion_history(deleted_at)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_deletion_history_aid  ON deletion_history(aid)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_deletion_history_type ON deletion_history(deletion_type)");

    LOG("DeletionHistoryManager: tables ensured");
}

// ---------------------------------------------------------------------------
// Record
// ---------------------------------------------------------------------------

void DeletionHistoryManager::recordDeletion(
        int lid, int aid, int eid,
        const QString &filePath, const QString &animeName,
        const QString &episodeLabel, qint64 fileSize,
        int tier, const QString &reason,
        double learnedScore, const QString &deletionType,
        qint64 spaceBefore, qint64 spaceAfter,
        int replacedByLid)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO deletion_history "
              "(lid, aid, eid, replaced_by_lid, file_path, anime_name, episode_label, "
              " file_size, tier, reason, learned_score, deletion_type, space_before, space_after, deleted_at) "
              "VALUES (:lid, :aid, :eid, :rbl, :fp, :an, :el, :fs, :t, :r, :ls, :dt, :sb, :sa, :da)");
    q.bindValue(":lid", lid);
    q.bindValue(":aid", aid);
    q.bindValue(":eid", eid);
    q.bindValue(":rbl", replacedByLid > 0 ? replacedByLid : QVariant());
    q.bindValue(":fp",  filePath);
    q.bindValue(":an",  animeName);
    q.bindValue(":el",  episodeLabel);
    q.bindValue(":fs",  fileSize);
    q.bindValue(":t",   tier);
    q.bindValue(":r",   reason);
    q.bindValue(":ls",  learnedScore);
    q.bindValue(":dt",  deletionType);
    q.bindValue(":sb",  spaceBefore);
    q.bindValue(":sa",  spaceAfter);
    q.bindValue(":da",  QDateTime::currentSecsSinceEpoch());
    q.exec();

    int historyId = q.lastInsertId().toInt();
    pruneOldest();
    LOG(QString("DeletionHistoryManager: recorded deletion lid=%1 type=%2").arg(lid).arg(deletionType));
    emit entryAdded(historyId);
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

QList<DeletionHistoryEntry> DeletionHistoryManager::allEntries(int limit, int offset) const
{
    QList<DeletionHistoryEntry> result;
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT * FROM deletion_history ORDER BY deleted_at DESC LIMIT :lim OFFSET :off");
    q.bindValue(":lim", limit);
    q.bindValue(":off", offset);
    if (q.exec()) {
        while (q.next()) result.append(rowToEntry(q));
    }
    return result;
}

QList<DeletionHistoryEntry> DeletionHistoryManager::entriesForAnime(int aid) const
{
    QList<DeletionHistoryEntry> result;
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT * FROM deletion_history WHERE aid = :aid ORDER BY deleted_at DESC");
    q.bindValue(":aid", aid);
    if (q.exec()) {
        while (q.next()) result.append(rowToEntry(q));
    }
    return result;
}

QList<DeletionHistoryEntry> DeletionHistoryManager::entriesByType(const QString &type) const
{
    QList<DeletionHistoryEntry> result;
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT * FROM deletion_history WHERE deletion_type = :dt ORDER BY deleted_at DESC");
    q.bindValue(":dt", type);
    if (q.exec()) {
        while (q.next()) result.append(rowToEntry(q));
    }
    return result;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

qint64 DeletionHistoryManager::totalSpaceFreed() const
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec("SELECT COALESCE(SUM(space_before - space_after), 0) FROM deletion_history");
    if (q.next()) return q.value(0).toLongLong();
    return 0;
}

int DeletionHistoryManager::totalDeletions() const
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec("SELECT COUNT(*) FROM deletion_history");
    if (q.next()) return q.value(0).toInt();
    return 0;
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void DeletionHistoryManager::pruneOldest()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec(QString("DELETE FROM deletion_history WHERE id IN "
                    "(SELECT id FROM deletion_history ORDER BY deleted_at ASC "
                    "LIMIT MAX(0, (SELECT COUNT(*) FROM deletion_history) - %1))")
           .arg(MAX_ENTRIES));
}

DeletionHistoryEntry DeletionHistoryManager::rowToEntry(const QSqlQuery &q) const
{
    DeletionHistoryEntry e;
    e.id            = q.value("id").toInt();
    e.lid           = q.value("lid").toInt();
    e.aid           = q.value("aid").toInt();
    e.eid           = q.value("eid").toInt();
    e.replacedByLid = q.value("replaced_by_lid").isNull() ? -1 : q.value("replaced_by_lid").toInt();
    e.filePath      = q.value("file_path").toString();
    e.animeName     = q.value("anime_name").toString();
    e.episodeLabel  = q.value("episode_label").toString();
    e.fileSize      = q.value("file_size").toLongLong();
    e.tier          = q.value("tier").toInt();
    e.reason        = q.value("reason").toString();
    e.learnedScore  = q.value("learned_score").isNull() ? -1.0 : q.value("learned_score").toDouble();
    e.deletionType  = q.value("deletion_type").toString();
    e.spaceBefore   = q.value("space_before").toLongLong();
    e.spaceAfter    = q.value("space_after").toLongLong();
    e.deletedAt     = q.value("deleted_at").toLongLong();
    return e;
}
