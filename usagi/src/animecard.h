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
#include "epno.h"
#include "aired.h"

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
    
    // Setters for anime data
    void setAnimeId(int aid);
    void setAnimeTitle(const QString& title);
    void setAnimeType(const QString& type);
    void setAired(const aired& airedDates);
    void setAiredText(const QString& airedText);
    void setStatistics(int normalEpisodes, int totalNormalEpisodes, int normalViewed, int otherEpisodes, int otherViewed);
    void setPoster(const QPixmap& pixmap);
    void setRating(const QString& rating);
    
    // Warning indicator for missing metadata
    void setNeedsFetch(bool needsFetch);
    
    // Tag structure for storing tag data
    struct TagInfo {
        QString name;
        int id;
        int weight;
        
        // For sorting by weight (highest first)
        bool operator<(const TagInfo& other) const {
            return weight > other.weight;
        }
    };
    
    void setTags(const QList<TagInfo>& tags);
    
    // File and Episode management structures
    struct FileInfo {
        int lid;
        int fid;
        QString fileName;
        QString state;
        bool viewed;
        QString storage;
        QString localFilePath;  // Path to local file if tracked
        qint64 lastPlayed;
        QString resolution;
        QString quality;
        QString groupName;
        int version;  // File version (1, 2, 3, etc.)
    };
    
    struct EpisodeInfo {
        int eid;
        epno episodeNumber;
        QString episodeTitle;
        QList<FileInfo> files;  // Multiple files per episode
    };
    
    void addEpisode(const EpisodeInfo& episode);
    void clearEpisodes();
    
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
    
    // Sorting support
    bool operator<(const AnimeCard& other) const;
    
    // Size management
    static QSize getCardSize() { return QSize(600, 450); }  // Increased from 500x350 to accommodate 50% larger poster
    QSize sizeHint() const override { return getCardSize(); }
    QSize minimumSizeHint() const override { return getCardSize(); }
    
signals:
    void episodeClicked(int lid);
    void cardClicked(int aid);
    void fetchDataRequested(int aid);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    
private:
    void setupUI();
    void updateStatisticsLabel();
    
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
    
    // UI elements
    QLabel *m_posterLabel;
    QLabel *m_titleLabel;
    QLabel *m_typeLabel;
    QLabel *m_airedLabel;
    QLabel *m_ratingLabel;
    QLabel *m_tagsLabel;
    QLabel *m_statsLabel;
    QLabel *m_warningLabel;  // Warning indicator for missing metadata
    QTreeWidget *m_episodeTree;  // Changed from QListWidget to support file hierarchy
    PlayButtonDelegate *m_playButtonDelegate;  // Delegate for play button column
    
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_topLayout;
    QVBoxLayout *m_infoLayout;
};

#endif // ANIMECARD_H
