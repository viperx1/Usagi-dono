#include "animecard.h"
#include "playbuttondelegate.h"
#include <QPainter>
#include <QMouseEvent>
#include <QTreeWidgetItem>
#include <QDateTime>

AnimeCard::AnimeCard(QWidget *parent)
    : QFrame(parent)
    , m_animeId(0)
    , m_episodesInList(0)
    , m_totalEpisodes(0)
    , m_viewedCount(0)
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
    
    // Poster (left side)
    m_posterLabel = new QLabel(this);
    m_posterLabel->setFixedSize(80, 110);
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
    
    m_statsLabel = new QLabel("Episodes: 0/0 | Viewed: 0/0", this);
    m_statsLabel->setStyleSheet("font-size: 9pt; color: #333;");
    m_infoLayout->addWidget(m_statsLabel);
    
    m_infoLayout->addStretch();
    
    m_topLayout->addLayout(m_infoLayout, 1);
    
    m_mainLayout->addLayout(m_topLayout);
    
    // Episode tree (bottom section) - now supports episode->file hierarchy with play button
    m_episodeTree = new QTreeWidget(this);
    m_episodeTree->setColumnCount(2);  // Column 0: Play button, Column 1: Episode/File info
    m_episodeTree->setHeaderHidden(true);
    m_episodeTree->setColumnWidth(0, 30);  // Narrow column for play button
    m_episodeTree->setStyleSheet("QTreeWidget { font-size: 8pt; }");
    m_episodeTree->setAlternatingRowColors(true);
    m_episodeTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_episodeTree->setIndentation(15);  // Indent for file children
    m_mainLayout->addWidget(m_episodeTree, 1);
    
    // Create play button delegate for column 0
    m_playButtonDelegate = new PlayButtonDelegate(this);
    m_episodeTree->setItemDelegateForColumn(0, m_playButtonDelegate);
    
    // Connect play button clicks to emit episodeClicked signal
    connect(m_playButtonDelegate, &PlayButtonDelegate::playButtonClicked, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        
        // Get the item from the model
        QTreeWidgetItem *item = m_episodeTree->itemFromIndex(index);
        if (item) {
            int lid = item->data(1, Qt::UserRole).toInt();
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
    m_titleLabel->setToolTip(title);
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

void AnimeCard::setStatistics(int episodesInList, int totalEpisodes, int viewedCount)
{
    m_episodesInList = episodesInList;
    m_totalEpisodes = totalEpisodes;
    m_viewedCount = viewedCount;
    updateStatisticsLabel();
}

void AnimeCard::updateStatisticsLabel()
{
    QString statsText = QString("Episodes: %1/%2 | Viewed: %3/%4")
        .arg(m_episodesInList)
        .arg(m_totalEpisodes)
        .arg(m_viewedCount)
        .arg(m_episodesInList);
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
    
    // Column 0: empty (no play button for episode parent)
    episodeItem->setText(0, "");
    episodeItem->setData(0, Qt::UserRole, 0);  // 0 means no button
    
    // Column 1: Episode info
    episodeItem->setText(1, episodeText);
    episodeItem->setData(1, Qt::UserRole, 0);  // 0 means it's an episode, not a file
    episodeItem->setData(1, Qt::UserRole + 1, episode.eid);
    
    // Add file children
    for (const FileInfo& file : episode.files) {
        QTreeWidgetItem *fileItem = new QTreeWidgetItem(episodeItem);
        
        // Column 0: Play button text (same as tree view)
        if (file.state == "Deleted") {
            fileItem->setText(0, "✗"); // X for deleted
            fileItem->setForeground(0, QBrush(QColor(Qt::red)));
        } else if (file.viewed) {
            fileItem->setText(0, "✓"); // Checkmark for watched
        } else {
            fileItem->setText(0, "▶"); // Play button
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
        
        // Column 1: File info (removed viewed indicator since it's in column 0 now)
        fileItem->setText(1, fileText);
        fileItem->setData(1, Qt::UserRole, file.lid);
        fileItem->setData(1, Qt::UserRole + 1, file.fid);
        
        // Color code file text based on state
        if (file.viewed) {
            fileItem->setForeground(1, QBrush(QColor(0, 150, 0))); // Green for viewed
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
        
        fileItem->setToolTip(1, tooltip);
    }
    
    // Collapse all episodes by default
    episodeItem->setExpanded(false);
    
    m_episodeTree->addTopLevelItem(episodeItem);
}

void AnimeCard::clearEpisodes()
{
    m_episodeTree->clear();
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
