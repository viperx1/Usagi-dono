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
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGroupBox>
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
#include "virtualflowlayout.h"
#include "mylistcardmanager.h"
#include "mylistfiltersidebar.h"
#include "watchsessionmanager.h"
#include "uicolors.h"
#include "localfileinfo.h"
#include "progresstracker.h"
#include "hashingtask.h"
#include "animemetadatacache.h"
#include "backgrounddatabaseworker.h"
//#include "hasherthread.h"

// Forward declarations
class PlayButtonDelegate;
class MyListCardManager;
class VirtualFlowLayout;
class WatchSessionManager;

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

// Worker thread for loading mylist anime IDs
class MylistLoaderWorker : public BackgroundDatabaseWorker<QList<int>>
{
    Q_OBJECT
public:
    explicit MylistLoaderWorker(const QString &dbName) 
        : BackgroundDatabaseWorker<QList<int>>(dbName, "MylistThread") {}

signals:
    void finished(const QList<int> &aids);

protected:
    QList<int> executeQuery(QSqlDatabase &db) override;
    QList<int> getDefaultResult() const override { return QList<int>(); }
    void emitFinished(const QList<int> &result) override { emit finished(result); }
};

// Worker thread for loading anime titles cache
// Note: This worker returns two results, so we use a QPair
class AnimeTitlesLoaderWorker : public BackgroundDatabaseWorker<QPair<QStringList, QMap<QString, int>>>
{
    Q_OBJECT
public:
    explicit AnimeTitlesLoaderWorker(const QString &dbName) 
        : BackgroundDatabaseWorker<QPair<QStringList, QMap<QString, int>>>(dbName, "AnimeTitlesThread") {}

signals:
    void finished(const QStringList &titles, const QMap<QString, int> &titleToAid);

protected:
    QPair<QStringList, QMap<QString, int>> executeQuery(QSqlDatabase &db) override;
    QPair<QStringList, QMap<QString, int>> getDefaultResult() const override { 
        return QPair<QStringList, QMap<QString, int>>(QStringList(), QMap<QString, int>()); 
    }
    void emitFinished(const QPair<QStringList, QMap<QString, int>> &result) override { 
        emit finished(result.first, result.second); 
    }
};

// Use LocalFileInfo class for unbound file information
// (replaced UnboundFileData struct with proper class)
using UnboundFileData = LocalFileInfo;

// Worker thread for loading unbound files
class UnboundFilesLoaderWorker : public BackgroundDatabaseWorker<QList<LocalFileInfo>>
{
    Q_OBJECT
public:
    explicit UnboundFilesLoaderWorker(const QString &dbName) 
        : BackgroundDatabaseWorker<QList<LocalFileInfo>>(dbName, "UnboundFilesThread") {}

signals:
    void finished(const QList<LocalFileInfo> &files);

protected:
    QList<LocalFileInfo> executeQuery(QSqlDatabase &db) override;
    QList<LocalFileInfo> getDefaultResult() const override { return QList<LocalFileInfo>(); }
    void emitFinished(const QList<LocalFileInfo> &result) override { emit finished(result); }
};

class Window : public QWidget
{
    Q_OBJECT
private:
	bool eventFilter(QObject *, QEvent *);
	void closeEvent(QCloseEvent *);
	void changeEvent(QEvent *event);
    QTimer *safeclose;
    QTimer *startupTimer;
    QElapsedTimer waitforlogout;
    
    // Hashing progress tracking using ProgressTracker utility class
    ProgressTracker m_hashingProgress;
    
    // Legacy progress tracking fields (kept for backward compatibility during transition)
    int totalHashParts;
    int completedHashParts;
    
    // Track last progress per thread to calculate deltas with throttled updates
    QMap<int, int> lastThreadProgress;
    
    // Mutex to protect file assignment from concurrent thread requests
    QMutex fileRequestMutex;
    
    // Constants for deferred processing
    static const int HASHED_FILES_BATCH_SIZE = 5; // Process 5 files per timer tick
    static const int HASHED_FILES_TIMER_INTERVAL = 10; // Process every 10ms
    QColor m_hashedFileColor; // Reusable color object for UI updates
    
    // Logout timeout constant
    static const int LOGOUT_TIMEOUT_MS = 5000; // Wait up to 5 seconds for logout to complete
    
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
    VirtualFlowLayout *mylistVirtualLayout;  // Virtual scrolling layout for efficient rendering
    QLabel *mylistStatusLabel;
    bool mylistSortAscending;  // Deprecated: moved to MyListFilterSidebar::getSortAscending() - still used at window.cpp:334 (migration incomplete)
    bool lastInMyListState;  // Track previous "In MyList" filter state for reload detection
    QSet<int> mylistAnimeIdSet;  // Set of anime IDs that are in the user's mylist (for fast filtering)
    QList<int> allAnimeIdsList;  // Full unfiltered list of anime IDs for filtering (populated by onMylistLoadingFinished/loadMylistAsCards, never modified by filters)
    bool allAnimeTitlesLoaded;  // Flag to track if all anime titles have been loaded
    MyListCardManager *cardManager;  // Manages card lifecycle and updates
    MyListFilterSidebar *filterSidebar;  // Filter sidebar widget
    QScrollArea *filterSidebarScrollArea;  // Scroll area containing filter sidebar
    QPushButton *toggleFilterBarButton;  // Button to show/hide filter sidebar
    QList<AnimeCard*> animeCards;  // Deprecated: kept for backward compatibility, use cardManager instead (migration incomplete)
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
    
