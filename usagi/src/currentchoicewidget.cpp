#include "currentchoicewidget.h"
#include "animecard.h"
#include "mylistcardmanager.h"
#include "deletionqueue.h"
#include "deletionhistorymanager.h"
#include "factorweightlearner.h"
#include "deletionlockmanager.h"
#include "deletioncandidate.h"
#include "deletionhistoryentry.h"
#include "logger.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollArea>
#include <QFrame>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <cmath>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CurrentChoiceWidget::CurrentChoiceWidget(DeletionQueue &queue,
                                         DeletionHistoryManager &history,
                                         FactorWeightLearner &learner,
                                         DeletionLockManager &lockManager,
                                         MyListCardManager &cardManager,
                                         QWidget *parent)
    : QWidget(parent)
    , m_queue(queue)
    , m_history(history)
    , m_learner(learner)
    , m_lockManager(lockManager)
    , m_cardManager(cardManager)
{
    setupUI();
}

// ---------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------

void CurrentChoiceWidget::refresh()
{
    populateWeights();
    populateQueue();
    populateHistory();

    if (m_queue.needsUserChoice()) {
        auto pair = m_queue.getAvsBPair();
        if (pair.second.lid > 0) {
            populateAvsB(pair.first, pair.second);
        } else {
            populateAvsBSingleConfirmation(pair.first);
        }
    } else {
        clearAvsB();
    }
}

void CurrentChoiceWidget::updateSpaceIndicator(qint64 usedBytes, qint64 totalBytes)
{
    double usedGB  = usedBytes  / (1024.0 * 1024 * 1024);
    double totalGB = totalBytes / (1024.0 * 1024 * 1024);
    m_spaceLabel->setText(QString("Space: %1 / %2 GB")
                          .arg(usedGB, 0, 'f', 1)
                          .arg(totalGB, 0, 'f', 0));
}

