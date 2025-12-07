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
#include "animestats.h"
#include "cachedanimedata.h"

// Forward declarations
class VirtualFlowLayout;
class WatchSessionManager;

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
    // Type aliases for new classes (for backward compatibility during migration)
    using AnimeStats = ::AnimeStats;
    using CachedAnimeData = ::CachedAnimeData;
    
    explicit MyListCardManager(QObject *parent = nullptr);
    virtual ~MyListCardManager();
    
    // Set the layout where cards will be displayed
    void setCardLayout(FlowLayout *layout);
    
    // Set the virtual layout for virtual scrolling (new)
    void setVirtualLayout(VirtualFlowLayout *layout);
    
    // Set the watch session manager for file mark queries
    void setWatchSessionManager(WatchSessionManager *manager) { m_watchSessionManager = manager; }
    
    // Get the list of anime IDs in the current order (for virtual scrolling)
    QList<int> getAnimeIdList() const;
    
    // Set the ordered list of anime IDs (for virtual scrolling after sorting/filtering)
    void setAnimeIdList(const QList<int>& aids);
    
    // Create a card for a specific index in the ordered list (for virtual scrolling)
    // This is the factory method called by VirtualFlowLayout
    AnimeCard* createCardForIndex(int index);
    
    // Check if virtual scrolling is enabled
    bool isVirtualScrollingEnabled() const { return m_virtualLayout != nullptr; }
    
    // Mark initial loading as complete (called after first mylist load)
    void setInitialLoadComplete() { m_initialLoadComplete = true; }
    
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
    
    // Refresh all cards to pick up changes (e.g., file markings)
    // DEPRECATED: Use refreshCardsForLids() instead for targeted updates
    void refreshAllCards();
    
    // Refresh only cards containing the specified lids (more efficient than refreshAllCards)
    // This looks up the anime ID for each lid and refreshes only those specific cards
    void refreshCardsForLids(const QSet<int>& lids);
    
    // Batch update methods for efficiency
    void updateMultipleCards(const QSet<int> &aids);
    
    // Add or update a single mylist entry (creates card if needed)
    void updateOrAddMylistEntry(int lid);
    
    // Preload anime data and statistics cache for better performance (call before creating many cards)
    void preloadAnimeDataCache(const QList<int>& aids);
    
    // Preload episode data cache for better performance (call before creating many cards)
    void preloadEpisodesCache(const QList<int>& aids);
    
    // Comprehensive preload function that loads ALL data needed for card creation
    // This should be called BEFORE any cards are created to eliminate all SQL queries from createCard()
    void preloadCardCreationData(const QList<int>& aids);
    
    // Create a card for an anime (data must be preloaded first via preloadCardCreationData)
    AnimeCard* createCard(int aid);
    
    // Get cached anime data for filtering/sorting without needing card widgets
    // This is essential for virtual scrolling where cards don't exist until visible
    CachedAnimeData getCachedAnimeData(int aid) const;
    
    // Check if cached data exists for an anime
    bool hasCachedData(int aid) const;
    
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
    
    // Emitted when a NEW anime is added to mylist (not during initial load)
    void newAnimeAdded(int aid);
    
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
    // Structure to cache episode data for an anime
    struct EpisodeCacheEntry {
        int lid;
        int eid;
        int fid;
        int state;
        int viewed;
        QString storage;
        QString episodeName;
        QString epno;
        QString filename;
        qint64 lastPlayed;
        QString localFilePath;
        QString resolution;
        QString quality;
        QString groupName;
        int localWatched;
    };
    
    // Comprehensive data structure containing ALL data needed for card creation
    // This eliminates the need for any SQL queries during card creation
    struct CardCreationData {
        // Anime basic info
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
        
        // Relation data (for chain support)
        QString relaidlist;  // Related anime IDs (apostrophe-separated)
        QString relaidtype;  // Related anime types (apostrophe-separated)
        
        // Statistics
        AnimeStats stats;
        
        // Episodes
        QList<EpisodeCacheEntry> episodes;
        
        // Flag to indicate if this anime has full data
        bool hasData;
        
        CardCreationData() : isHidden(false), is18Restricted(false), eptotal(0), hasData(false) {}
    };
    
    // Update card data from database
    void updateCardFromDatabase(int aid);
    
    // Load episode data for a card
    void loadEpisodesForCard(AnimeCard *card, int aid);
    
    // Load episode data for a card from cache (no SQL queries)
    void loadEpisodesForCardFromCache(AnimeCard *card, int aid, const QList<EpisodeCacheEntry>& episodes);
    
    // Request missing anime metadata
    void requestAnimeMetadata(int aid);
    
    // Download poster for anime
    void downloadPoster(int aid, const QString &picname);
    
    AnimeStats calculateStatistics(int aid);
    
    // Helper functions for common operations
    QString determineAnimeName(const QString& nameRomaji, const QString& nameEnglish, const QString& animeTitle, int aid);
    QList<AnimeCard::TagInfo> getTagsOrCategoryFallback(const QString& tagNames, const QString& tagIds, const QString& tagWeights, const QString& category);
    void updateCardAiredDates(AnimeCard* card, const QString& startDate, const QString& endDate);
    
    // Cache anime titles for bulk loading (aid -> title)
    void preloadAnimeTitlesCache(const QList<int>& aids);
    void clearAnimeTitlesCache();
    
    // Card cache indexed by anime ID
    QMap<int, AnimeCard*> m_cards;
    
    // Comprehensive card creation data cache - contains ALL data needed for card creation
    // This is populated once before any cards are created
    QMap<int, CardCreationData> m_cardCreationDataCache;
    
    // Layout where cards are displayed
    FlowLayout *m_layout;
    
    // Virtual layout for virtual scrolling (optional)
    VirtualFlowLayout *m_virtualLayout;
    
    // Watch session manager for file mark queries
    WatchSessionManager *m_watchSessionManager;
    
    // Ordered list of anime IDs for virtual scrolling
    QList<int> m_orderedAnimeIds;
    
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
    
    // Flag to track if initial loading is complete
    bool m_initialLoadComplete;
    
    // Constants
    static const int BATCH_UPDATE_DELAY = 50; // ms
};

#endif // MYLISTCARDMANAGER_H
