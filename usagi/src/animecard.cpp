#include "animecard.h"
#include "playbuttondelegate.h"
#include "logger.h"
#include "uicolors.h"
#include "fileconsts.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTreeWidgetItem>
#include <QDateTime>
#include <QFile>
#include <QScreen>
#include <QGuiApplication>
#include <QCursor>

// UI icon constants
namespace {
    const QString DOWNLOAD_ICON = QString::fromUtf8("\xE2\xAC\x87");  // UTF-8 encoded down arrow (⬇)
}

AnimeCard::AnimeCard(QWidget *parent)
    : QFrame(parent)
    , m_animeId(0)
    , m_normalEpisodes(0)
    , m_totalNormalEpisodes(0)
    , m_normalViewed(0)
    , m_otherEpisodes(0)
    , m_otherViewed(0)
    , m_lastPlayed(0)
    , m_isHidden(false)
    , m_needsFetch(false)
    , m_is18Restricted(false)
    , m_prequelAid(0)
    , m_sequelAid(0)
    , m_posterOverlay(nullptr)
{
    setupUI();
    
    // Set frame style for card appearance
    setFrameStyle(QFrame::Box | QFrame::Raised);
    setLineWidth(2);
    
    // Set fixed size for cards
    setFixedSize(getCardSize());
    
    // Enable mouse tracking for hover effects
    setMouseTracking(true);
}

AnimeCard::~AnimeCard()
{
}

