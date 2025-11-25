#ifndef WINDOW_H
#define WINDOW_H
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QBrush>
#include <QtGui/QPixmap>
#include <QtGui/QKeyEvent>
#include <QtGui/QCloseEvent>
#include <QtAlgorithms>
#include <QRegularExpression>
#include <QMutex>
#include <QProcess>
#include <QDateTime>
#include <QDir>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QListView>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QMessageBox>
#include <QXmlStreamReader>
#include <QThread>
#include "hash/ed2k.h"
#include "anidbapi.h"
#include "epno.h"
#include "aired.h"
#include "directorywatcher.h"
#include "playbackmanager.h"
#include "animecard.h"
#include "flowlayout.h"
#include "mylistcardmanager.h"
#include "uicolors.h"
//#include "hasherthread.h"

// Forward declarations
class PlayButtonDelegate;
class MyListCardManager;

// MyList tree widget column indices (using enum for type safety and maintainability)
// Column order: Anime, Play, Episode, Episode Title, State, Viewed, Storage, Mylist ID, Type, Aired, Last Played
enum MyListColumn {
    COL_ANIME = 0,
    COL_PLAY = 1,
    COL_EPISODE = 2,
    COL_EPISODE_TITLE = 3,
    COL_STATE = 4,
    COL_VIEWED = 5,
    COL_STORAGE = 6,
    COL_MYLIST_ID = 7,
    COL_TYPE = 8,
    COL_AIRED = 9,
    COL_LAST_PLAYED = 10
};

// Play button icons used in tree widget
namespace PlayIcons {
    constexpr const char* PLAY = "▶";       // Play button (file exists, not watched)
    constexpr const char* WATCHED = "✓";    // Checkmark (file watched)
    constexpr const char* NOT_FOUND = "✗";  // X mark (file not found)
    constexpr const char* EMPTY = "";       // No button/icon
}

class hashes_ : public QTableWidget
{
	Q_OBJECT
public:
	bool event(QEvent *e);
};

// Custom table widget for unknown files (files not in AniDB database)
class unknown_files_ : public QTableWidget
{
    Q_OBJECT
public:
    unknown_files_(QWidget *parent = nullptr);
    bool event(QEvent *e) override;
};

// Custom tree widget item that can sort episodes using epno type
class EpisodeTreeWidgetItem : public QTreeWidgetItem
{
public:
    EpisodeTreeWidgetItem(QTreeWidgetItem *parent) : QTreeWidgetItem(parent) {}
    
    // Prevent copying to avoid slicing
    EpisodeTreeWidgetItem(const EpisodeTreeWidgetItem&) = delete;
    EpisodeTreeWidgetItem& operator=(const EpisodeTreeWidgetItem&) = delete;
    
    void setEpno(const epno& ep) { m_epno = ep; }
    epno getEpno() const { return m_epno; }
    
    bool operator<(const QTreeWidgetItem &other) const override
    {
        int column = treeWidget()->sortColumn();
        
        // If sorting by episode number column (column 2) and both items have epno data
        if(column == COL_EPISODE)
        {
            const EpisodeTreeWidgetItem *otherEpisode = dynamic_cast<const EpisodeTreeWidgetItem*>(&other);
            if(otherEpisode && m_epno.isValid() && otherEpisode->m_epno.isValid())
            {
                return m_epno < otherEpisode->m_epno;
            }
        }
        
        // If sorting by Play column, sort by state stored in UserRole (not display text)
        if(column == COL_PLAY)
        {
            int thisSortKey = data(COL_PLAY, Qt::UserRole).toInt();
            int otherSortKey = other.data(COL_PLAY, Qt::UserRole).toInt();
            return thisSortKey < otherSortKey;
        }
        
        // If sorting by Last Played column, sort by timestamp stored in UserRole
        if(column == COL_LAST_PLAYED)
        {
            qint64 thisTimestamp = data(COL_LAST_PLAYED, Qt::UserRole).toLongLong();
            qint64 otherTimestamp = other.data(COL_LAST_PLAYED, Qt::UserRole).toLongLong();
            
            // Entries with timestamp 0 (never played) should always be at the bottom
            // regardless of sort order (ascending or descending)
            Qt::SortOrder order = treeWidget()->header()->sortIndicatorOrder();
            
            if(thisTimestamp == 0 && otherTimestamp == 0) {
                return false; // Both never played, keep current order
            }
            if(thisTimestamp == 0) {
                // This is never played - should be at bottom
                // In ascending order, return false (not less than, so comes after)
                // In descending order, return true (less than, so comes after when reversed)
                return (order == Qt::DescendingOrder);
            }
            if(otherTimestamp == 0) {
                // Other is never played - should be at bottom
                // In ascending order, return true (this is less than, so comes before)
                // In descending order, return false (not less than, so comes before when reversed)
                return (order == Qt::AscendingOrder);
            }
            
            return thisTimestamp < otherTimestamp;
        }
        
        // Default comparison for other columns
        return QTreeWidgetItem::operator<(other);
    }
    
private:
    epno m_epno;
};