void CurrentChoiceWidget::setPreviewMode(bool preview)
{
    m_previewMode = preview;
    m_previewLabel->setVisible(preview);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void CurrentChoiceWidget::setupUI()
{
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outerLayout->addWidget(scroll);

    QWidget *content = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    scroll->setWidget(content);

    setupHeaderBar(mainLayout);

    // Top row: A vs B + Weights side by side
    QHBoxLayout *topRow = new QHBoxLayout;
    setupAvsBGroupBox();
    setupWeightsGroupBox();
    topRow->addWidget(m_avsbGroupBox, 3);
    topRow->addWidget(m_weightsGroupBox, 1);
    mainLayout->addLayout(topRow);

    setupQueueGroupBox();
    mainLayout->addWidget(m_queueGroupBox);

    setupHistoryGroupBox();
    mainLayout->addWidget(m_historyGroupBox);
}

void CurrentChoiceWidget::setupHeaderBar(QVBoxLayout *targetLayout)
{
    QHBoxLayout *header = new QHBoxLayout;

    QLabel *titleLabel = new QLabel("Deletion Management");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    header->addWidget(titleLabel);

    m_previewLabel = new QLabel("[PREVIEW]");
    m_previewLabel->setStyleSheet("color: #888; font-weight: bold; font-size: 14px;");
    header->addWidget(m_previewLabel);

    header->addStretch();

    m_spaceLabel = new QLabel("Space: — / — GB");
    header->addWidget(m_spaceLabel);

    m_runNowButton = new QPushButton(QString::fromUtf8("\u25B6 Run Now"));  // ▶
    connect(m_runNowButton, &QPushButton::clicked, this, &CurrentChoiceWidget::onRunNowClicked);
    header->addWidget(m_runNowButton);

    m_pauseButton = new QPushButton(QString::fromUtf8("\u23F8 Pause"));  // ⏸
    connect(m_pauseButton, &QPushButton::clicked, this, &CurrentChoiceWidget::onPauseClicked);
    header->addWidget(m_pauseButton);

    targetLayout->addLayout(header);
}

void CurrentChoiceWidget::setupAvsBGroupBox()
{
    m_avsbGroupBox = new QGroupBox("A vs B");
    QVBoxLayout *lay = new QVBoxLayout(m_avsbGroupBox);

    m_avsbStatusLabel = new QLabel("No pending choice");
    m_avsbStatusLabel->setStyleSheet("font-weight: bold;");
    lay->addWidget(m_avsbStatusLabel);

    // Two side-by-side columns: each has a card slot + info label
    QHBoxLayout *filesRow = new QHBoxLayout;

    // Side A
    QWidget *sideAWidget = new QWidget;
    m_sideALayout = new QVBoxLayout(sideAWidget);
    m_sideALayout->setContentsMargins(0, 0, 0, 0);
    m_infoALabel = new QLabel;
    m_infoALabel->setWordWrap(true);
    m_infoALabel->setFrameShape(QFrame::StyledPanel);
    m_sideALayout->addWidget(m_infoALabel);

    // Side B
    QWidget *sideBWidget = new QWidget;
    m_sideBLayout = new QVBoxLayout(sideBWidget);
    m_sideBLayout->setContentsMargins(0, 0, 0, 0);
    m_infoBLabel = new QLabel;
    m_infoBLabel->setWordWrap(true);
    m_infoBLabel->setFrameShape(QFrame::StyledPanel);
    m_sideBLayout->addWidget(m_infoBLabel);

    filesRow->addWidget(sideAWidget, 1);
    filesRow->addWidget(sideBWidget, 1);
    lay->addLayout(filesRow);

    // Action buttons
    QHBoxLayout *btnRow = new QHBoxLayout;
    m_deleteAButton = new QPushButton("Delete A");
    m_deleteBButton = new QPushButton("Delete B");
    m_skipButton    = new QPushButton("Skip");
    m_backToQueueButton = new QPushButton("Back to queue");
    m_backToQueueButton->setVisible(false);

    connect(m_deleteAButton, &QPushButton::clicked, this, &CurrentChoiceWidget::onDeleteAClicked);
    connect(m_deleteBButton, &QPushButton::clicked, this, &CurrentChoiceWidget::onDeleteBClicked);
    connect(m_skipButton,    &QPushButton::clicked, this, &CurrentChoiceWidget::onSkipClicked);
    connect(m_backToQueueButton, &QPushButton::clicked, this, [this]() {
        m_readOnlyMode = false;
        refresh();
    });

    btnRow->addStretch();
    btnRow->addWidget(m_deleteAButton);
    btnRow->addWidget(m_deleteBButton);
    btnRow->addWidget(m_skipButton);
    btnRow->addWidget(m_backToQueueButton);
    btnRow->addStretch();
    lay->addLayout(btnRow);
}

void CurrentChoiceWidget::setupWeightsGroupBox()
{
    m_weightsGroupBox = new QGroupBox("Learned Weights");
    QVBoxLayout *lay = new QVBoxLayout(m_weightsGroupBox);

    m_choicesCountLabel = new QLabel("Choices: 0");
    lay->addWidget(m_choicesCountLabel);

    m_weightsTree = new QTreeWidget;
    m_weightsTree->setHeaderLabels({"Factor", "Weight", ""});
    m_weightsTree->setColumnCount(3);
    m_weightsTree->setRootIsDecorated(false);
    m_weightsTree->header()->setStretchLastSection(true);
    m_weightsTree->setMaximumHeight(200);
    lay->addWidget(m_weightsTree);

    m_resetWeightsButton = new QPushButton(QString::fromUtf8("Reset weights \u26A0"));  // ⚠
    connect(m_resetWeightsButton, &QPushButton::clicked, this, &CurrentChoiceWidget::onResetWeightsClicked);
    lay->addWidget(m_resetWeightsButton);
}

void CurrentChoiceWidget::setupQueueGroupBox()
{
    m_queueGroupBox = new QGroupBox("Deletion Queue");
    QVBoxLayout *lay = new QVBoxLayout(m_queueGroupBox);

    m_queueSummaryLabel = new QLabel;
    m_queueSummaryLabel->setStyleSheet("color: #888;");
    lay->addWidget(m_queueSummaryLabel);

    m_queueTree = new QTreeWidget;
    m_queueTree->setHeaderLabels({"#", "File", "Anime", "Tier", "Reason"});
    m_queueTree->setColumnCount(5);
    m_queueTree->setRootIsDecorated(false);
    m_queueTree->header()->setStretchLastSection(true);
    m_queueTree->setMinimumHeight(150);
    connect(m_queueTree, &QTreeWidget::itemClicked, this, &CurrentChoiceWidget::onQueueItemClicked);
    lay->addWidget(m_queueTree);
}

void CurrentChoiceWidget::setupHistoryGroupBox()
{
    m_historyGroupBox = new QGroupBox("Deletion History");
    QVBoxLayout *lay = new QVBoxLayout(m_historyGroupBox);

    // Filter row
    QHBoxLayout *filterRow = new QHBoxLayout;
    m_totalFreedLabel = new QLabel("Total freed: 0 GB");
    filterRow->addWidget(m_totalFreedLabel);
    filterRow->addStretch();

    filterRow->addWidget(new QLabel("Filter:"));
    m_historyAnimeFilter = new QComboBox;
    m_historyAnimeFilter->addItem("All");
    connect(m_historyAnimeFilter, &QComboBox::currentIndexChanged, this, &CurrentChoiceWidget::onHistoryFilterChanged);
    filterRow->addWidget(m_historyAnimeFilter);

    m_historyTypeFilter = new QComboBox;
    m_historyTypeFilter->addItem("All types");
    m_historyTypeFilter->addItem("procedural");
    m_historyTypeFilter->addItem("learned_auto");
    m_historyTypeFilter->addItem("user_avsb");
    m_historyTypeFilter->addItem("manual");
    connect(m_historyTypeFilter, &QComboBox::currentIndexChanged, this, &CurrentChoiceWidget::onHistoryFilterChanged);
    filterRow->addWidget(m_historyTypeFilter);
    lay->addLayout(filterRow);

    m_historyTree = new QTreeWidget;
    m_historyTree->setHeaderLabels({"Date", "File", "Anime", "Type", "Size"});
    m_historyTree->setColumnCount(5);
    m_historyTree->setRootIsDecorated(false);
    m_historyTree->header()->setStretchLastSection(true);
    m_historyTree->setMinimumHeight(150);
    connect(m_historyTree, &QTreeWidget::itemClicked, this, &CurrentChoiceWidget::onHistoryItemClicked);
    lay->addWidget(m_historyTree);
}

// ---------------------------------------------------------------------------
// Populate helpers
// ---------------------------------------------------------------------------

void CurrentChoiceWidget::populateAvsB(const DeletionCandidate &a, const DeletionCandidate &b)
{
    clearCards();
    m_currentALid = a.lid;
    m_currentBLid = b.lid;
    m_readOnlyMode = false;

    m_avsbStatusLabel->setText(QString::fromUtf8("\u26A1 Choice needed"));  // ⚡
    showCardForSide(a.aid, m_sideALayout, m_cardA);
    m_infoALabel->setText(QString("[A] %1\n%2").arg(a.filePath, formatFileDetails(a)));
    showCardForSide(b.aid, m_sideBLayout, m_cardB);
    m_infoBLabel->setText(QString("[B] %1\n%2").arg(b.filePath, formatFileDetails(b)));

    m_deleteAButton->setVisible(true);
    m_deleteBButton->setVisible(true);
    m_skipButton->setVisible(true);
    m_backToQueueButton->setVisible(false);
}

void CurrentChoiceWidget::populateAvsBSingleConfirmation(const DeletionCandidate &candidate)
{
    clearCards();
    m_currentALid = candidate.lid;
    m_currentBLid = -1;
    m_readOnlyMode = false;

    m_avsbStatusLabel->setText("Delete this file?");
    showCardForSide(candidate.aid, m_sideALayout, m_cardA);
    m_infoALabel->setText(QString("%1\n%2").arg(candidate.filePath, formatFileDetails(candidate)));
    m_infoBLabel->setText("");

    m_deleteAButton->setVisible(true);
    m_deleteAButton->setText("Yes");
    m_deleteBButton->setVisible(false);
    m_skipButton->setVisible(true);
    m_backToQueueButton->setVisible(false);
}

void CurrentChoiceWidget::populateAvsBReadOnly(const DeletionHistoryEntry &entry)
{
    clearCards();
    m_readOnlyMode = true;
    m_currentALid = -1;
    m_currentBLid = -1;

    m_avsbStatusLabel->setText("History entry (read-only)");
    showCardForSide(entry.aid, m_sideALayout, m_cardA);
    m_infoALabel->setText(QString("%1\n%2\nTier: %3\n%4")
                          .arg(entry.filePath, entry.animeName, formatTier(entry.tier), entry.reason));
    m_infoBLabel->setText(entry.replacedByLid > 0
                          ? QString("Replaced by lid %1").arg(entry.replacedByLid)
                          : "[File no longer present]");

    m_deleteAButton->setVisible(false);
    m_deleteBButton->setVisible(false);
    m_skipButton->setVisible(false);
    m_backToQueueButton->setVisible(true);
}

void CurrentChoiceWidget::clearAvsB()
{
    clearCards();
    m_currentALid = -1;
    m_currentBLid = -1;
    m_avsbStatusLabel->setText("No pending choice");
    m_infoALabel->setText("");
    m_infoBLabel->setText("");
    m_deleteAButton->setVisible(false);
    m_deleteAButton->setText("Delete A");
    m_deleteBButton->setVisible(false);
    m_skipButton->setVisible(false);
    m_backToQueueButton->setVisible(false);
}

void CurrentChoiceWidget::populateWeights()
{
    m_weightsTree->clear();
    int total = m_learner.totalChoicesMade();
    m_choicesCountLabel->setText(QString("Choices: %1%2")
                                .arg(total)
                                .arg(m_learner.isTrained() ? " (trained)" : ""));

    QMap<QString, double> weights = m_learner.allWeights();
    for (auto it = weights.constBegin(); it != weights.constEnd(); ++it) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_weightsTree);
        item->setText(0, it.key());
        item->setText(1, QString::number(it.value(), 'f', 2));
        // Simple bar visualization
        int barLen = static_cast<int>(std::abs(it.value()) * 10);
        QString bar = (it.value() >= 0)
                      ? QString(barLen, QChar(0x2588))   // █
                      : QString(barLen, QChar(0x2591));   // ░
        item->setText(2, bar);
    }
}