    // Tray settings
    QCheckBox *trayMinimizeToTray;
    QCheckBox *trayCloseToTray;
    QCheckBox *trayStartMinimized;
    
    // Auto-start settings
    QCheckBox *autoStartEnabled;
    
    // Playback settings
    QLineEdit *mediaPlayerPath;
    QPushButton *mediaPlayerBrowseButton;
    
    // Session manager settings
    QSpinBox *sessionAheadBufferSpinBox;
    QComboBox *sessionThresholdTypeComboBox;
    QDoubleSpinBox *sessionThresholdValueSpinBox;
    QCheckBox *sessionAutoMarkDeletionCheckbox;
    
    // File deletion settings
    QCheckBox *sessionEnableAutoDeletionCheckbox;
    QCheckBox *sessionForceDeletePermissionsCheckbox;

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
	
	// Watch session manager
	WatchSessionManager *watchSessionManager;
	
	// System tray icon
	QSystemTrayIcon *trayIcon;
	QMenu *trayIconMenu;
	bool closeToTrayEnabled;
	bool minimizeToTrayEnabled;
	bool startMinimizedEnabled;
	bool exitingFromTray;  // Flag to indicate exit was triggered from tray menu
	Qt::WindowStates windowStateBeforeHide;  // Store window state before hiding to tray
	QRect windowGeometryBeforeHide;  // Store window geometry before hiding to tray
	
	// Batch processing for hashed files
	// Note: HashedFileData structure removed as identification now happens immediately
	// Only keep hash updates for efficient database batching
	QList<QPair<QString, QString>> pendingHashUpdates; // path, hash pairs for database update
	
	// Deferred processing for already-hashed files to prevent UI freeze
	QList<HashingTask> pendingHashedFilesQueue;
	QTimer *hashedFilesProcessingTimer;
	
	// Background loading support to prevent UI freeze
	QThread *mylistLoadingThread;
	QThread *animeTitlesLoadingThread;
	QThread *unboundFilesLoadingThread;
	void startBackgroundLoading();
	
	// Mutex for protecting shared data between threads
	QMutex backgroundLoadingMutex;
	
	// Mutex for protecting filter/sort operations to prevent race conditions
	QMutex filterOperationsMutex;
	
	// Cache for hasher filter patterns (for performance)
	QString cachedFilterMasks;
	QList<QRegularExpression> cachedFilterRegexes;
	void updateFilterCache();


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
    void applyMylistFilters();
    void checkAndRequestChainRelations(int aid);  // Check and request missing relation data for anime's chain
    
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
    void onUnknownFileDeleteClicked(int row);
    
    // Filter bar toggle slot
    void onToggleFilterBarClicked();
    
    // System tray slots
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayShowHideAction();
    void onTrayExitAction();
    
    // Application quit handler (for external termination)
    void onApplicationAboutToQuit();

signals:
	void notifyStopHasher();

private slots:
	// Background loading completion handlers
	void onMylistLoadingFinished(const QList<int> &aids);
	void onAnimeTitlesLoadingFinished(const QStringList &titles, const QMap<QString, int> &titleToAid);
	void onUnboundFilesLoadingFinished(const QList<LocalFileInfo> &files);
	
	// Timer-based processing handlers
	void processPendingHashedFiles();

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
    // Unknown files data structure - now using LocalFileInfo class
    QMap<int, LocalFileInfo> unknownFilesData; // row index -> file data
    
    // Cached anime titles for unknown files widget (to avoid repeated DB queries)
    QStringList cachedAnimeTitles;
    QMap<QString, int> cachedTitleToAid;
    bool animeTitlesCacheLoaded;
    void loadAnimeTitlesCache();
    
    // Helper cache for mylist filtering - now using proper class
    AnimeMetadataCache animeAlternativeTitlesCache;  // aid -> alternative titles
    void loadAnimeAlternativeTitlesForFiltering();
    void updateAnimeAlternativeTitlesInCache(int aid);  // Update single anime in cache
    void addAnimeTitlesToList(QStringList& titles, const QString& romaji, const QString& english,
                              const QString& other, const QString& shortNames, const QString& synonyms);  // Helper for title parsing
    bool matchesSearchFilter(AnimeCard *card, const QString &searchText);
    bool matchesSearchFilter(int aid, const QString &animeName, const QString &searchText);
    
    // Helper method for hasher file filtering
    bool shouldFilterFile(const QString &filePath);
    
    // Helper method for adding files from directory (extracted to avoid code duplication)
    void addFilesFromDirectory(const QString &dirPath);
    
    bool validateDatabaseConnection(const QSqlDatabase& db, const QString& methodName);
    void debugPrintDatabaseInfoForLid(int lid);
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
    
    // System tray helper methods
    void createTrayIcon();
    void updateTrayIconVisibility();
    void loadTraySettings();
    void saveTraySettings();
    
    // Icon helper method
    QIcon loadUsagiIcon();  // Loads usagi.png from various paths, falls back to default icon
    
    // Auto-start helper methods
    void setAutoStartEnabled(bool enabled);
    bool isAutoStartEnabled();
    void registerAutoStart();
    void unregisterAutoStart();
};

#endif // WINDOW_H