// Custom tree widget item for anime items that can sort by aired dates
class AnimeTreeWidgetItem : public QTreeWidgetItem
{
public:
    AnimeTreeWidgetItem(QTreeWidget *parent) : QTreeWidgetItem(parent) {}
    
    // Prevent copying to avoid slicing
    AnimeTreeWidgetItem(const AnimeTreeWidgetItem&) = delete;
    AnimeTreeWidgetItem& operator=(const AnimeTreeWidgetItem&) = delete;
    
    void setAired(aired airedDates) { m_aired = airedDates; }
    aired getAired() const { return m_aired; }
    
    bool operator<(const QTreeWidgetItem &other) const override
    {
        int column = treeWidget()->sortColumn();
        
        // If sorting by Aired column (column 9) and both items have aired data
        if(column == COL_AIRED)
        {
            const AnimeTreeWidgetItem *otherAnime = dynamic_cast<const AnimeTreeWidgetItem*>(&other);
            if(otherAnime && m_aired.isValid() && otherAnime->m_aired.isValid())
            {
                return m_aired < otherAnime->m_aired;
            }
            // If one has aired data and the other doesn't, put the one with data first
            else if(m_aired.isValid() && otherAnime && !otherAnime->m_aired.isValid())
            {
                return true; // this item comes before other
            }
            else if(!m_aired.isValid() && otherAnime && otherAnime->m_aired.isValid())
            {
                return false; // other item comes before this
            }
        }
        
        // If sorting by Play column, sort by state stored in UserRole (not display text)
        if(column == COL_PLAY)
        {
            int thisSortKey = data(COL_PLAY, Qt::UserRole).toInt();
            int otherSortKey = other.data(COL_PLAY, Qt::UserRole).toInt();
            return thisSortKey < otherSortKey;
        }
        
        // If sorting by Last Played column, sort by timestamp stored in UserRole
        if(column == COL_LAST_PLAYED)
        {
            qint64 thisTimestamp = data(COL_LAST_PLAYED, Qt::UserRole).toLongLong();
            qint64 otherTimestamp = other.data(COL_LAST_PLAYED, Qt::UserRole).toLongLong();
            
            // Entries with timestamp 0 (never played) should always be at the bottom
            // regardless of sort order (ascending or descending)
            Qt::SortOrder order = treeWidget()->header()->sortIndicatorOrder();
            
            if(thisTimestamp == 0 && otherTimestamp == 0) {
                return false; // Both never played, keep current order
            }
            if(thisTimestamp == 0) {
                // This is never played - should be at bottom
                // In ascending order, return false (not less than, so comes after)
                // In descending order, return true (less than, so comes after when reversed)
                return (order == Qt::DescendingOrder);
            }
            if(otherTimestamp == 0) {
                // Other is never played - should be at bottom
                // In ascending order, return true (this is less than, so comes before)
                // In descending order, return false (not less than, so comes before when reversed)
                return (order == Qt::AscendingOrder);
            }
            
            return thisTimestamp < otherTimestamp;
        }
        
        // Default comparison for other columns
        return QTreeWidgetItem::operator<(other);
    }
    
private:
    aired m_aired;
};

// Custom tree widget item for file items (third level in hierarchy)
class FileTreeWidgetItem : public QTreeWidgetItem
{
public:
    enum FileType {
        Video,
        Subtitle,
        Audio,
        Other
    };
    
    explicit FileTreeWidgetItem(QTreeWidgetItem *parent) : QTreeWidgetItem(parent), m_fileType(Other) {}
    