void CurrentChoiceWidget::populateQueue()
{
    m_queueTree->clear();

    m_queueSummaryLabel->setText(
        QString("%1 candidates \u2022 %2 protected \u2022 %3 locked \u2014 %4 local files classified")
            .arg(m_queue.candidates().size())
            .arg(m_queue.protectedCount())
            .arg(m_queue.lockedFiles().size())
            .arg(m_queue.totalClassified()));

    int rank = 0;

    for (const DeletionCandidate &c : m_queue.candidates()) {
        ++rank;
        QTreeWidgetItem *item = new QTreeWidgetItem(m_queueTree);
        item->setText(0, QString::number(rank));
        item->setText(1, c.filePath);
        item->setText(2, c.animeName);
        item->setText(3, formatTier(c.tier));
        item->setText(4, c.reason);
        item->setData(0, Qt::UserRole, c.lid);
    }

    for (const DeletionCandidate &c : m_queue.lockedFiles()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_queueTree);
        item->setText(0, QString::fromUtf8("\xF0\x9F\x94\x92"));   // 🔒
        item->setText(1, c.filePath);
        item->setText(2, c.animeName);
        item->setText(3, QString::fromUtf8("\u2014"));  // —
        item->setText(4, c.reason);
        item->setData(0, Qt::UserRole, c.lid);
        item->setData(0, Qt::UserRole + 1, true);  // isLocked flag
    }
}

