#include "animecard.h"
#include <QPainter>
#include <QMouseEvent>
#include <QListWidgetItem>
#include <QDateTime>

AnimeCard::AnimeCard(QWidget *parent)
    : QFrame(parent)
    , m_animeId(0)
    , m_episodesInList(0)
    , m_totalEpisodes(0)
    , m_viewedCount(0)
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
    
    // Episode list (bottom section)
    m_episodeList = new QListWidget(this);
    m_episodeList->setStyleSheet("QListWidget { font-size: 8pt; }");
    m_episodeList->setAlternatingRowColors(true);
    m_episodeList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mainLayout->addWidget(m_episodeList, 1);
    
    // Connect signals
    connect(m_episodeList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item) {
            int lid = item->data(Qt::UserRole).toInt();
            emit episodeClicked(lid);
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
    QString itemText;
    
    // Format: "Episode X: Title [State] [Viewed]"
    if (episode.episodeNumber.isValid()) {
        itemText = QString("Ep %1: %2")
            .arg(episode.episodeNumber.toDisplayString())
            .arg(episode.episodeTitle);
    } else {
        itemText = QString("Episode: %1").arg(episode.episodeTitle);
    }
    
    // Add state indicator
    if (!episode.state.isEmpty()) {
        itemText += QString(" [%1]").arg(episode.state);
    }
    
    // Add viewed indicator
    if (episode.viewed) {
        itemText += " ✓";
    }
    
    QListWidgetItem *item = new QListWidgetItem(itemText, m_episodeList);
    item->setData(Qt::UserRole, episode.lid);
    item->setData(Qt::UserRole + 1, episode.eid);
    
    // Color code based on state
    if (episode.viewed) {
        item->setForeground(QBrush(QColor(0, 150, 0))); // Green for viewed
    }
    
    // Add tooltip with file info
    QString tooltip = QString("Episode: %1\nFile: %2\nStorage: %3\nState: %4\nViewed: %5")
        .arg(episode.episodeTitle)
        .arg(episode.fileName)
        .arg(episode.storage)
        .arg(episode.state)
        .arg(episode.viewed ? "Yes" : "No");
    
    if (episode.lastPlayed > 0) {
        QDateTime lastPlayedTime = QDateTime::fromSecsSinceEpoch(episode.lastPlayed);
        tooltip += QString("\nLast Played: %1").arg(lastPlayedTime.toString("yyyy-MM-dd hh:mm"));
    }
    
    item->setToolTip(tooltip);
    
    m_episodeList->addItem(item);
}

void AnimeCard::clearEpisodes()
{
    m_episodeList->clear();
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
        // Emit signal when card is clicked (but not when episode list is clicked)
        QPoint pos = event->pos();
        if (!m_episodeList->geometry().contains(pos)) {
            emit cardClicked(m_animeId);
        }
    }
    QFrame::mousePressEvent(event);
}