    // Prevent copying to avoid slicing
    FileTreeWidgetItem(const FileTreeWidgetItem&) = delete;
    FileTreeWidgetItem& operator=(const FileTreeWidgetItem&) = delete;
    
    void setFileType(FileType type) { m_fileType = type; }
    FileType getFileType() const { return m_fileType; }
    
    void setResolution(const QString& res) { m_resolution = res; }
    QString getResolution() const { return m_resolution; }
    
    void setQuality(const QString& qual) { m_quality = qual; }
    QString getQuality() const { return m_quality; }
    
    void setGroupName(const QString& group) { m_groupName = group; }
    QString getGroupName() const { return m_groupName; }
    
    bool operator<(const QTreeWidgetItem &other) const override
    {
        int column = treeWidget()->sortColumn();
        
        // If sorting by Play column, sort by state stored in UserRole (not display text)
        if(column == COL_PLAY)
        {
            int thisSortKey = data(COL_PLAY, Qt::UserRole).toInt();
            int otherSortKey = other.data(COL_PLAY, Qt::UserRole).toInt();
            return thisSortKey < otherSortKey;
        }
        
        // If sorting by Last Played column, sort by timestamp stored in UserRole
        if(column == COL_LAST_PLAYED)
        {
            qint64 thisTimestamp = data(COL_LAST_PLAYED, Qt::UserRole).toLongLong();
            qint64 otherTimestamp = other.data(COL_LAST_PLAYED, Qt::UserRole).toLongLong();
            
            // Entries with timestamp 0 (never played) should always be at the bottom
            // regardless of sort order (ascending or descending)
            Qt::SortOrder order = treeWidget()->header()->sortIndicatorOrder();
            
            if(thisTimestamp == 0 && otherTimestamp == 0) {
                return false; // Both never played, keep current order
            }
            if(thisTimestamp == 0) {
                // This is never played - should be at bottom
                // In ascending order, return false (not less than, so comes after)
                // In descending order, return true (less than, so comes after when reversed)
                return (order == Qt::DescendingOrder);
            }
            if(otherTimestamp == 0) {
                // Other is never played - should be at bottom
                // In ascending order, return true (this is less than, so comes before)
                // In descending order, return false (not less than, so comes before when reversed)
                return (order == Qt::AscendingOrder);
            }
            
            return thisTimestamp < otherTimestamp;
        }
        
        // Default comparison for other columns
        return QTreeWidgetItem::operator<(other);
    }
    
private:
    FileType m_fileType;
    QString m_resolution;
    QString m_quality;
    QString m_groupName;
};

// Worker thread for loading mylist anime IDs
class MylistLoaderWorker : public QObject
{
    Q_OBJECT
public:
    explicit MylistLoaderWorker(const QString &dbName) : m_dbName(dbName) {}

    void doWork();

signals:
    void finished(const QList<int> &aids);

private:
    QString m_dbName;
};

// Worker thread for loading anime titles cache
class AnimeTitlesLoaderWorker : public QObject
{
    Q_OBJECT
public:
    explicit AnimeTitlesLoaderWorker(const QString &dbName) : m_dbName(dbName) {}

    void doWork();

signals:
    void finished(const QStringList &titles, const QMap<QString, int> &titleToAid);

private:
    QString m_dbName;
};

// Data structure for unbound file information
struct UnboundFileData {
    QString filename;
    QString filepath;
    QString hash;
    qint64 size;
};

// Worker thread for loading unbound files
class UnboundFilesLoaderWorker : public QObject
{
    Q_OBJECT
public:
    explicit UnboundFilesLoaderWorker(const QString &dbName) : m_dbName(dbName) {}

    void doWork();

signals:
    void finished(const QList<UnboundFileData> &files);

private:
    QString m_dbName;
};

class Window : public QWidget
{
    Q_OBJECT
private:
	bool eventFilter(QObject *, QEvent *);
	void closeEvent(QCloseEvent *);
    QTimer *safeclose;
    QTimer *startupTimer;
    QElapsedTimer waitforlogout;
    QElapsedTimer hashingTimer;
    QElapsedTimer lastEtaUpdate;
    int totalHashParts;
    int completedHashParts;
    
