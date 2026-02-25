#include "hybriddeletionclassifier.h"
#include "deletionlockmanager.h"
#include "factorweightlearner.h"
#include "watchsessionmanager.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <cmath>

HybridDeletionClassifier::HybridDeletionClassifier(
        const DeletionLockManager &lockManager,
        const FactorWeightLearner &learner,
        const WatchSessionManager &sessionManager,
        QObject *parent)
    : QObject(parent)
    , m_lockManager(lockManager)
    , m_learner(learner)
    , m_sessionManager(sessionManager)
{
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

DeletionCandidate HybridDeletionClassifier::classify(int lid) const
{
    DeletionCandidate c;
    c.lid = lid;

    // Fetch basic metadata once
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT m.aid, m.eid, lf.path, a.nameromaji "
              "FROM mylist m "
              "LEFT JOIN local_files lf ON lf.id = m.local_file "
              "LEFT JOIN anime a ON a.aid = m.aid "
              "WHERE m.lid = :lid");
    q.bindValue(":lid", lid);
    if (!q.exec() || !q.next()) {
        LOG(QString("HybridDeletionClassifier: classify query failed for lid=%1: %2")
            .arg(lid).arg(q.lastError().text()));
        c.tier = DeletionTier::PROTECTED;
        c.reason = "File not found in database";
        return c;
    }
    c.aid       = q.value(0).toInt();
    c.eid       = q.value(1).toInt();
    c.filePath  = q.value(2).toString();
    c.animeName = q.value(3).toString();

    // ── Absolute protections ──
    if (m_lockManager.isFileLocked(lid)) {
        c.tier   = DeletionTier::PROTECTED;
        c.locked = true;
        c.reason = m_lockManager.isAnimeLocked(c.aid)
                   ? "Anime locked (highest rated kept)"
                   : "Episode locked (highest rated kept)";
        return c;
    }

    // Gap protection delegated to WatchSessionManager's existing wouldCreateGap logic
    // (called externally by DeletionQueue before acting on a candidate)

    // ── Tier 0: superseded revision ──
    DeletionCandidate t0 = classifyTier0(lid);
    if (t0.tier == DeletionTier::SUPERSEDED_REVISION) {
        t0.aid = c.aid; t0.eid = c.eid; t0.filePath = c.filePath; t0.animeName = c.animeName;
        return t0;
    }

    // ── Tier 1: low-quality duplicate ──
    DeletionCandidate t1 = classifyTier1(lid);
    if (t1.tier == DeletionTier::LOW_QUALITY_DUPLICATE) {
        t1.aid = c.aid; t1.eid = c.eid; t1.filePath = c.filePath; t1.animeName = c.animeName;
        return t1;
    }

    // ── Tier 2: language mismatch ──
    DeletionCandidate t2 = classifyTier2(lid);
    if (t2.tier == DeletionTier::LANGUAGE_MISMATCH) {
        t2.aid = c.aid; t2.eid = c.eid; t2.filePath = c.filePath; t2.animeName = c.animeName;
        return t2;
    }

    // ── Tier 3: learned preference ──
    if (isEligibleForDeletion(lid)) {
        DeletionCandidate t3 = classifyTier3(lid);
        t3.aid = c.aid; t3.eid = c.eid; t3.filePath = c.filePath; t3.animeName = c.animeName;
        return t3;
    }

    // ── Protected ──
    c.tier   = DeletionTier::PROTECTED;
    c.reason = "Protected (not eligible for deletion)";
    return c;
}