void CurrentChoiceWidget::populateHistory()
{
    m_historyTree->clear();
    QString typeFilter = m_historyTypeFilter->currentText();
    QList<DeletionHistoryEntry> entries;
    if (typeFilter == "All types") {
        entries = m_history.allEntries(200);
    } else {
        entries = m_history.entriesByType(typeFilter);
    }

    for (const DeletionHistoryEntry &e : entries) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_historyTree);
        item->setText(0, QDateTime::fromSecsSinceEpoch(e.deletedAt).toString("yyyy-MM-dd hh:mm"));
        item->setText(1, e.filePath);
        item->setText(2, e.animeName);
        item->setText(3, e.deletionType);
        double sizeGB = e.fileSize / (1024.0 * 1024 * 1024);
        item->setText(4, sizeGB >= 1.0
                         ? QString("%1 GB").arg(sizeGB, 0, 'f', 1)
                         : QString("%1 MB").arg(e.fileSize / (1024.0 * 1024), 0, 'f', 0));
        item->setData(0, Qt::UserRole, e.id);
    }

    double totalFreedGB = m_history.totalSpaceFreed() / (1024.0 * 1024 * 1024);
    m_totalFreedLabel->setText(QString("Total freed: %1 GB").arg(totalFreedGB, 0, 'f', 1));
}

