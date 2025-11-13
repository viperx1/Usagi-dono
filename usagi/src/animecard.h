#ifndef ANIMECARD_H
#define ANIMECARD_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QListWidget>
#include <QPushButton>
#include <QPixmap>
#include "epno.h"
#include "aired.h"

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
    void setStatistics(int episodesInList, int totalEpisodes, int viewedCount);
    void setPoster(const QPixmap& pixmap);
    
    // Episode management
    struct EpisodeInfo {
        int eid;
        int lid;
        epno episodeNumber;
        QString episodeTitle;
        QString fileName;
        QString state;
        bool viewed;
        QString storage;
        qint64 lastPlayed;
    };
    
    void addEpisode(const EpisodeInfo& episode);
    void clearEpisodes();
    
    // Getters
    int getAnimeId() const { return m_animeId; }
    QString getAnimeTitle() const { return m_animeTitle; }
    QString getAnimeType() const { return m_animeType; }
    QString getAiredText() const { return m_airedText; }
    int getEpisodesInList() const { return m_episodesInList; }
    int getTotalEpisodes() const { return m_totalEpisodes; }
    int getViewedCount() const { return m_viewedCount; }
    qint64 getLastPlayed() const { return m_lastPlayed; }
    
    // Sorting support
    bool operator<(const AnimeCard& other) const;
    
    // Size management
    static QSize getCardSize() { return QSize(400, 300); }
    QSize sizeHint() const override { return getCardSize(); }
    QSize minimumSizeHint() const override { return getCardSize(); }
    
signals:
    void episodeClicked(int lid);
    void cardClicked(int aid);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    
private:
    void setupUI();
    void updateStatisticsLabel();
    
    // Data members
    int m_animeId;
    QString m_animeTitle;
    QString m_animeType;
    QString m_airedText;
    aired m_aired;
    int m_episodesInList;
    int m_totalEpisodes;
    int m_viewedCount;
    qint64 m_lastPlayed;  // Most recent last_played timestamp from episodes
    
    // UI elements
    QLabel *m_posterLabel;
    QLabel *m_titleLabel;
    QLabel *m_typeLabel;
    QLabel *m_airedLabel;
    QLabel *m_statsLabel;
    QListWidget *m_episodeList;
    
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_topLayout;
    QVBoxLayout *m_infoLayout;
};

#endif // ANIMECARD_H