    // Track last progress per thread to calculate deltas with throttled updates
    QMap<int, int> lastThreadProgress;
    
    // Track progress history for ETA calculation using moving average
    struct ProgressSnapshot {
        qint64 timestamp;  // Time in milliseconds
        int completedParts;
    };
    QList<ProgressSnapshot> progressHistory;
    
    // Mutex to protect file assignment from concurrent thread requests
    QMutex fileRequestMutex;
    
    // Constants for deferred processing
    static const int HASHED_FILES_BATCH_SIZE = 5; // Process 5 files per timer tick
    static const int HASHED_FILES_TIMER_INTERVAL = 10; // Process every 10ms
    QColor m_hashedFileColor; // Reusable color object for UI updates
    
    // Legacy constants for backward compatibility (deprecated, use enum instead)
    static const int PLAY_COLUMN = COL_PLAY;
    static const int MYLIST_ID_COLUMN = COL_MYLIST_ID;
    
	// main layout
    QBoxLayout *layout;
    QTabWidget *tabwidget;
    QPushButton *loginbutton;

    // pages
    QBoxLayout *pageHasher;
    QWidget *pageHasherParent;
    QBoxLayout *pageMylist;
    QWidget *pageMylistParent;
    QBoxLayout *pageNotify;
    QWidget *pageNotifyParent;
    QGridLayout *pageSettings;
    QWidget *pageSettingsParent;
	QBoxLayout *pageLog;
	QWidget *pageLogParent;
	QBoxLayout *pageApiTester;
	QWidget *pageApiTesterParent;

	// page hasher
    QBoxLayout *pageHasherAddButtons;
    QGridLayout *pageHasherSettings;
    QWidget *pageHasherAddButtonsParent;
    QWidget *pageHasherSettingsParent;
    QPushButton *button1;
    QPushButton *button2;
	QPushButton *button3;
    QTextEdit *hasherOutput;
    QPushButton *buttonstart;
    QPushButton *buttonstop;
	QPushButton *buttonclear;
	QVector<QProgressBar*> threadProgressBars; // One progress bar per hasher thread
	QProgressBar *progressTotal;
	QLabel *progressTotalLabel;
	QCheckBox *addtomylist;
	QCheckBox *markwatched;
	QLineEdit *storage;
	QComboBox *hasherFileState;
	QCheckBox *moveto;
	QCheckBox *renameto;
	QLineEdit *movetodir;
	QLineEdit *renametopattern;


	QString lastDir;

    // page mylist (card view only - tree view removed)
    QScrollArea *mylistCardScrollArea;
    QWidget *mylistCardContainer;
    FlowLayout *mylistCardLayout;
    QComboBox *mylistSortComboBox;
    QPushButton *mylistSortOrderButton;
    QLabel *mylistStatusLabel;
    bool mylistSortAscending;  // true for ascending, false for descending
    MyListCardManager *cardManager;  // Manages card lifecycle and updates
    QList<AnimeCard*> animeCards;  // Deprecated: kept for backward compatibility, use cardManager instead
	QSet<int> episodesNeedingData;  // Deprecated: moved to MyListCardManager
	QSet<int> animeNeedingMetadata;  // Deprecated: moved to MyListCardManager
	QSet<int> animeMetadataRequested;  // Deprecated: moved to MyListCardManager
	QSet<int> animeNeedingPoster;  // Deprecated: moved to MyListCardManager
	QMap<int, QString> animePicnames;  // Deprecated: moved to MyListCardManager
	QNetworkAccessManager *posterNetworkManager;  // Deprecated: moved to MyListCardManager
	QMap<QNetworkReply*, int> posterDownloadRequests;  // Deprecated: moved to MyListCardManager
	// page settings

    QLabel *labelLogin;
    QLineEdit *editLogin;
    QLabel *labelPassword;
    QLineEdit *editPassword;
    QPushButton *buttonSaveSettings;
    QPushButton *buttonRequestMylistExport;
    
    // Directory watcher settings
    QCheckBox *watcherEnabled;
    QLineEdit *watcherDirectory;
    QPushButton *watcherBrowseButton;
    QCheckBox *watcherAutoStart;
    QLabel *watcherStatusLabel;
    
    // Auto-fetch settings
    QCheckBox *autoFetchEnabled;
    