// ---------------------------------------------------------------------------
// Format helpers
// ---------------------------------------------------------------------------

QString CurrentChoiceWidget::formatFileDetails(const DeletionCandidate &c) const
{
    QStringList parts;
    parts << c.animeName;
    if (!c.episodeLabel.isEmpty()) parts << c.episodeLabel;
    parts << QString("Tier: %1").arg(formatTier(c.tier));
    parts << c.reason;
    if (c.locked) parts << QString::fromUtf8("\xF0\x9F\x94\x92 Locked");
    return parts.join("\n");
}

QString CurrentChoiceWidget::formatTier(int tier) const
{
    switch (tier) {
    case DeletionTier::HIDDEN_ANIME:          return "T0 Hidden anime";
    case DeletionTier::SUPERSEDED_REVISION:   return "T1 Superseded";
    case DeletionTier::LOW_QUALITY_DUPLICATE: return "T2 Low-quality dup";
    case DeletionTier::LANGUAGE_MISMATCH:     return "T3 Lang mismatch";
    case DeletionTier::LEARNED_PREFERENCE:    return "T4 Learned";
    case DeletionTier::PROTECTED:             return "Protected";
    default:                                   return "?";
    }
}

// ---------------------------------------------------------------------------
// Queue inspection (read-only A vs B)
// ---------------------------------------------------------------------------

void CurrentChoiceWidget::showCandidateInAvsB(const DeletionCandidate &c, bool isLocked)
{
    clearCards();
    showCardForSide(c.aid, m_sideALayout, m_cardA);
    m_infoALabel->setText(QString("[A] %1\n%2").arg(c.filePath, formatFileDetails(c)));

    if (isLocked) {
        m_currentALid = -1;
        m_currentBLid = -1;
        m_readOnlyMode = true;
        m_avsbStatusLabel->setText(QString::fromUtf8("\xF0\x9F\x94\x92 Locked file"));
        m_infoBLabel->setText("");
        m_deleteAButton->setVisible(false);
        m_deleteBButton->setVisible(false);
        m_skipButton->setVisible(false);
        m_backToQueueButton->setVisible(true);
    } else {
        m_currentALid = c.lid;
        m_currentBLid = -1;
        m_readOnlyMode = false;
        m_avsbStatusLabel->setText(QString("Queue item \u2014 %1").arg(formatTier(c.tier)));
        m_infoBLabel->setText("");
        m_deleteAButton->setVisible(true);
        m_deleteAButton->setText("Delete A");
        m_deleteBButton->setVisible(false);
        m_skipButton->setVisible(true);
        m_backToQueueButton->setVisible(true);
    }
}

void CurrentChoiceWidget::showCandidatePair(const DeletionCandidate &a, const DeletionCandidate &b)
{
    clearCards();
    m_currentALid = a.lid;
    m_currentBLid = b.lid;
    m_readOnlyMode = false;

    m_avsbStatusLabel->setText(QString("Queue item \u2014 %1").arg(formatTier(a.tier)));
    showCardForSide(a.aid, m_sideALayout, m_cardA);
    m_infoALabel->setText(QString("[A] %1\n%2").arg(a.filePath, formatFileDetails(a)));
    showCardForSide(b.aid, m_sideBLayout, m_cardB);
    m_infoBLabel->setText(QString("[B] %1\n%2").arg(b.filePath, formatFileDetails(b)));

    m_deleteAButton->setVisible(true);
    m_deleteAButton->setText("Delete A");
    m_deleteBButton->setVisible(true);
    m_skipButton->setVisible(true);
    m_backToQueueButton->setVisible(true);
}

// ---------------------------------------------------------------------------
// Card helpers
// ---------------------------------------------------------------------------

void CurrentChoiceWidget::clearCards()
{
    if (m_cardA) {
        m_sideALayout->removeWidget(m_cardA);
        delete m_cardA;
        m_cardA = nullptr;
    }
    if (m_cardB) {
        m_sideBLayout->removeWidget(m_cardB);
        delete m_cardB;
        m_cardB = nullptr;
    }
}

void CurrentChoiceWidget::showCardForSide(int aid, QVBoxLayout *container, AnimeCard *&cardSlot)
{
    if (aid <= 0) return;
    AnimeCard *card = m_cardManager.createStandaloneCard(aid, this);
    if (!card) return;
    cardSlot = card;
    card->setFixedSize(AnimeCard::getCardSize());
    // Insert card before the info label (which is always the last widget)
    container->insertWidget(0, card);
}