void AnimeCard::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(5);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    
    // Top section: poster + info
    m_topLayout = new QHBoxLayout();
    
    // Poster (left side) - increased by 50%
    m_posterLabel = new QLabel(this);
    m_posterLabel->setFixedSize(240, 330);  // Increased by 50% from 160x220
    m_posterLabel->setScaledContents(false);  // Changed to false to maintain aspect ratio
    m_posterLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_posterLabel->setAlignment(Qt::AlignCenter);
    m_posterLabel->setText("No\nImage");
    m_posterLabel->setStyleSheet("background-color: #f0f0f0; color: #999;");
    m_posterLabel->setMouseTracking(true);
    m_topLayout->addWidget(m_posterLabel);
    
    // Info section (right side)
    m_infoLayout = new QVBoxLayout();
    m_infoLayout->setSpacing(2);
    
    m_titleLabel = new QLabel("Anime Title", this);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    m_titleLabel->setMaximumHeight(40);
    m_infoLayout->addWidget(m_titleLabel);
    
    m_typeLabel = new QLabel("Type: Unknown", this);
    m_typeLabel->setStyleSheet("font-size: 9pt; color: #666;");
    m_infoLayout->addWidget(m_typeLabel);
    
    m_airedLabel = new QLabel("Aired: Unknown", this);
    m_airedLabel->setStyleSheet("font-size: 9pt; color: #666;");
    m_infoLayout->addWidget(m_airedLabel);
    
    m_ratingLabel = new QLabel("", this);
    m_ratingLabel->setStyleSheet("font-size: 9pt; color: #666;");
    m_infoLayout->addWidget(m_ratingLabel);
    
    m_tagsLabel = new QLabel("", this);
    m_tagsLabel->setWordWrap(true);
    m_tagsLabel->setStyleSheet("font-size: 8pt; color: #888; font-style: italic;");
    // Remove maximum height to allow vertical expansion
    m_infoLayout->addWidget(m_tagsLabel);
    
    m_statsLabel = new QLabel("Episodes: 0/0 | Viewed: 0/0", this);
    m_statsLabel->setStyleSheet("font-size: 9pt; color: #333;");
    m_infoLayout->addWidget(m_statsLabel);
    
    // Next episode indicator
    m_nextEpisodeLabel = new QLabel("Next: N/A", this);
    m_nextEpisodeLabel->setStyleSheet("font-size: 9pt; color: #666; font-style: italic;");
    m_infoLayout->addWidget(m_nextEpisodeLabel);
    
    // Button layout for play and reset buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(5);
    
    m_playButton = new QPushButton("▶ Play Next", this);
    m_playButton->setStyleSheet("font-size: 9pt; padding: 4px 8px;");
    m_playButton->setToolTip("Play the next unwatched episode");
    connect(m_playButton, &QPushButton::clicked, this, [this]() {
        // Find first unwatched episode by checking episode-level watch status
        int topLevelCount = m_episodeTree->topLevelItemCount();
        
        for (int i = 0; i < topLevelCount; i++) {
            QTreeWidgetItem *episodeItem = m_episodeTree->topLevelItem(i);
            
            // Check episode-level watch status from the play button column
            QString episodePlayText = episodeItem->text(1);
            // Episode is unwatched if it shows play button (▶), not checkmark (✓) or X (✗)
            if (episodePlayText == "▶") {
                // Found first unwatched episode - get its lid and emit episodeClicked
                int lid = episodeItem->data(2, Qt::UserRole).toInt();
                if (lid > 0) {
                    emit episodeClicked(lid);
                    return;
                }
            }
        }
        
        // Fallback: emit playAnimeRequested if no unwatched episode found (plays first episode)
        emit playAnimeRequested(m_animeId);
    });
    buttonLayout->addWidget(m_playButton);
    
    // Small download button with icon only
    m_downloadButton = new QPushButton(DOWNLOAD_ICON, this);
    m_downloadButton->setStyleSheet("font-size: 11pt; padding: 4px 8px;");
    m_downloadButton->setToolTip("Download next unwatched episode");
    m_downloadButton->setMaximumWidth(30);  // Make it a small stub button
    connect(m_downloadButton, &QPushButton::clicked, this, [this]() {
        emit downloadAnimeRequested(m_animeId);
    });
    buttonLayout->addWidget(m_downloadButton);
    
    m_resetSessionButton = new QPushButton("↻ Reset Session", this);
    m_resetSessionButton->setStyleSheet("font-size: 9pt; padding: 4px 8px;");
    m_resetSessionButton->setToolTip("Clear local watch status for all episodes");
    connect(m_resetSessionButton, &QPushButton::clicked, this, [this]() {
        emit resetWatchSessionRequested(m_animeId);
    });
    buttonLayout->addWidget(m_resetSessionButton);
    
    m_infoLayout->addLayout(buttonLayout);
    
    m_infoLayout->addStretch();
    
    m_topLayout->addLayout(m_infoLayout, 1);
    
    m_mainLayout->addLayout(m_topLayout);
    
    // Warning label (positioned absolutely in top-right corner)
    m_warningLabel = new QLabel(this);
    m_warningLabel->setText("⚠");  // Warning triangle emoji
    m_warningLabel->setStyleSheet("font-size: 20pt; color: #FFD700; background-color: transparent;");
    m_warningLabel->setFixedSize(30, 30);
    m_warningLabel->setAlignment(Qt::AlignCenter);
    m_warningLabel->move(getCardSize().width() - 35, 5);  // Position in top-right corner
    m_warningLabel->setToolTip("Missing metadata - Right-click to fetch data");
    m_warningLabel->hide();  // Hidden by default
    
    // Episode tree (bottom section) - now supports episode->file hierarchy with play button
    m_episodeTree = new QTreeWidget(this);
    m_episodeTree->setColumnCount(3);  // Column 0: Expand button, Column 1: Play button, Column 2: Episode/File info
    m_episodeTree->setHeaderHidden(true);
    m_episodeTree->setColumnWidth(0, 30);  // Column for expand button (narrow)
    m_episodeTree->setColumnWidth(1, 50);  // Column for play button
    m_episodeTree->setStyleSheet("QTreeWidget { font-size: 8pt; }");
    m_episodeTree->setAlternatingRowColors(true);
    m_episodeTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_episodeTree->setIndentation(15);  // Indent for file children
    m_episodeTree->setUniformRowHeights(false);  // Allow rows to have different heights
    m_mainLayout->addWidget(m_episodeTree, 1);
    
    // Create play button delegate for column 1
    m_playButtonDelegate = new PlayButtonDelegate(this);
    m_episodeTree->setItemDelegateForColumn(1, m_playButtonDelegate);
    
    // Connect play button clicks to emit episodeClicked signal
    connect(m_playButtonDelegate, &PlayButtonDelegate::playButtonClicked, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        
        // Get the item from the model
        QTreeWidgetItem *item = m_episodeTree->itemFromIndex(index);
        if (item) {
            int lid = item->data(2, Qt::UserRole).toInt();  // Changed from column 1 to column 2
            if (lid > 0) {  // Only emit if it's a file item (has lid)
                emit episodeClicked(lid);
            }
        }
    });
    
    // Connect tree widget context menu
    m_episodeTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_episodeTree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTreeWidgetItem *item = m_episodeTree->itemAt(pos);
        if (!item) {
            return;
        }
        
        QMenu contextMenu(this);
        
        // Check if this is an episode item (has no parent) or a file item (has parent)
        if (!item->parent()) {
            // Episode item - show episode-level options
            int eid = item->data(2, Qt::UserRole + 1).toInt();
            int lid = item->data(2, Qt::UserRole).toInt();
            
            if (lid > 0) {
                // Add "Start session from here" option
                QAction *startSessionAction = contextMenu.addAction("Start session from here");
                connect(startSessionAction, &QAction::triggered, this, [this, lid]() {
                    emit startSessionFromEpisodeRequested(lid);
                });
                contextMenu.addSeparator();
            }
            
            if (eid > 0) {
                QAction *markWatchedAction = contextMenu.addAction("Mark episode as watched");
                connect(markWatchedAction, &QAction::triggered, this, [this, eid]() {
                    emit markEpisodeWatchedRequested(eid);
                });
            }
            
            if (!contextMenu.isEmpty()) {
                contextMenu.exec(m_episodeTree->mapToGlobal(pos));
            }
        } else {
            // File item - show file-level options
            int lid = item->data(2, Qt::UserRole).toInt();
            if (lid > 0) {
                // Add "Start session from here" option
                QAction *startSessionAction = contextMenu.addAction("Start session from here");
                connect(startSessionAction, &QAction::triggered, this, [this, lid]() {
                    emit startSessionFromEpisodeRequested(lid);
                });
                contextMenu.addSeparator();
                
                QAction *markFileWatchedAction = contextMenu.addAction("Mark file as watched");
                connect(markFileWatchedAction, &QAction::triggered, this, [this, lid]() {
                    emit markFileWatchedRequested(lid);
                });
                
                contextMenu.addSeparator();
                

                contextMenu.addSeparator();
                
                // Destructive action - delete file completely
                QAction *deleteFileAction = contextMenu.addAction("Delete file...");
                connect(deleteFileAction, &QAction::triggered, this, [this, lid]() {
                    emit deleteFileRequested(lid);
                });
                
                contextMenu.exec(m_episodeTree->mapToGlobal(pos));
            }
        }
    });
}

