#ifndef ANIMECARD_H
#define ANIMECARD_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTreeWidget>
#include <QPushButton>
#include <QPixmap>
#include <QSet>
#include "epno.h"
#include "aired.h"
#include "taginfo.h"
#include "cardfileinfo.h"
#include "cardepisodeinfo.h"

// Forward declaration
class PlayButtonDelegate;
/**
 * AnimeCard - A card widget that displays anime information
 * 
 * Layout:
 * +---+-------+
 * |   |Title  |
 * |pic|Type   |
 * |   |Aired  |
 * |   |Stats  |
 * +---+-------+
 * |Episode 1  |
 * |\File 1    |
 * |Episode 2  |
 * +-----------+
 */
class AnimeCard : public QFrame
{
    Q_OBJECT
    
public:
    explicit AnimeCard(QWidget *parent = nullptr);
    virtual ~AnimeCard();
    
    // Type aliases for new classes (for backward compatibility during migration)
    using TagInfo = ::TagInfo;
    using FileInfo = ::CardFileInfo;
    using EpisodeInfo = ::CardEpisodeInfo;
    
    // Getters
    int getAnimeId() const { return m_animeId; }
    QString getAnimeTitle() const { return m_animeTitle; }
    QString getAnimeType() const { return m_animeType; }
    QString getAiredText() const { return m_airedText; }
    aired getAired() const { return m_aired; }  // For proper sorting
    int getNormalEpisodes() const { return m_normalEpisodes; }
    int getTotalNormalEpisodes() const { return m_totalNormalEpisodes; }
    int getNormalViewed() const { return m_normalViewed; }
    int getOtherEpisodes() const { return m_otherEpisodes; }
    int getOtherViewed() const { return m_otherViewed; }
    qint64 getLastPlayed() const { return m_lastPlayed; }
    bool isHidden() const { return m_isHidden; }
    bool is18Restricted() const { return m_is18Restricted; }
    
    // Get series chain connection points (in global coordinates)
    QPoint getLeftConnectionPoint() const;   // Connection point for prequel arrow (left edge, centered)
    QPoint getRightConnectionPoint() const;  // Connection point for sequel arrow (right edge, centered)
    int getPrequelAid() const { return m_prequelAid; }
    int getSequelAid() const { return m_sequelAid; }
    
    // Sorting support
    bool operator<(const AnimeCard& other) const;
    
    // Size management
    static QSize getCardSize() { return QSize(600, 450); }  // Increased from 500x350 to accommodate 50% larger poster
    QSize sizeHint() const override { return getCardSize(); }
    QSize minimumSizeHint() const override { return getCardSize(); }
    
public slots:
    // Slots for anime data updates
    void setAnimeId(int aid);
    void setAnimeTitle(const QString& title);
    void setAnimeType(const QString& type);
    void setAired(const aired& airedDates);
    void setAiredText(const QString& airedText);
    void setStatistics(int normalEpisodes, int totalNormalEpisodes, int normalViewed, int otherEpisodes, int otherViewed);
    void setPoster(const QPixmap& pixmap);
    void setRating(const QString& rating);
    void setNeedsFetch(bool needsFetch);
    void setTags(const QList<AnimeCard::TagInfo>& tags);
    void addEpisode(const AnimeCard::EpisodeInfo& episode);
    void clearEpisodes();
    void updateNextEpisodeIndicator();  // Update which episode will play next
    void setHidden(bool hidden);  // Set card hidden state
    void setIs18Restricted(bool restricted);  // Set 18+ restriction status
    void setSeriesChainInfo(int prequelAid, int sequelAid);  // Set prequel/sequel AIDs for arrow connections
    void setAnimeLocked(bool locked);  // Set anime-level lock state (shows 🔒 in title)
    void setEpisodeLocked(int eid, bool locked);  // Set episode-level lock state (shows 🔒 on episode row)
    
signals:
    void episodeClicked(int lid);
    void cardClicked(int aid);
    void fetchDataRequested(int aid);
    void playAnimeRequested(int aid);  // Play next unwatched episode
    void downloadAnimeRequested(int aid);  // Download next unwatched episode
    void resetWatchSessionRequested(int aid);  // Reset local watch status
    void hideCardRequested(int aid);  // Hide card request
    void markEpisodeWatchedRequested(int eid);  // Mark episode as watched
    void markFileWatchedRequested(int lid);  // Mark file as watched
    void startSessionFromEpisodeRequested(int lid);  // Start session from specific episode/file
    void deleteFileRequested(int lid);  // Delete file completely (from disk and mylist)
    void lockAnimeRequested(int aid);       // Lock anime against auto-deletion
    void unlockAnimeRequested(int aid);     // Unlock anime
    void lockEpisodeRequested(int eid);     // Lock episode against auto-deletion
    void unlockEpisodeRequested(int eid);   // Unlock episode
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    
private:
    void setupUI();
    void updateStatisticsLabel();
    void showPosterOverlay();
    void hidePosterOverlay();
    void updateCardBackgroundForUnwatchedEpisodes();
    
    // Data members
    int m_animeId;
    QString m_animeTitle;
    QString m_animeType;
    QString m_airedText;
    aired m_aired;
    int m_normalEpisodes;
    int m_totalNormalEpisodes;
    int m_normalViewed;
    int m_otherEpisodes;
    int m_otherViewed;
    qint64 m_lastPlayed;  // Most recent last_played timestamp from episodes
    bool m_isHidden;  // Hidden state of card
    bool m_needsFetch;  // Whether card needs data fetching
    bool m_is18Restricted;  // Whether anime is 18+ restricted
    int m_prequelAid;  // Prequel anime ID (for arrow connection), 0 if none
    int m_sequelAid;  // Sequel anime ID (for arrow connection), 0 if none
    QPixmap m_originalPoster;  // Store original poster for overlay
    bool m_isAnimeLocked;  // Whether the anime is locked against auto-deletion
    QSet<int> m_lockedEpisodeIds;  // Set of eid values that are episode-locked
    
    // UI elements
    QLabel *m_posterLabel;
    QLabel *m_titleLabel;
    QLabel *m_typeLabel;
    QLabel *m_airedLabel;
    QLabel *m_ratingLabel;
    QLabel *m_tagsLabel;
    QLabel *m_statsLabel;
    QLabel *m_warningLabel;  // Warning indicator for missing metadata
    QLabel *m_nextEpisodeLabel;  // Shows which episode will play next
    QPushButton *m_playButton;  // Play button for the anime
    QPushButton *m_downloadButton;  // Download button for the anime
    QPushButton *m_resetSessionButton;  // Reset watch session button
    QTreeWidget *m_episodeTree;  // Changed from QListWidget to support file hierarchy
    PlayButtonDelegate *m_playButtonDelegate;  // Delegate for play button column
    QLabel *m_posterOverlay;  // Full-size poster overlay
    
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_topLayout;
    QVBoxLayout *m_infoLayout;
};

#endif // ANIMECARD_H
