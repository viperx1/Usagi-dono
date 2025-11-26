#ifndef MYLISTCARDMANAGER_H
#define MYLISTCARDMANAGER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include "animecard.h"
#include "flowlayout.h"

/**
 * MyListCardManager - Manages the lifecycle and updates of anime cards
 * 
 * This class provides:
 * - Card caching and lookup by anime ID
 * - Individual card updates without full reload
 * - Asynchronous data loading and updates
 * - Decoupled card management from main Window class
 * 
 * Design principles:
 * - Cards are created once and updated individually
 * - All updates are asynchronous using Qt signals/slots
 * - Thread-safe operations with mutex protection
 * - Efficient batch processing for multiple updates
 */
class MyListCardManager : public QObject
{
    Q_OBJECT
    
public:
    explicit MyListCardManager(QObject *parent = nullptr);
    virtual ~MyListCardManager();
    
    // Set the layout where cards will be displayed
    void setCardLayout(FlowLayout *layout);
    
    // Initialize cards from database (call once on startup or view switch)
    void loadAllCards();
    
    // Initialize cards for all anime from anime_titles table (for discovery mode)
    void loadAllAnimeTitles();
    
    // Clear all cards (when switching views or shutting down)
    void clearAllCards();
    
    // Get card by anime ID (returns nullptr if not found)
    AnimeCard* getCard(int aid);
    
    // Check if a card exists for given anime ID
    bool hasCard(int aid) const;
    
    // Get all cards (for sorting operations)
    QList<AnimeCard*> getAllCards() const;
    
    // Individual update methods (asynchronous)
    void updateCardAnimeInfo(int aid);
    void updateCardEpisode(int aid, int eid);
    void updateCardStatistics(int aid);
    void updateCardPoster(int aid, const QString &picname);
    
    // Batch update methods for efficiency
    void updateMultipleCards(const QSet<int> &aids);
    
    // Add or update a single mylist entry (creates card if needed)
    void updateOrAddMylistEntry(int lid);
    
signals:
    // Emitted when a card is created
    void cardCreated(int aid, AnimeCard *card);
    
    // Emitted when a card is updated
    void cardUpdated(int aid);
    
    // Emitted when all cards are loaded
    void allCardsLoaded(int count);
    
    // Emitted when a card needs to be sorted
    void cardNeedsSorting(int aid);
    
    // Emitted when a file should be marked watched on AniDB API
    void fileNeedsApiUpdate(int lid, int size, QString ed2khash, int viewed);
    
    // Emitted when play buttons need to be updated for an anime
    void playButtonsNeedUpdate(int aid);
    
    // Emitted when episode data should be requested from AniDB
    void episodeDataRequested(int eid);
    
public slots:
    // Slot to handle episode updates from API
    void onEpisodeUpdated(int eid, int aid);
    
    // Slot to handle anime updates from API
    void onAnimeUpdated(int aid);
    
    // Slot to handle poster download completion
    void onPosterDownloadFinished(QNetworkReply *reply);
    
    // Slot to handle manual data fetch request
    void onFetchDataRequested(int aid);
    
    // Slot to handle hide card request
    void onHideCardRequested(int aid);
    
    // Slot to handle mark episode watched request (sets both AniDB and local status)
    void onMarkEpisodeWatchedRequested(int eid);
    
    // Slot to handle mark file watched request (sets both AniDB and local status)
    void onMarkFileWatchedRequested(int lid);
    
private slots:
    // Timer slot for batched updates
    void processBatchedUpdates();
    
private:
    // Create a single card for an anime
    AnimeCard* createCard(int aid);
    
    // Update card data from database
    void updateCardFromDatabase(int aid);
    
    // Load episode data for a card
    void loadEpisodesForCard(AnimeCard *card, int aid);
    
    // Request missing anime metadata
    void requestAnimeMetadata(int aid);
    
    // Download poster for anime
    void downloadPoster(int aid, const QString &picname);
    
    // Helper to calculate statistics for an anime
    struct AnimeStats {
        int normalEpisodes;
        int totalNormalEpisodes;
        int normalViewed;
        int otherEpisodes;
        int otherViewed;
    };
    AnimeStats calculateStatistics(int aid);
    
    // Helper functions for common operations
    QString determineAnimeName(const QString& nameRomaji, const QString& nameEnglish, const QString& animeTitle, int aid);
    QList<AnimeCard::TagInfo> getTagsOrCategoryFallback(const QString& tagNames, const QString& tagIds, const QString& tagWeights, const QString& category);
    void updateCardAiredDates(AnimeCard* card, const QString& startDate, const QString& endDate);
    
    // Cache anime titles for bulk loading (aid -> title)
    void preloadAnimeTitlesCache(const QList<int>& aids);
    void clearAnimeTitlesCache();
    
    // Bulk preload all data needed for card creation
    struct AnimeData {
        QString nameRomaji;
        QString nameEnglish;
        QString animeTitle;
        QString typeName;
        QString startDate;
        QString endDate;
        QString picname;
        QByteArray posterData;
        QString category;
        QString rating;
        QString tagNameList;
        QString tagIdList;
        QString tagWeightList;
        bool isHidden;
        bool is18Restricted;
        int eptotal;
    };
    void preloadAnimeDataCache(const QList<int>& aids);
    
    // Card cache indexed by anime ID
    QMap<int, AnimeCard*> m_cards;
    
    // Anime titles cache for efficient bulk loading
    QMap<int, QString> m_animeTitlesCache;
    
    // Anime data cache for efficient bulk loading
    QMap<int, AnimeData> m_animeDataCache;
    
    // Statistics cache for efficient bulk loading  
    QMap<int, AnimeStats> m_statsCache;
    
    // Layout where cards are displayed
    FlowLayout *m_layout;
    
    // Network manager for poster downloads
    QNetworkAccessManager *m_networkManager;
    
    // Tracking for pending operations
    QSet<int> m_episodesNeedingData;
    QSet<int> m_animeNeedingMetadata;
    QSet<int> m_animeMetadataRequested;
    QSet<int> m_animeNeedingPoster;
    QMap<int, QString> m_animePicnames;
    QMap<QNetworkReply*, int> m_posterDownloadRequests;
    
    // Batched updates for efficiency
    QSet<int> m_pendingCardUpdates;
    QTimer *m_batchUpdateTimer;
    
    // Thread safety
    mutable QMutex m_mutex;
    
    // Constants
    static const int BATCH_UPDATE_DELAY = 50; // ms
};

#endif // MYLISTCARDMANAGER_H
