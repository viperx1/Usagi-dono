#include "animecard.h"
#include "playbuttondelegate.h"
#include "logger.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTreeWidgetItem>
#include <QDateTime>
#include <QFile>

AnimeCard::AnimeCard(QWidget *parent)
    : QFrame(parent)
    , m_animeId(0)
    , m_normalEpisodes(0)
    , m_totalNormalEpisodes(0)
    , m_normalViewed(0)
    , m_otherEpisodes(0)
    , m_otherViewed(0)
    , m_lastPlayed(0)
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
    m_posterLabel->setScaledContents(true);
    m_posterLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_posterLabel->setAlignment(Qt::AlignCenter);
    m_posterLabel->setText("No\nImage");
    m_posterLabel->setStyleSheet("background-color: #f0f0f0; color: #999;");
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
    
    // Debug: Log tree widget configuration
    LOG(QString("AnimeCard: Tree widget configured with %1 columns, widths: [0]=%2px, [1]=%3px")
        .arg(m_episodeTree->columnCount())
        .arg(m_episodeTree->columnWidth(0))
        .arg(m_episodeTree->columnWidth(1)));
    
    // Create play button delegate for column 1
    m_playButtonDelegate = new PlayButtonDelegate(this);
    m_episodeTree->setItemDelegateForColumn(1, m_playButtonDelegate);
    LOG(QString("AnimeCard: PlayButtonDelegate attached to column 1"));
    
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
        tagNames << tag.name;
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
        .arg(episodeText)
        .arg(viewedText);
    m_statsLabel->setText(statsText);
}

