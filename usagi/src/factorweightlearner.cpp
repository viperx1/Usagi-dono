#include "factorweightlearner.h"
#include "logger.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <cmath>

// ---------------------------------------------------------------------------
// Factor names (canonical list)
// ---------------------------------------------------------------------------

const QStringList &FactorWeightLearner::factorNames()
{
    static const QStringList names = {
        QStringLiteral("anime_rating"),
        QStringLiteral("size_weighted_distance"),
        QStringLiteral("group_status"),
        QStringLiteral("watch_recency"),
        QStringLiteral("view_percentage")
    };
    return names;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FactorWeightLearner::FactorWeightLearner(QObject *parent)
    : QObject(parent)
{
    for (const QString &f : factorNames()) {
        m_weights[f] = 0.0;
    }
}

// ---------------------------------------------------------------------------
// Table setup
// ---------------------------------------------------------------------------

void FactorWeightLearner::ensureTablesExist()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("FactorWeightLearner: database not open");
        return;
    }
    QSqlQuery q(db);

    q.exec("CREATE TABLE IF NOT EXISTS deletion_factor_weights ("
           "factor TEXT PRIMARY KEY,"
           "weight REAL DEFAULT 0.0,"
           "total_adjustments INTEGER DEFAULT 0"
           ")");

    q.exec("CREATE TABLE IF NOT EXISTS deletion_choices ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "kept_lid INTEGER,"
           "deleted_lid INTEGER,"
           "kept_factors TEXT,"
           "deleted_factors TEXT,"
           "chosen_at INTEGER"
           ")");
    q.exec("CREATE INDEX IF NOT EXISTS idx_deletion_choices_time ON deletion_choices(chosen_at)");

    loadWeights();
    LOG("FactorWeightLearner: tables ensured");
}

// ---------------------------------------------------------------------------
// Weight access
// ---------------------------------------------------------------------------

double FactorWeightLearner::getWeight(const QString &factor) const
{
    return m_weights.value(factor, 0.0);
}

QMap<QString, double> FactorWeightLearner::allWeights() const
{
    return m_weights;
}

int FactorWeightLearner::totalChoicesMade() const
{
    return m_totalChoices;
}

bool FactorWeightLearner::isTrained() const
{
    return m_totalChoices >= MIN_CHOICES;
}

// ---------------------------------------------------------------------------
// Score computation
// ---------------------------------------------------------------------------

double FactorWeightLearner::computeScore(const QMap<QString, double> &normalizedFactors) const
{
    double score = 0.0;
    for (auto it = m_weights.constBegin(); it != m_weights.constEnd(); ++it) {
        score += it.value() * normalizedFactors.value(it.key(), 0.0);
    }
    return score;
}

// ---------------------------------------------------------------------------
// A vs B choice processing
// ---------------------------------------------------------------------------

void FactorWeightLearner::recordChoice(int keptLid, int deletedLid,
                                       const QMap<QString, double> &keptFactors,
                                       const QMap<QString, double> &deletedFactors)
{
    // 1. Adjust weights
    for (const QString &f : factorNames()) {
        double diff = keptFactors.value(f, 0.0) - deletedFactors.value(f, 0.0);
        if (std::abs(diff) < MIN_FACTOR_DIFFERENCE) {
            continue;
        }
        double direction = (diff > 0.0) ? 1.0 : -1.0;
        adjustWeight(f, LEARNING_RATE * direction);
    }

    // 2. Store choice in history
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    QJsonObject keptObj, deletedObj;
    for (auto it = keptFactors.constBegin(); it != keptFactors.constEnd(); ++it)
        keptObj[it.key()] = it.value();
    for (auto it = deletedFactors.constBegin(); it != deletedFactors.constEnd(); ++it)
        deletedObj[it.key()] = it.value();

    q.prepare("INSERT INTO deletion_choices (kept_lid, deleted_lid, kept_factors, deleted_factors, chosen_at) "
              "VALUES (:kl, :dl, :kf, :df, :ts)");
    q.bindValue(":kl", keptLid);
    q.bindValue(":dl", deletedLid);
    q.bindValue(":kf", QString::fromUtf8(QJsonDocument(keptObj).toJson(QJsonDocument::Compact)));
    q.bindValue(":df", QString::fromUtf8(QJsonDocument(deletedObj).toJson(QJsonDocument::Compact)));
    q.bindValue(":ts", QDateTime::currentSecsSinceEpoch());
    q.exec();

    ++m_totalChoices;
    saveWeights();
    LOG(QString("FactorWeightLearner: recorded choice (kept=%1, deleted=%2, total=%3)")
        .arg(keptLid).arg(deletedLid).arg(m_totalChoices));
    emit weightsUpdated();
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void FactorWeightLearner::resetAllWeights()
{
    for (auto it = m_weights.begin(); it != m_weights.end(); ++it) {
        it.value() = 0.0;
    }
    m_totalChoices = 0;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec("DELETE FROM deletion_factor_weights");
    q.exec("DELETE FROM deletion_choices");

    LOG("FactorWeightLearner: all weights and history reset");
    emit weightsUpdated();
}

// ---------------------------------------------------------------------------
// Confidence
// ---------------------------------------------------------------------------

double FactorWeightLearner::scoreDifference(const QMap<QString, double> &factors1,
                                             const QMap<QString, double> &factors2) const
{
    return std::abs(computeScore(factors1) - computeScore(factors2));
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void FactorWeightLearner::loadWeights()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.exec("SELECT factor, weight FROM deletion_factor_weights");
    while (q.next()) {
        QString factor = q.value(0).toString();
        if (m_weights.contains(factor)) {
            m_weights[factor] = q.value(1).toDouble();
        }
    }

    q.exec("SELECT COUNT(*) FROM deletion_choices");
    if (q.next()) {
        m_totalChoices = q.value(0).toInt();
    }
}

void FactorWeightLearner::saveWeights()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    for (auto it = m_weights.constBegin(); it != m_weights.constEnd(); ++it) {
        q.prepare("INSERT OR REPLACE INTO deletion_factor_weights (factor, weight, total_adjustments) "
                  "VALUES (:f, :w, COALESCE((SELECT total_adjustments FROM deletion_factor_weights WHERE factor = :f2), 0))");
        q.bindValue(":f",  it.key());
        q.bindValue(":w",  it.value());
        q.bindValue(":f2", it.key());
        q.exec();
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void FactorWeightLearner::adjustWeight(const QString &factor, double delta)
{
    m_weights[factor] += delta;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO deletion_factor_weights (factor, weight, total_adjustments) "
              "VALUES (:f, :d, 1) "
              "ON CONFLICT(factor) DO UPDATE SET weight = weight + :d2, total_adjustments = total_adjustments + 1");
    q.bindValue(":f",  factor);
    q.bindValue(":d",  delta);
    q.bindValue(":d2", delta);
    q.exec();
}
