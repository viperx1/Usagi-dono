#ifndef WINDOW_H
#define WINDOW_H
#include <QtGui>
#include <QtAlgorithms>
#include <QRegularExpression>
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
//#include "hasherthread.h"

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
        
        // If sorting by episode number column (column 1) and both items have epno data
        if(column == 1)
        {
            const EpisodeTreeWidgetItem *otherEpisode = dynamic_cast<const EpisodeTreeWidgetItem*>(&other);
            if(otherEpisode && m_epno.isValid() && otherEpisode->m_epno.isValid())
            {
                return m_epno < otherEpisode->m_epno;
            }
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
        
        // If sorting by Aired column (column 8) and both items have aired data
        if(column == 8)
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
    
    FileTreeWidgetItem(QTreeWidgetItem *parent) : QTreeWidgetItem(parent), m_fileType(Other) {}
    
    void setFileType(FileType type) { m_fileType = type; }
    FileType getFileType() const { return m_fileType; }
    
    void setResolution(const QString& res) { m_resolution = res; }
    QString getResolution() const { return m_resolution; }
    
    void setQuality(const QString& qual) { m_quality = qual; }
    QString getQuality() const { return m_quality; }
    
    void setGroupName(const QString& group) { m_groupName = group; }
    QString getGroupName() const { return m_groupName; }
    
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
    
    // Constants for deferred processing
    static const int HASHED_FILES_BATCH_SIZE = 5; // Process 5 files per timer tick
    static const int HASHED_FILES_TIMER_INTERVAL = 10; // Process every 10ms
    QColor m_hashedFileColor; // Reusable color object for UI updates
    
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
	QProgressBar *progressFile;
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

    // page mylist
    QTreeWidget *mylistTreeWidget;
    QLabel *mylistStatusLabel;
	QSet<int> episodesNeedingData;  // Track EIDs that need EPISODE API call
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
    void getNotifyPartsDone(int, int);
    void getNotifyFileHashed(ed2k::ed2kfilestruct);
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
	void onMylistItemExpanded(QTreeWidgetItem *item);
    void safeClose();
    void loadMylistFromDatabase();
    void updateEpisodeInTree(int eid, int aid);
    void updateOrAddMylistEntry(int lid);
    void startupInitialization();
    bool isMylistFirstRunComplete();
    void setMylistFirstRunComplete();
    void requestMylistExportManually();
    
    // Directory watcher slots
    void onWatcherEnabledChanged(int state);
    void onWatcherBrowseClicked();
    void onWatcherNewFilesDetected(const QStringList &filePaths);
    
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
    int calculateTotalHashParts(const QStringList &files);
    void setupHashingProgress(const QStringList &files);
    QStringList getFilesNeedingHash();
    
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
