#ifndef WINDOW_H
#define WINDOW_H
#include <QtGui>
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
#include <QXmlStreamReader>
#include "hash/ed2k.h"
#include "anidbapi.h"
#include "epno.h"
#include "aired.h"
#include "directorywatcher.h"
#include "playbackmanager.h"
//#include "hasherthread.h"

// Forward declarations
class PlayButtonDelegate;

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

class hashes_ : public QTableWidget
{
public:
	bool event(QEvent *e);
};

// Custom tree widget item that can sort episodes using epno type
class EpisodeTreeWidgetItem : public QTreeWidgetItem
{
public:
    EpisodeTreeWidgetItem(QTreeWidgetItem *parent) : QTreeWidgetItem(parent) {}
    
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
    
    void setAired(const aired& airedDates) { m_aired = airedDates; }
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
	QCheckBox *serializedIO; // Serialize file I/O for HDD performance
	QLineEdit *storage;
	QComboBox *hasherFileState;
	QCheckBox *moveto;
	QCheckBox *renameto;
	QLineEdit *movetodir;
	QLineEdit *renametopattern;


	QString lastDir;

    // page mylist
    QTreeWidget *mylistTreeWidget;
    QLabel *mylistStatusLabel;
	QSet<int> episodesNeedingData;  // Track EIDs that need EPISODE API call
	QSet<int> animeNeedingMetadata;  // Track AIDs that need metadata (typename, startdate, enddate)
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
	void processPendingHashedFiles();


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
    void onAnimationTimerTimeout();
    
    // Hasher slots
    void provideNextFileToHash();

signals:
	void notifyStopHasher();
public:
	// page hasher
    hashes_ *hashes;
	void hashesinsertrow(QFileInfo, Qt::CheckState, const QString& preloadedHash = QString());
	int parseMylistExport(const QString &tarGzPath);
    Window();
	~Window();
private:
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