void AnimeCard::setPoster(const QPixmap& pixmap)
{
    if (!pixmap.isNull()) {
        m_posterLabel->setPixmap(pixmap);
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
    if (episode.episodeNumber.isValid()) {
        episodeText = QString("Ep %1: %2")
            .arg(episode.episodeNumber.toDisplayString())
            .arg(episode.episodeTitle);
    } else {
        episodeText = QString("Episode: %1").arg(episode.episodeTitle);
    }
    
    // Show file count
    if (episode.files.size() > 1) {
        episodeText += QString(" (%1 files)").arg(episode.files.size());
    }
    
    // Column 0: Empty - expand button only (Qt handles this automatically)
    episodeItem->setText(0, "");
    
    // Column 1: Play button if any file exists
    // Check if any file in this episode exists locally
    bool anyFileExists = false;
    int existingFileLid = 0;
    for (const FileInfo& file : episode.files) {
        if (!file.localFilePath.isEmpty() && QFile::exists(file.localFilePath)) {
            anyFileExists = true;
            existingFileLid = file.lid;
            break;  // Found at least one file that exists
        }
    }
    
    // Debug logging for play button visibility
    LOG(QString("AnimeCard: Episode %1 (%2) - files: %3, anyFileExists: %4, existingFileLid: %5")
        .arg(episode.eid)
        .arg(episodeText)
        .arg(episode.files.size())
        .arg(anyFileExists ? "YES" : "NO")
        .arg(existingFileLid));
    
    if (anyFileExists) {
        episodeItem->setText(1, "▶"); // Play button if any file exists
        episodeItem->setTextAlignment(1, Qt::AlignCenter);  // Center the play button
        episodeItem->setData(1, Qt::UserRole, 1);  // 1 means show button
        episodeItem->setForeground(1, QBrush(QColor(0, 150, 0))); // Green for available
        // Store the lid of the first available file for playback
        episodeItem->setData(2, Qt::UserRole, existingFileLid);
        LOG(QString("AnimeCard: Play button SET for episode %1 in column 1").arg(episode.eid));
    } else {
        episodeItem->setText(1, "");
        episodeItem->setData(1, Qt::UserRole, 0);  // 0 means no button
        episodeItem->setData(2, Qt::UserRole, 0);  // 0 means it's an episode with no files
        LOG(QString("AnimeCard: Play button NOT set for episode %1 (no existing files)").arg(episode.eid));
    }
    
    // Column 2: Episode info
    episodeItem->setText(2, episodeText);
    episodeItem->setData(2, Qt::UserRole + 1, episode.eid);
    
    // Add file children
    for (const FileInfo& file : episode.files) {
        QTreeWidgetItem *fileItem = new QTreeWidgetItem(episodeItem);
        
        // Column 0: Empty - no expand button for files
        fileItem->setText(0, "");
        
        // Column 1: Play button - reflects file existence, not watch state
        // Check if local file exists
        bool fileExists = false;
        if (!file.localFilePath.isEmpty()) {
            fileExists = QFile::exists(file.localFilePath);
        }
        
        // Debug logging for file play button
        LOG(QString("AnimeCard: File lid=%1, localPath='%2', exists=%3")
            .arg(file.lid)
            .arg(file.localFilePath)
            .arg(fileExists ? "YES" : "NO"));
        
        if (fileExists) {
            fileItem->setText(1, "▶"); // Play button for existing files
            fileItem->setForeground(1, QBrush(QColor(0, 150, 0))); // Green for available
            LOG(QString("AnimeCard: Play button SET for file lid=%1 in column 1").arg(file.lid));
        } else {
            fileItem->setText(1, "✗"); // X for missing files
            fileItem->setForeground(1, QBrush(QColor(Qt::red)));
            LOG(QString("AnimeCard: X marker SET for file lid=%1 in column 1 (file missing)").arg(file.lid));
        }
        
        // Format file text with version indicator
        QString fileText = QString("\\");
        
        // Add version if there are multiple files
        if (episode.files.size() > 1 && file.version > 0) {
            fileText += QString(" v%1").arg(file.version);
        }
        
        // Add file details
        QStringList fileDetails;
        if (!file.resolution.isEmpty()) {
            fileDetails << file.resolution;
        }
        if (!file.quality.isEmpty()) {
            fileDetails << file.quality;
        }
        if (!file.groupName.isEmpty()) {
            fileDetails << QString("[%1]").arg(file.groupName);
        }
        
        if (!fileDetails.isEmpty()) {
            fileText += " " + fileDetails.join(" ");
        } else if (!file.fileName.isEmpty()) {
            fileText += " " + file.fileName;
        } else {
            fileText += QString(" FID:%1").arg(file.fid);
        }
        
        // Add state indicator
        if (!file.state.isEmpty()) {
            fileText += QString(" [%1]").arg(file.state);
        }
        
        // Column 2: File info
        fileItem->setText(2, fileText);
        fileItem->setData(2, Qt::UserRole, file.lid);
        fileItem->setData(2, Qt::UserRole + 1, file.fid);
        
        // Color code file text based on state
        if (file.viewed) {
            fileItem->setForeground(2, QBrush(QColor(0, 150, 0))); // Green for viewed
        }
        
        // Add tooltip with file info
        QString tooltip = QString("File: %1\nStorage: %2\nState: %3\nViewed: %4")
            .arg(file.fileName)
            .arg(file.storage)
            .arg(file.state)
            .arg(file.viewed ? "Yes" : "No");
        
        if (!file.resolution.isEmpty()) {
            tooltip += QString("\nResolution: %1").arg(file.resolution);
        }
        if (!file.quality.isEmpty()) {
            tooltip += QString("\nQuality: %1").arg(file.quality);
        }
        if (!file.groupName.isEmpty()) {
            tooltip += QString("\nGroup: %1").arg(file.groupName);
        }
        if (file.version > 0) {
            tooltip += QString("\nVersion: v%1").arg(file.version);
        }
        
        if (file.lastPlayed > 0) {
            QDateTime lastPlayedTime = QDateTime::fromSecsSinceEpoch(file.lastPlayed);
            tooltip += QString("\nLast Played: %1").arg(lastPlayedTime.toString("yyyy-MM-dd hh:mm"));
            
            // Track most recent last played time for this anime
            if (file.lastPlayed > m_lastPlayed) {
                m_lastPlayed = file.lastPlayed;
            }
        }
        
        fileItem->setToolTip(2, tooltip);
    }
    
    // Collapse all episodes by default
    episodeItem->setExpanded(false);
    
    m_episodeTree->addTopLevelItem(episodeItem);
}

void AnimeCard::clearEpisodes()
{
    m_episodeTree->clear();
}

void AnimeCard::setNeedsFetch(bool needsFetch)
{
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
    
    // Additional custom painting can be done here if needed
}

void AnimeCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Emit signal when card is clicked (but not when episode tree is clicked)
        QPoint pos = event->pos();
        if (!m_episodeTree->geometry().contains(pos)) {
            emit cardClicked(m_animeId);
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
    
    contextMenu.exec(event->globalPos());
}