    // Playback settings
    QLineEdit *mediaPlayerPath;
    QPushButton *mediaPlayerBrowseButton;

	// page notify
	QTextEdit *notifyOutput;

	// Notification tracking for mylist export
	int expectedNotificationsToCheck;
	int notificationsCheckedWithoutExport;
	bool isCheckingNotifications;

	// page log
	QTextEdit *logOutput;

	// page apitester
	QLineEdit *apitesterInput;
	QTextEdit *apitesterOutput;
	
	// Directory watcher
	DirectoryWatcher *directoryWatcher;
	
	// Playback manager and UI
	PlaybackManager *playbackManager;
	PlayButtonDelegate *playButtonDelegate;
	QMap<int, int> m_playingItems; // lid -> animation frame (0, 1, 2)
	QTimer *m_animationTimer;
	
	// Batch processing for hashed files
	// Note: HashedFileData structure removed as identification now happens immediately
	// Only keep hash updates for efficient database batching
	QList<QPair<QString, QString>> pendingHashUpdates; // path, hash pairs for database update
	
	// Deferred processing for already-hashed files to prevent UI freeze
	struct HashedFileInfo {
		int rowIndex;
		QString filePath;
		QString filename;
		QString hexdigest;
		qint64 fileSize;
		bool useUserSettings;  // If true, use UI settings; if false, use auto-watcher defaults
		bool addToMylist;      // Whether to add to mylist
		int markWatchedState;  // Used only when useUserSettings is true
		int fileState;         // Used only when useUserSettings is true
	};
	QList<HashedFileInfo> pendingHashedFilesQueue;
	QTimer *hashedFilesProcessingTimer;
	
	// Background loading support to prevent UI freeze
	QThread *mylistLoadingThread;
	QThread *animeTitlesLoadingThread;
	QThread *unboundFilesLoadingThread;
	void startBackgroundLoading();
	
	// Progressive card loading to keep UI responsive
	QTimer *progressiveCardLoadingTimer;
	QList<int> pendingCardsToLoad;
	static const int CARD_LOADING_BATCH_SIZE = 10; // Load 10 cards per timer tick
	static const int CARD_LOADING_TIMER_INTERVAL = 10; // Process every 10ms
	
	// Mutex for protecting shared data between threads
	QMutex backgroundLoadingMutex;


public slots:
    void Button1Click();
    void Button2Click();
	void Button3Click();
    void ButtonHasherStartClick();
    void ButtonHasherStopClick();
    void ButtonHasherClearClick();
    void getNotifyPartsDone(int threadId, int total, int done);
    void getNotifyFileHashed(int threadId, ed2k::ed2kfilestruct fileData);
    void getNotifyLogAppend(QString);
    void getNotifyLoginChagned(QString);
    void getNotifyPasswordChagned(QString);
    void hasherFinished();
    void shot();
    void saveSettings();
	void apitesterProcess();
	void markwatchedStateChanged(int);

    void ButtonLoginClick();
	void getNotifyMylistAdd(QString, int);
    void getNotifyLoggedIn(QString, int);
    void getNotifyLoggedOut(QString, int);
	void getNotifyMessageReceived(int nid, QString message);
	void getNotifyCheckStarting(int count);
	void getNotifyExportQueued(QString tag);
	void getNotifyExportAlreadyInQueue(QString tag);
	void getNotifyExportNoSuchTemplate(QString tag);
	void getNotifyEpisodeUpdated(int eid, int aid);
	void getNotifyAnimeUpdated(int aid);
	void onMylistItemExpanded(QTreeWidgetItem *item);
    void safeClose();
    void loadMylistFromDatabase();
    void updateEpisodeInTree(int eid, int aid);
    void updateOrAddMylistEntry(int lid);
    void startupInitialization();
    bool isMylistFirstRunComplete();
    void setMylistFirstRunComplete();
    void requestMylistExportManually();
    void saveMylistSorting();
    void restoreMylistSorting();
    void onMylistSortChanged(int column, Qt::SortOrder order);
    
    // MyList card view slots
    void sortMylistCards(int sortIndex);
    void toggleSortOrder();
    void loadMylistAsCards();
    void onCardClicked(int aid);
    void onCardEpisodeClicked(int lid);
    void onPlayAnimeFromCard(int aid);
    void onResetWatchSession(int aid);
    
