#ifndef FACTORWEIGHTLEARNER_H
#define FACTORWEIGHTLEARNER_H

#include <QObject>
#include <QMap>
#include <QString>

/**
 * @brief Manages learned factor weights and processes A vs B choices.
 *
 * Learnable factors:
 *   anime_rating, size_weighted_distance, group_status,
 *   watch_recency, view_percentage
 *
 * All weights start at 0.  Each A vs B choice adjusts the weights of
 * factors that meaningfully differ between the two files.
 */
class FactorWeightLearner : public QObject
{
    Q_OBJECT

public:
    explicit FactorWeightLearner(QObject *parent = nullptr);

    // ── Table setup ──
    void ensureTablesExist();

    // ── Weight access ──
    double getWeight(const QString &factor) const;
    QMap<QString, double> allWeights() const;
    int totalChoicesMade() const;
    bool isTrained() const;   ///< totalChoicesMade >= MIN_CHOICES

    // ── Score computation ──
    double computeScore(const QMap<QString, double> &normalizedFactors) const;

    // ── A vs B choice processing ──
    void recordChoice(int keptLid, int deletedLid,
                      const QMap<QString, double> &keptFactors,
                      const QMap<QString, double> &deletedFactors);

    // ── Reset ──
    void resetAllWeights();

    // ── Confidence ──
    double scoreDifference(const QMap<QString, double> &factors1,
                           const QMap<QString, double> &factors2) const;

    // ── Persistence ──
    void loadWeights();
    void saveWeights();

    // ── Constants ──
    static constexpr int    MIN_CHOICES            = 50;
    static constexpr double LEARNING_RATE          = 0.1;
    static constexpr double MIN_FACTOR_DIFFERENCE  = 0.01;
    static constexpr double CONFIDENCE_THRESHOLD   = 0.1;

    // ── Factor names ──
    static const QStringList &factorNames();

signals:
    void weightsUpdated();

private:
    void adjustWeight(const QString &factor, double delta);

    QMap<QString, double> m_weights;
    int m_totalChoices = 0;
};

#endif // FACTORWEIGHTLEARNER_H