void AnimeCard::setAnimeId(int aid)
{
    m_animeId = aid;
}

void AnimeCard::setAnimeTitle(const QString& title)
{
    m_animeTitle = title;
    m_titleLabel->setText(title);
}

void AnimeCard::setAnimeType(const QString& type)
{
    m_animeType = type;
    m_typeLabel->setText("Type: " + type);
}

void AnimeCard::setAired(const aired& airedDates)
{
    m_aired = airedDates;
    m_airedText = airedDates.toDisplayString();
    m_airedLabel->setText("Aired: " + m_airedText);
}

void AnimeCard::setAiredText(const QString& airedText)
{
    m_airedText = airedText;
    m_airedLabel->setText("Aired: " + airedText);
}

void AnimeCard::setStatistics(int normalEpisodes, int totalNormalEpisodes, int normalViewed, int otherEpisodes, int otherViewed)
{
    m_normalEpisodes = normalEpisodes;
    m_totalNormalEpisodes = totalNormalEpisodes;
    m_normalViewed = normalViewed;
    m_otherEpisodes = otherEpisodes;
    m_otherViewed = otherViewed;
    updateStatisticsLabel();
}

void AnimeCard::setTags(const QList<TagInfo>& tags)
{
    if (tags.isEmpty()) {
        m_tagsLabel->setText("");
        return;
    }
    
    // Tags are already sorted by weight (highest first)
    QStringList tagNames;
    for (const TagInfo& tag : tags) {
        tagNames << tag.name();
    }
    
    m_tagsLabel->setText("Tags: " + tagNames.join(", "));
}

void AnimeCard::setRating(const QString& rating)
{
    if (!rating.isEmpty()) {
        m_ratingLabel->setText("Rating: " + rating);
    } else {
        m_ratingLabel->setText("");
    }
}