QMap<QString, double> HybridDeletionClassifier::normalizeFactors(int lid) const
{
    QMap<QString, double> factors;
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // anime_rating: AniDB rating / 1000 → 0.0–1.0
    q.prepare("SELECT a.rating FROM mylist m JOIN anime a ON a.aid = m.aid WHERE m.lid = :lid");
    q.bindValue(":lid", lid);
    if (q.exec() && q.next()) {
        int rating = q.value(0).toInt();
        factors["anime_rating"] = qBound(0.0, rating / 1000.0, 1.0);
    } else {
        factors["anime_rating"] = 0.5;
    }

    // size_weighted_distance: simplified — use episode distance × file size,
    // normalized to 0-1 with per-anime max.
    // For now: use a placeholder 0.5 (full implementation needs session context).
    factors["size_weighted_distance"] = 0.5;

    // group_status: active=1.0, stalled=0.5, disbanded=0.0
    q.prepare("SELECT g.status FROM mylist m "
              "JOIN file f ON f.fid = m.fid "
              "LEFT JOIN anidb_groups g ON g.gid = f.gid "
              "WHERE m.lid = :lid");
    q.bindValue(":lid", lid);
    if (q.exec() && q.next()) {
        int status = q.value(0).toInt();
        if (status == 1) factors["group_status"] = 1.0;       // ongoing/active
        else if (status == 2) factors["group_status"] = 0.5;   // stalled
        else if (status == 3) factors["group_status"] = 0.0;   // disbanded
        else factors["group_status"] = 0.5;
    } else {
        factors["group_status"] = 0.5;
    }

    // watch_recency: placeholder 0.5 (full implementation needs watch timestamps)
    factors["watch_recency"] = 0.5;

    // view_percentage: default 0.5 for no-session anime (Q5/Q11)
    factors["view_percentage"] = 0.5;

    return factors;
}

// ---------------------------------------------------------------------------
// Tier 0: superseded revision
// ---------------------------------------------------------------------------

DeletionCandidate HybridDeletionClassifier::classifyTier0(int lid) const
{
    DeletionCandidate c;
    c.lid = lid;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT m.eid, f.state FROM mylist m JOIN file f ON f.fid = m.fid WHERE m.lid = :lid");
    q.bindValue(":lid", lid);
    if (!q.exec() || !q.next()) return c;
    int eid = q.value(0).toInt();
    int state = q.value(1).toInt();

    int version = 1;
    if (state & 32) version = 5;
    else if (state & 16) version = 4;
    else if (state & 8) version = 3;
    else if (state & 4) version = 2;

    q.prepare("SELECT m2.lid, lf.path FROM mylist m2 "
              "JOIN file f2 ON f2.fid = m2.fid "
              "JOIN local_files lf ON lf.id = m2.local_file "
              "WHERE m2.eid = :eid AND m2.lid != :lid AND lf.path IS NOT NULL "
              "AND ((f2.state & 32) > 0 AND :v < 5 "
              "  OR (f2.state & 16) > 0 AND :v2 < 4 "
              "  OR (f2.state & 8) > 0  AND :v3 < 3 "
              "  OR (f2.state & 4) > 0  AND :v4 < 2) "
              "LIMIT 1");
    q.bindValue(":eid", eid);
    q.bindValue(":lid", lid);
    q.bindValue(":v", version);
    q.bindValue(":v2", version);
    q.bindValue(":v3", version);
    q.bindValue(":v4", version);
    if (q.exec() && q.next()) {
        c.tier = DeletionTier::SUPERSEDED_REVISION;
        c.replacementLid  = q.value(0).toInt();
        c.replacementPath = q.value(1).toString();
        c.reason = "Superseded by newer local revision";
        c.learnedScore = 0.0;
    }
    return c;
}

// ---------------------------------------------------------------------------
// Tier 1: low-quality duplicate
// ---------------------------------------------------------------------------

DeletionCandidate HybridDeletionClassifier::classifyTier1(int lid) const
{
    DeletionCandidate c;
    c.lid = lid;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // Check if file is unwatched and a better-quality file exists for the same episode
    q.prepare("SELECT m.eid, m.viewed, f.quality FROM mylist m "
              "JOIN file f ON f.fid = m.fid WHERE m.lid = :lid");
    q.bindValue(":lid", lid);
    if (!q.exec() || !q.next()) return c;
    int eid = q.value(0).toInt();
    int viewed = q.value(1).toInt();
    QString quality = q.value(2).toString();

    if (viewed > 0) return c;  // Watched files don't qualify for T1

    // Check for higher-quality local file for the same episode
    q.prepare("SELECT m2.lid, lf.path FROM mylist m2 "
              "JOIN file f2 ON f2.fid = m2.fid "
              "JOIN local_files lf ON lf.id = m2.local_file "
              "WHERE m2.eid = :eid AND m2.lid != :lid AND lf.path IS NOT NULL "
              "AND f2.quality > :q "
              "LIMIT 1");
    q.bindValue(":eid", eid);
    q.bindValue(":lid", lid);
    q.bindValue(":q", quality);
    if (q.exec() && q.next()) {
        c.tier = DeletionTier::LOW_QUALITY_DUPLICATE;
        c.replacementLid  = q.value(0).toInt();
        c.replacementPath = q.value(1).toString();
        c.reason = QString("Lower quality duplicate (quality: %1)").arg(quality);
        c.learnedScore = 0.0;
    }
    return c;
}

