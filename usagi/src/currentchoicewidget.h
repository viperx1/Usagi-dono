#ifndef CURRENTCHOICEWIDGET_H
#define CURRENTCHOICEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>

class AnimeCard;
class MyListCardManager;
class DeletionQueue;
class DeletionHistoryManager;
class FactorWeightLearner;
class DeletionLockManager;
struct DeletionCandidate;
struct DeletionHistoryEntry;

/**
 * @brief The "Deletion" tab — central workspace for understanding
 *        and controlling the hybrid deletion system.
 *
 * Layout (vertical groupboxes, no sub-tabs):
 *   1. Header bar:  [PREVIEW] label + Space indicator + Run Now / Pause
 *   2. A vs B  (top-left)  +  Learned Weights (top-right) — side by side
 *   3. Deletion Queue  (full width)
 *   4. Deletion History (full width)
 */
class CurrentChoiceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CurrentChoiceWidget(DeletionQueue &queue,
                                 DeletionHistoryManager &history,
                                 FactorWeightLearner &learner,
                                 DeletionLockManager &lockManager,
                                 MyListCardManager &cardManager,
                                 QWidget *parent = nullptr);

    /// Refresh all sub-widgets from the current queue/history state.
    void refresh();

    /// Update the space indicator label.
    void updateSpaceIndicator(qint64 usedBytes, qint64 totalBytes);

    /// Switch between preview and active mode.
    void setPreviewMode(bool preview);

signals:
    void deleteFileRequested(int lid);
    void runNowRequested();
    void pauseRequested();
    void lockAnimeRequested(int aid);
    void unlockAnimeRequested(int aid);
    void lockEpisodeRequested(int eid);
    void unlockEpisodeRequested(int eid);

private slots:
    void onDeleteAClicked();
    void onDeleteBClicked();
    void onSkipClicked();
    void onRunNowClicked();
    void onPauseClicked();
    void onResetWeightsClicked();
    void onQueueItemClicked(QTreeWidgetItem *item, int column);
    void onHistoryItemClicked(QTreeWidgetItem *item, int column);
    void onHistoryFilterChanged();

private:
    void setupUI();
    void setupHeaderBar(QVBoxLayout *targetLayout);
    void setupAvsBGroupBox();
    void setupWeightsGroupBox();
    void setupQueueGroupBox();
    void setupHistoryGroupBox();

    void populateAvsB(const DeletionCandidate &a, const DeletionCandidate &b);
    void populateAvsBReadOnly(const DeletionHistoryEntry &entry);
    void populateAvsBSingleConfirmation(const DeletionCandidate &candidate);
    void populateWeights();
    void populateQueue();
    void populateHistory();
    void clearAvsB();
    void showCandidateInAvsB(const DeletionCandidate &c, bool isLocked);
    void showCandidatePair(const DeletionCandidate &a, const DeletionCandidate &b);
    DeletionCandidate queryFileDetails(int lid) const;

    void clearCards();
    void showCardForSide(int aid, QVBoxLayout *container, AnimeCard *&cardSlot);

    QString formatFileDetails(const DeletionCandidate &c) const;
    QString formatTier(int tier) const;

    // ── References ──
    DeletionQueue &m_queue;
    DeletionHistoryManager &m_history;
    FactorWeightLearner &m_learner;
    DeletionLockManager &m_lockManager;
    MyListCardManager &m_cardManager;

    // ── State ──
    int m_currentALid = -1;
    int m_currentBLid = -1;
    bool m_readOnlyMode = false;
    bool m_previewMode  = true;

    // ── Header ──
    QLabel *m_previewLabel;
    QLabel *m_spaceLabel;
    QPushButton *m_runNowButton;
    QPushButton *m_pauseButton;

    // ── A vs B ──
    QGroupBox *m_avsbGroupBox;
    QLabel *m_avsbStatusLabel;
    QVBoxLayout *m_sideALayout;    ///< Container for card A + info label
    QVBoxLayout *m_sideBLayout;    ///< Container for card B + info label
    AnimeCard *m_cardA = nullptr;  ///< Standalone card for side A (owned)
    AnimeCard *m_cardB = nullptr;  ///< Standalone card for side B (owned)
    QLabel *m_infoALabel;          ///< Deletion info below card A
    QLabel *m_infoBLabel;          ///< Deletion info below card B
    QPushButton *m_deleteAButton;
    QPushButton *m_deleteBButton;
    QPushButton *m_skipButton;
    QPushButton *m_backToQueueButton;

    // ── Weights ──
    QGroupBox *m_weightsGroupBox;
    QLabel *m_choicesCountLabel;
    QTreeWidget *m_weightsTree;
    QPushButton *m_resetWeightsButton;

    // ── Queue ──
    QGroupBox *m_queueGroupBox;
    QLabel *m_queueSummaryLabel;
    QTreeWidget *m_queueTree;

    // ── History ──
    QGroupBox *m_historyGroupBox;
    QLabel *m_totalFreedLabel;
    QComboBox *m_historyAnimeFilter;
    QComboBox *m_historyTypeFilter;
    QTreeWidget *m_historyTree;
};

#endif // CURRENTCHOICEWIDGET_H