void AnimeCard::updateStatisticsLabel()
{
    // Format episode count like tree view: "A/B+C" where A=normalEpisodes, B=totalNormalEpisodes, C=otherEpisodes
    QString episodeText;
    if (m_totalNormalEpisodes > 0) {
        if (m_otherEpisodes > 0) {
            episodeText = QString("%1/%2+%3").arg(m_normalEpisodes).arg(m_totalNormalEpisodes).arg(m_otherEpisodes);
        } else {
            episodeText = QString("%1/%2").arg(m_normalEpisodes).arg(m_totalNormalEpisodes);
        }
    } else {
        if (m_otherEpisodes > 0) {
            episodeText = QString("%1/?+%2").arg(m_normalEpisodes).arg(m_otherEpisodes);
        } else {
            episodeText = QString("%1/?").arg(m_normalEpisodes);
        }
    }
    
    // Format viewed count like tree view: "A/B+C" where A=normalViewed, B=normalEpisodes, C=otherViewed
    QString viewedText;
    if (m_otherEpisodes > 0) {
        viewedText = QString("%1/%2+%3").arg(m_normalViewed).arg(m_normalEpisodes).arg(m_otherViewed);
    } else {
        viewedText = QString("%1/%2").arg(m_normalViewed).arg(m_normalEpisodes);
    }
    
    QString statsText = QString("Episodes: %1 | Viewed: %2")
        .arg(episodeText, viewedText);
    m_statsLabel->setText(statsText);
}