// ---------------------------------------------------------------------------
// Tier 2: language mismatch
// ---------------------------------------------------------------------------

DeletionCandidate HybridDeletionClassifier::classifyTier2(int lid) const
{
    DeletionCandidate c;
    c.lid = lid;

    // Check if file doesn't match audio/subtitle preferences AND a better alternative exists.
    bool audioMatch = m_sessionManager.matchesPreferredAudioLanguage(lid);
    bool subMatch   = m_sessionManager.matchesPreferredSubtitleLanguage(lid);

    if (audioMatch && subMatch) return c;

    QString myAudio = m_sessionManager.getFileAudioLanguage(lid);
    QString mySub   = m_sessionManager.getFileSubtitleLanguage(lid);

    // Check if a better-matching alternative exists for the same episode
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT m2.lid, lf.path FROM mylist m2 "
              "JOIN local_files lf ON lf.id = m2.local_file "
              "WHERE m2.eid = (SELECT eid FROM mylist WHERE lid = :lid) "
              "AND m2.lid != :lid2 AND lf.path IS NOT NULL");
    q.bindValue(":lid", lid);
    q.bindValue(":lid2", lid);
    if (!q.exec()) return c;

    while (q.next()) {
        int altLid = q.value(0).toInt();
        bool altAudio = m_sessionManager.matchesPreferredAudioLanguage(altLid);
        bool altSub   = m_sessionManager.matchesPreferredSubtitleLanguage(altLid);
        // Alternative is better if it matches at least as well and better on at least one
        if ((altAudio || !audioMatch) && (altSub || !subMatch)
            && (altAudio > audioMatch || altSub > subMatch)) {
            c.tier = DeletionTier::LANGUAGE_MISMATCH;
            c.replacementLid  = altLid;
            c.replacementPath = q.value(1).toString();
            c.learnedScore = 0.0;

            QString altAudioLang = m_sessionManager.getFileAudioLanguage(altLid);
            QString altSubLang   = m_sessionManager.getFileSubtitleLanguage(altLid);

            QStringList parts;
            if (!audioMatch)
                parts << QString("dub: %1 → %2").arg(myAudio.isEmpty() ? "none" : myAudio,
                                                       altAudioLang.isEmpty() ? "none" : altAudioLang);
            if (!subMatch)
                parts << QString("sub: %1 → %2").arg(mySub.isEmpty() ? "none" : mySub,
                                                       altSubLang.isEmpty() ? "none" : altSubLang);
            c.reason = QString("Language mismatch (%1)").arg(parts.join(", "));
            return c;
        }
    }
    return c;
}

// ---------------------------------------------------------------------------
// Tier 3: learned preference
// ---------------------------------------------------------------------------

DeletionCandidate HybridDeletionClassifier::classifyTier3(int lid) const
{
    DeletionCandidate c;
    c.lid = lid;
    c.tier = DeletionTier::LEARNED_PREFERENCE;
    c.factorValues = normalizeFactors(lid);
    c.learnedScore = m_learner.computeScore(c.factorValues);
    c.reason = QString("Score: %1").arg(c.learnedScore, 0, 'f', 2);
    return c;
}

// ---------------------------------------------------------------------------
// Eligibility check (simplified)
// ---------------------------------------------------------------------------

bool HybridDeletionClassifier::isEligibleForDeletion(int lid) const
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT m.viewed FROM mylist m WHERE m.lid = :lid");
    q.bindValue(":lid", lid);
    if (!q.exec() || !q.next()) return false;
    int viewed = q.value(0).toInt();

    // For now: watched files and files far from current session are eligible.
    // Full session-distance logic would be added with per-chain session migration.
    return viewed > 0;
}