DeletionCandidate CurrentChoiceWidget::queryFileDetails(int lid) const
{
    DeletionCandidate c;
    c.lid = lid;
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT lf.path, a.nameromaji, m.aid FROM mylist m "
              "LEFT JOIN local_files lf ON lf.id = m.local_file "
              "LEFT JOIN anime a ON a.aid = m.aid "
              "WHERE m.lid = :lid");
    q.bindValue(":lid", lid);
    if (q.exec() && q.next()) {
        c.filePath  = q.value(0).toString();
        c.animeName = q.value(1).toString();
        c.aid       = q.value(2).toInt();
    }
    return c;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void CurrentChoiceWidget::onDeleteAClicked()
{
    if (m_currentALid <= 0) return;
    if (m_currentBLid > 0) {
        // A vs B: delete A, keep B
        m_queue.recordChoice(m_currentBLid, m_currentALid);
        emit deleteFileRequested(m_currentALid);
    } else {
        // Single confirmation: delete
        emit deleteFileRequested(m_currentALid);
    }
    refresh();
}

void CurrentChoiceWidget::onDeleteBClicked()
{
    if (m_currentBLid <= 0) return;
    // A vs B: delete B, keep A
    m_queue.recordChoice(m_currentALid, m_currentBLid);
    emit deleteFileRequested(m_currentBLid);
    refresh();
}

void CurrentChoiceWidget::onSkipClicked()
{
    // Skip — no learning, no deletion
    clearAvsB();
}

void CurrentChoiceWidget::onRunNowClicked()
{
    emit runNowRequested();
}

void CurrentChoiceWidget::onPauseClicked()
{
    emit pauseRequested();
}

void CurrentChoiceWidget::onResetWeightsClicked()
{
    // Double confirmation per design doc (Q7/Q14)
    auto r1 = QMessageBox::warning(this, "Reset Weights",
                "This will reset all learned weights to 0 and clear all A vs B choice history.\n"
                "This action cannot be undone.\n\nContinue?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (r1 != QMessageBox::Yes) return;

    auto r2 = QMessageBox::warning(this, "Confirm Reset",
                "Are you sure? All learned deletion preferences will be lost.",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (r2 != QMessageBox::Yes) return;

    m_learner.resetAllWeights();
    refresh();
}

void CurrentChoiceWidget::onQueueItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    int lid = item->data(0, Qt::UserRole).toInt();
    bool isLocked = item->data(0, Qt::UserRole + 1).toBool();

    LOG(QString("[Queue] Click lid=%1 locked=%2").arg(lid).arg(isLocked));

    if (isLocked) {
        for (const DeletionCandidate &c : m_queue.lockedFiles()) {
            if (c.lid == lid) {
                showCandidateInAvsB(c, true);
                break;
            }
        }
        return;
    }

    const auto &candidates = m_queue.candidates();
    for (int i = 0; i < candidates.size(); ++i) {
        if (candidates[i].lid != lid) continue;
        const DeletionCandidate &c = candidates[i];

        LOG(QString("[Queue] Found candidate lid=%1 tier=%2 replacementLid=%3")
            .arg(c.lid).arg(c.tier).arg(c.replacementLid));

        if (c.replacementLid > 0) {
            // Tiers 0-2: pair with the replacement (kept) file
            DeletionCandidate replacement = queryFileDetails(c.replacementLid);
            replacement.tier = DeletionTier::PROTECTED;
            replacement.reason = "Better alternative (kept)";
            showCandidatePair(c, replacement);
        } else if (i + 1 < candidates.size()) {
            // Tier 3 or no replacement: pair with next candidate
            showCandidatePair(c, candidates[i + 1]);
        } else {
            // Last item in queue, no pair available
            showCandidateInAvsB(c, false);
        }
        break;
    }
}

void CurrentChoiceWidget::onHistoryItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    int historyId = item->data(0, Qt::UserRole).toInt();

    QList<DeletionHistoryEntry> entries = m_history.allEntries(5000);
    for (const DeletionHistoryEntry &e : entries) {
        if (e.id == historyId) {
            populateAvsBReadOnly(e);
            break;
        }
    }
}

void CurrentChoiceWidget::onHistoryFilterChanged()
{
    populateHistory();
}