void AnimeCard::setPoster(const QPixmap& pixmap)
{
    if (!pixmap.isNull()) {
        // Store original poster for overlay
        m_originalPoster = pixmap;
        
        // Scale pixmap to fit within poster label while maintaining aspect ratio
        QPixmap scaledPixmap = pixmap.scaled(m_posterLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_posterLabel->setPixmap(scaledPixmap);
        m_posterLabel->setText("");
        m_posterLabel->setStyleSheet("");
    }
}

void AnimeCard::addEpisode(const EpisodeInfo& episode)
{
    // Create episode parent item
    QTreeWidgetItem *episodeItem = new QTreeWidgetItem(m_episodeTree);
    
    // Format episode text
    QString episodeText;
    if (episode.episodeNumber().isValid()) {
        episodeText = QString("Ep %1: %2")
            .arg(episode.episodeNumber().toDisplayString(), episode.episodeTitle());
    } else {
        episodeText = QString("Episode: %1").arg(episode.episodeTitle());
    }
    
    // Show file count
    if (episode.fileCount() > 1) {
        episodeText += QString(" (%1 files)").arg(episode.fileCount());
    }
    
    // Column 0: Empty - expand button only (Qt handles this automatically)
    episodeItem->setText(0, "");
    
    // Column 1: Play button for episode - check file availability and episode watch status
    // Find any existing playable file (exclude deleted files, prefer highest version)
    bool anyFileExists = false;
    int existingFileLid = 0;
    int highestFileVersion = -1;  // Start at -1 to handle files with version 0
    
    for (const FileInfo& file : episode.files()) {
        // Skip deleted files - they don't count as playable
        if (file.state() == FileStates::DELETED) {
            continue;
        }
        
        if (!file.localFilePath().isEmpty()) {
            bool exists = QFile::exists(file.localFilePath());
            if (exists) {
                anyFileExists = true;
                // Track highest version file for playback
                if (file.version() > highestFileVersion) {
                    existingFileLid = file.lid();
                    highestFileVersion = file.version();
                }
            }
        } 
    }
    
    // Set play button based on episode watch status and file availability
    // Watch state is tracked at episode level only (persists across file replacements)
    if (!anyFileExists) {
        // Show X marker for episodes with missing files
        episodeItem->setText(1, FileSymbols::X_MARK); // X for missing files
        episodeItem->setTextAlignment(1, Qt::AlignCenter);
        episodeItem->setData(1, Qt::UserRole, 0);  // 0 means no playable file
        episodeItem->setForeground(1, QBrush(UIColors::FILE_NOT_FOUND)); // Red for missing
        episodeItem->setData(2, Qt::UserRole, 0);
    } else if (episode.episodeWatched()) {
        // Show checkmark if episode is watched at episode level
        episodeItem->setText(1, FileSymbols::CHECKMARK); // Checkmark for watched
        episodeItem->setTextAlignment(1, Qt::AlignCenter);
        episodeItem->setData(1, Qt::UserRole, 2);  // 2 means watched
        episodeItem->setForeground(1, QBrush(UIColors::FILE_WATCHED));
        episodeItem->setData(2, Qt::UserRole, existingFileLid);
    } else {
        // Show play button if files exist and episode not watched
        episodeItem->setText(1, FileSymbols::PLAY_BUTTON); // Play button if files exist
        episodeItem->setTextAlignment(1, Qt::AlignCenter);
        episodeItem->setData(1, Qt::UserRole, 1);  // 1 means show button
        episodeItem->setForeground(1, QBrush(UIColors::FILE_AVAILABLE)); // Green for available
        episodeItem->setData(2, Qt::UserRole, existingFileLid);
    }
    
    // Column 2: Episode info
    episodeItem->setText(2, episodeText);
    episodeItem->setData(2, Qt::UserRole + 1, episode.eid());
    
    // Add file children
    for (const FileInfo& file : episode.files()) {
        QTreeWidgetItem *fileItem = new QTreeWidgetItem(episodeItem);
        
        // Column 0: Empty - no expand button for files
        fileItem->setText(0, "");
        
        // Column 1: Indicator - show file availability and deletion status
        // Watch state is tracked at episode level, not file level
        // Check if file is marked as deleted (state == FileStates::DELETED)
        bool fileDeleted = (file.state() == FileStates::DELETED);
        
        // Check if local file exists
        bool fileExists = false;
        if (!file.localFilePath().isEmpty()) {
            fileExists = QFile::exists(file.localFilePath());
        }
        
        if (fileDeleted) {
            // Deleted files get black color with circled X symbol
            fileItem->setText(1, FileSymbols::CIRCLED_TIMES); // Circled times for deleted files
            fileItem->setForeground(1, QBrush(UIColors::FILE_DELETED));
        } else if (!fileExists) {
            // Missing files get red color with X symbol
            fileItem->setText(1, FileSymbols::X_MARK); // X for missing files
            fileItem->setForeground(1, QBrush(UIColors::FILE_NOT_FOUND));
        } else {
            // Available files get green color with play button
            fileItem->setText(1, FileSymbols::PLAY_BUTTON); // Indicator for available files
            fileItem->setForeground(1, QBrush(UIColors::FILE_AVAILABLE)); // Green for available
        }
        
        // Format file text with version indicator
        QString fileText = QString("\\");
        
        // Add version if there are multiple files
        if (episode.fileCount() > 1 && file.version() > 0) {
            fileText += QString(" v%1").arg(file.version());
        }
        
        // Add file details
        QStringList fileDetails;
        if (!file.resolution().isEmpty()) {
            fileDetails << file.resolution();
        }
        if (!file.quality().isEmpty()) {
            fileDetails << file.quality();
        }
        if (!file.groupName().isEmpty()) {
            fileDetails << QString("[%1]").arg(file.groupName());
        }
        
        if (!fileDetails.isEmpty()) {
            fileText += " " + fileDetails.join(" ");
        } else if (!file.fileName().isEmpty()) {
            fileText += " " + file.fileName();
        } else {
            fileText += QString(" FID:%1").arg(file.fid());
        }
        
        // Add state indicator
        if (!file.state().isEmpty()) {
            fileText += QString(" [%1]").arg(file.state());
        }
        
        // Column 2: File info
        fileItem->setText(2, fileText);
        fileItem->setData(2, Qt::UserRole, file.lid());
        fileItem->setData(2, Qt::UserRole + 1, file.fid());
        
        // Color code file text based on state
        if (fileDeleted) {
            // Deleted files get black color and strikethrough
            fileItem->setForeground(2, QBrush(UIColors::FILE_DELETED));
            QFont font = fileItem->font(2);
            font.setStrikeOut(true);
            fileItem->setFont(2, font);
        } else if (file.viewed()) {
            // Viewed files get green color
            fileItem->setForeground(2, QBrush(UIColors::FILE_WATCHED)); // Green for viewed
        }
        
        // Add tooltip with file info
        QString tooltip = QString("File: %1\nStorage: %2\nState: %3\nViewed: %4")
            .arg(file.fileName(), file.storage(), file.state(), file.viewed() ? "Yes" : "No");
        
        if (!file.resolution().isEmpty()) {
            tooltip += QString("\nResolution: %1").arg(file.resolution());
        }
        if (!file.quality().isEmpty()) {
            tooltip += QString("\nQuality: %1").arg(file.quality());
        }
        if (!file.groupName().isEmpty()) {
            tooltip += QString("\nGroup: %1").arg(file.groupName());
        }
        if (file.version() > 0) {
            tooltip += QString("\nVersion: v%1").arg(file.version());
        }
        
        if (file.lastPlayed() > 0) {
            QDateTime lastPlayedTime = QDateTime::fromSecsSinceEpoch(file.lastPlayed());
            tooltip += QString("\nLast Played: %1").arg(lastPlayedTime.toString("yyyy-MM-dd hh:mm"));
            
            // Track most recent last played time for this anime
            if (file.lastPlayed() > m_lastPlayed) {
                m_lastPlayed = file.lastPlayed();
            }
        }
        
        fileItem->setToolTip(2, tooltip);
    }
    
    // Collapse all episodes by default
    episodeItem->setExpanded(false);
    
    m_episodeTree->addTopLevelItem(episodeItem);
    
    // Update next episode indicator
    updateNextEpisodeIndicator();
    
    // Update card background for unwatched episodes
    updateCardBackgroundForUnwatchedEpisodes();
}

void AnimeCard::clearEpisodes()
{
    m_episodeTree->clear();
}

void AnimeCard::updateNextEpisodeIndicator()
{
    // Find the first unwatched episode (based on episode-level watch status)
    int topLevelCount = m_episodeTree->topLevelItemCount();
    
    for (int i = 0; i < topLevelCount; i++) {
        QTreeWidgetItem *episodeItem = m_episodeTree->topLevelItem(i);
        
        // Check episode-level watch status from the play button column
        QString episodePlayText = episodeItem->text(1);
        // Episode is unwatched if it shows play button (▶), not checkmark (✓) or X (✗)
        if (episodePlayText == "▶") {
            // Found the first unwatched episode
            QString episodeText = episodeItem->text(2);
            m_nextEpisodeLabel->setText("Next: " + episodeText);
            m_playButton->setEnabled(true);
            return;
        }
    }
    
    // All episodes watched or no episodes
    m_nextEpisodeLabel->setText("Next: All watched");
    m_playButton->setEnabled(false);
}

void AnimeCard::setNeedsFetch(bool needsFetch)
{
    m_needsFetch = needsFetch;
    if (needsFetch) {
        m_warningLabel->show();
    } else {
        m_warningLabel->hide();
    }
}

bool AnimeCard::operator<(const AnimeCard& other) const
{
    // Default comparison by anime title
    return m_animeTitle < other.m_animeTitle;
}

void AnimeCard::paintEvent(QPaintEvent *event)
{
    // Call base class paint event for frame drawing
    QFrame::paintEvent(event);
    
    // Note: Series chain arrows are now drawn by the parent layout (VirtualFlowLayout)
    // to connect cards together, not inside each card
}

void AnimeCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if click is on poster - show overlay (only if not hidden)
        QPoint pos = event->pos();
        if (!m_isHidden && m_posterLabel->geometry().contains(pos) && m_posterLabel->isVisible()) {
            showPosterOverlay();
            // Don't propagate to parent - we handled it
            event->accept();
            return;
        }
        
        // Emit signal when card is clicked (but not when episode tree is clicked or poster is clicked)
        if (!m_episodeTree->geometry().contains(pos)) {
            LOG(QString("[AnimeCard] Card clicked for aid=%1, m_needsFetch=%2").arg(m_animeId).arg(m_needsFetch));
            emit cardClicked(m_animeId);
            
            // Auto-fetch missing data when card is clicked if data is needed
            // Always emit fetchDataRequested - let MyListCardManager decide what needs fetching
            LOG(QString("[AnimeCard] Emitting fetchDataRequested for aid=%1 (m_needsFetch=%2)")
                .arg(m_animeId).arg(m_needsFetch));
            emit fetchDataRequested(m_animeId);
        }
    }
    QFrame::mousePressEvent(event);
}