    // Poster download slots
    void onPosterDownloadFinished(QNetworkReply *reply);
    void downloadPosterForAnime(int aid, const QString &picname);
    
    // Directory watcher slots
    void onWatcherEnabledChanged(int state);
    void onWatcherBrowseClicked();
    void onWatcherNewFilesDetected(const QStringList &filePaths);
    
    // Playback slots
    void onMediaPlayerBrowseClicked();
    void onPlayButtonClicked(const QModelIndex &index);
    void onPlaybackPositionUpdated(int lid, int position, int duration);
    void onPlaybackCompleted(int lid);
    void onPlaybackStopped(int lid, int position);
    void onPlaybackStateChanged(int lid, bool isPlaying);
    void onFileMarkedAsLocallyWatched(int lid);
    void onAnimationTimerTimeout();
    
    // Hasher slots
    void provideNextFileToHash();
    
    // Unknown files slots
    void onUnknownFileAnimeSearchChanged(int row);
    void onUnknownFileEpisodeSelected(int row);
    void onUnknownFileBindClicked(int row, const QString& epno);
    void onUnknownFileNotAnimeClicked(int row);
    void onUnknownFileRecheckClicked(int row);

signals:
	void notifyStopHasher();

private slots:
	// Background loading completion handlers
	void onMylistLoadingFinished(const QList<int> &aids);
	void onAnimeTitlesLoadingFinished(const QStringList &titles, const QMap<QString, int> &titleToAid);
	void onUnboundFilesLoadingFinished(const QList<UnboundFileData> &files);
	
	// Timer-based processing handlers
	void processPendingHashedFiles();
	void loadNextCardBatch();

public:
	// page hasher
    hashes_ *hashes;
    unknown_files_ *unknownFiles;
	void hashesinsertrow(QFileInfo, Qt::CheckState, const QString& preloadedHash = QString());
    void unknownFilesInsertRow(const QString& filename, const QString& filepath, const QString& hash, qint64 size);
    void loadUnboundFiles();
	int parseMylistExport(const QString &tarGzPath);
    Window();
	~Window();
private:
    // Unknown files data structure
    struct UnknownFileData {
        QString filename;
        QString filepath;
        QString hash;
        qint64 size;
        int selectedAid;
        int selectedEid;
    };
    QMap<int, UnknownFileData> unknownFilesData; // row index -> file data
    
    // Cached anime titles for unknown files widget (to avoid repeated DB queries)
    QStringList cachedAnimeTitles;
    QMap<QString, int> cachedTitleToAid;
    bool animeTitlesCacheLoaded;
    void loadAnimeTitlesCache();
    
    bool validateDatabaseConnection(const QSqlDatabase& db, const QString& methodName);
    void debugPrintDatabaseInfoForLid(int lid);
    void insertMissingEpisodePlaceholders(int aid, QTreeWidgetItem* animeItem, const QMap<QPair<int, int>, QTreeWidgetItem*>& episodeItems);
    int calculateTotalHashParts(const QStringList &files);
    void setupHashingProgress(const QStringList &files);
    QStringList getFilesNeedingHash();
    
    // Helper methods for playback
    QString getFilePathForPlayback(int lid);
    int getPlaybackResumePosition(int lid);
    void startPlaybackForFile(int lid);
    void updatePlayButtonForItem(QTreeWidgetItem *item);
    void updatePlayButtonsInTree(QTreeWidgetItem *rootItem = nullptr);
    bool isItemPlaying(QTreeWidgetItem *item) const;
    void updateUIForWatchedFile(int lid);  // Update tree view and anime card for a watched file
    
    // Helper method to determine file type from filetype string
    FileTreeWidgetItem::FileType determineFileType(const QString& filetype);
    
    // File selection for playback (stub for future implementation)
    struct FilePreference {
        QString preferredResolution; // e.g., "1920x1080", "1280x720"
        QString preferredGroup;      // e.g., "Baka-Anime", "HorribleSubs"
        bool preferHigherQuality;    // true = prefer higher quality
    };
    FileTreeWidgetItem* selectPreferredFile(const QList<FileTreeWidgetItem*>& files, const FilePreference& pref);
};

#endif // WINDOW_H