void AnimeCard::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu contextMenu(this);
    
    QAction *fetchDataAction = contextMenu.addAction("Fetch data");
    connect(fetchDataAction, &QAction::triggered, this, [this]() {
        emit fetchDataRequested(m_animeId);
    });
    
    QAction *hideAction = contextMenu.addAction(m_isHidden ? "Unhide" : "Hide");
    connect(hideAction, &QAction::triggered, this, [this]() {
        emit hideCardRequested(m_animeId);
    });
    
    contextMenu.exec(event->globalPos());
}

void AnimeCard::setHidden(bool hidden)
{
    m_isHidden = hidden;
    
    // Update visual representation based on hidden state
    if (hidden) {
        // Hide most elements, show only title as a compact view
        m_posterLabel->hide();
        m_typeLabel->hide();
        m_airedLabel->hide();
        m_ratingLabel->hide();
        m_tagsLabel->hide();
        m_statsLabel->hide();
        m_nextEpisodeLabel->hide();
        m_playButton->hide();
        m_downloadButton->hide();
        m_resetSessionButton->hide();
        m_episodeTree->hide();
        m_warningLabel->hide();
        
        // Make card smaller for title-only display
        setFixedSize(QSize(600, 40));
        
        // Make title more prominent
        m_titleLabel->setStyleSheet("font-weight: bold; font-size: 10pt; color: #888;");
    } else {
        // Show all elements in normal view
        m_posterLabel->show();
        m_typeLabel->show();
        m_airedLabel->show();
        m_ratingLabel->show();
        m_tagsLabel->show();
        m_statsLabel->show();
        m_nextEpisodeLabel->show();
        m_playButton->show();
        m_downloadButton->show();
        m_resetSessionButton->show();
        m_episodeTree->show();
        
        // Restore normal size
        setFixedSize(getCardSize());
        
        // Restore title style
        m_titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
        
        // Show warning if needed
        if (m_warningLabel && !m_warningLabel->toolTip().isEmpty()) {
            // Warning visibility is managed by setNeedsFetch
        }
    }
}

void AnimeCard::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
}

void AnimeCard::leaveEvent(QEvent *event)
{
    // Don't hide overlay here - it's handled by eventFilter on poster label
    QFrame::leaveEvent(event);
}

bool AnimeCard::eventFilter(QObject *watched, QEvent *event)
{
    // Check if this is a leave event on the poster label
    if (watched == m_posterLabel && event->type() == QEvent::Leave) {
        // Only hide if mouse is not over the overlay
        if (m_posterOverlay && m_posterOverlay->isVisible()) {
            // Check if mouse is actually over the overlay
            QPoint globalPos = QCursor::pos();
            QRect overlayRect = m_posterOverlay->geometry();
            if (!overlayRect.contains(globalPos)) {
                // Mouse has left poster and is not over overlay - hide it
                hidePosterOverlay();
            }
        }
    }
    
    // Check if mouse enters poster label while overlay is visible - keep it visible
    if (watched == m_posterLabel && event->type() == QEvent::Enter) {
        // If overlay is visible, keep it that way
        if (m_posterOverlay && m_posterOverlay->isVisible()) {
            m_posterOverlay->raise();
        }
    }
    
    // Check if mouse leaves the overlay itself
    if (watched == m_posterOverlay && event->type() == QEvent::Leave) {
        // Check if mouse has moved to poster label - if not, hide overlay
        QPoint globalPos = QCursor::pos();
        QPoint posterGlobalPos = m_posterLabel->mapToGlobal(QPoint(0, 0));
        QRect posterRect(posterGlobalPos, m_posterLabel->size());
        if (!posterRect.contains(globalPos)) {
            // Mouse has left overlay and is not over poster - hide it
            hidePosterOverlay();
        }
    }
    
    return QFrame::eventFilter(watched, event);
}

void AnimeCard::showPosterOverlay()
{
    if (m_originalPoster.isNull()) {
        return;
    }
    
    // Create overlay if it doesn't exist - parent it to the top-level widget to avoid clipping
    if (!m_posterOverlay) {
        // Find the top-level widget (main window)
        QWidget *topWidget = this;
        while (topWidget->parentWidget()) {
            topWidget = topWidget->parentWidget();
        }
        
        m_posterOverlay = new QLabel(topWidget);
        m_posterOverlay->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_posterOverlay->setAttribute(Qt::WA_TranslucentBackground);
        m_posterOverlay->setAttribute(Qt::WA_Hover);
        m_posterOverlay->setScaledContents(false);
        m_posterOverlay->setAlignment(Qt::AlignCenter);
        m_posterOverlay->setStyleSheet("background-color: rgba(0, 0, 0, 200); border: 2px solid white;");
        m_posterOverlay->setMouseTracking(true);
        
        // Install event filter on overlay itself to track mouse leave
        m_posterOverlay->installEventFilter(this);
        
        // Install event filter to detect when mouse leaves the poster label
        m_posterLabel->installEventFilter(this);
    }
    
    // Get the original poster size (100% size, no scaling)
    QSize originalSize = m_originalPoster.size();
    
    // Get poster label position in global coordinates
    QPoint globalPos = m_posterLabel->mapToGlobal(QPoint(0, 0));
    
    // Center the overlay over the poster label position, using original poster dimensions
    int x = globalPos.x() + (m_posterLabel->width() - originalSize.width()) / 2;
    int y = globalPos.y() + (m_posterLabel->height() - originalSize.height()) / 2;
    
    // Get screen geometry to ensure overlay stays within screen bounds
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    
    if (screen) {
        QRect screenGeometry = screen->geometry();
        
        // Adjust x position if overlay goes off right edge of screen
        if (x + originalSize.width() > screenGeometry.right()) {
            x = screenGeometry.right() - originalSize.width();
        }
        // Adjust x position if overlay goes off left edge of screen
        if (x < screenGeometry.left()) {
            x = screenGeometry.left();
        }
        
        // Adjust y position if overlay goes off bottom edge of screen
        if (y + originalSize.height() > screenGeometry.bottom()) {
            y = screenGeometry.bottom() - originalSize.height();
        }
        // Adjust y position if overlay goes off top edge of screen
        if (y < screenGeometry.top()) {
            y = screenGeometry.top();
        }
    }
    
    m_posterOverlay->setGeometry(x, y, originalSize.width(), originalSize.height());
    
    // Show original poster at 100% original size
    m_posterOverlay->setPixmap(m_originalPoster);
    m_posterOverlay->show();
    m_posterOverlay->raise();
}

void AnimeCard::hidePosterOverlay()
{
    if (m_posterOverlay) {
        m_posterOverlay->hide();
    }
}

void AnimeCard::updateCardBackgroundForUnwatchedEpisodes()
{
    // Check if there are unwatched episodes
    bool hasUnwatchedEpisodes = false;
    int topLevelCount = m_episodeTree->topLevelItemCount();
    
    for (int i = 0; i < topLevelCount; i++) {
        QTreeWidgetItem *episodeItem = m_episodeTree->topLevelItem(i);
        
        // Check episode-level watch status from the play button column
        QString episodePlayText = episodeItem->text(1);
        // Episode is unwatched if it shows play button (▶)
        if (episodePlayText == "▶") {
            hasUnwatchedEpisodes = true;
            break;
        }
    }
    
    // Apply green background if there are unwatched episodes
    if (hasUnwatchedEpisodes) {
        setStyleSheet("QFrame { background-color: #e8f5e9; }");  // Light green background
    } else {
        setStyleSheet("");  // Reset to default
    }
}

void AnimeCard::setIs18Restricted(bool restricted)
{
    m_is18Restricted = restricted;
}

void AnimeCard::setSeriesChainInfo(int prequelAid, int sequelAid)
{
    m_prequelAid = prequelAid;
    m_sequelAid = sequelAid;
}

QPoint AnimeCard::getLeftConnectionPoint() const
{
    // Return the center point of the left edge in global coordinates
    return mapToGlobal(QPoint(0, height() / 2));
}

QPoint AnimeCard::getRightConnectionPoint() const
{
    // Return the center point of the right edge in global coordinates
    return mapToGlobal(QPoint(width(), height() / 2));
}

