#include "main.h"
#include "window.h"
#include "animeutils.h"
#include "hasherthreadpool.h"
#include "hasherthread.h"
#include "crashlog.h"
#include "logger.h"
#include "playbuttondelegate.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <algorithm>
#include <functional>
#include <memory>
#include <QDirIterator>

HasherThreadPool *hasherThreadPool = nullptr;
myAniDBApi *adbapi;
extern Window *window;
settings_struct settings;

// Define static constants for Window class
const int Window::CARD_LOADING_BATCH_SIZE;
const int Window::CARD_LOADING_TIMER_INTERVAL;


Window::Window()
{
	qRegisterMetaType<ed2k::ed2kfilestruct>("ed2k::ed2kfilestruct");
	
	// Initialize global hasher thread pool
	hasherThreadPool = new HasherThreadPool();
	
	adbapi = new myAniDBApi("usagi", 1);
//	settings = new QSettings("settings.dat", QSettings::IniFormat);
//	adbapi->SetUsername(settings->value("username").toString());
//	adbapi->SetPassword(settings->value("password").toString());
	
	// Test unified logging system
	LOG("Window constructor initializing [window.cpp]");
	
	// Initialize notification tracking
	expectedNotificationsToCheck = 0;
	notificationsCheckedWithoutExport = 0;
	isCheckingNotifications = false;
	
	// Initialize hashing progress tracking
	totalHashParts = 0;
	completedHashParts = 0;
	
	// Initialize UI color constants (use Qt's predefined yellow color)
	m_hashedFileColor = QColor(Qt::yellow);
	
	// Initialize anime titles cache flag
	animeTitlesCacheLoaded = false;
	
    safeclose = new QTimer;
    safeclose->setInterval(100);
    connect(safeclose, SIGNAL(timeout()), this, SLOT(safeClose()));

    // Timer for startup initialization (delayed to ensure UI is ready)
    startupTimer = new QTimer(this);
    startupTimer->setSingleShot(true);
    startupTimer->setInterval(1000);
    connect(startupTimer, SIGNAL(timeout()), this, SLOT(startupInitialization()));

    // main window
    this->setWindowTitle("Usagi");
//    this->setFixedWidth(800);
    this->setMinimumSize(800, 600);
//    this->setFixedHeight(600);

    // main layout
    layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
    tabwidget = new QTabWidget;
    loginbutton = new QPushButton("Login");

    // pages
    pageHasherParent = new QWidget;
    pageHasher = new QBoxLayout(QBoxLayout::TopToBottom, pageHasherParent);
    pageMylistParent = new QWidget;
    pageMylist = new QBoxLayout(QBoxLayout::TopToBottom, pageMylistParent);
    pageNotifyParent = new QWidget;
    pageNotify = new QBoxLayout(QBoxLayout::TopToBottom, pageNotifyParent);
    pageSettingsParent = new QWidget;
    pageSettings = new QGridLayout(pageSettingsParent);
    pageLogParent = new QWidget;
    pageLog = new QBoxLayout(QBoxLayout::TopToBottom, pageLogParent);
	pageApiTesterParent = new QWidget;
	pageApiTester = new QBoxLayout(QBoxLayout::TopToBottom, pageApiTesterParent);

    layout->addWidget(tabwidget, 1);

	// tabs
    tabwidget->addTab(pageHasherParent, "Hasher");
    tabwidget->addTab(pageMylistParent, "Mylist");
    tabwidget->addTab(pageNotifyParent, "Notify");
    tabwidget->addTab(pageSettingsParent, "Settings");
    tabwidget->addTab(pageLogParent, "Log");
	tabwidget->addTab(pageApiTesterParent, "ApiTester");

    // page hasher
    pageHasherSettings = new QGridLayout;
    button1 = new QPushButton("Add files...");
    button2 = new QPushButton("Add directories...");
	button3 = new QPushButton("Last directory");
    hashes = new hashes_; // QTableWidget
    unknownFiles = new unknown_files_(this); // Unknown files widget
    hasherOutput = new QTextEdit;
	hasherFileState = new QComboBox;
	addtomylist = new QCheckBox("Add file(s) to MyList");
	markwatched = new QCheckBox("Mark watched (no change)");
    QLabel *label1 = new QLabel("Set state:");
    buttonstart = new QPushButton("Start");
    buttonstop = new QPushButton("Stop");
    buttonclear = new QPushButton("Clear");
	moveto = new QCheckBox("Move to:");
	renameto = new QCheckBox("Rename to:");
	movetodir = new QLineEdit;
	renametopattern = new QLineEdit;
	storage = new QLineEdit;
    QBoxLayout *layout1 = new QBoxLayout(QBoxLayout::LeftToRight);
    QBoxLayout *layout2 = new QBoxLayout(QBoxLayout::LeftToRight);
    QPushButton *movetodirbutton = new QPushButton("...");
    QPushButton *patternhelpbutton = new QPushButton("?");
    QBoxLayout *progress = new QBoxLayout(QBoxLayout::TopToBottom);
    
    // Create one progress bar per hasher thread
    int numThreads = hasherThreadPool->threadCount();
    for (int i = 0; i < numThreads; ++i) {
        QProgressBar *threadProgress = new QProgressBar;
        threadProgress->setFormat(QString("Thread %1: %p%").arg(i));
        threadProgressBars.append(threadProgress);
        progress->addWidget(threadProgress);
    }
    
    progressTotal = new QProgressBar;
    progressTotalLabel = new QLabel;
    QBoxLayout *progressTotalLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    progressTotalLayout->addWidget(progressTotal);
    progressTotalLayout->addWidget(progressTotalLabel);

    pageHasher->addWidget(hashes, 1);
    
    // Add unknown files widget with a label (initially hidden)
    QLabel *unknownFilesLabel = new QLabel("Unknown Files (not in AniDB database):");
    unknownFilesLabel->setObjectName("unknownFilesLabel");
    pageHasher->addWidget(unknownFilesLabel);
    pageHasher->addWidget(unknownFiles, 0, Qt::AlignTop);
    
    // Hide unknown files section initially
    unknownFilesLabel->hide();
    unknownFiles->hide();
    
    pageHasher->addLayout(pageHasherSettings);
    pageHasher->addLayout(progress);
    pageHasher->addWidget(hasherOutput, 0, Qt::AlignTop);

    // page mylist
    // Initialize card view flag (default to card view as per requirement)
    mylistUseCardView = true;
    mylistSortAscending = true;
    
    // Initialize network manager for poster downloads (deprecated, kept for backward compatibility)
    posterNetworkManager = new QNetworkAccessManager(this);
    connect(posterNetworkManager, &QNetworkAccessManager::finished, this, &Window::onPosterDownloadFinished);
    
    // Initialize card manager for efficient card lifecycle management
    cardManager = new MyListCardManager(this);
    
    // Connect card manager signals to window slots
    connect(cardManager, &MyListCardManager::allCardsLoaded, this, [this](int count) {
        mylistStatusLabel->setText(QString("MyList Status: Loaded %1 anime").arg(count));
        animeCards = cardManager->getAllCards();  // Update legacy list for backward compatibility
    });
    
    connect(cardManager, &MyListCardManager::cardCreated, this, [this](int aid, AnimeCard *card) {
        // Connect individual card signals
        connect(card, &AnimeCard::cardClicked, this, &Window::onCardClicked);
        connect(card, &AnimeCard::episodeClicked, this, &Window::onCardEpisodeClicked);
    });
    
    connect(cardManager, &MyListCardManager::cardUpdated, this, [this](int aid) {
        // Card was updated, may need to resort
        if (mylistUseCardView) {
            sortMylistCards(mylistSortComboBox->currentIndex());
        }
    });
    
    // Add toolbar for view toggle and sorting
    QHBoxLayout *mylistToolbar = new QHBoxLayout();
    
    mylistViewToggleButton = new QPushButton("Switch to Tree View");
    mylistViewToggleButton->setMaximumWidth(150);
    connect(mylistViewToggleButton, SIGNAL(clicked()), this, SLOT(toggleMylistView()));
    mylistToolbar->addWidget(mylistViewToggleButton);
    
    mylistToolbar->addWidget(new QLabel("Sort by:"));
    mylistSortComboBox = new QComboBox();
    mylistSortComboBox->addItem("Anime Title");
    mylistSortComboBox->addItem("Type");
    mylistSortComboBox->addItem("Aired Date");
    mylistSortComboBox->addItem("Episodes (Count)");
    mylistSortComboBox->addItem("Completion %");
    mylistSortComboBox->addItem("Last Played");
    mylistSortComboBox->setCurrentIndex(0);
    connect(mylistSortComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(sortMylistCards(int)));
    mylistToolbar->addWidget(mylistSortComboBox);
    
    mylistSortOrderButton = new QPushButton("↑ Asc");
    mylistSortOrderButton->setMaximumWidth(80);
    mylistSortOrderButton->setToolTip("Toggle sort order (ascending/descending)");
    connect(mylistSortOrderButton, SIGNAL(clicked()), this, SLOT(toggleSortOrder()));
    mylistToolbar->addWidget(mylistSortOrderButton);
    
    mylistToolbar->addStretch();
    pageMylist->addLayout(mylistToolbar);
    
    // Tree widget (for tree view mode)
    mylistTreeWidget = new QTreeWidget(this);
    mylistTreeWidget->setColumnCount(11);
    // New column order: Anime, Play, Episode, Episode Title, State, Viewed, Storage, Mylist ID, Type, Aired, Last Played
    mylistTreeWidget->setHeaderLabels(QStringList() << "Anime" << "Play" << "Episode" << "Episode Title" << "State" << "Viewed" << "Storage" << "Mylist ID" << "Type" << "Aired" << "Last Played");
    mylistTreeWidget->setColumnWidth(0, 300);  // Anime
    mylistTreeWidget->setColumnWidth(1, 50);   // Play (narrow for button)
    mylistTreeWidget->setColumnWidth(2, 80);   // Episode
    mylistTreeWidget->setColumnWidth(3, 250);  // Episode Title
    mylistTreeWidget->setColumnWidth(4, 100);  // State
    mylistTreeWidget->setColumnWidth(5, 80);   // Viewed
    mylistTreeWidget->setColumnWidth(6, 200);  // Storage
    mylistTreeWidget->setColumnWidth(7, 100);  // Mylist ID
    mylistTreeWidget->setColumnWidth(8, 180);  // Type
    mylistTreeWidget->setColumnWidth(9, 120);  // Aired
    mylistTreeWidget->setColumnWidth(10, 120); // Last Played
    mylistTreeWidget->setAlternatingRowColors(true);
    mylistTreeWidget->setSortingEnabled(true);
    mylistTreeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pageMylist->addWidget(mylistTreeWidget);
    
    // Card view (for card view mode)
    mylistCardScrollArea = new QScrollArea(this);
    mylistCardScrollArea->setWidgetResizable(true);
    mylistCardScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mylistCardScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    mylistCardContainer = new QWidget();
    mylistCardLayout = new FlowLayout(mylistCardContainer, 10, 10, 10);
    mylistCardContainer->setLayout(mylistCardLayout);
    mylistCardScrollArea->setWidget(mylistCardContainer);
    
    pageMylist->addWidget(mylistCardScrollArea);
    
    // Add progress status label
    mylistStatusLabel = new QLabel("MyList Status: Ready");
    mylistStatusLabel->setAlignment(Qt::AlignCenter);
    mylistStatusLabel->setStyleSheet("padding: 5px; background-color: #f0f0f0;");
    pageMylist->addWidget(mylistStatusLabel);
    
    // Initially show card view (hide tree view)
    mylistTreeWidget->hide();
    mylistCardScrollArea->show();

    // page hasher - signals
    connect(button1, SIGNAL(clicked()), this, SLOT(Button1Click()));
    connect(button2, SIGNAL(clicked()), this, SLOT(Button2Click()));
	connect(button3, SIGNAL(clicked()), this, SLOT(Button3Click()));
    connect(buttonstart, SIGNAL(clicked()), this, SLOT(ButtonHasherStartClick()));
    connect(buttonclear, SIGNAL(clicked()), this, SLOT(ButtonHasherClearClick()));
    connect(hasherThreadPool, SIGNAL(requestNextFile()), this, SLOT(provideNextFileToHash()));
    connect(hasherThreadPool, SIGNAL(sendHash(QString)), hasherOutput, SLOT(append(QString)));
    connect(hasherThreadPool, SIGNAL(finished()), this, SLOT(hasherFinished()));
    // Connect thread pool signals for hashing progress and completion with thread ID
    connect(hasherThreadPool, SIGNAL(notifyPartsDone(int,int,int)), this, SLOT(getNotifyPartsDone(int,int,int)));
    connect(hasherThreadPool, SIGNAL(notifyFileHashed(int,ed2k::ed2kfilestruct)), this, SLOT(getNotifyFileHashed(int,ed2k::ed2kfilestruct)));
    connect(buttonstop, SIGNAL(clicked()), this, SLOT(ButtonHasherStopClick()));
    connect(this, SIGNAL(notifyStopHasher()), adbapi, SLOT(getNotifyStopHasher()));
    connect(adbapi, SIGNAL(notifyLogAppend(QString)), this, SLOT(getNotifyLogAppend(QString)));
	connect(adbapi, SIGNAL(notifyMylistAdd(QString,int)), this, SLOT(getNotifyMylistAdd(QString,int)));
	connect(markwatched, SIGNAL(stateChanged(int)), this, SLOT(markwatchedStateChanged(int)));
	
	// Connect unified Logger to log tab using modern Qt5+ syntax for type safety
	connect(Logger::instance(), &Logger::logMessage, this, &Window::getNotifyLogAppend);

    // page hasher - hashes
	hashes->setColumnCount(10);
    hashes->setRowCount(0);
    hashes->setRowHeight(0, 20);
    hashes->verticalHeader()->setDefaultSectionSize(20);
    hashes->setSelectionBehavior(QAbstractItemView::SelectRows);
    hashes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hashes->verticalHeader()->hide();
    hashes->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    hashes->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	hashes->setHorizontalHeaderLabels((QStringList() << "Filename" << "Progress" << "path" << "LF" << "LL" << "RF" << "RL" << "Ren" << "FP" << "Hash"));
    hashes->setColumnWidth(0, 600);
    hashes->setColumnWidth(9, 250); // Hash column width
//	hashes->setCellWidget(3, 3, button2);
//	hashes->setColumnHidden(2, 1);
//	hashes->setColumnHidden(3, 1);

    // page hasher - settings
    pageHasherSettings->addWidget(label1, 1, 0, Qt::AlignRight);
    pageHasherSettings->addWidget(hasherFileState, 1, 1, Qt::AlignLeft);
	pageHasherSettings->addWidget(storage, 1, 2, Qt::AlignLeft);
    pageHasherSettings->addWidget(addtomylist, 0, 1, Qt::AlignLeft);
    pageHasherSettings->addWidget(markwatched, 0, 2, Qt::AlignLeft);
    pageHasherSettings->addWidget(button1, 1, 4, Qt::AlignLeft);
    pageHasherSettings->addWidget(button2, 1, 5, Qt::AlignLeft);
	pageHasherSettings->addWidget(button3, 1, 6, Qt::AlignLeft);
    pageHasherSettings->addWidget(buttonstart, 0, 4, Qt::AlignLeft);
    pageHasherSettings->addWidget(buttonstop, 0, 5, Qt::AlignLeft);
    pageHasherSettings->addWidget(buttonclear, 0, 6, Qt::AlignLeft);
    pageHasherSettings->addWidget(moveto, 2, 0, Qt::AlignRight);
//    pageHasherSettings->addWidget(renameto, 3, 0, Qt::AlignRight); // hide rename to
    pageHasherSettings->addLayout(layout1, 2, 1, 1, 6);
//	pageHasherSettings->addLayout(layout2, 3, 1, 1, 6); // ^^^
	pageHasherSettings->setColumnStretch(3, 100);

    hasherFileState->addItem("Unknown");
    hasherFileState->addItem("Internal (HDD)");
    hasherFileState->addItem("External (CD/DVD)");
    hasherFileState->addItem("Deleted");
    hasherFileState->setCurrentIndex(1);

    addtomylist->setChecked(1);
	markwatched->setTristate();
	markwatched->setChecked(0);
    renameto->setChecked(1);

	patternhelpbutton->setToolTip("TODO: rename pattern help");
	layout1->addWidget(movetodir);
	layout1->addWidget(movetodirbutton);
	layout2->addWidget(renametopattern);
	layout2->addWidget(patternhelpbutton);

	// page hasher - progress (already added in loop above)
	progress->addLayout(progressTotalLayout);

    // page settings
    labelLogin = new QLabel("Login:");
    editLogin = new QLineEdit;
	editLogin->setText(adbapi->getUsername());
    labelPassword = new QLabel("Password:");
    editPassword = new QLineEdit;
	editPassword->setText(adbapi->getPassword());
    editPassword->setEchoMode(QLineEdit::Password);
    buttonSaveSettings = new QPushButton("Save");
    buttonRequestMylistExport = new QPushButton("Request MyList Export");
    
    // Directory watcher UI
    QLabel *watcherLabel = new QLabel("Directory Watcher:");
    watcherEnabled = new QCheckBox("Enable Directory Watcher");
    watcherDirectory = new QLineEdit;
    watcherBrowseButton = new QPushButton("Browse...");
    watcherAutoStart = new QCheckBox("Auto-start on application launch");
    watcherStatusLabel = new QLabel("Status: Not watching");

    pageSettings->addWidget(labelLogin, 0, 0);
    pageSettings->addWidget(editLogin, 0, 1);
    pageSettings->addWidget(labelPassword, 1, 0);
    pageSettings->addWidget(editPassword, 1, 1);
    
    // Add directory watcher controls right after login/password
    pageSettings->addWidget(watcherLabel, 2, 0, 1, 2);
    pageSettings->addWidget(watcherEnabled, 3, 0, 1, 2);
    pageSettings->addWidget(new QLabel("Watch Directory:"), 4, 0);
    pageSettings->addWidget(watcherDirectory, 4, 1);
    pageSettings->addWidget(watcherBrowseButton, 4, 2);
    pageSettings->addWidget(watcherAutoStart, 5, 0, 1, 2);
    pageSettings->addWidget(watcherStatusLabel, 6, 0, 1, 2);
    
    // Add auto-fetch settings after directory watcher
    QLabel *autoFetchLabel = new QLabel("Auto-fetch:");
    autoFetchEnabled = new QCheckBox("Automatically download anime titles and other data on startup");
    
    pageSettings->addWidget(autoFetchLabel, 7, 0, 1, 2);
    pageSettings->addWidget(autoFetchEnabled, 8, 0, 1, 2);
    
    // Add playback settings after auto-fetch
    QLabel *playbackLabel = new QLabel("Playback:");
    mediaPlayerPath = new QLineEdit;
    mediaPlayerBrowseButton = new QPushButton("Browse...");
    
    pageSettings->addWidget(playbackLabel, 9, 0, 1, 2);
    pageSettings->addWidget(new QLabel("Media Player:"), 10, 0);
    pageSettings->addWidget(mediaPlayerPath, 10, 1);
    pageSettings->addWidget(mediaPlayerBrowseButton, 10, 2);
    
    pageSettings->setRowStretch(11, 1000);
    pageSettings->addWidget(buttonSaveSettings, 12, 0);
    pageSettings->addWidget(buttonRequestMylistExport, 13, 0);

    pageSettings->setColumnStretch(1, 100);
    pageSettings->setRowStretch(14, 100);

	// page settings - signals
    connect(buttonSaveSettings, SIGNAL(clicked()), this, SLOT(saveSettings()));
    connect(buttonRequestMylistExport, SIGNAL(clicked()), this, SLOT(requestMylistExportManually()));
    connect(watcherEnabled, SIGNAL(stateChanged(int)), this, SLOT(onWatcherEnabledChanged(int)));
    connect(watcherBrowseButton, SIGNAL(clicked()), this, SLOT(onWatcherBrowseClicked()));
    connect(mediaPlayerBrowseButton, SIGNAL(clicked()), this, SLOT(onMediaPlayerBrowseClicked()));

    // page log
    logOutput = new QTextEdit;
    pageLog->addWidget(logOutput);

	// page apitester
	apitesterInput = new QLineEdit;
	apitesterOutput = new QTextEdit;
	pageApiTester->addWidget(apitesterInput);
	pageApiTester->addWidget(apitesterOutput);

	// page apitester - signals
	connect(apitesterInput, SIGNAL(returnPressed()), this, SLOT(apitesterProcess()));

    // window - footer
    layout->addWidget(loginbutton);

    // footer - signals
    connect(adbapi, SIGNAL(notifyLoggedIn(QString,int)), this, SLOT(getNotifyLoggedIn(QString,int)));
    connect(adbapi, SIGNAL(notifyLoggedOut(QString,int)), this, SLOT(getNotifyLoggedOut(QString,int)));
	connect(adbapi, SIGNAL(notifyMessageReceived(int,QString)), this, SLOT(getNotifyMessageReceived(int,QString)));
	connect(adbapi, SIGNAL(notifyCheckStarting(int)), this, SLOT(getNotifyCheckStarting(int)));
	connect(adbapi, SIGNAL(notifyExportQueued(QString)), this, SLOT(getNotifyExportQueued(QString)));
	connect(adbapi, SIGNAL(notifyExportAlreadyInQueue(QString)), this, SLOT(getNotifyExportAlreadyInQueue(QString)));
	connect(adbapi, SIGNAL(notifyExportNoSuchTemplate(QString)), this, SLOT(getNotifyExportNoSuchTemplate(QString)));
	connect(adbapi, SIGNAL(notifyEpisodeUpdated(int,int)), this, SLOT(getNotifyEpisodeUpdated(int,int)));
	connect(adbapi, SIGNAL(notifyAnimeUpdated(int)), this, SLOT(getNotifyAnimeUpdated(int)));
	connect(mylistTreeWidget, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(onMylistItemExpanded(QTreeWidgetItem*)));
	connect(mylistTreeWidget->header(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)), this, SLOT(onMylistSortChanged(int,Qt::SortOrder)));
    connect(loginbutton, SIGNAL(clicked()), this, SLOT(ButtonLoginClick()));
    
    // Initialize directory watcher
    directoryWatcher = new DirectoryWatcher(this);
    connect(directoryWatcher, &DirectoryWatcher::newFilesDetected, 
            this, &Window::onWatcherNewFilesDetected);
    
    // Initialize playback manager
    playbackManager = new PlaybackManager(this);
    connect(playbackManager, &PlaybackManager::playbackPositionUpdated,
            this, &Window::onPlaybackPositionUpdated);
    connect(playbackManager, &PlaybackManager::playbackCompleted,
            this, &Window::onPlaybackCompleted);
    connect(playbackManager, &PlaybackManager::playbackStopped,
            this, &Window::onPlaybackStopped);
    connect(playbackManager, &PlaybackManager::playbackStateChanged,
            this, &Window::onPlaybackStateChanged);
    
    // Initialize play button delegate for mylist tree
    playButtonDelegate = new PlayButtonDelegate(this);
    mylistTreeWidget->setItemDelegateForColumn(PLAY_COLUMN, playButtonDelegate);
    connect(playButtonDelegate, &PlayButtonDelegate::playButtonClicked,
            this, &Window::onPlayButtonClicked);
    
    // Setup animation timer for play button animation
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(300);  // 300ms between animation frames
    connect(m_animationTimer, &QTimer::timeout, this, &Window::onAnimationTimerTimeout);
    mylistTreeWidget->setMouseTracking(true); // Enable mouse tracking for hover effects
    
    // Initialize timer for deferred processing of already-hashed files
    hashedFilesProcessingTimer = new QTimer(this);
    hashedFilesProcessingTimer->setSingleShot(false);
    hashedFilesProcessingTimer->setInterval(HASHED_FILES_TIMER_INTERVAL);
    connect(hashedFilesProcessingTimer, &QTimer::timeout, this, &Window::processPendingHashedFiles);
    
    // Initialize background loading watchers
    mylistLoadingWatcher = new QFutureWatcher<QList<int>>(this);
    animeTitlesLoadingWatcher = new QFutureWatcher<void>(this);
    unboundFilesLoadingWatcher = new QFutureWatcher<void>(this);
    
    connect(mylistLoadingWatcher, &QFutureWatcher<QList<int>>::finished,
            this, &Window::onMylistLoadingFinished);
    connect(animeTitlesLoadingWatcher, &QFutureWatcher<void>::finished,
            this, &Window::onAnimeTitlesLoadingFinished);
    connect(unboundFilesLoadingWatcher, &QFutureWatcher<void>::finished,
            this, &Window::onUnboundFilesLoadingFinished);
    
    // Initialize timer for progressive card loading
    progressiveCardLoadingTimer = new QTimer(this);
    progressiveCardLoadingTimer->setSingleShot(false);
    progressiveCardLoadingTimer->setInterval(CARD_LOADING_TIMER_INTERVAL);
    connect(progressiveCardLoadingTimer, &QTimer::timeout, this, &Window::loadNextCardBatch);
    
    // Load directory watcher settings from database
    bool watcherEnabledSetting = adbapi->getWatcherEnabled();
    QString watcherDir = adbapi->getWatcherDirectory();
    bool watcherAutoStartSetting = adbapi->getWatcherAutoStart();
    
    // Load auto-fetch settings from database
    bool autoFetchEnabledSetting = adbapi->getAutoFetchEnabled();
    
    // Block signals while setting UI values to prevent premature slot activation
    watcherEnabled->blockSignals(true);
    watcherDirectory->blockSignals(true);
    watcherAutoStart->blockSignals(true);
    autoFetchEnabled->blockSignals(true);
    
    watcherEnabled->setChecked(watcherEnabledSetting);
    watcherDirectory->setText(watcherDir);
    watcherAutoStart->setChecked(watcherAutoStartSetting);
    autoFetchEnabled->setChecked(autoFetchEnabledSetting);
    
    // Restore signal connections
    watcherEnabled->blockSignals(false);
    watcherDirectory->blockSignals(false);
    watcherAutoStart->blockSignals(false);
    autoFetchEnabled->blockSignals(false);
    
    // Load media player path from settings
    QString playerPath = PlaybackManager::getMediaPlayerPath();
    mediaPlayerPath->setText(playerPath);
    
    adbapi->CreateSocket();

    // Automatically load mylist on startup (using timer for delayed initialization)
    startupTimer->start();

    // Auto-start directory watcher if enabled
    if (watcherEnabledSetting && watcherAutoStartSetting && !watcherDir.isEmpty()) {
        directoryWatcher->startWatching(watcherDir);
        watcherStatusLabel->setText("Status: Watching " + watcherDir);
    } else if (watcherEnabledSetting && !watcherDir.isEmpty()) {
        // If watcher is enabled but auto-start is not, just update status
        watcherStatusLabel->setText("Status: Enabled (not auto-started)");
    } else if (watcherEnabledSetting && watcherDir.isEmpty()) {
        // If watcher is enabled but no directory is set
        watcherStatusLabel->setText("Status: Enabled (no directory set)");
    }

    // end
    this->setLayout(layout);


}

Window::~Window()
{
    // Stop directory watcher on cleanup
    if (directoryWatcher) {
        directoryWatcher->stopWatching();
    }
    
    // Clean up hasher thread pool
    if (hasherThreadPool) {
        hasherThreadPool->stop();
        hasherThreadPool->wait();
        delete hasherThreadPool;
        hasherThreadPool = nullptr;
    }
}

bool Window::validateDatabaseConnection(const QSqlDatabase& db, const QString& methodName)
{
	if(!db.isValid() || !db.isOpen())
	{
		LOG("Error: Database connection is not valid or not open in " + methodName);
		return false;
	}
	return true;
}

void Window::debugPrintDatabaseInfoForLid(int lid)
{
	LOG("=================================================================");
	LOG(QString("DEBUG: Database information for LID: %1").arg(lid));
	LOG("=================================================================");
	
	QSqlDatabase db = QSqlDatabase::database();
	if(!validateDatabaseConnection(db, "debugPrintDatabaseInfoForLid"))
	{
		LOG("ERROR: Cannot debug database info - database not available");
		return;
	}
	
	// Query mylist table
	LOG(QString("--- MYLIST TABLE (lid=%1) ---").arg(lid));
	QSqlQuery mylistQuery(db);
	mylistQuery.prepare("SELECT * FROM mylist WHERE lid = ?");
	mylistQuery.bindValue(0, lid);
	if(mylistQuery.exec() && mylistQuery.next())
	{
		LOG(QString("  lid: %1").arg(mylistQuery.value("lid").toString()));
		LOG(QString("  fid: %1").arg(mylistQuery.value("fid").toString()));
		LOG(QString("  eid: %1").arg(mylistQuery.value("eid").toString()));
		LOG(QString("  aid: %1").arg(mylistQuery.value("aid").toString()));
		LOG(QString("  gid: %1").arg(mylistQuery.value("gid").toString()));
		LOG(QString("  date: %1").arg(mylistQuery.value("date").toString()));
		LOG(QString("  state: %1").arg(mylistQuery.value("state").toString()));
		LOG(QString("  viewed: %1").arg(mylistQuery.value("viewed").toString()));
		LOG(QString("  viewdate: %1").arg(mylistQuery.value("viewdate").toString()));
		LOG(QString("  storage: %1").arg(mylistQuery.value("storage").toString()));
		LOG(QString("  source: %1").arg(mylistQuery.value("source").toString()));
		LOG(QString("  other: %1").arg(mylistQuery.value("other").toString()));
		LOG(QString("  filestate: %1").arg(mylistQuery.value("filestate").toString()));
		LOG(QString("  local_file: %1").arg(mylistQuery.value("local_file").toString()));
		LOG(QString("  playback_position: %1").arg(mylistQuery.value("playback_position").toString()));
		LOG(QString("  playback_duration: %1").arg(mylistQuery.value("playback_duration").toString()));
		LOG(QString("  last_played: %1").arg(mylistQuery.value("last_played").toString()));
		
		int fid = mylistQuery.value("fid").toInt();
		int eid = mylistQuery.value("eid").toInt();
		int aid = mylistQuery.value("aid").toInt();
		int gid = mylistQuery.value("gid").toInt();
		int local_file_id = mylistQuery.value("local_file").toInt();
		
		// Query file table
		if(fid > 0)
		{
			LOG(QString("--- FILE TABLE (fid=%1) ---").arg(fid));
			QSqlQuery fileQuery(db);
			fileQuery.prepare("SELECT * FROM file WHERE fid = ?");
			fileQuery.bindValue(0, fid);
			if(fileQuery.exec() && fileQuery.next())
			{
				LOG(QString("  fid: %1").arg(fileQuery.value("fid").toString()));
				LOG(QString("  aid: %1").arg(fileQuery.value("aid").toString()));
				LOG(QString("  eid: %1").arg(fileQuery.value("eid").toString()));
				LOG(QString("  gid: %1").arg(fileQuery.value("gid").toString()));
				LOG(QString("  lid: %1").arg(fileQuery.value("lid").toString()));
				LOG(QString("  othereps: %1").arg(fileQuery.value("othereps").toString()));
				LOG(QString("  isdepr: %1").arg(fileQuery.value("isdepr").toString()));
				LOG(QString("  state: %1").arg(fileQuery.value("state").toString()));
				LOG(QString("  size: %1").arg(fileQuery.value("size").toString()));
				LOG(QString("  ed2k: %1").arg(fileQuery.value("ed2k").toString()));
				LOG(QString("  md5: %1").arg(fileQuery.value("md5").toString()));
				LOG(QString("  sha1: %1").arg(fileQuery.value("sha1").toString()));
				LOG(QString("  crc: %1").arg(fileQuery.value("crc").toString()));
				LOG(QString("  quality: %1").arg(fileQuery.value("quality").toString()));
				LOG(QString("  source: %1").arg(fileQuery.value("source").toString()));
				LOG(QString("  codec_audio: %1").arg(fileQuery.value("codec_audio").toString()));
				LOG(QString("  bitrate_audio: %1").arg(fileQuery.value("bitrate_audio").toString()));
				LOG(QString("  codec_video: %1").arg(fileQuery.value("codec_video").toString()));
				LOG(QString("  bitrate_video: %1").arg(fileQuery.value("bitrate_video").toString()));
				LOG(QString("  resolution: %1").arg(fileQuery.value("resolution").toString()));
				LOG(QString("  filetype: %1").arg(fileQuery.value("filetype").toString()));
				LOG(QString("  lang_dub: %1").arg(fileQuery.value("lang_dub").toString()));
				LOG(QString("  lang_sub: %1").arg(fileQuery.value("lang_sub").toString()));
				LOG(QString("  length: %1").arg(fileQuery.value("length").toString()));
				LOG(QString("  description: %1").arg(fileQuery.value("description").toString()));
				LOG(QString("  airdate: %1").arg(fileQuery.value("airdate").toString()));
				LOG(QString("  filename: %1").arg(fileQuery.value("filename").toString()));
			}
			else
			{
				LOG(QString("  No data found in file table for fid=%1").arg(fid));
			}
		}
		else
		{
			LOG("--- FILE TABLE: fid is 0 or NULL, skipping ---");
		}
		
		// Query anime table
		if(aid > 0)
		{
			LOG(QString("--- ANIME TABLE (aid=%1) ---").arg(aid));
			QSqlQuery animeQuery(db);
			animeQuery.prepare("SELECT * FROM anime WHERE aid = ?");
			animeQuery.bindValue(0, aid);
			if(animeQuery.exec() && animeQuery.next())
			{
				LOG(QString("  aid: %1").arg(animeQuery.value("aid").toString()));
				LOG(QString("  eptotal: %1").arg(animeQuery.value("eptotal").toString()));
				LOG(QString("  eps: %1").arg(animeQuery.value("eps").toString()));
				LOG(QString("  eplast: %1").arg(animeQuery.value("eplast").toString()));
				LOG(QString("  year: %1").arg(animeQuery.value("year").toString()));
				LOG(QString("  type: %1").arg(animeQuery.value("type").toString()));
				LOG(QString("  relaidlist: %1").arg(animeQuery.value("relaidlist").toString()));
				LOG(QString("  relaidtype: %1").arg(animeQuery.value("relaidtype").toString()));
				LOG(QString("  category: %1").arg(animeQuery.value("category").toString()));
				LOG(QString("  nameromaji: %1").arg(animeQuery.value("nameromaji").toString()));
				LOG(QString("  namekanji: %1").arg(animeQuery.value("namekanji").toString()));
				LOG(QString("  nameenglish: %1").arg(animeQuery.value("nameenglish").toString()));
				LOG(QString("  nameother: %1").arg(animeQuery.value("nameother").toString()));
				LOG(QString("  nameshort: %1").arg(animeQuery.value("nameshort").toString()));
				LOG(QString("  synonyms: %1").arg(animeQuery.value("synonyms").toString()));
				LOG(QString("  typename: %1").arg(animeQuery.value("typename").toString()));
				LOG(QString("  startdate: %1").arg(animeQuery.value("startdate").toString()));
				LOG(QString("  enddate: %1").arg(animeQuery.value("enddate").toString()));
			}
			else
			{
				LOG(QString("  No data found in anime table for aid=%1").arg(aid));
			}
			
			// Query anime_titles table
			LOG(QString("--- ANIME_TITLES TABLE (aid=%1) ---").arg(aid));
			QSqlQuery titlesQuery(db);
			titlesQuery.prepare("SELECT * FROM anime_titles WHERE aid = ? ORDER BY type, language");
			titlesQuery.bindValue(0, aid);
			if(titlesQuery.exec())
			{
				int titleCount = 0;
				while(titlesQuery.next())
				{
					titleCount++;
					LOG(QString("  Title #%1: type=%2, language=%3, title=%4")
						.arg(titleCount)
						.arg(titlesQuery.value("type").toString())
						.arg(titlesQuery.value("language").toString())
						.arg(titlesQuery.value("title").toString()));
				}
				if(titleCount == 0)
				{
					LOG(QString("  No titles found in anime_titles table for aid=%1").arg(aid));
				}
			}
		}
		else
		{
			LOG("--- ANIME TABLE: aid is 0 or NULL, skipping ---");
		}
		
		// Query episode table
		if(eid > 0)
		{
			LOG(QString("--- EPISODE TABLE (eid=%1) ---").arg(eid));
			QSqlQuery episodeQuery(db);
			episodeQuery.prepare("SELECT * FROM episode WHERE eid = ?");
			episodeQuery.bindValue(0, eid);
			if(episodeQuery.exec() && episodeQuery.next())
			{
				LOG(QString("  eid: %1").arg(episodeQuery.value("eid").toString()));
				LOG(QString("  name: %1").arg(episodeQuery.value("name").toString()));
				LOG(QString("  nameromaji: %1").arg(episodeQuery.value("nameromaji").toString()));
				LOG(QString("  namekanji: %1").arg(episodeQuery.value("namekanji").toString()));
				LOG(QString("  rating: %1").arg(episodeQuery.value("rating").toString()));
				LOG(QString("  votecount: %1").arg(episodeQuery.value("votecount").toString()));
				LOG(QString("  epno: %1").arg(episodeQuery.value("epno").toString()));
			}
			else
			{
				LOG(QString("  No data found in episode table for eid=%1").arg(eid));
			}
		}
		else
		{
			LOG("--- EPISODE TABLE: eid is 0 or NULL, skipping ---");
		}
		
		// Query group table
		if(gid > 0)
		{
			LOG(QString("--- GROUP TABLE (gid=%1) ---").arg(gid));
			QSqlQuery groupQuery(db);
			groupQuery.prepare("SELECT * FROM `group` WHERE gid = ?");
			groupQuery.bindValue(0, gid);
			if(groupQuery.exec() && groupQuery.next())
			{
				LOG(QString("  gid: %1").arg(groupQuery.value("gid").toString()));
				LOG(QString("  name: %1").arg(groupQuery.value("name").toString()));
				LOG(QString("  shortname: %1").arg(groupQuery.value("shortname").toString()));
			}
			else
			{
				LOG(QString("  No data found in group table for gid=%1").arg(gid));
			}
		}
		else
		{
			LOG("--- GROUP TABLE: gid is 0 or NULL, skipping ---");
		}
		
		// Query local_files table
		if(local_file_id > 0)
		{
			LOG(QString("--- LOCAL_FILES TABLE (id=%1) ---").arg(local_file_id));
			QSqlQuery localFileQuery(db);
			localFileQuery.prepare("SELECT * FROM local_files WHERE id = ?");
			localFileQuery.bindValue(0, local_file_id);
			if(localFileQuery.exec() && localFileQuery.next())
			{
				LOG(QString("  id: %1").arg(localFileQuery.value("id").toString()));
				LOG(QString("  path: %1").arg(localFileQuery.value("path").toString()));
				LOG(QString("  filename: %1").arg(localFileQuery.value("filename").toString()));
				LOG(QString("  status: %1").arg(localFileQuery.value("status").toString()));
				LOG(QString("  ed2k_hash: %1").arg(localFileQuery.value("ed2k_hash").toString()));
			}
			else
			{
				LOG(QString("  No data found in local_files table for id=%1").arg(local_file_id));
			}
		}
		else
		{
			LOG("--- LOCAL_FILES TABLE: local_file is 0 or NULL, skipping ---");
		}
	}
	else
	{
		LOG(QString("  No data found in mylist table for lid=%1").arg(lid));
	}
	
	LOG("=================================================================");
	LOG(QString("DEBUG: End of database information for LID: %1").arg(lid));
	LOG("=================================================================");
}

void Window::insertMissingEpisodePlaceholders(int aid, QTreeWidgetItem* animeItem, const QMap<QPair<int, int>, QTreeWidgetItem*>& episodeItems)
{
	// Collect all episode numbers that exist for this anime
	QSet<int> existingEpisodes;
	for(QMap<QPair<int, int>, QTreeWidgetItem*>::const_iterator it = episodeItems.constBegin(); 
		it != episodeItems.constEnd(); ++it)
	{
		if(it.key().first == aid) // Check if episode belongs to this anime
		{
			EpisodeTreeWidgetItem* episodeItem = static_cast<EpisodeTreeWidgetItem*>(it.value());
			if(episodeItem->getEpno().isValid() && episodeItem->getEpno().type() == 1)
			{
				existingEpisodes.insert(episodeItem->getEpno().number());
			}
		}
	}
	
	if(existingEpisodes.isEmpty())
		return; // No episodes to work with
	
	// Find min and max episode numbers
	int minEp = *std::min_element(existingEpisodes.begin(), existingEpisodes.end());
	int maxEp = *std::max_element(existingEpisodes.begin(), existingEpisodes.end());
	
	// Find gaps and create placeholder rows
	QList<int> sortedEpisodes = existingEpisodes.values();
	std::sort(sortedEpisodes.begin(), sortedEpisodes.end());
	
	for(int expectedEp = minEp; expectedEp <= maxEp; expectedEp++)
	{
		if(!existingEpisodes.contains(expectedEp))
		{
			// Found a missing episode, create a range
			int rangeStart = expectedEp;
			int rangeEnd = expectedEp;
			
			// Extend the range to include consecutive missing episodes
			while(rangeEnd + 1 <= maxEp && !existingEpisodes.contains(rangeEnd + 1))
			{
				rangeEnd++;
			}
			
			// Create placeholder item
			QTreeWidgetItem* placeholderItem = new QTreeWidgetItem(animeItem);
			placeholderItem->setText(0, "");
			
			// Column 2: Episode number(s) in red
			QString epRange;
			if(rangeStart == rangeEnd)
			{
				epRange = QString::number(rangeStart);
			}
			else
			{
				epRange = QString("%1-%2").arg(rangeStart).arg(rangeEnd);
			}
			placeholderItem->setText(COL_EPISODE, epRange);
			placeholderItem->setForeground(COL_EPISODE, QBrush(QColor(Qt::red)));
			
			// Column 3: Episode title in red
			placeholderItem->setText(COL_EPISODE_TITLE, "Missing");
			placeholderItem->setForeground(COL_EPISODE_TITLE, QBrush(QColor(Qt::red)));
			
			// Set data for sorting - use negative value to sort before real episodes
			placeholderItem->setData(0, Qt::UserRole, -rangeStart);
			
			// Skip to the end of the range
			expectedEp = rangeEnd;
		}
	}
}

void Window::Button1Click() // add files
{
    QStringList files = QFileDialog::getOpenFileNames(0, 0, adbapi->getLastDirectory());
	QColor colorgray;
	colorgray.setRgb(230, 230, 230);
    if(!files.isEmpty())
    {
        adbapi->setLastDirectory(QFileInfo(files.first()).filePath());
        while(!files.isEmpty())
        {
//            QFileInfo file = files.first();
            QFileInfo file = QFileInfo(files.first());
            files.pop_front();
            hashesinsertrow(file, renameto->checkState());
    //		delete item1, item2, item3;
        }
    }
}

void Window::Button2Click() // add directories
{
	hashes->setUpdatesEnabled(0);
    QStringList files;
    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::Directory);
    QListView *l = dialog.findChild<QListView*>("listView");
    if(l)
    {
		l->setSelectionMode(QAbstractItemView::MultiSelection);
    }
    QTreeView *t = dialog.findChild<QTreeView*>();
    if(t)
    {
		t->setSelectionMode(QAbstractItemView::MultiSelection);
    }

	dialog.setDirectory(adbapi->getLastDirectory());

    if(dialog.exec())
    {
		files = dialog.selectedFiles();
		if(files.count() == 1)
		{
			adbapi->setLastDirectory(files.last());
		}
		while(!files.isEmpty())
		{
			QDirIterator directory_walker(files.first(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
			files.pop_front();
			while(directory_walker.hasNext())
            {
                QFileInfo file = QFileInfo(directory_walker.next());
				hashesinsertrow(file, renameto->checkState());
//				adbapi.ed2khash(file.absoluteFilePath());
		    }
		}
    }
    hashes->setUpdatesEnabled(1);
}

void Window::Button3Click()
{
	hashes->setUpdatesEnabled(0);
	QStringList files;

	files.append(adbapi->getLastDirectory());
    if(!files.last().isEmpty())
    {
        while(!files.isEmpty())
        {
            QDirIterator directory_walker(files.first(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            files.pop_front();
            while(directory_walker.hasNext())
            {
                QFileInfo file = QFileInfo(directory_walker.next());
                hashesinsertrow(file, renameto->checkState());
            }
        }
    }
	hashes->setUpdatesEnabled(1);
}

int Window::calculateTotalHashParts(const QStringList &files)
{
	QFileInfo file;
	int totalparts = 0;
	for(const QString &filepath : files)
	{
		file.setFile(filepath);
		double a = file.size();
		double b = a/102400;
		double c = ceil(b);
		totalparts += c;
	}
	return totalparts;
}

void Window::setupHashingProgress(const QStringList &files)
{
	totalHashParts = calculateTotalHashParts(files);
	completedHashParts = 0;
	lastThreadProgress.clear(); // Reset per-thread progress tracking
	progressHistory.clear(); // Reset progress history for ETA calculation
	progressTotal->setValue(0);
	progressTotal->setMaximum(totalHashParts > 0 ? totalHashParts : 1);
	progressTotal->setFormat("ETA: calculating...");
	progressTotalLabel->setText("0%");
	hashingTimer.start();
	lastEtaUpdate.start();
}

QStringList Window::getFilesNeedingHash()
{
	QStringList filesToHash;
	for(int i=0; i<hashes->rowCount(); i++)
	{
		QString progress = hashes->item(i, 1)->text();
		QString existingHash = hashes->item(i, 9)->text();
		if(progress == "0" && existingHash.isEmpty())
		{
			filesToHash.append(hashes->item(i, 2)->text());
		}
	}
	return filesToHash;
}

void Window::ButtonHasherStartClick()
{
	QList<int> rowsWithHashes; // Rows that already have hashes
	int filesToHashCount = 0;
	
	for(int i=0; i<hashes->rowCount(); i++)
	{
		QString progress = hashes->item(i, 1)->text();
		QString existingHash = hashes->item(i, 9)->text(); // Check hash column
		
		// Process files with progress="0" or progress="1" (already hashed but not yet API-processed)
		if(progress == "0" || progress == "1")
		{
			// Check if file has pending API calls (tags in columns 5 or 6)
			// Tags are "?" initially, set to actual tag when API call is queued, and "0" when completed/not needed
			QString fileTag = hashes->item(i, 5)->text();
			QString mylistTag = hashes->item(i, 6)->text();
			bool hasPendingAPICalls = (!fileTag.isEmpty() && fileTag != "?" && fileTag != "0") || 
			                          (!mylistTag.isEmpty() && mylistTag != "?" && mylistTag != "0");
			
			if (!existingHash.isEmpty())
			{
				// Skip files with pending API calls to avoid duplicate processing
				if (!hasPendingAPICalls)
				{
					// File already has a hash - queue for deferred processing
					rowsWithHashes.append(i);
				}
			}
			else
			{
				// File needs to be hashed
				// Note: If progress="1" but no hash, this is an inconsistent state
				if (progress == "1")
				{
					LOG(QString("Warning: File at row %1 has progress=1 but no hash - inconsistent state").arg(i));
				}
				filesToHashCount++;
			}
		}
	}
	
	// Queue files with existing hashes for deferred processing to prevent UI freeze
	for (int rowIndex : rowsWithHashes)
	{
		QString filename = hashes->item(rowIndex, 0)->text();
		QString filePath = hashes->item(rowIndex, 2)->text();
		QString hexdigest = hashes->item(rowIndex, 9)->text();
		
		LOG(QString("Queueing already-hashed file for processing: %1").arg(filename));
		
		// Get file size
		QFileInfo fileInfo(filePath);
		qint64 fileSize = fileInfo.size();
		
		// Queue for deferred processing
		HashedFileInfo info;
		info.rowIndex = rowIndex;
		info.filePath = filePath;
		info.filename = filename;
		info.hexdigest = hexdigest;
		info.fileSize = fileSize;
		info.useUserSettings = true;
		info.addToMylist = (addtomylist->checkState() > 0);
		info.markWatchedState = markwatched->checkState();
		info.fileState = hasherFileState->currentIndex();
		pendingHashedFilesQueue.append(info);
	}
	
	// Start timer to process queued files in batches (keeps UI responsive)
	if (!rowsWithHashes.isEmpty())
	{
		LOG(QString("Queued %1 already-hashed file(s) for deferred processing").arg(rowsWithHashes.size()));
		hashedFilesProcessingTimer->start();
	}
	
	// Start hashing for files without existing hashes
	if (filesToHashCount > 0)
	{
		// Calculate total hash parts for progress tracking
		QStringList filesToHash = getFilesNeedingHash();
		setupHashingProgress(filesToHash);
		
		buttonstart->setEnabled(0);
		buttonclear->setEnabled(0);
		hasherThreadPool->start();
	}
	else if (rowsWithHashes.isEmpty())
	{
		// No files to process at all
		LOG("No files to process");
	}
	else
	{
		// Only had pre-hashed files, queued for processing
		LOG(QString("Queued %1 already-hashed file(s) for processing").arg(rowsWithHashes.size()));
	}
}

void Window::ButtonHasherStopClick()
{
	buttonstart->setEnabled(1);
	buttonclear->setEnabled(1);
	progressTotal->setValue(0);
	progressTotal->setMaximum(1);
	progressTotal->setFormat("");
	progressTotalLabel->setText("");
	
	// Reset all thread progress bars
	for (QProgressBar* const bar : std::as_const(threadProgressBars)) {
		bar->setValue(0);
		bar->setMaximum(1);
	}
	
	// Reset progress of files that were assigned but not completed
	// Files with progress "0.1" were assigned to threads but stopped before completion
	// Reset them to "0" so they can be picked up again on next start
	for (int i = 0; i < hashes->rowCount(); i++)
	{
		QString progress = hashes->item(i, 1)->text();
		if (progress == "0.1")
		{
			hashes->item(i, 1)->setText("0");
		}
	}
	
	// Notify all worker threads to stop hashing
	// 1. First, notify ed2k instances in all worker threads to interrupt current hashing
	//    This sets a flag that ed2khash checks, causing it to return early
	hasherThreadPool->broadcastStopHasher();
	emit notifyStopHasher(); // Also notify main adbapi for consistency
	
	// 2. Then signal the thread pool to stop processing more files
	hasherThreadPool->stop();
	
	// 3. Don't wait here - let threads finish asynchronously to prevent UI freeze
	//    The hasherFinished() slot will be called automatically when all threads complete
}

void Window::provideNextFileToHash()
{
	// Thread-safe file assignment: only one thread can request a file at a time
	QMutexLocker locker(&fileRequestMutex);
	
	// Look through the hashes widget for the next file that needs hashing (progress="0" and no hash)
	for(int i=0; i<hashes->rowCount(); i++)
	{
		QString progress = hashes->item(i, 1)->text();
		QString existingHash = hashes->item(i, 9)->text();
		
		if(progress == "0" && existingHash.isEmpty())
		{
			QString filePath = hashes->item(i, 2)->text();
			
			// Immediately mark this file as assigned to prevent other threads from picking it up
			QTableWidgetItem *itemProgressAssigned = new QTableWidgetItem(QString("0.1"));
			hashes->setItem(i, 1, itemProgressAssigned);
			
			hasherThreadPool->addFile(filePath);
			return;
		}
	}
	
	// No more files to hash, send empty string to signal completion
	hasherThreadPool->addFile(QString());
}

void Window::ButtonHasherClearClick()
{
	hashes->setUpdatesEnabled(0);
	while(hashes->rowCount() > 0)
	{
		hashes->removeRow(0);
	}
	hashes->setUpdatesEnabled(1);
}

void Window::ButtonLoginClick()
{
    bool loggedin = adbapi->LoggedIn();
    QString logMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " loggedin=" + (loggedin ? "true" : "false");
    LOG(logMsg);
    if(loggedin == true)
    {
        adbapi->Logout();
    }
    else
    {
        adbapi->Auth();
    }
}

void Window::markwatchedStateChanged(int state)
{
	switch(state)
	{
	case Qt::Unchecked:
		markwatched->setText("Mark watched (no change)");
		break;
	case Qt::PartiallyChecked:
		markwatched->setText("Mark watched (unwatched)");
		break;
	case Qt::Checked:
		markwatched->setText("Mark watched (watched)");
		break;
	default:
		break;
	}
}

void Window::getNotifyPartsDone(int threadId, int total, int done)
{
	// Calculate the delta from last update for this thread
	int lastProgress = lastThreadProgress.value(threadId, 0);
	int delta = done - lastProgress;
	lastThreadProgress[threadId] = done;
	
	// Update completed parts by the actual delta (not just +1)
	completedHashParts += delta;
	
	// Update the specific thread's progress bar
	if (threadId >= 0 && threadId < threadProgressBars.size()) {
		threadProgressBars[threadId]->setMaximum(total);
		threadProgressBars[threadId]->setValue(done);
	}
	
	progressTotal->setValue(completedHashParts);
	
	// Update percentage label
	int percentage = totalHashParts > 0 ? (completedHashParts * 100 / totalHashParts) : 0;
	progressTotalLabel->setText(QString("%1%").arg(percentage));
	
	// Calculate and display ETA - throttled to once per second to prevent UI freeze
	// Note: Progress updates from hasher are throttled by HASHER_PROGRESS_UPDATE_INTERVAL,
	// but we track actual delta in completedHashParts, so rate calculation is accurate
	if (completedHashParts > 0 && totalHashParts > 0 && lastEtaUpdate.elapsed() >= 1000) {
		qint64 currentTime = hashingTimer.elapsed();
		
		// Add current progress snapshot to history
		ProgressSnapshot snapshot;
		snapshot.timestamp = currentTime;
		snapshot.completedParts = completedHashParts;
		progressHistory.append(snapshot);
		
		// Keep only snapshots from the last 30 seconds for moving average
		const qint64 WINDOW_SIZE_MS = 30000;
		while (!progressHistory.isEmpty() && (currentTime - progressHistory.first().timestamp) > WINDOW_SIZE_MS) {
			progressHistory.removeFirst();
		}
		
		// Calculate rate based on recent progress (moving average)
		double partsPerMs = 0.0;
		if (progressHistory.size() >= 2) {
			// Use the earliest and latest snapshots in the window
			const ProgressSnapshot &oldest = progressHistory.first();
			const ProgressSnapshot &newest = progressHistory.last();
			qint64 timeWindow = newest.timestamp - oldest.timestamp;
			int partsInWindow = newest.completedParts - oldest.completedParts;
			
			if (timeWindow > 0 && partsInWindow > 0) {
				partsPerMs = static_cast<double>(partsInWindow) / timeWindow;
			}
		}
		
		// If we don't have enough history yet, fall back to average from start
		if (partsPerMs == 0.0 && currentTime > 0) {
			partsPerMs = static_cast<double>(completedHashParts) / currentTime;
		}
		
		int remainingParts = totalHashParts - completedHashParts;
		
		if (remainingParts > 0 && partsPerMs > 0) {
			qint64 etaMs = static_cast<qint64>(remainingParts / partsPerMs);
			int etaSec = etaMs / 1000;
			int etaMin = etaSec / 60;
			int etaHour = etaMin / 60;
			
			QString etaStr;
			if (etaHour > 0) {
				etaStr = QString("%1h %2m").arg(etaHour).arg(etaMin % 60);
			} else if (etaMin > 0) {
				etaStr = QString("%1m %2s").arg(etaMin).arg(etaSec % 60);
			} else {
				etaStr = QString("%1s").arg(etaSec);
			}
			
			progressTotal->setFormat(QString("ETA: %1").arg(etaStr));
		} else {
			progressTotal->setFormat("ETA: calculating...");
		}
		lastEtaUpdate.restart();
	}
}

void Window::getNotifyFileHashed(int threadId, ed2k::ed2kfilestruct data)
{
	for(int i=0; i<hashes->rowCount(); i++)
	{
		// Match by filename AND verify it's the file being hashed (progress starts with "0" - either "0" or "0.1" for assigned)
		// This prevents matching wrong file when there are multiple files with the same name
		QString progress = hashes->item(i, 1)->text();
		if(hashes->item(i, 0)->text() == data.filename && progress.startsWith("0"))
		{
			// Additional check: verify file size matches to ensure we have the right file
			QString filePath = hashes->item(i, 2)->text();
			QFileInfo fileInfo(filePath);
			if(!fileInfo.exists() || fileInfo.size() != data.size)
			{
				// Size mismatch or file doesn't exist - this is not the right file, continue searching
				continue;
			}
            // Mark as hashed in UI (use pre-allocated color object)
            hashes->item(i, 0)->setBackground(m_hashedFileColor);
			QTableWidgetItem *itemprogress = new QTableWidgetItem(QString("1"));
		    hashes->setItem(i, 1, itemprogress);
		    getNotifyLogAppend(QString("File hashed: %1").arg(data.filename));
			
			// Update the hash column (column 9) in the UI to reflect the newly computed hash
			if (QTableWidgetItem* hashItem = hashes->item(i, 9))
			{
				hashItem->setText(data.hexdigest);
			}
			else
			{
				// Column 9 doesn't exist, create it
				QTableWidgetItem* newHashItem = new QTableWidgetItem(data.hexdigest);
				hashes->setItem(i, 9, newHashItem);
			}
			
			// Process file identification immediately instead of batching
			if (addtomylist->checkState() > 0)
			{
				// Update hash in database with status=1 immediately
				adbapi->updateLocalFileHash(filePath, data.hexdigest, 1);
				
				// Perform LocalIdentify immediately
				// Note: This is done per-file instead of batching to provide immediate feedback
				// LocalIdentify is a fast indexed database query, so the performance difference
				// is negligible compared to the user experience improvement of seeing results
				// immediately after each file is hashed rather than waiting for the entire batch
				std::bitset<2> li = adbapi->LocalIdentify(data.size, data.hexdigest);
				
				// Update UI with LocalIdentify results
				hashes->item(i, 3)->setText(QString((li[AniDBApi::LI_FILE_IN_DB])?"1":"0")); // File in database
				
				QString tag;
				if(li[AniDBApi::LI_FILE_IN_DB] == 0)
				{
					tag = adbapi->File(data.size, data.hexdigest);
					hashes->item(i, 5)->setText(tag);
					// File info not in local DB yet - File() API call queued to fetch from AniDB
				}
				else
				{
					hashes->item(i, 5)->setText("0");
					// File is in local DB (previously fetched from AniDB)
					// Update status to 2 (in anidb) to prevent re-detection
					adbapi->UpdateLocalFileStatus(filePath, 2);
				}

				hashes->item(i, 4)->setText(QString((li[AniDBApi::LI_FILE_IN_MYLIST])?"1":"0")); // File in mylist
				if(li[AniDBApi::LI_FILE_IN_MYLIST] == 0)
				{
					tag = adbapi->MylistAdd(data.size, data.hexdigest, markwatched->checkState(), hasherFileState->currentIndex(), storage->text());
					hashes->item(i, 6)->setText(tag);
					// Status will be updated when MylistAdd completes (via UpdateLocalPath)
				}
				else
				{
					hashes->item(i, 6)->setText("0");
					// File already in mylist - link the local_file and update status
					adbapi->LinkLocalFileToMylist(data.size, data.hexdigest, filePath);
				}
			}
			else
			{
				// Not adding to mylist - just accumulate hash for batch database update later
				pendingHashUpdates.append(qMakePair(filePath, data.hexdigest));
			}
			
			break; // Found the file, no need to continue
		}
	}
}

void Window::safeClose()
{
    this->close();
}

void Window::startupInitialization()
{
    // This slot is called 1 second after the window is constructed
    // to allow the UI to be fully initialized before loading data
    
    // DEBUG: Print database info for specific lid values as requested in issue
    LOG("DEBUG: Printing database information for requested lid values...");
//    debugPrintDatabaseInfoForLid(424374769);
//    debugPrintDatabaseInfoForLid(424184693);
    LOG("DEBUG: Finished printing database information for requested lid values");
    
    mylistStatusLabel->setText("MyList Status: Loading in background...");
    
    // Start background loading - this will offload heavy operations to worker threads
    startBackgroundLoading();
    
    // Note: The rest of the initialization (sorting, unbound files) will be done
    // in the completion handlers: onMylistLoadingFinished(), onAnimeTitlesLoadingFinished(), etc.
}

void Window::loadUnboundFiles()
{
    LOG("Loading unbound files from database...");
    
    // Get unbound files from database
    QList<AniDBApi::FileHashInfo> unboundFiles = adbapi->getUnboundFiles();
    
    if(unboundFiles.isEmpty())
    {
        LOG("No unbound files found");
        return;
    }
    
    LOG(QString("Found %1 unbound files, adding to unknown files widget").arg(unboundFiles.size()));
    
    // Load anime titles cache once before processing files
    loadAnimeTitlesCache();
    
    // Disable updates during bulk insertion for performance
    unknownFiles->setUpdatesEnabled(false);
    
    // Add each unbound file to the unknown files widget
    for(const AniDBApi::FileHashInfo& fileInfo : std::as_const(unboundFiles))
    {
        QFileInfo qFileInfo(fileInfo.path);
        QString filename = qFileInfo.fileName();
        
        // Get file size from filesystem if available
        qint64 fileSize = 0;
        if(qFileInfo.exists())
        {
            fileSize = qFileInfo.size();
        }
        
        unknownFilesInsertRow(filename, fileInfo.path, fileInfo.hash, fileSize);
    }
    
    // Re-enable updates after bulk insertion
    unknownFiles->setUpdatesEnabled(true);
    
    LOG(QString("Successfully loaded %1 unbound files").arg(unboundFiles.size()));
}

void Window::loadAnimeTitlesCache()
{
    if(animeTitlesCacheLoaded)
    {
        return; // Already loaded
    }
    
    LOG("Loading anime titles cache for unknown files widget...");
    
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    query.exec("SELECT DISTINCT aid, title FROM anime_titles ORDER BY title");
    
    cachedAnimeTitles.clear();
    cachedTitleToAid.clear();
    
    while(query.next())
    {
        int aid = query.value(0).toInt();
        QString title = query.value(1).toString();
        QString displayText = QString("%1: %2").arg(aid).arg(title);
        cachedAnimeTitles << displayText;
        cachedTitleToAid[displayText] = aid;
    }
    
    animeTitlesCacheLoaded = true;
    LOG(QString("Loaded %1 anime titles into cache").arg(cachedAnimeTitles.size()));
}

// Start background loading for all heavy operations
void Window::startBackgroundLoading()
{
    LOG("Starting background loading of mylist data, anime titles, and unbound files...");
    
    // Capture database name in main thread before launching background threads
    QString dbName;
    {
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isValid() && db.isOpen()) {
            dbName = db.databaseName();
        } else {
            LOG("Error: Main database is not open");
            return;
        }
    }
    
    // Load mylist anime IDs in background thread
    // The database query is done in background, but card creation happens in UI thread
    QFuture<QList<int>> mylistFuture = QtConcurrent::run([dbName]() -> QList<int> {
        // This runs in a background thread
        LOG("Background thread: Loading mylist anime IDs...");
        
        QList<int> aids;
        
        {
            // Create separate database connection for this thread
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "MylistThread");
            db.setDatabaseName(dbName);
            
            if (!db.open()) {
                LOG("Background thread: Failed to open database for mylist");
                QSqlDatabase::removeDatabase("MylistThread");
                return QList<int>();
            }
            
            // Query all anime IDs in mylist (same query as MyListCardManager but only get AIDs)
            QString query = "SELECT DISTINCT m.aid FROM mylist m ORDER BY "
                           "(SELECT nameromaji FROM anime WHERE aid = m.aid)";
            
            QSqlQuery q(db);
            
            if (q.exec(query)) {
                while (q.next()) {
                    aids.append(q.value(0).toInt());
                }
            } else {
                LOG(QString("Background thread: Error loading mylist: %1").arg(q.lastError().text()));
            }
            
            db.close();
            // Query object q is destroyed here when leaving scope
        }
        
        // Now safe to remove database connection
        QSqlDatabase::removeDatabase("MylistThread");
        
        LOG(QString("Background thread: Loaded %1 mylist anime IDs").arg(aids.size()));
        
        return aids;
    });
    mylistLoadingWatcher->setFuture(mylistFuture);
    
    // Start anime titles cache loading in background thread
    QFuture<void> titlesFuture = QtConcurrent::run([this, dbName]() {
        // This runs in a background thread
        // Load anime titles from database into temporary storage
        if (animeTitlesCacheLoaded) {
            return; // Already loaded
        }
        
        LOG("Background thread: Loading anime titles cache...");
        
        QStringList tempTitles;
        QMap<QString, int> tempTitleToAid;
        
        {
            // We need to use a separate database connection for this thread
            // Qt SQL requires each thread to have its own connection
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "AnimeTitlesThread");
            db.setDatabaseName(dbName);
            
            if (!db.open()) {
                LOG("Background thread: Failed to open database for anime titles");
                QSqlDatabase::removeDatabase("AnimeTitlesThread");
                return;
            }
            
            QSqlQuery query(db);
            query.exec("SELECT DISTINCT aid, title FROM anime_titles ORDER BY title");
            
            while (query.next()) {
                int aid = query.value(0).toInt();
                QString title = query.value(1).toString();
                QString displayText = QString("%1: %2").arg(aid).arg(title);
                tempTitles << displayText;
                tempTitleToAid[displayText] = aid;
            }
            
            db.close();
            // Query object is destroyed here when leaving scope
        }
        
        // Now safe to remove database connection
        QSqlDatabase::removeDatabase("AnimeTitlesThread");
        
        LOG(QString("Background thread: Loaded %1 anime titles").arg(tempTitles.size()));
        
        // Store the data in member variables with mutex protection
        QMutexLocker locker(&backgroundLoadingMutex);
        cachedAnimeTitles = tempTitles;
        cachedTitleToAid = tempTitleToAid;
    });
    animeTitlesLoadingWatcher->setFuture(titlesFuture);
    
    // Start unbound files loading in background thread
    QFuture<void> unboundFuture = QtConcurrent::run([this, dbName]() {
        // This runs in a background thread
        LOG("Background thread: Loading unbound files...");
        
        QList<UnboundFileData> tempUnboundFiles;
        
        {
            // Create separate database connection for this thread
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "UnboundFilesThread");
            db.setDatabaseName(dbName);
            
            if (!db.open()) {
                LOG("Background thread: Failed to open database for unbound files");
                QSqlDatabase::removeDatabase("UnboundFilesThread");
                return;
            }
            
            // Query unbound files using same criteria as AniDBApi::getUnboundFiles
            // binding_status = 0 (not_bound) AND status = 3 (not in anidb)
            QSqlQuery query(db);
            query.prepare("SELECT `path`, `filename`, `ed2k_hash` FROM `local_files` WHERE `binding_status` = 0 AND `status` = 3 AND `ed2k_hash` IS NOT NULL AND `ed2k_hash` != ''");
            
            if (!query.exec()) {
                LOG(QString("Background thread: Failed to query unbound files: %1").arg(query.lastError().text()));
            } else {
                while (query.next()) {
                    UnboundFileData fileData;
                    fileData.filepath = query.value(0).toString();
                    fileData.filename = query.value(1).toString();
                    if (fileData.filename.isEmpty()) {
                        QFileInfo qFileInfo(fileData.filepath);
                        fileData.filename = qFileInfo.fileName();
                    }
                    fileData.hash = query.value(2).toString();
                    
                    // Get file size if file exists
                    QFileInfo qFileInfo(fileData.filepath);
                    fileData.size = 0;
                    if (qFileInfo.exists()) {
                        fileData.size = qFileInfo.size();
                    }
                    
                    tempUnboundFiles.append(fileData);
                }
            }
            
            db.close();
            // Query object is destroyed here when leaving scope
        }
        
        // Now safe to remove database connection
        QSqlDatabase::removeDatabase("UnboundFilesThread");
        
        LOG(QString("Background thread: Loaded %1 unbound files").arg(tempUnboundFiles.size()));
        
        // Store the data with mutex protection
        QMutexLocker locker(&backgroundLoadingMutex);
        loadedUnboundFiles = tempUnboundFiles;
    });
    unboundFilesLoadingWatcher->setFuture(unboundFuture);
}

// Called when mylist loading finishes (in UI thread)
void Window::onMylistLoadingFinished()
{
    // Get the loaded AIDs from the future
    QList<int> aids = mylistLoadingWatcher->result();
    
    LOG(QString("Background loading: Mylist query complete with %1 anime, starting progressive card creation...").arg(aids.size()));
    
    // Store AIDs for progressive loading
    pendingCardsToLoad = aids;
    
    // Clear existing cards
    cardManager->clearAllCards();
    
    // Start progressive loading timer
    if (!pendingCardsToLoad.isEmpty()) {
        progressiveCardLoadingTimer->start();
        mylistStatusLabel->setText(QString("MyList Status: Loading %1 anime...").arg(pendingCardsToLoad.size()));
    } else {
        mylistStatusLabel->setText("MyList Status: No anime in mylist");
    }
}

// Load cards progressively in small batches to keep UI responsive
void Window::loadNextCardBatch()
{
    if (pendingCardsToLoad.isEmpty()) {
        // All cards loaded
        progressiveCardLoadingTimer->stop();
        
        // Get all cards for backward compatibility
        animeCards = cardManager->getAllCards();
        
        // Apply sorting
        restoreMylistSorting();
        sortMylistCards(mylistSortComboBox->currentIndex());
        
        mylistStatusLabel->setText(QString("MyList Status: Loaded %1 anime").arg(animeCards.size()));
        LOG(QString("[Progressive Loading] All cards loaded: %1 anime").arg(animeCards.size()));
        return;
    }
    
    // Load next batch of cards
    int cardsToLoad = qMin(CARD_LOADING_BATCH_SIZE, pendingCardsToLoad.size());
    
    for (int i = 0; i < cardsToLoad; ++i) {
        int aid = pendingCardsToLoad.takeFirst();
        
        // Create card using the card manager
        // This calls createCard() which does the database query for this specific anime
        AnimeCard *card = cardManager->getCard(aid);
        if (!card) {
            // Card doesn't exist, create it
            // We need to trigger card creation through updateOrAddMylistEntry
            // but we need a lid. Since we only have aid, let's query for one lid
            QSqlDatabase db = QSqlDatabase::database();
            if (db.isOpen()) {
                QSqlQuery q(db);
                q.prepare("SELECT lid FROM mylist WHERE aid = ? LIMIT 1");
                q.addBindValue(aid);
                if (q.exec() && q.next()) {
                    int lid = q.value(0).toInt();
                    cardManager->updateOrAddMylistEntry(lid);
                }
            }
        }
    }
    
    // Update status
    int remaining = pendingCardsToLoad.size();
    int loaded = animeCards.size();
    mylistStatusLabel->setText(QString("MyList Status: Loading... %1 of %2").arg(loaded).arg(loaded + remaining));
}

// Called when anime titles loading finishes (in UI thread)
void Window::onAnimeTitlesLoadingFinished()
{
    QMutexLocker locker(&backgroundLoadingMutex);
    LOG("Background loading: Anime titles cache loaded successfully");
    animeTitlesCacheLoaded = true;
    
    // The data is already loaded in cachedAnimeTitles and cachedTitleToAid
    // by the background thread, so we just need to mark it as loaded
}

// Called when unbound files loading finishes (in UI thread)
void Window::onUnboundFilesLoadingFinished()
{
    QMutexLocker locker(&backgroundLoadingMutex);
    
    LOG(QString("Background loading: Unbound files loaded, adding %1 files to UI...").arg(loadedUnboundFiles.size()));
    
    if (loadedUnboundFiles.isEmpty()) {
        LOG("No unbound files found");
        return;
    }
    
    // Make a copy to release the mutex quickly
    QList<UnboundFileData> filesToAdd = loadedUnboundFiles;
    loadedUnboundFiles.clear();
    locker.unlock();
    
    // Disable updates during bulk insertion for performance
    unknownFiles->setUpdatesEnabled(false);
    
    // Add each unbound file to the unknown files widget
    for (const UnboundFileData& fileData : std::as_const(filesToAdd)) {
        unknownFilesInsertRow(fileData.filename, fileData.filepath, fileData.hash, fileData.size);
    }
    
    // Re-enable updates after bulk insertion
    unknownFiles->setUpdatesEnabled(true);
    
    LOG(QString("Successfully added %1 unbound files to UI").arg(filesToAdd.size()));
}

void Window::saveMylistSorting()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    int sortColumn = mylistTreeWidget->sortColumn();
    Qt::SortOrder sortOrder = mylistTreeWidget->header()->sortIndicatorOrder();
    
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_sort_column', ?)");
    q.addBindValue(sortColumn);
    q.exec();
    
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_sort_order', ?)");
    q.addBindValue(static_cast<int>(sortOrder));
    q.exec();
    
    LOG(QString("Saved mylist sorting: column=%1, order=%2").arg(sortColumn).arg(sortOrder));
}

void Window::restoreMylistSorting()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        // Use default sorting if database is not available
        mylistTreeWidget->sortByColumn(COL_ANIME, Qt::AscendingOrder);
        return;
    }
    
    // Try to load saved sorting preferences
    QSqlQuery q(db);
    q.prepare("SELECT value FROM settings WHERE name = 'mylist_sort_column'");
    
    int sortColumn = COL_ANIME; // Default to Anime column
    Qt::SortOrder sortOrder = Qt::AscendingOrder; // Default to ascending
    
    if (q.exec() && q.next()) {
        sortColumn = q.value(0).toInt();
    }
    
    q.prepare("SELECT value FROM settings WHERE name = 'mylist_sort_order'");
    if (q.exec() && q.next()) {
        sortOrder = static_cast<Qt::SortOrder>(q.value(0).toInt());
    }
    
    // Apply the sorting
    mylistTreeWidget->sortByColumn(sortColumn, sortOrder);
    
    LOG(QString("Restored mylist sorting: column=%1, order=%2").arg(sortColumn).arg(sortOrder));
}

void Window::onMylistSortChanged(int column, Qt::SortOrder order)
{
    Q_UNUSED(column);
    Q_UNUSED(order);
    // Save sorting preferences when user changes sorting
    saveMylistSorting();
}


void Window::hasherFinished()
{
	// Batch update all accumulated hashes to database for files not already updated
	// (files where addtomylist was unchecked are updated here)
	if (!pendingHashUpdates.isEmpty())
	{
		adbapi->batchUpdateLocalFileHashes(pendingHashUpdates, 1);
		pendingHashUpdates.clear();
	}
	
	// Note: File identification now happens immediately in getNotifyFileHashed()
	// This ensures UI remains responsive and files are identified as soon as they are hashed
	
	buttonstart->setEnabled(1);
	buttonclear->setEnabled(1);
	progressTotal->setFormat("");
	progressTotalLabel->setText("");
}

void Window::processPendingHashedFiles()
{
	// Process files in small batches to keep UI responsive
	int processed = 0;
	
	while (!pendingHashedFilesQueue.isEmpty() && processed < HASHED_FILES_BATCH_SIZE) {
		HashedFileInfo info = pendingHashedFilesQueue.takeFirst();
		processed++;
		
		// Mark as hashed in UI (use pre-allocated color object)
		hashes->item(info.rowIndex, 0)->setBackground(m_hashedFileColor);
		hashes->item(info.rowIndex, 1)->setText("1");
		
		// Update hash in database with status=1
		adbapi->updateLocalFileHash(info.filePath, info.hexdigest, 1);
		
		// If adding to mylist and logged in, perform API calls
		if (info.addToMylist && adbapi->LoggedIn()) {
			// Perform LocalIdentify
			std::bitset<2> li = adbapi->LocalIdentify(info.fileSize, info.hexdigest);
			
			// Update UI with LocalIdentify results
			hashes->item(info.rowIndex, 3)->setText(QString((li[AniDBApi::LI_FILE_IN_DB])?"1":"0"));
			
			QString tag;
			if(li[AniDBApi::LI_FILE_IN_DB] == 0) {
				tag = adbapi->File(info.fileSize, info.hexdigest);
				hashes->item(info.rowIndex, 5)->setText(tag);
				// File info not in local DB yet - File() API call queued to fetch from AniDB
				// Status will be updated to 2 when subsequent MylistAdd completes (via UpdateLocalPath)
			} else {
				hashes->item(info.rowIndex, 5)->setText("0");
				// File is in local DB (previously fetched from AniDB)
				// Update status to 2 (in anidb) to prevent re-detection
				adbapi->UpdateLocalFileStatus(info.filePath, 2);
			}

			hashes->item(info.rowIndex, 4)->setText(QString((li[AniDBApi::LI_FILE_IN_MYLIST])?"1":"0"));
			if(li[AniDBApi::LI_FILE_IN_MYLIST] == 0) {
				// Use settings from the info struct
				int markWatched = info.useUserSettings ? info.markWatchedState : Qt::Unchecked;
				int fileState = info.useUserSettings ? info.fileState : 1;
				tag = adbapi->MylistAdd(info.fileSize, info.hexdigest, markWatched, fileState, storage->text());
				hashes->item(info.rowIndex, 6)->setText(tag);
				// Status will be updated when MylistAdd completes (via UpdateLocalPath)
			} else {
				hashes->item(info.rowIndex, 6)->setText("0");
				// File already in mylist - no API call needed
				// Update status to 2 (in anidb) to prevent re-detection
				adbapi->UpdateLocalFileStatus(info.filePath, 2);
			}
		} else {
			// Not logged in - mark as fully processed to prevent re-detection
			adbapi->UpdateLocalFileStatus(info.filePath, 2);
		}
	}
	
	// Stop timer if queue is empty
	if (pendingHashedFilesQueue.isEmpty()) {
		hashedFilesProcessingTimer->stop();
		LOG("Finished processing all already-hashed files");
	}
}

bool Window::eventFilter(QObject *obj, QEvent *event)
{
	if(obj == this && event->type() == QEvent::Close)
	{
		QMessageBox(QMessageBox::NoIcon, "", "ding dong").exec();
		return 1;
	}
	else
	{
		return QObject::eventFilter(obj, event);
	}
	return QObject::eventFilter(obj, event);
}

bool hashes_::event(QEvent *e)
{
	if(e->type() == QEvent::KeyPress)
	{
		QKeyEvent *keyEvent = static_cast<QKeyEvent*>(e);
		if(keyEvent->key() == Qt::Key_Delete)
		{
			if(!hasherThreadPool->isRunning())
			{
				this->setUpdatesEnabled(0);
				QList<QTableWidgetItem *> selitems = this->selectedItems();
				QList<int> selrows;
				while(!selitems.isEmpty())
				{
					if(!selrows.contains(selitems.first()->row()))
					{
						selrows.append(selitems.first()->row());
						selitems.pop_front();
					}
					else
					{
						selitems.pop_front();
					}
				}
//                qsort(selrows.begin(), selrows.end());
                std::sort(selrows.begin(), selrows.end());
				while(!selrows.isEmpty())
				{
				int item = selrows.last();
					selrows.pop_back();
					this->removeRow(item);
				}
				this->setUpdatesEnabled(1);
			}
			return 1;
		}
		else
		{
			return QTableWidget::event(e);
		}
	}
	else
	{
		return QTableWidget::event(e);
	}
}

// unknown_files_ implementation
unknown_files_::unknown_files_(QWidget *parent) : QTableWidget(parent)
{
    setColumnCount(4);
    setRowCount(0);
    setRowHeight(0, 20);
    verticalHeader()->setDefaultSectionSize(20);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->hide();
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalHeaderLabels(QStringList() << "Filename" << "Anime" << "Episode" << "Action");
    setColumnWidth(0, 400);
    setColumnWidth(1, 300);
    setColumnWidth(2, 200);
    setColumnWidth(3, 220);  // Increased to accommodate Re-check button
    setMaximumHeight(200); // Limit height so it doesn't dominate the UI
}

bool unknown_files_::event(QEvent *e)
{
    // Disable delete key for unknown files to avoid data inconsistency
    // Users should use the Bind button or clear the hasher to remove entries
    if(e->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(e);
        if(keyEvent->key() == Qt::Key_Delete)
        {
            // Ignore delete key for now
            return true;
        }
        else
        {
            return QTableWidget::event(e);
        }
    }
    else
    {
        return QTableWidget::event(e);
    }
}

void Window::getNotifyLogAppend(QString str)
{
	QTime t;
	t = t.currentTime();
	QString a;
	a = QString("%1: %2").arg(t.toString()).arg(str);
	logOutput->append(a);
	
	// Note: This slot updates the UI log tab with timestamp
	// CrashLog is kept separate for emergency crash situations only
}

void Window::getNotifyLoginChagned(QString login)
{
	adbapi->setUsername(login);
}

void Window::getNotifyPasswordChagned(QString password)
{
	adbapi->setPassword(password);
}

void Window::shot()
{
	getNotifyLogAppend("shot");
}

void Window::closeEvent(QCloseEvent *event)
{
    if(adbapi->LoggedIn() && !safeclose->isActive())
	{
		adbapi->Logout();
        waitforlogout.start();
        safeclose->start();
        event->ignore();
	}
    if(!adbapi->LoggedIn() || waitforlogout.elapsed() > 5000)
    {
        event->accept();
    }
}

void Window::saveSettings()
{
	LOG("Saving settings - username: " + editLogin->text());
	adbapi->setUsername(editLogin->text());
	adbapi->setPassword(editPassword->text());
	
	// Save directory watcher settings to database
	adbapi->setWatcherEnabled(watcherEnabled->isChecked());
	adbapi->setWatcherDirectory(watcherDirectory->text());
	adbapi->setWatcherAutoStart(watcherAutoStart->isChecked());
	
	// Save auto-fetch settings to database
	adbapi->setAutoFetchEnabled(autoFetchEnabled->isChecked());
	
	// Save media player path
	PlaybackManager::setMediaPlayerPath(mediaPlayerPath->text());
	
	LOG("Settings saved");
}

void Window::apitesterProcess()
{
	QString data;
	data = apitesterInput->text();
	if(data.length() > 0)
	{
		apitesterOutput->append(data + "\n");
		apitesterInput->clear();
		adbapi->Send(data, "", "zzz");
	}
}

void Window::getNotifyMylistAdd(QString tag, int code)
{
    QString logMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " getNotifyMylistAdd() tag=" + tag + " code=" + QString::number(code);
    LOG(logMsg);
	for(int i=0; i<hashes->rowCount(); i++)
	{
        if(hashes->item(i, 5)->text() == tag || hashes->item(i, 6)->text() == tag)
		{
			QColor green_light; green_light.setRgb(0, 255, 0);
			QColor green_dark; green_dark.setRgb(0, 140, 0);
            QColor red; red.setRgb(255, 0, 0);
            if(code == 310) // already in mylist
            {
                hashes->item(i, 0)->setBackground(green_light.toRgb());
                hashes->item(i, 1)->setText("2");
                QString msg310 = "310-2";
                LOG(msg310);
                
                // Store local file path for already existing entry
                QString localPath = hashes->item(i, 2)->text();
                int lid = adbapi->UpdateLocalPath(tag, localPath);
                
                // Update only the specific mylist entry instead of reloading entire tree
                if(lid > 0)
                {
                    updateOrAddMylistEntry(lid);
                }
                
                // Remove from unknown files widget if present (re-check succeeded)
                for(int row = 0; row < unknownFiles->rowCount(); ++row)
                {
                    QTableWidgetItem *item = unknownFiles->item(row, 0);
                    if(item && item->toolTip() == localPath)
                    {
                        LOG(QString("Re-check succeeded (310), removing from unknown files: %1").arg(item->text()));
                        unknownFiles->removeRow(row);
                        unknownFilesData.remove(row);
                        
                        // Update row indices in unknownFilesData map
                        QMap<int, UnknownFileData> newMap;
                        for(auto it = unknownFilesData.begin(); it != unknownFilesData.end(); ++it)
                        {
                            int oldRow = it.key();
                            if(oldRow > row) {
                                newMap[oldRow - 1] = it.value();
                            } else {
                                newMap[oldRow] = it.value();
                            }
                        }
                        unknownFilesData = newMap;
                        
                        // Hide the widget if no more unknown files
                        if(unknownFiles->rowCount() == 0)
                        {
                            unknownFiles->hide();
                            QWidget *unknownFilesLabel = this->findChild<QWidget*>("unknownFilesLabel");
                            if(unknownFilesLabel)
                            {
                                unknownFilesLabel->hide();
                            }
                        }
                        break;
                    }
                }
                
                return;
            }
            if(code == 320)
            {
                hashes->item(i, 0)->setBackground(red.toRgb());
                hashes->item(i, 1)->setText("4"); // no such file
                QString msg320 = "320-4";
                LOG(msg320);
                
                // Update status in local_files to 3 (not in anidb)
                QString localPath = hashes->item(i, 2)->text();
                adbapi->UpdateLocalFileStatus(localPath, 3);
                
                // Add to unknown files widget for manual binding (only if not already there)
                QString filename = hashes->item(i, 0)->text();
                QString filepath = hashes->item(i, 2)->text();
                QString hash = hashes->item(i, 9)->text();
                
                // Check if file is already in unknown files widget (avoid duplicates)
                bool alreadyExists = false;
                for(int row = 0; row < unknownFiles->rowCount(); ++row)
                {
                    QTableWidgetItem *item = unknownFiles->item(row, 0);
                    if(item && item->toolTip() == filepath)
                    {
                        alreadyExists = true;
                        break;
                    }
                }
                
                if(!alreadyExists)
                {
                    // Get file size
                    QFileInfo fileInfo(filepath);
                    qint64 fileSize = fileInfo.size();
                    
                    unknownFilesInsertRow(filename, filepath, hash, fileSize);
                    LOG(QString("Added unknown file to manual binding widget: %1").arg(filename));
                }
                else
                {
                    LOG(QString("File already in unknown files widget, skipping: %1").arg(filename));
                }
                
                return;
            }
            else if(code == 311 || code == 210)
			{
                hashes->item(i, 0)->setBackground(green_dark.toRgb());
				hashes->item(i, 1)->setText("3");
                QString msg311 = "311/210-3";
                LOG(msg311);
				
				// Store local file path for newly added entry
				QString localPath = hashes->item(i, 2)->text();
				int lid = adbapi->UpdateLocalPath(tag, localPath);
				
				if(renameto->checkState() > 0)
				{
					// TODO: rename
				}
				
				// Update only the specific mylist entry instead of reloading entire tree
				if(lid > 0)
				{
					updateOrAddMylistEntry(lid);
				}
				
				// Remove from unknown files widget if present (re-check succeeded)
				for(int row = 0; row < unknownFiles->rowCount(); ++row)
				{
					QTableWidgetItem *item = unknownFiles->item(row, 0);
					if(item && item->toolTip() == localPath)
					{
						LOG(QString("Re-check succeeded (311/210), removing from unknown files: %1").arg(item->text()));
						unknownFiles->removeRow(row);
						unknownFilesData.remove(row);
						
						// Update row indices in unknownFilesData map
						QMap<int, UnknownFileData> newMap;
						for(auto it = unknownFilesData.begin(); it != unknownFilesData.end(); ++it)
						{
							int oldRow = it.key();
							if(oldRow > row) {
								newMap[oldRow - 1] = it.value();
							} else {
								newMap[oldRow] = it.value();
							}
						}
						unknownFilesData = newMap;
						
						// Hide the widget if no more unknown files
						if(unknownFiles->rowCount() == 0)
						{
							unknownFiles->hide();
							QWidget *unknownFilesLabel = this->findChild<QWidget*>("unknownFilesLabel");
							if(unknownFilesLabel)
							{
								unknownFilesLabel->hide();
							}
						}
						break;
					}
				}
				
				return;
			}
		}
	}
}

void Window::getNotifyLoggedIn(QString tag, int code)
{
    QString logMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " [Window] Login notification received - Tag: " + tag + " Code: " + QString::number(code);
    LOG(logMsg);
    loginbutton->setText(QString("Logout - logged in with tag %1 and code %2").arg(tag).arg(code));
	
	// Enable notifications after successful login
	adbapi->NotifyEnable();
	LOG("Notifications enabled");
}

void Window::getNotifyLoggedOut(QString tag, int code)
{
    QString logMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " [Window] getNotifyLoggedOut";
    LOG(logMsg);
    loginbutton->setText(QString("Login - logged out with tag %1 and code %2").arg(tag).arg(code));
}

void Window::getNotifyMessageReceived(int nid, QString message)
{
	LOG(QString("Notification %1 received: %2").arg(nid).arg(message));
	
	// Prevent downloading multiple exports simultaneously
	static bool isDownloadingExport = false;
	
	// Check if message contains mylist export link
	// AniDB notification format uses BBCode: [url=https://...]...[/url]
	// First try BBCode format, then fallback to plain URL
	QString exportUrl;
	
	// Try BBCode format first: [url=URL]text[/url]
	static const QRegularExpression bbcodeRegex("\\[url=(https?://[^\\]]+\\.tgz)\\]");
	QRegularExpressionMatch bbcodeMatch = bbcodeRegex.match(message);
	
	if(bbcodeMatch.hasMatch())
	{
		exportUrl = bbcodeMatch.captured(1);  // Capture group 1 is the URL inside [url=...]
	}
	else
	{
		// Fallback to plain URL format
		static const QRegularExpression plainRegex("https?://[^\\s\\]]+\\.tgz");
		QRegularExpressionMatch plainMatch = plainRegex.match(message);
		if(plainMatch.hasMatch())
		{
			exportUrl = plainMatch.captured(0);
		}
	}
	
	if(!exportUrl.isEmpty() && !isDownloadingExport)
	{
		// Verify that the notification message contains the expected template name
		QString expectedTemplate = adbapi->GetRequestedExportTemplate();
		
		// If a template was requested, verify the message mentions it
		if(!expectedTemplate.isEmpty() && !message.contains(expectedTemplate, Qt::CaseInsensitive))
		{
			LOG(QString("MyList export link found but template mismatch: expected '%1', skipping").arg(expectedTemplate));
			
			// Track notifications checked without finding correct export
			if(isCheckingNotifications)
			{
				notificationsCheckedWithoutExport++;
				
				// If we've checked all expected notifications and found no matching export
				if(notificationsCheckedWithoutExport >= expectedNotificationsToCheck)
				{
					// Only auto-request export on first run
					if(!isMylistFirstRunComplete())
					{
						LOG(QString("Checked %1 notifications with no matching export link found - requesting new export (first run)").arg(expectedNotificationsToCheck));
						mylistStatusLabel->setText("MyList Status: Requesting export (first run)...");
						
						// Request MYLISTEXPORT with xml-plain-cs template (default)
						// If a template was already requested, use that; otherwise use xml-plain-cs
						QString templateToRequest = expectedTemplate.isEmpty() ? "xml-plain-cs" : expectedTemplate;
						adbapi->MylistExport(templateToRequest);
					}
					else
					{
						LOG(QString("Checked %1 notifications with no matching export link found - use 'Request MyList Export' in Settings to manually request").arg(expectedNotificationsToCheck));
						mylistStatusLabel->setText("MyList Status: No export found - request manually in Settings");
					}
					
					// Reset state
					isCheckingNotifications = false;
					expectedNotificationsToCheck = 0;
					notificationsCheckedWithoutExport = 0;
				}
			}
			return;
		}
		
		isDownloadingExport = true;
		LOG(QString("MyList export link found: %1").arg(exportUrl));
		mylistStatusLabel->setText("MyList Status: Downloading export...");
		
		// Reset notification checking state since we found an export
		isCheckingNotifications = false;
		expectedNotificationsToCheck = 0;
		notificationsCheckedWithoutExport = 0;
		
		// Download the file
		QNetworkAccessManager *manager = new QNetworkAccessManager(this);
		QNetworkRequest request{QUrl(exportUrl)};
		request.setHeader(QNetworkRequest::UserAgentHeader, "Usagi/1");
		
		QNetworkReply *reply = manager->get(request);
		
		// Connect to download finished signal
		connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
			if(reply->error() == QNetworkReply::NoError)
			{
				// Save to temporary file
				QString tempPath = QDir::tempPath() + "/mylist_export_" + 
					QString::number(QDateTime::currentMSecsSinceEpoch()) + ".tgz";
				
				QFile file(tempPath);
				if(file.open(QIODevice::WriteOnly))
				{
					file.write(reply->readAll());
					file.close();
					
					LOG(QString("Export downloaded to: %1").arg(tempPath));
					mylistStatusLabel->setText("MyList Status: Parsing export...");
					
					// Parse the xml-plain-cs file
					int count = parseMylistExport(tempPath);
					
					if(count > 0)
					{
						LOG(QString("Successfully imported %1 mylist entries").arg(count));
						mylistStatusLabel->setText(QString("MyList Status: %1 entries loaded").arg(count));
						loadMylistFromDatabase();  // Refresh the display
						
						// Mark first run as complete after successful import
						setMylistFirstRunComplete();
					}
					else
					{
						LOG("No entries imported from notification export");
						mylistStatusLabel->setText("MyList Status: Import failed");
					}
					
					// Clean up temporary file
					QFile::remove(tempPath);
				}
				else
				{
					LOG("Error: Cannot save export file");
					mylistStatusLabel->setText("MyList Status: Download failed");
				}
			}
			else
			{
				LOG(QString("Error downloading export: %1").arg(reply->errorString()));
				mylistStatusLabel->setText("MyList Status: Download failed");
			}
			
			// Reset the flag to allow future downloads
			isDownloadingExport = false;
			
			reply->deleteLater();
			manager->deleteLater();
		});
	}
	else
	{
		LOG("No mylist export link found in notification");
		
		// Track notifications checked without finding export
		if(isCheckingNotifications)
		{
			notificationsCheckedWithoutExport++;
			
			// If we've checked all expected notifications and found no export
			if(notificationsCheckedWithoutExport >= expectedNotificationsToCheck)
			{
				// Only auto-request export on first run
				if(!isMylistFirstRunComplete())
				{
					LOG(QString("Checked %1 notifications with no export link found - requesting new export (first run)").arg(expectedNotificationsToCheck));
					mylistStatusLabel->setText("MyList Status: Requesting export (first run)...");
					
					// Request MYLISTEXPORT with xml-plain-cs template
					adbapi->MylistExport("xml-plain-cs");
				}
				else
				{
					LOG(QString("Checked %1 notifications with no export link found - use 'Request MyList Export' in Settings to manually request").arg(expectedNotificationsToCheck));
					mylistStatusLabel->setText("MyList Status: No export found - request manually in Settings");
				}
				
				// Reset state
				isCheckingNotifications = false;
				expectedNotificationsToCheck = 0;
				notificationsCheckedWithoutExport = 0;
			}
		}
	}
}

void Window::getNotifyCheckStarting(int count)
{
	// Called when anidbapi starts checking notifications
	isCheckingNotifications = true;
	expectedNotificationsToCheck = count;
	notificationsCheckedWithoutExport = 0;
	LOG(QString("Starting to check %1 notifications for mylist export link").arg(count));
}

void Window::getNotifyExportQueued(QString tag)
{
	// 217 EXPORT QUEUED - Export request accepted
	LOG(QString("MyList export queued successfully (Tag: %1)").arg(tag));
	mylistStatusLabel->setText("MyList Status: Export queued - waiting for notification...");
	// AniDB will send a notification when the export is ready
	// The notification will contain the download link
}

void Window::getNotifyExportAlreadyInQueue(QString tag)
{
	// 318 EXPORT ALREADY IN QUEUE - Cannot queue another export
	LOG(QString("MyList export already in queue (Tag: %1) - waiting for current export to complete").arg(tag));
	mylistStatusLabel->setText("MyList Status: Export already queued - waiting...");
	// No need to take action - wait for the existing export notification
}

void Window::getNotifyExportNoSuchTemplate(QString tag)
{
	// 317 EXPORT NO SUCH TEMPLATE - Invalid template name
	LOG(QString("ERROR: MyList export template not found (Tag: %1)").arg(tag));
	mylistStatusLabel->setText("MyList Status: Export failed - invalid template");
	// This should not happen with "xml-plain-cs" template, but log for debugging
}

void Window::onMylistItemExpanded(QTreeWidgetItem *item)
{
	// When an anime item is expanded, queue API requests for missing data
	if(!item)
		return;
	
	// Get the AID from the item (stored in UserRole)
	int aid = item->data(0, Qt::UserRole).toInt();
	if(aid == 0)
		return;  // Not an anime item (might be an episode child)
	
	// Check if anime metadata needs updating
	if(animeNeedingMetadata.contains(aid))
	{
		LOG(QString("Requesting anime metadata update for AID %1").arg(aid));
		// Use ANIME command to fetch anime metadata directly
		adbapi->Anime(aid);
		animeNeedingMetadata.remove(aid);  // Remove from tracking set
	}
	
	// Iterate through child episodes and queue API requests for missing data
	int childCount = item->childCount();
	for(int i = 0; i < childCount; i++)
	{
		QTreeWidgetItem *episodeItem = item->child(i);
		int eid = episodeItem->data(0, Qt::UserRole).toInt();
		
		// Check if this episode needs data and hasn't been requested yet
		if(episodesNeedingData.contains(eid))
		{
			LOG(QString("Requesting episode data for EID %1 (AID %2)").arg(eid).arg(aid));
			adbapi->Episode(eid);
			episodesNeedingData.remove(eid);  // Remove from tracking set to avoid duplicate requests
		}
	}
}

void Window::getNotifyEpisodeUpdated(int eid, int aid)
{
	// Episode data was updated in the database, update only the specific episode item
	LOG(QString("Episode data received for EID %1 (AID %2), updating field...").arg(eid).arg(aid));
	
	// Update tree view if active
	if (!mylistUseCardView) {
		updateEpisodeInTree(eid, aid);
	}
	
	// Update card manager if in card view
	if (mylistUseCardView && cardManager) {
		cardManager->onEpisodeUpdated(eid, aid);
		// Backward compatibility: update animeCards list
		animeCards = cardManager->getAllCards();
	}
}

void Window::getNotifyAnimeUpdated(int aid)
{
	QElapsedTimer timer;
	timer.start();
	
	// Anime metadata was updated in the database
	LOG(QString("Anime metadata received for AID %1").arg(aid));
	
	// Remove from tracking to prevent re-requests
	animeNeedingMetadata.remove(aid);
	animeMetadataRequested.remove(aid);
	
	qint64 elapsed = timer.elapsed();
	LOG(QString("[Timing] Initial cleanup took %1 ms").arg(elapsed));
	
	// Check if we got picname and need to download poster
	bool needsPosterDownload = false;
	if (animeNeedingPoster.contains(aid)) {
		qint64 startQuery = timer.elapsed();
		QSqlDatabase db = QSqlDatabase::database();
		if (validateDatabaseConnection(db, "getNotifyAnimeUpdated")) {
			QSqlQuery query(db);
			query.prepare("SELECT picname, poster_image FROM anime WHERE aid = ?");
			query.addBindValue(aid);
			
			if (query.exec() && query.next()) {
				QString picname = query.value(0).toString();
				QByteArray posterData = query.value(1).toByteArray();
				
				qint64 queryElapsed = timer.elapsed() - startQuery;
				LOG(QString("[Timing] Poster query took %1 ms").arg(queryElapsed));
				
				// If we got picname but no poster image, download it
				if (!picname.isEmpty() && posterData.isEmpty()) {
					animePicnames[aid] = picname;
					
					// Use card manager to download poster if in card view
					if (mylistUseCardView && cardManager) {
						cardManager->updateCardPoster(aid, picname);
					} else {
						downloadPosterForAnime(aid, picname);
					}
					
					needsPosterDownload = true;
					LOG(QString("[Timing] Triggered poster download for AID %1").arg(aid));
				}
			}
		}
	}
	
	// Update views
	if (mylistUseCardView && cardManager) {
		// Use card manager for efficient update
		cardManager->onAnimeUpdated(aid);
		// Backward compatibility: update animeCards list
		animeCards = cardManager->getAllCards();
		
		// If not just triggering poster download, may need to resort
		if (!needsPosterDownload) {
			sortMylistCards(mylistSortComboBox->currentIndex());
		}
	} else if (!needsPosterDownload) {
		// Tree view: reload entire view (old behavior)
		qint64 startReload = timer.elapsed();
		LOG(QString("[Timing] Starting view reload for AID %1").arg(aid));
		
		loadMylistFromDatabase();
		
		qint64 reloadElapsed = timer.elapsed() - startReload;
		LOG(QString("[Timing] View reload took %1 ms").arg(reloadElapsed));
	}
	
	qint64 totalElapsed = timer.elapsed();
	LOG(QString("[Timing] Total getNotifyAnimeUpdated for AID %1 took %2 ms").arg(aid).arg(totalElapsed));
}

void Window::updateEpisodeInTree(int eid, int aid)
{
	// Query the database for the updated episode data
	QSqlDatabase db = QSqlDatabase::database();
	
	// Validate database connection
	if(!validateDatabaseConnection(db, "updateEpisodeInTree"))
	{
		return;
	}
	
	QString query = QString("SELECT epno, name FROM episode WHERE eid = %1").arg(eid);
	QSqlQuery q(db);
	
	if(!q.exec(query))
	{
		LOG("Error querying episode data: " + q.lastError().text());
		return;
	}
	
	if(!q.next())
	{
		LOG(QString("No episode data found for EID %1").arg(eid));
		return;
	}
	
	QString epnoStr = q.value(0).toString();
	QString episodeName = q.value(1).toString();
	
	// Find the episode item in the tree by iterating through anime items
	int topLevelCount = mylistTreeWidget->topLevelItemCount();
	for(int i = 0; i < topLevelCount; i++)
	{
		QTreeWidgetItem *animeItem = mylistTreeWidget->topLevelItem(i);
		int animeAid = animeItem->data(0, Qt::UserRole).toInt();
		
		// Only check children of the matching anime
		if(animeAid == aid)
		{
			int childCount = animeItem->childCount();
			for(int j = 0; j < childCount; j++)
			{
				QTreeWidgetItem *episodeItem = animeItem->child(j);
				int episodeEid = episodeItem->data(0, Qt::UserRole).toInt();
				
				if(episodeEid == eid)
				{
					// Found the episode item - update its fields
					
					// Update episode number (Column 1) using epno type
					if(!epnoStr.isEmpty())
					{
						::epno episodeNumber(epnoStr);
						
						// Store epno object if this is an EpisodeTreeWidgetItem
						EpisodeTreeWidgetItem *epItem = dynamic_cast<EpisodeTreeWidgetItem*>(episodeItem);
						if(epItem)
						{
							epItem->setEpno(episodeNumber);
						}
						
						// Display formatted episode number (with leading zeros removed)
						episodeItem->setText(2, episodeNumber.toDisplayString());
					}
					else
					{
						episodeItem->setText(2, "Unknown");
					}
					
					// Update episode name (Column 2)
					if(!episodeName.isEmpty())
					{
						episodeItem->setText(3, episodeName);
					}
					else
					{
						episodeItem->setText(3, "Unknown");
					}
					
					// Remove from tracking set since data has been loaded
					episodesNeedingData.remove(eid);
					
					LOG(QString("Updated episode in tree: EID %1, epno: %2, name: %3")
						.arg(eid).arg(episodeItem->text(2)).arg(episodeName));
					return;
				}
			}
		}
	}
	
	// If we get here, the episode item wasn't found in the tree
	LOG(QString("Episode item not found in tree for EID %1 (AID %2)").arg(eid).arg(aid));
}

void Window::updateOrAddMylistEntry(int lid)
{
	// Update or add a single mylist entry without clearing the entire tree
	// This preserves selection, sorting, focus, and expanded state
	
	// If in card view, use card manager for efficient updates
	if (mylistUseCardView && cardManager) {
		cardManager->updateOrAddMylistEntry(lid);
		// Backward compatibility: update animeCards list
		animeCards = cardManager->getAllCards();
		return;
	}
	
	QSqlDatabase db = QSqlDatabase::database();
	
	// Validate database connection
	if(!validateDatabaseConnection(db, "updateOrAddMylistEntry"))
	{
		return;
	}
	
	// Query the database for this specific mylist entry
	QSqlQuery q(db);
	q.prepare("SELECT m.lid, m.aid, m.eid, m.state, m.viewed, m.storage, "
			  "a.nameromaji, a.nameenglish, a.eptotal, "
			  "e.name as episode_name, e.epno, "
			  "(SELECT title FROM anime_titles WHERE aid = m.aid AND type = 1 LIMIT 1) as anime_title, "
			  "a.eps, a.typename, a.startdate, a.enddate "
			  "FROM mylist m "
			  "LEFT JOIN anime a ON m.aid = a.aid "
			  "LEFT JOIN episode e ON m.eid = e.eid "
			  "WHERE m.lid = ?");
	q.addBindValue(lid);
	
	if(!q.exec())
	{
		LOG(QString("Error querying mylist entry (lid=%1): %2").arg(lid).arg(q.lastError().text()));
		return;
	}
	
	if(!q.next())
	{
		LOG(QString("No mylist entry found for lid=%1").arg(lid));
		return;
	}
	
	// Extract data from query
	int aid = q.value(1).toInt();
	int eid = q.value(2).toInt();
	int state = q.value(3).toInt();
	int viewed = q.value(4).toInt();
	QString storage = q.value(5).toString();
	QString animeName = q.value(6).toString();
	QString animeNameEnglish = q.value(7).toString();
	int epTotal = q.value(8).toInt();
	QString episodeName = q.value(9).toString();
	QString epnoStr = q.value(10).toString();
	QString animeTitle = q.value(11).toString();
	int eps = q.value(12).toInt();
	QString typeName = q.value(13).toString();
	QString startDate = q.value(14).toString();
	QString endDate = q.value(15).toString();
	
	// Determine anime name using utility function
	animeName = AnimeUtils::determineAnimeName(animeName, animeNameEnglish, animeTitle, aid);
	
	// Disable sorting temporarily to prevent issues during updates
	bool sortingEnabled = mylistTreeWidget->isSortingEnabled();
	int currentSortColumn = mylistTreeWidget->sortColumn();
	Qt::SortOrder currentSortOrder = mylistTreeWidget->header()->sortIndicatorOrder();
	mylistTreeWidget->setSortingEnabled(false);
	
	// Find or create the anime parent item
	AnimeTreeWidgetItem *animeItem = nullptr;
	int topLevelCount = mylistTreeWidget->topLevelItemCount();
	for(int i = 0; i < topLevelCount; i++)
	{
		QTreeWidgetItem *item = mylistTreeWidget->topLevelItem(i);
		if(item && item->data(0, Qt::UserRole).toInt() == aid)
		{
			// Use dynamic_cast for safe type checking
			animeItem = dynamic_cast<AnimeTreeWidgetItem*>(item);
			if(animeItem)
			{
				break;
			}
		}
	}
	
	bool animeItemWasExpanded = false;
	if(!animeItem)
	{
		// Create new anime item
		animeItem = new AnimeTreeWidgetItem(mylistTreeWidget);
		animeItem->setText(0, animeName);
		animeItem->setData(0, Qt::UserRole, aid);
		mylistTreeWidget->addTopLevelItem(animeItem);
		
		// Set Type and Aired for new anime items
		if(!typeName.isEmpty())
		{
			animeItem->setText(8, typeName);
		}
		if(!startDate.isEmpty())
		{
			aired airedDates(startDate, endDate);
			animeItem->setText(9, airedDates.toDisplayString());
			animeItem->setAired(airedDates);
		}
	}
	else
	{
		// Remember if anime was expanded
		animeItemWasExpanded = animeItem->isExpanded();
		
		// Update Type and Aired if they're not already set
		if(!typeName.isEmpty() && animeItem->text(8).isEmpty())
		{
			animeItem->setText(8, typeName);
		}
		if(!startDate.isEmpty() && animeItem->text(9).isEmpty())
		{
			aired airedDates(startDate, endDate);
			animeItem->setText(9, airedDates.toDisplayString());
			animeItem->setAired(airedDates);
		}
	}
	
	// Find or create the episode child item
	EpisodeTreeWidgetItem *episodeItem = nullptr;
	int childCount = animeItem->childCount();
	for(int j = 0; j < childCount; j++)
	{
		QTreeWidgetItem *child = animeItem->child(j);
		if(child && child->data(0, Qt::UserRole).toInt() == eid)
		{
			episodeItem = dynamic_cast<EpisodeTreeWidgetItem*>(child);
			// If dynamic_cast returns nullptr, episodeItem will remain nullptr
			// and a new item will be created below
			if(episodeItem)
			{
				break;
			}
		}
	}
	
	if(!episodeItem)
	{
		// Create new episode item (either not found or wrong type)
		episodeItem = new EpisodeTreeWidgetItem(animeItem);
		episodeItem->setText(0, ""); // Empty for episode child
		episodeItem->setData(0, Qt::UserRole, eid);
		episodeItem->setData(0, Qt::UserRole + 1, lid);
	}
	
	// Update episode fields
	// Column 1: Episode number
	int episodeType = 1;  // Default to normal episode
	if(!epnoStr.isEmpty())
	{
		::epno episodeNumber(epnoStr);
		episodeItem->setEpno(episodeNumber);
		episodeItem->setText(2, episodeNumber.toDisplayString());
		episodeType = episodeNumber.type();
	}
	else
	{
		episodeItem->setText(2, "Loading...");
		episodesNeedingData.insert(eid);
	}
	
	// Column 2: Episode title
	if(episodeName.isEmpty())
	{
		episodeName = "Loading...";
		episodesNeedingData.insert(eid);
	}
	episodeItem->setText(3, episodeName);
	
	// Column 3: State
	QString stateStr;
	switch(state)
	{
		case 0: stateStr = "Unknown"; break;
		case 1: stateStr = "HDD"; break;
		case 2: stateStr = "CD/DVD"; break;
		case 3: stateStr = "Deleted"; break;
		default: stateStr = QString::number(state); break;
	}
	episodeItem->setText(4, stateStr);
	
	// Column 4: Viewed
	episodeItem->setText(5, viewed ? "Yes" : "No");
	
	// Column 5: Storage
	episodeItem->setText(6, storage);
	
	// Column 6: Mylist ID
	episodeItem->setText(7, QString::number(lid));
	
	// Now recalculate anime parent statistics
	// Count all episodes for this anime
	int animeEpisodeCount = 0;
	int animeViewedCount = 0;
	int normalEpisodes = 0;
	int otherEpisodes = 0;
	int normalViewed = 0;
	int otherViewed = 0;
	
	childCount = animeItem->childCount();
	for(int j = 0; j < childCount; j++)
	{
		EpisodeTreeWidgetItem *child = dynamic_cast<EpisodeTreeWidgetItem*>(animeItem->child(j));
		if(!child)
		{
			continue;
		}
		
		int childEpisodeType = 1;
		::epno childEpno = child->getEpno();
		if(childEpno.isValid())
		{
			childEpisodeType = childEpno.type();
		}
		
		bool childViewed = (child->text(COL_VIEWED) == "Yes");
		
		animeEpisodeCount++;
		if(childEpisodeType == 1)
		{
			normalEpisodes++;
			if(childViewed)
			{
				normalViewed++;
			}
		}
		else
		{
			otherEpisodes++;
			if(childViewed)
			{
				otherViewed++;
			}
		}
		
		if(childViewed)
		{
			animeViewedCount++;
		}
	}
	
	// Update anime parent statistics (columns 1 and 4)
	
	// Column 1 (Episode): show format "A/B+C"
	QString episodeText;
	if(eps > 0)
	{
		if(otherEpisodes > 0)
		{
			episodeText = QString("%1/%2+%3").arg(normalEpisodes).arg(eps).arg(otherEpisodes);
		}
		else
		{
			episodeText = QString("%1/%2").arg(normalEpisodes).arg(eps);
		}
	}
	else
	{
		if(otherEpisodes > 0)
		{
			episodeText = QString("%1/?+%2").arg(normalEpisodes).arg(otherEpisodes);
		}
		else
		{
			episodeText = QString("%1/?").arg(normalEpisodes);
		}
	}
	animeItem->setText(2, episodeText);
	
	// Column 4 (Viewed): show format "A/B+C"
	QString viewedText;
	if(otherEpisodes > 0)
	{
		viewedText = QString("%1/%2+%3").arg(normalViewed).arg(normalEpisodes).arg(otherViewed);
	}
	else
	{
		viewedText = QString("%1/%2").arg(normalViewed).arg(normalEpisodes);
	}
	animeItem->setText(5, viewedText);
	
	// Column 9 (Play button): Show checkmark if all normal episodes are watched
	bool allNormalWatched = (eps > 0 && normalViewed >= eps);
	if(allNormalWatched)
	{
		animeItem->setText(COL_PLAY, "✓"); // Checkmark for fully watched
	}
	else if(normalEpisodes > 0 || otherEpisodes > 0)
	{
		animeItem->setText(COL_PLAY, "▶"); // Play button
	}
	else
	{
		animeItem->setText(COL_PLAY, ""); // No button if no episodes
	}
	
	// Update episode play button
	if(viewed)
	{
		episodeItem->setText(COL_PLAY, "✓"); // Checkmark for watched
	}
	else
	{
		episodeItem->setText(COL_PLAY, "▶"); // Play button
	}
	
	// Restore expanded state
	if(animeItemWasExpanded)
	{
		animeItem->setExpanded(true);
	}
	
	// Re-enable sorting with the previous sort order
	if(sortingEnabled)
	{
		mylistTreeWidget->setSortingEnabled(true);
		mylistTreeWidget->sortByColumn(currentSortColumn, currentSortOrder);
	}
	
	LOG(QString("Updated mylist entry: lid=%1, aid=%2, eid=%3, anime=%4, episode=%5")
		.arg(lid).arg(aid).arg(eid).arg(animeName).arg(episodeItem->text(2)));
}

void Window::hashesinsertrow(QFileInfo file, Qt::CheckState ren, const QString& preloadedHash)
{
	QTableWidgetItem *item1 = new QTableWidgetItem(QTableWidgetItem(QString(file.fileName())));
	QColor colorgray;
	colorgray.setRgb(230, 230, 230);
    item1->setBackground(colorgray.toRgb());
	QTableWidgetItem *item2 = new QTableWidgetItem(QTableWidgetItem(QString("0")));
	QTableWidgetItem *item3 = new QTableWidgetItem(QTableWidgetItem(QString(file.absoluteFilePath())));
	QTableWidgetItem *item4 = new QTableWidgetItem(QTableWidgetItem(QString("?")));
	QTableWidgetItem *item5 = new QTableWidgetItem(QTableWidgetItem(QString("?")));
	QTableWidgetItem *item6 = new QTableWidgetItem(QTableWidgetItem(QString("?")));
	QTableWidgetItem *item7 = new QTableWidgetItem(QTableWidgetItem(QString("?")));
	QTableWidgetItem *item8 = new QTableWidgetItem(QTableWidgetItem(QString(ren > 0 ? "1" : "0")));
	QTableWidgetItem *item9 = new QTableWidgetItem(QTableWidgetItem(QString("0")));
	
	// Use preloaded hash if provided, otherwise query database
	QString existingHash = preloadedHash.isEmpty() ? adbapi->getLocalFileHash(file.absoluteFilePath()) : preloadedHash;
	QTableWidgetItem *item10 = new QTableWidgetItem(QTableWidgetItem(existingHash.isEmpty() ? QString("") : existingHash));
	
	hashes->insertRow(hashes->rowCount());
	hashes->setItem(hashes->rowCount()-1, 0, item1);
	hashes->setItem(hashes->rowCount()-1, 1, item2);
	hashes->setItem(hashes->rowCount()-1, 2, item3);
	hashes->setItem(hashes->rowCount()-1, 3, item4);
	hashes->setItem(hashes->rowCount()-1, 4, item5);
	hashes->setItem(hashes->rowCount()-1, 5, item6);
	hashes->setItem(hashes->rowCount()-1, 6, item7);
	hashes->setItem(hashes->rowCount()-1, 7, item8);
	hashes->setItem(hashes->rowCount()-1, 8, item9);
	hashes->setItem(hashes->rowCount()-1, 9, item10);
}

void Window::unknownFilesInsertRow(const QString& filename, const QString& filepath, const QString& hash, qint64 size)
{
    // Show the unknown files widget if it's hidden
    QWidget *unknownFilesLabel = this->findChild<QWidget*>("unknownFilesLabel");
    if(unknownFilesLabel && unknownFilesLabel->isHidden())
    {
        unknownFilesLabel->show();
    }
    if(unknownFiles->isHidden())
    {
        unknownFiles->show();
    }
    
    int row = unknownFiles->rowCount();
    unknownFiles->insertRow(row);
    
    // Column 0: Filename
    QTableWidgetItem *filenameItem = new QTableWidgetItem(filename);
    filenameItem->setToolTip(filepath);
    unknownFiles->setItem(row, 0, filenameItem);
    
    // Column 1: Anime search field (using QLineEdit with autocomplete)
    QLineEdit *animeSearch = new QLineEdit();
    animeSearch->setPlaceholderText("Search anime title...");
    
    // Load anime titles cache if not already loaded
    loadAnimeTitlesCache();
    
    // Use cached anime titles for completer (avoid repeated DB queries)
    QCompleter *completer = new QCompleter(cachedAnimeTitles, animeSearch);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    animeSearch->setCompleter(completer);
    
    unknownFiles->setCellWidget(row, 1, animeSearch);
    
    // Column 2: Episode input (QLineEdit with suggestions, allows manual entry)
    QLineEdit *episodeInput = new QLineEdit();
    episodeInput->setPlaceholderText("Enter episode number...");
    episodeInput->setEnabled(false);
    unknownFiles->setCellWidget(row, 2, episodeInput);
    
    // Column 3: Action buttons (Bind and Not Anime in a container)
    QWidget *actionContainer = new QWidget();
    QHBoxLayout *actionLayout = new QHBoxLayout(actionContainer);
    actionLayout->setContentsMargins(2, 2, 2, 2);
    actionLayout->setSpacing(4);
    
    QPushButton *bindButton = new QPushButton("Bind");
    bindButton->setEnabled(false);
    // Store filepath in button property for later lookup
    bindButton->setProperty("filepath", filepath);
    
    QPushButton *notAnimeButton = new QPushButton("Not Anime");
    // Store filepath in button property for later lookup
    notAnimeButton->setProperty("filepath", filepath);
    
    QPushButton *recheckButton = new QPushButton("Re-check");
    // Store filepath in button property for later lookup
    recheckButton->setProperty("filepath", filepath);
    recheckButton->setToolTip("Re-validate this file against AniDB (in case it was added since last check)");
    
    actionLayout->addWidget(bindButton);
    actionLayout->addWidget(notAnimeButton);
    actionLayout->addWidget(recheckButton);
    actionContainer->setLayout(actionLayout);
    
    unknownFiles->setCellWidget(row, 3, actionContainer);
    
    // Store file data with filepath as additional key for stable lookup
    UnknownFileData fileData;
    fileData.filename = filename;
    fileData.filepath = filepath;
    fileData.hash = hash;
    fileData.size = size;
    fileData.selectedAid = -1;
    fileData.selectedEid = -1;
    unknownFilesData[row] = fileData;
    
    // Connect anime search to populate episode suggestions
    connect(animeSearch, &QLineEdit::textChanged, this, [this, filepath, animeSearch, episodeInput, bindButton]() {
        QString searchText = animeSearch->text();
        
        // Find current row by filepath (stable identifier)
        int currentRow = -1;
        for(int i = 0; i < unknownFiles->rowCount(); ++i) {
            QTableWidgetItem *item = unknownFiles->item(i, 0);
            if(item && item->toolTip() == filepath) {
                currentRow = i;
                break;
            }
        }
        
        if(currentRow < 0 || !unknownFilesData.contains(currentRow)) return;
        
        // Check if the text matches a valid anime from autocomplete (use cached data)
        if(cachedTitleToAid.contains(searchText))
        {
            int aid = cachedTitleToAid[searchText];
            unknownFilesData[currentRow].selectedAid = aid;
            
            // Enable episode input
            episodeInput->setEnabled(true);
            episodeInput->setPlaceholderText("Enter episode number (e.g., 1, S1, etc.)...");
            
            // Optionally, we could add a completer with episode numbers from database
            // For now, just allow manual entry
        }
        else
        {
            // Clear episode selection if anime is not valid
            episodeInput->clear();
            episodeInput->setEnabled(false);
            episodeInput->setPlaceholderText("Select anime first...");
            bindButton->setEnabled(false);
            unknownFilesData[currentRow].selectedAid = -1;
        }
    });
    
    // Connect episode input to enable bind button when text is entered
    connect(episodeInput, &QLineEdit::textChanged, this, [this, filepath, episodeInput, bindButton]() {
        // Find current row by filepath (stable identifier)
        int currentRow = -1;
        for(int i = 0; i < unknownFiles->rowCount(); ++i) {
            QTableWidgetItem *item = unknownFiles->item(i, 0);
            if(item && item->toolTip() == filepath) {
                currentRow = i;
                break;
            }
        }
        
        if(currentRow < 0 || !unknownFilesData.contains(currentRow)) return;
        
        QString epnoText = episodeInput->text().trimmed();
        if(!epnoText.isEmpty() && unknownFilesData[currentRow].selectedAid > 0)
        {
            bindButton->setEnabled(true);
        }
        else
        {
            bindButton->setEnabled(false);
        }
    });
    
    // Connect bind button - use filepath to find current row dynamically
    connect(bindButton, &QPushButton::clicked, this, [this, bindButton, episodeInput, filepath]() {
        // Find current row by filepath
        int currentRow = -1;
        for(int i = 0; i < unknownFiles->rowCount(); ++i) {
            QTableWidgetItem *item = unknownFiles->item(i, 0);
            if(item && item->toolTip() == filepath) {
                currentRow = i;
                break;
            }
        }
        
        if(currentRow >= 0) {
            onUnknownFileBindClicked(currentRow, episodeInput->text().trimmed());
        }
    });
    
    // Connect "Not Anime" button - use filepath to find current row dynamically
    connect(notAnimeButton, &QPushButton::clicked, this, [this, notAnimeButton, filepath]() {
        LOG("Not Anime button clicked");
        LOG(QString("Not Anime button filepath: %1").arg(filepath));
        
        // Find current row by filepath
        int currentRow = -1;
        for(int i = 0; i < unknownFiles->rowCount(); ++i) {
            QTableWidgetItem *item = unknownFiles->item(i, 0);
            if(item && item->toolTip() == filepath) {
                currentRow = i;
                break;
            }
        }
        
        LOG(QString("Not Anime button found row: %1").arg(currentRow));
        
        if(currentRow >= 0) {
            onUnknownFileNotAnimeClicked(currentRow);
        } else {
            LOG("ERROR: Could not find row for filepath");
        }
    });
    
    // Connect "Re-check" button - use filepath to find current row dynamically
    connect(recheckButton, &QPushButton::clicked, this, [this, recheckButton, filepath]() {
        LOG("Re-check button clicked");
        LOG(QString("Re-check button filepath: %1").arg(filepath));
        
        // Find current row by filepath
        int currentRow = -1;
        for(int i = 0; i < unknownFiles->rowCount(); ++i) {
            QTableWidgetItem *item = unknownFiles->item(i, 0);
            if(item && item->toolTip() == filepath) {
                currentRow = i;
                break;
            }
        }
        
        LOG(QString("Re-check button found row: %1").arg(currentRow));
        
        if(currentRow >= 0) {
            onUnknownFileRecheckClicked(currentRow);
        } else {
            LOG("ERROR: Could not find row for filepath");
        }
    });
}

void Window::loadMylistFromDatabase()
{
	// If card view is active, use card loading instead
	if (mylistUseCardView) {
		animeMetadataRequested.clear();  // Clear request tracking for fresh load
		loadMylistAsCards();
		return;
	}
	
	mylistTreeWidget->clear();
	episodesNeedingData.clear();  // Clear tracking set
	animeNeedingMetadata.clear();  // Clear tracking set
	
	// Query the database for mylist entries joined with anime, episode, and file data
	// Structure: anime -> episode -> file (three-level hierarchy)
	QSqlDatabase db = QSqlDatabase::database();
	
	// Validate database connection before proceeding
	if(!validateDatabaseConnection(db, "loadMylistFromDatabase"))
	{
		mylistStatusLabel->setText("MyList Status: Database Error");
		return;
	}
	
	// Query all mylist entries with joined data including file information
	QString query = "SELECT m.lid, m.aid, m.eid, m.fid, m.state, m.viewed, m.storage, "
					"a.nameromaji, a.nameenglish, a.eptotal, "
					"e.name as episode_name, e.epno, "
					"(SELECT title FROM anime_titles WHERE aid = m.aid AND type = 1 LIMIT 1) as anime_title, "
					"a.eps, a.typename, a.startdate, a.enddate, "
					"f.filetype, f.resolution, f.quality, "
					"(SELECT name FROM `group` WHERE gid = m.gid) as group_name, "
					"f.filename, m.gid, m.last_played, "
					"lf.path as local_file_path "
					"FROM mylist m "
					"LEFT JOIN anime a ON m.aid = a.aid "
					"LEFT JOIN episode e ON m.eid = e.eid "
					"LEFT JOIN file f ON m.fid = f.fid "
					"LEFT JOIN local_files lf ON m.local_file = lf.id "
					"ORDER BY a.nameromaji, m.eid, m.lid";
	
	QSqlQuery q(db);
	
	if(!q.exec(query))
	{
		LOG("Error loading mylist: " + q.lastError().text());
		return;
	}
	
	QMap<int, QTreeWidgetItem*> animeItems; // aid -> tree item
	QMap<QPair<int, int>, QTreeWidgetItem*> episodeItems; // (aid, eid) -> tree item
	
	// Track statistics per anime
	QMap<int, int> animeEpisodeCount;  // aid -> count of unique episodes in mylist
	QMap<int, int> animeViewedCount;   // aid -> count of viewed episodes
	QMap<int, int> animeEpTotal;       // aid -> total primary type episodes (eptotal)
	QMap<int, int> animeEps;           // aid -> total normal episodes (eps)
	// Track normal (type 1) vs other types separately
	QMap<int, int> animeNormalEpisodeCount;    // aid -> count of normal episodes (type 1) in mylist
	QMap<int, int> animeOtherEpisodeCount;     // aid -> count of other type episodes (types 2-6) in mylist
	QMap<int, int> animeNormalViewedCount;     // aid -> count of normal episodes viewed
	QMap<int, int> animeOtherViewedCount;      // aid -> count of other type episodes viewed
	
	// Track which episodes we've already counted (to avoid counting same episode multiple times)
	QSet<QPair<int, int>> countedEpisodes; // (aid, eid) pairs
	
	int totalFiles = 0;
	
	while(q.next())
	{
		int lid = q.value(0).toInt();
		int aid = q.value(1).toInt();
		int eid = q.value(2).toInt();
		int fid = q.value(3).toInt();
		int state = q.value(4).toInt();
		int viewed = q.value(5).toInt();
		QString storage = q.value(6).toString();
		QString animeName = q.value(7).toString();
		QString animeNameEnglish = q.value(8).toString();
		int epTotal = q.value(9).toInt();
		QString episodeName = q.value(10).toString();
		QString epno = q.value(11).toString();
		QString animeTitle = q.value(12).toString();
		int eps = q.value(13).toInt();
		QString typeName = q.value(14).toString();
		QString startDate = q.value(15).toString();
		QString endDate = q.value(16).toString();
		QString filetype = q.value(17).toString();
		QString resolution = q.value(18).toString();
		QString quality = q.value(19).toString();
		QString groupName = q.value(20).toString();
		QString filename = q.value(21).toString();
		int gid = q.value(22).toInt();
		qint64 lastPlayed = q.value(23).toLongLong();
		QString localFilePath = q.value(24).toString();
		
		// Check if file exists
		bool fileExists = false;
		if (!localFilePath.isEmpty()) {
			QFileInfo fileInfo(localFilePath);
			fileExists = fileInfo.exists() && fileInfo.isFile();
		} else if (!storage.isEmpty()) {
			// Fallback to storage path if local_file not set
			QFileInfo fileInfo(storage);
			fileExists = fileInfo.exists() && fileInfo.isFile();
		}
		
		// Determine anime name using utility function
		animeName = AnimeUtils::determineAnimeName(animeName, animeNameEnglish, animeTitle, aid);
		
		// Get or create the anime parent item
		AnimeTreeWidgetItem *animeItem;
		if(animeItems.contains(aid))
		{
			animeItem = static_cast<AnimeTreeWidgetItem*>(animeItems[aid]);
		}
		else
		{
			animeItem = new AnimeTreeWidgetItem(mylistTreeWidget);
			animeItem->setText(0, animeName);
			animeItem->setData(0, Qt::UserRole, aid);
			
			animeItems[aid] = animeItem;
			mylistTreeWidget->addTopLevelItem(animeItem);
			
			// Initialize counters for this anime
			animeEpisodeCount[aid] = 0;
			animeViewedCount[aid] = 0;
			animeEpTotal[aid] = epTotal;
			animeEps[aid] = eps;
			animeNormalEpisodeCount[aid] = 0;
			animeOtherEpisodeCount[aid] = 0;
			animeNormalViewedCount[aid] = 0;
			animeOtherViewedCount[aid] = 0;
		}
		
		// Set Type column (column 7) for anime parent item
		if(!typeName.isEmpty() && animeItem->text(8).isEmpty())
		{
			animeItem->setText(8, typeName);
		}
		else if(typeName.isEmpty() && animeItem->text(8).isEmpty())
		{
			// Mark anime as needing metadata if typename is missing
			animeNeedingMetadata.insert(aid);
		}
		
		// Set Aired column (column 8) for anime parent item
		if(!startDate.isEmpty() && animeItem->text(9).isEmpty())
		{
			aired airedDates(startDate, endDate);
			animeItem->setText(9, airedDates.toDisplayString());
			animeItem->setAired(airedDates);
		}
		else if(startDate.isEmpty() && animeItem->text(9).isEmpty())
		{
			// Mark anime as needing metadata if startdate is missing
			animeNeedingMetadata.insert(aid);
		}
		
		// Get or create the episode item
		QPair<int, int> episodeKey(aid, eid);
		EpisodeTreeWidgetItem *episodeItem;
		
		if(episodeItems.contains(episodeKey))
		{
			episodeItem = static_cast<EpisodeTreeWidgetItem*>(episodeItems[episodeKey]);
		}
		else
		{
			// Create new episode item
			episodeItem = new EpisodeTreeWidgetItem(animeItem);
			episodeItem->setText(0, "");
			episodeItem->setData(0, Qt::UserRole, eid);
			
			// Column 1: Episode number using epno type
			int episodeType = 1;
			if(!epno.isEmpty())
			{
				::epno episodeNumber(epno);
				episodeItem->setEpno(episodeNumber);
				episodeItem->setText(2, episodeNumber.toDisplayString());
				episodeType = episodeNumber.type();
			}
			else
			{
				episodeItem->setText(2, "Loading...");
				episodesNeedingData.insert(eid);
			}
			
			// Column 2: Episode title
			if(episodeName.isEmpty())
			{
				episodeName = "Loading...";
				episodesNeedingData.insert(eid);
			}
			episodeItem->setText(3, episodeName);
			
			// Empty columns 3-6 for episode items (will be shown in file children)
			episodeItem->setText(4, "");
			episodeItem->setText(5, "");
			episodeItem->setText(6, "");
			episodeItem->setText(7, "");
			
			episodeItems[episodeKey] = episodeItem;
			
			// Update statistics only once per episode
			if(!countedEpisodes.contains(episodeKey))
			{
				countedEpisodes.insert(episodeKey);
				animeEpisodeCount[aid]++;
				
				if(episodeType == 1)
				{
					animeNormalEpisodeCount[aid]++;
				}
				else
				{
					animeOtherEpisodeCount[aid]++;
				}
				
				// We'll update viewed count when we see the first viewed file for this episode
			}
		}
		
		// Create file item as child of episode
		FileTreeWidgetItem *fileItem = new FileTreeWidgetItem(episodeItem);
		fileItem->setText(0, "");
		
		// Column 1: Display file info (resolution, quality, group)
		QString fileInfo;
		if(!resolution.isEmpty())
		{
			fileInfo = resolution;
		}
		if(!quality.isEmpty())
		{
			if(!fileInfo.isEmpty()) fileInfo += " ";
			fileInfo += quality;
		}
		if(!groupName.isEmpty())
		{
			if(!fileInfo.isEmpty()) fileInfo += " ";
			fileInfo += "[" + groupName + "]";
		}
		else if(gid > 0)
		{
			if(!fileInfo.isEmpty()) fileInfo += " ";
			fileInfo += "[GID:" + QString::number(gid) + "]";
		}
		
		// If no file info available, show filename or fid
		if(fileInfo.isEmpty())
		{
			if(!filename.isEmpty())
			{
				fileInfo = filename;
			}
			else if(fid > 0)
			{
				fileInfo = QString("FID:%1").arg(fid);
			}
			else
			{
				fileInfo = "File";
			}
		}
		
		fileItem->setText(2, fileInfo);
		
		// Column 2: Show file type
		// Use constant for default file type
		static const QString DEFAULT_FILETYPE = "video";
		QString fileTypeDisplay = filetype.isEmpty() ? DEFAULT_FILETYPE : filetype;
		fileItem->setText(3, fileTypeDisplay);
		
		// Determine file type for color coding using helper method
		FileTreeWidgetItem::FileType type = determineFileType(filetype);
		fileItem->setFileType(type);
		fileItem->setResolution(resolution);
		fileItem->setQuality(quality);
		fileItem->setGroupName(groupName);
		
		// Apply color based on file type
		QColor fileColor;
		switch(type)
		{
			case FileTreeWidgetItem::Video:
				fileColor = QColor(230, 240, 255); // Light blue
				break;
			case FileTreeWidgetItem::Subtitle:
				fileColor = QColor(255, 250, 230); // Light yellow
				break;
			case FileTreeWidgetItem::Audio:
				fileColor = QColor(255, 230, 240); // Light pink
				break;
			case FileTreeWidgetItem::Other:
				fileColor = QColor(240, 240, 240); // Light gray
				break;
		}
		
		// Apply background color to all columns
		for(int col = 0; col < mylistTreeWidget->columnCount(); col++)
		{
			fileItem->setBackground(col, fileColor);
		}
		
		// State: 0=unknown, 1=on hdd, 2=on cd, 3=deleted
		QString stateStr;
		switch(state)
		{
			case 0: stateStr = "Unknown"; break;
			case 1: stateStr = "HDD"; break;
			case 2: stateStr = "CD/DVD"; break;
			case 3: stateStr = "Deleted"; break;
			default: stateStr = QString::number(state); break;
		}
		fileItem->setText(4, stateStr);
		
		// Viewed status
		fileItem->setText(5, viewed ? "Yes" : "No");
		fileItem->setText(6, storage);
		fileItem->setText(7, QString::number(lid));
		
		// Last played column (column 10)
		if (lastPlayed > 0) {
			QDateTime lastPlayedTime = QDateTime::fromSecsSinceEpoch(lastPlayed);
			fileItem->setText(COL_LAST_PLAYED, lastPlayedTime.toString("yyyy-MM-dd hh:mm"));
			// Store the timestamp for sorting
			fileItem->setData(COL_LAST_PLAYED, Qt::UserRole, lastPlayed);
		} else {
			fileItem->setText(COL_LAST_PLAYED, "Never");
			fileItem->setData(COL_LAST_PLAYED, Qt::UserRole, 0);
		}
		
		// Add play button text indicator (column 1 - PLAY)
		if (type == FileTreeWidgetItem::Video) {
			if (!fileExists) {
				fileItem->setText(COL_PLAY, "✗"); // X for file not found
				fileItem->setForeground(COL_PLAY, QBrush(QColor(Qt::red))); // Red color for disabled
				fileItem->setData(COL_PLAY, Qt::UserRole, 2); // Sort key for unavailable
			} else if (viewed) {
				fileItem->setText(COL_PLAY, "✓"); // Checkmark for watched
				fileItem->setData(COL_PLAY, Qt::UserRole, 1); // Sort key for viewed
			} else {
				fileItem->setText(COL_PLAY, "▶"); // Play button
				fileItem->setData(COL_PLAY, Qt::UserRole, 0); // Sort key for not viewed
			}
		} else {
			fileItem->setText(COL_PLAY, ""); // No button for non-video files
			fileItem->setData(COL_PLAY, Qt::UserRole, 2); // Sort key for unavailable
		}
		
		fileItem->setData(0, Qt::UserRole, fid);
		fileItem->setData(0, Qt::UserRole + 1, lid);
		
		totalFiles++;
	}
	
	// Second pass: Calculate viewed counts per episode (more efficient than checking siblings for each file)
	QSet<QPair<int, int>> viewedEpisodes; // Track which episodes have at least one viewed file
	for(QMap<QPair<int, int>, QTreeWidgetItem*>::const_iterator it = episodeItems.constBegin(); it != episodeItems.constEnd(); ++it)
	{
		QPair<int, int> episodeKey = it.key();
		int aid = episodeKey.first;
		int eid = episodeKey.second;
		EpisodeTreeWidgetItem *episodeItem = static_cast<EpisodeTreeWidgetItem*>(it.value());
		
		if(!episodeItem)
			continue;
		
		// Check if any file for this episode is viewed
		bool episodeViewed = false;
		bool hasVideoFile = false;
		bool hasAvailableFile = false; // At least one file exists
		qint64 mostRecentPlayed = 0; // Track most recent playback for this episode
		
		for(int i = 0; i < episodeItem->childCount(); i++)
		{
			QTreeWidgetItem *fileItem = episodeItem->child(i);
			FileTreeWidgetItem *file = dynamic_cast<FileTreeWidgetItem*>(fileItem);
			if(file && file->getFileType() == FileTreeWidgetItem::Video)
			{
				hasVideoFile = true;
				// Check if file is available (not marked with X)
				if(fileItem->text(COL_PLAY) != "✗")
				{
					hasAvailableFile = true;
				}
				if(fileItem->text(COL_VIEWED) == "Yes")
				{
					episodeViewed = true;
				}
				
				// Track most recent playback
				qint64 filePlayed = fileItem->data(COL_LAST_PLAYED, Qt::UserRole).toLongLong();
				if(filePlayed > mostRecentPlayed)
				{
					mostRecentPlayed = filePlayed;
				}
			}
		}
		
		// Set last played for episode (most recent file)
		if(mostRecentPlayed > 0)
		{
			QDateTime lastPlayedTime = QDateTime::fromSecsSinceEpoch(mostRecentPlayed);
			episodeItem->setText(COL_LAST_PLAYED, lastPlayedTime.toString("yyyy-MM-dd hh:mm"));
			episodeItem->setData(COL_LAST_PLAYED, Qt::UserRole, mostRecentPlayed);
		}
		else
		{
			episodeItem->setText(COL_LAST_PLAYED, "Never");
			episodeItem->setData(COL_LAST_PLAYED, Qt::UserRole, 0);
		}
		
		// Set play button for episode
		if(hasVideoFile)
		{
			if(!hasAvailableFile)
			{
				episodeItem->setText(COL_PLAY, "✗"); // X if no files exist
				episodeItem->setForeground(COL_PLAY, QBrush(QColor(Qt::red)));
				episodeItem->setData(COL_PLAY, Qt::UserRole, 2); // Sort key for unavailable
			}
			else if(episodeViewed)
			{
				episodeItem->setText(COL_PLAY, "✓"); // Checkmark for watched
				episodeItem->setData(COL_PLAY, Qt::UserRole, 1); // Sort key for viewed
			}
			else
			{
				episodeItem->setText(COL_PLAY, "▶"); // Play button
				episodeItem->setData(COL_PLAY, Qt::UserRole, 0); // Sort key for not viewed
			}
		}
		else
		{
			episodeItem->setText(COL_PLAY, ""); // No button if no video file
			episodeItem->setData(COL_PLAY, Qt::UserRole, 2); // Sort key for unavailable
		}
		
		if(episodeViewed && !viewedEpisodes.contains(episodeKey))
		{
			viewedEpisodes.insert(episodeKey);
			animeViewedCount[aid]++;
			
			// Get episode type safely
			int episodeType = 1; // Default to normal
			if(episodeItem->getEpno().isValid())
			{
				episodeType = episodeItem->getEpno().type();
			}
			
			if(episodeType == 1)
			{
				animeNormalViewedCount[aid]++;
			}
			else
			{
				animeOtherViewedCount[aid]++;
			}
		}
	}
	
	// Insert placeholder rows for missing episodes
	for(QMap<int, QTreeWidgetItem*>::const_iterator it = animeItems.constBegin(); it != animeItems.constEnd(); ++it)
	{
		int aid = it.key();
		QTreeWidgetItem *animeItem = it.value();
		insertMissingEpisodePlaceholders(aid, animeItem, episodeItems);
	}
	
	// Update anime rows with aggregate statistics
	for(QMap<int, QTreeWidgetItem*>::const_iterator it = animeItems.constBegin(); it != animeItems.constEnd(); ++it)
	{
		int aid = it.key();
		QTreeWidgetItem *animeItem = it.value();
		
		int totalNormalEpisodes = animeEps[aid];
		int normalEpisodes = animeNormalEpisodeCount[aid];
		int otherEpisodes = animeOtherEpisodeCount[aid];
		int normalViewed = animeNormalViewedCount[aid];
		int otherViewed = animeOtherViewedCount[aid];
		
		// Column 1 (Episode): show format "A/B+C"
		QString episodeText;
		if(totalNormalEpisodes > 0)
		{
			if(otherEpisodes > 0)
			{
				episodeText = QString("%1/%2+%3").arg(normalEpisodes).arg(totalNormalEpisodes).arg(otherEpisodes);
			}
			else
			{
				episodeText = QString("%1/%2").arg(normalEpisodes).arg(totalNormalEpisodes);
			}
		}
		else
		{
			if(otherEpisodes > 0)
			{
				episodeText = QString("%1/?+%2").arg(normalEpisodes).arg(otherEpisodes);
			}
			else
			{
				episodeText = QString("%1/?").arg(normalEpisodes);
			}
		}
		animeItem->setText(2, episodeText);
		
		// Column 4 (Viewed): show format "A/B+C"
		QString viewedText;
		if(otherEpisodes > 0)
		{
			viewedText = QString("%1/%2+%3").arg(normalViewed).arg(normalEpisodes).arg(otherViewed);
		}
		else
		{
			viewedText = QString("%1/%2").arg(normalViewed).arg(normalEpisodes);
		}
		animeItem->setText(5, viewedText);
		
		// Column 1 (Play button): Check if any episodes have available files
		bool allNormalWatched = (totalNormalEpisodes > 0 && normalViewed >= totalNormalEpisodes);
		bool hasAnyAvailableFile = false;
		qint64 mostRecentPlayed = 0; // Track most recent playback for this anime
		
		// Check all episodes for this anime to see if any have available files and track last played
		for(int i = 0; i < animeItem->childCount(); i++)
		{
			QTreeWidgetItem *episodeItem = animeItem->child(i);
			if(episodeItem && episodeItem->text(COL_PLAY) != "" && episodeItem->text(COL_PLAY) != "✗")
			{
				hasAnyAvailableFile = true;
			}
			
			// Track most recent playback from episodes
			qint64 episodePlayed = episodeItem->data(COL_LAST_PLAYED, Qt::UserRole).toLongLong();
			if(episodePlayed > mostRecentPlayed)
			{
				mostRecentPlayed = episodePlayed;
			}
		}
		
		// Set last played for anime (most recent episode)
		if(mostRecentPlayed > 0)
		{
			QDateTime lastPlayedTime = QDateTime::fromSecsSinceEpoch(mostRecentPlayed);
			animeItem->setText(COL_LAST_PLAYED, lastPlayedTime.toString("yyyy-MM-dd hh:mm"));
			animeItem->setData(COL_LAST_PLAYED, Qt::UserRole, mostRecentPlayed);
		}
		else
		{
			animeItem->setText(COL_LAST_PLAYED, "Never");
			animeItem->setData(COL_LAST_PLAYED, Qt::UserRole, 0);
		}
		
		if(!hasAnyAvailableFile && (normalEpisodes > 0 || otherEpisodes > 0))
		{
			animeItem->setText(COL_PLAY, "✗"); // X if no files exist for any episode
			animeItem->setForeground(COL_PLAY, QBrush(QColor(Qt::red)));
			animeItem->setData(COL_PLAY, Qt::UserRole, 2); // Sort key for unavailable
		}
		else if(allNormalWatched)
		{
			animeItem->setText(COL_PLAY, "✓"); // Checkmark for fully watched
			animeItem->setData(COL_PLAY, Qt::UserRole, 1); // Sort key for viewed
		}
		else if(normalEpisodes > 0 || otherEpisodes > 0)
		{
			animeItem->setText(COL_PLAY, "▶"); // Play button
			animeItem->setData(COL_PLAY, Qt::UserRole, 0); // Sort key for not viewed
		}
		else
		{
			animeItem->setText(COL_PLAY, ""); // No button if no episodes
			animeItem->setData(COL_PLAY, Qt::UserRole, 2); // Sort key for unavailable
		}
	}
	
	LOG(QString("Loaded %1 files for %2 episodes in %3 anime").arg(totalFiles).arg(episodeItems.size()).arg(animeItems.size()));
	mylistStatusLabel->setText(QString("MyList Status: %1 files loaded").arg(totalFiles));
}

int Window::parseMylistExport(const QString &tarGzPath)
{
	// Parse xml-plain-cs format mylist export (tar.gz containing XML file with proper lid values)
	int count = 0;
	QSqlDatabase db = QSqlDatabase::database();
	
	// Validate database connection
	if(!validateDatabaseConnection(db, "parseMylistExport"))
	{
		return 0;
	}
	
	// Extract tar.gz to temporary directory
	QString tempDir = QDir::tempPath() + "/usagi_mylist_" + QString::number(QDateTime::currentMSecsSinceEpoch());
	QDir().mkpath(tempDir);
	
	// Use QProcess to extract tar.gz
	QProcess tarProcess;
	tarProcess.setWorkingDirectory(tempDir);
	tarProcess.start("tar", QStringList() << "-xzf" << tarGzPath);
	
	if(!tarProcess.waitForFinished(30000))  // 30 second timeout
	{
		LOG("Error: Failed to extract tar.gz file (timeout)");
		QDir(tempDir).removeRecursively();
		return 0;
	}
	
	if(tarProcess.exitCode() != 0)
	{
		LOG("Error: Failed to extract tar.gz file: " + tarProcess.readAllStandardError());
		QDir(tempDir).removeRecursively();
		return 0;
	}
	
	// Find XML file in extracted directory
	QDir extractedDir(tempDir);
	QStringList xmlFiles = extractedDir.entryList(QStringList() << "*.xml", QDir::Files);
	
	if(xmlFiles.isEmpty())
	{
		LOG("Error: No XML file found in tar.gz");
		QDir(tempDir).removeRecursively();
		return 0;
	}
	
	// Parse the XML file
	QString xmlFilePath = extractedDir.absoluteFilePath(xmlFiles.first());
	QFile xmlFile(xmlFilePath);
	
	if(!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		LOG("Error: Cannot open XML file");
		QDir(tempDir).removeRecursively();
		return 0;
	}
	
	QXmlStreamReader xml(&xmlFile);
	db.transaction();
	
	// Parse XML structure: <MyList><Anime><Ep><File LId="..." Id="..."/></Ep></Anime></MyList>
	// LId is the mylist ID (lid), Id is the file ID (fid)
	QString currentAid;
	QString currentEid;
	QString currentEpNo;
	QString currentEpName;
	
	while(!xml.atEnd() && !xml.hasError())
	{
		QXmlStreamReader::TokenType token = xml.readNext();
		
		if(token == QXmlStreamReader::StartElement)
		{
			if(xml.name() == QString("Anime"))
			{
				// Get anime ID and episode counts from Anime element
				QXmlStreamAttributes attributes = xml.attributes();
				currentAid = attributes.value("Id").toString();
				QString epsTotal = attributes.value("EpsTotal").toString();
				QString eps = attributes.value("Eps").toString();
				QString typeName = attributes.value("TypeName").toString();
				QString startDate = attributes.value("StartDate").toString();
				QString endDate = attributes.value("EndDate").toString();
				
				// Extract additional fields from mylist export
				QString epsSpecial = attributes.value("EpsSpecial").toString();
				QString url = attributes.value("Url").toString();
				QString rating = attributes.value("Rating").toString();
				QString votes = attributes.value("Votes").toString();
				QString tmpRating = attributes.value("TmpRating").toString();
				QString tmpVotes = attributes.value("TmpVotes").toString();
				QString reviewRating = attributes.value("ReviewRating").toString();
				QString reviews = attributes.value("Reviews").toString();
				QString annId = attributes.value("AnnId").toString();
				QString allCinemaId = attributes.value("AllCinemaId").toString();
				QString animeNfoId = attributes.value("AnimeNfoId").toString();
				
				// Extract year, name, and other metadata fields
				QString yearStart = attributes.value("YearStart").toString();
				QString yearEnd = attributes.value("YearEnd").toString();
				QString name = attributes.value("Name").toString();
				QString titleJapKanji = attributes.value("TitleJapKanji").toString();
				QString titleEng = attributes.value("TitleEng").toString();
				QString updateTimestamp = attributes.value("Update").toString();
				QString awardIcons = attributes.value("AwardIcons").toString();
				
				// Build year string (format: "YYYY" or "YYYY-YYYY" if different)
				QString year;
				if(!yearStart.isEmpty())
				{
					if(!yearEnd.isEmpty() && yearStart != yearEnd)
						year = yearStart + "-" + yearEnd;
					else
						year = yearStart;
				}
				
				// Ensure anime record exists in database
				// This runs for all anime, regardless of whether we have eptotal or typename data
				if(!currentAid.isEmpty())
				{
					QSqlQuery animeQueryExec(db);
					animeQueryExec.prepare("INSERT OR IGNORE INTO `anime` (`aid`) VALUES (:aid)");
					animeQueryExec.bindValue(":aid", currentAid.toInt());
					
					if(!animeQueryExec.exec())
					{
						LOG(QString("Warning: Failed to insert anime record (aid=%1): %2")
							.arg(currentAid, animeQueryExec.lastError().text()));
					}
				}
				
				// Update anime episode counts if we have valid data
				if(!currentAid.isEmpty() && !epsTotal.isEmpty())
				{
					// Update eptotal and eps only if they're currently 0 or NULL (not set by FILE command)
					// typename, startdate, enddate are handled separately below
					QSqlQuery animeQueryExec(db);
					animeQueryExec.prepare("UPDATE `anime` SET `eptotal` = :eptotal, `eps` = :eps "
						"WHERE `aid` = :aid AND ((eptotal IS NULL OR eptotal = 0) OR (eps IS NULL OR eps = 0))");
					animeQueryExec.bindValue(":eptotal", epsTotal.toInt());
					// QVariant() creates a NULL value for the database when eps is not available
					animeQueryExec.bindValue(":eps", eps.isEmpty() ? QVariant() : eps.toInt());
					animeQueryExec.bindValue(":aid", currentAid.toInt());
					
					if(!animeQueryExec.exec())
					{
						LOG(QString("Warning: Failed to update anime episode counts (aid=%1): %2")
							.arg(currentAid, animeQueryExec.lastError().text()));
					}
				}
				
				// Always update metadata fields from mylist export (including new fields)
				// This runs independently of the eptotal/eps check above
				if(!currentAid.isEmpty())
				{
					QSqlQuery animeQueryExec(db);
					// Update typename, startdate, enddate and new fields from mylist export
					animeQueryExec.prepare("UPDATE `anime` SET `typename` = :typename, "
						"`startdate` = :startdate, `enddate` = :enddate, "
						"`special_ep_count` = :special_ep_count, `url` = :url, "
						"`rating` = :rating, `vote_count` = :vote_count, "
						"`temp_rating` = :temp_rating, `temp_vote_count` = :temp_vote_count, "
						"`avg_review_rating` = :avg_review_rating, `review_count` = :review_count, "
						"`ann_id` = :ann_id, `allcinema_id` = :allcinema_id, `animenfo_id` = :animenfo_id, "
						"`year` = :year, `nameromaji` = :nameromaji, `namekanji` = :namekanji, "
						"`nameenglish` = :nameenglish, `date_record_updated` = :date_record_updated, "
						"`award_list` = :award_list "
						"WHERE `aid` = :aid");
					animeQueryExec.bindValue(":typename", typeName.isEmpty() ? QVariant() : typeName);
					animeQueryExec.bindValue(":startdate", startDate.isEmpty() ? QVariant() : startDate);
					animeQueryExec.bindValue(":enddate", endDate.isEmpty() ? QVariant() : endDate);
					animeQueryExec.bindValue(":special_ep_count", epsSpecial.isEmpty() ? QVariant() : epsSpecial.toInt());
					animeQueryExec.bindValue(":url", url.isEmpty() ? QVariant() : url);
					animeQueryExec.bindValue(":rating", rating.isEmpty() ? QVariant() : rating);
					animeQueryExec.bindValue(":vote_count", votes.isEmpty() ? QVariant() : votes.toInt());
					animeQueryExec.bindValue(":temp_rating", tmpRating.isEmpty() ? QVariant() : tmpRating);
					animeQueryExec.bindValue(":temp_vote_count", tmpVotes.isEmpty() ? QVariant() : tmpVotes.toInt());
					animeQueryExec.bindValue(":avg_review_rating", reviewRating.isEmpty() ? QVariant() : reviewRating);
					animeQueryExec.bindValue(":review_count", reviews.isEmpty() ? QVariant() : reviews.toInt());
					animeQueryExec.bindValue(":ann_id", annId.isEmpty() ? QVariant() : annId.toInt());
					animeQueryExec.bindValue(":allcinema_id", allCinemaId.isEmpty() ? QVariant() : allCinemaId.toInt());
					animeQueryExec.bindValue(":animenfo_id", animeNfoId.isEmpty() ? QVariant() : animeNfoId);
					animeQueryExec.bindValue(":year", year.isEmpty() ? QVariant() : year);
					animeQueryExec.bindValue(":nameromaji", name.isEmpty() ? QVariant() : name);
					animeQueryExec.bindValue(":namekanji", titleJapKanji.isEmpty() ? QVariant() : titleJapKanji);
					animeQueryExec.bindValue(":nameenglish", titleEng.isEmpty() ? QVariant() : titleEng);
					animeQueryExec.bindValue(":date_record_updated", updateTimestamp.isEmpty() ? QVariant() : updateTimestamp.toLongLong());
					animeQueryExec.bindValue(":award_list", awardIcons.isEmpty() ? QVariant() : awardIcons);
					animeQueryExec.bindValue(":aid", currentAid.toInt());
					
					if(!animeQueryExec.exec())
					{
						LOG(QString("Warning: Failed to update anime metadata (aid=%1): %2")
							.arg(currentAid, animeQueryExec.lastError().text()));
					}
				}
			}
			else if(xml.name() == QString("Ep"))
			{
				// Get episode ID, number, and name from Ep element
				QXmlStreamAttributes attributes = xml.attributes();
				currentEid = attributes.value("Id").toString();
				currentEpNo = attributes.value("EpNo").toString();
				currentEpName = attributes.value("Name").toString();
				QString currentEpNameRomaji = attributes.value("NameRomaji").toString();
				QString currentEpNameKanji = attributes.value("NameKanji").toString();
				
				// Store episode data in episode table if we have valid data
				if(!currentEid.isEmpty() && (!currentEpNo.isEmpty() || !currentEpName.isEmpty()))
				{
					QString epName_escaped = QString(currentEpName).replace("'", "''");
					QString epNo_escaped = QString(currentEpNo).replace("'", "''");
					QString epNameRomaji_escaped = QString(currentEpNameRomaji).replace("'", "''");
					QString epNameKanji_escaped = QString(currentEpNameKanji).replace("'", "''");
					
					QString episodeQuery = QString("INSERT OR REPLACE INTO `episode` "
						"(`eid`, `epno`, `name`, `nameromaji`, `namekanji`) "
						"VALUES (%1, '%2', '%3', '%4', '%5')")
						.arg(currentEid, epNo_escaped, epName_escaped, epNameRomaji_escaped, epNameKanji_escaped);
					
					QSqlQuery episodeQueryExec(db);
					if(!episodeQueryExec.exec(episodeQuery))
					{
						LOG(QString("Warning: Failed to insert episode data (eid=%1): %2")
							.arg(currentEid, episodeQueryExec.lastError().text()));
					}
				}
			}
			else if(xml.name() == QString("File"))
			{
				// Get file information from File element
				QXmlStreamAttributes attributes = xml.attributes();
				
				QString lid = attributes.value("LId").toString();  // MyList ID
				QString fid = attributes.value("Id").toString();   // File ID
				QString gid = attributes.value("GroupId").toString();
				QString storage = attributes.value("Storage").toString();
				QString viewdate = attributes.value("ViewDate").toString();
				QString myState = attributes.value("MyState").toString();
				
				// Validate required fields
				if(lid.isEmpty() || currentAid.isEmpty())
					continue;
				
				// Determine viewed status from viewdate
				QString viewed = (!viewdate.isEmpty() && viewdate != "0") ? "1" : "0";
				
				// Escape single quotes in storage
				QString storage_escaped = QString(storage).replace("'", "''");
				
				// Insert into database with proper lid value, preserving local_file if it exists
				QString q = QString("INSERT OR REPLACE INTO `mylist` "
					"(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`, `local_file`, `playback_position`, `playback_duration`, `last_played`) "
					"VALUES (%1, %2, %3, %4, %5, %6, %7, '%8', "
					"(SELECT `local_file` FROM `mylist` WHERE `lid` = %1), "
					"COALESCE((SELECT `playback_position` FROM `mylist` WHERE `lid` = %1), 0), "
					"COALESCE((SELECT `playback_duration` FROM `mylist` WHERE `lid` = %1), 0), "
					"COALESCE((SELECT `last_played` FROM `mylist` WHERE `lid` = %1), 0))")
					.arg(lid, fid.isEmpty() ? "0" : fid, currentEid.isEmpty() ? "0" : currentEid, currentAid, gid.isEmpty() ? "0" : gid, myState.isEmpty() ? "0" : myState, viewed, storage_escaped);
				
				QSqlQuery query(db);
				if(query.exec(q))
				{
					count++;
				}
				else
				{
					LOG(QString("Error inserting mylist entry (lid=%1): %2").arg(lid, query.lastError().text()));
				}
			}
		}
	}
	
	if(xml.hasError())
	{
		LOG(QString("XML parsing error: %1").arg(xml.errorString()));
	}
	
	xmlFile.close();
	db.commit();
	
	// Clean up temporary directory
	QDir(tempDir).removeRecursively();
	
	return count;
}

bool Window::isMylistFirstRunComplete()
{
	// Check if mylist first run has been completed
	QSqlDatabase db = QSqlDatabase::database();
	
	// Validate database connection
	if(!validateDatabaseConnection(db, "isMylistFirstRunComplete"))
	{
		return false;  // Default to false if database is not available
	}
	
	QSqlQuery query(db);
	query.exec("SELECT `value` FROM `settings` WHERE `name` = 'mylist_first_run_complete'");
	
	if(query.next())
	{
		return query.value(0).toString() == "1";
	}
	
	return false;  // Default: first run not complete
}

void Window::setMylistFirstRunComplete()
{
	// Mark mylist first run as complete
	QSqlDatabase db = QSqlDatabase::database();
	
	// Validate database connection
	if(!validateDatabaseConnection(db, "setMylistFirstRunComplete"))
	{
		return;
	}
	
	QSqlQuery query(db);
	QString q = QString("INSERT OR REPLACE INTO `settings` VALUES (NULL, 'mylist_first_run_complete', '1')");
	query.exec(q);
	LOG("MyList first run marked as complete");
}

void Window::requestMylistExportManually()
{
	// Manual mylist export request from Settings
	LOG("Manually requesting MyList export...");
	mylistStatusLabel->setText("MyList Status: Requesting export...");
	adbapi->MylistExport("xml-plain-cs");
}

void Window::onWatcherEnabledChanged(int state)
{
	if (state == Qt::Checked) {
		QString dir = watcherDirectory->text();
		if (!dir.isEmpty() && QDir(dir).exists()) {
			directoryWatcher->startWatching(dir);
			watcherStatusLabel->setText("Status: Watching " + dir);
			LOG("Directory watcher started: " + dir);
		} else if (dir.isEmpty()) {
			watcherStatusLabel->setText("Status: Enabled (no directory set)");
			LOG("Directory watcher enabled but no directory specified");
		} else {
			watcherStatusLabel->setText("Status: Enabled (invalid directory)");
			LOG("Directory watcher enabled but directory is invalid: " + dir);
		}
	} else {
		directoryWatcher->stopWatching();
		watcherStatusLabel->setText("Status: Not watching");
		LOG("Directory watcher stopped");
	}
}

void Window::onWatcherBrowseClicked()
{
	QString dir = QFileDialog::getExistingDirectory(this, "Select Directory to Watch",
		watcherDirectory->text().isEmpty() ? QDir::homePath() : watcherDirectory->text());
	
	if (!dir.isEmpty()) {
		watcherDirectory->setText(dir);
		
		// If watcher is enabled, restart with new directory
		if (watcherEnabled->isChecked()) {
			directoryWatcher->startWatching(dir);
			watcherStatusLabel->setText("Status: Watching " + dir);
			LOG("Directory watcher changed to: " + dir);
		}
	}
}

void Window::onWatcherNewFilesDetected(const QStringList &filePaths)
{
	LOG(QString("Window::onWatcherNewFilesDetected() called with %1 file(s)").arg(filePaths.size()));
	
	if (filePaths.isEmpty()) {
		return;
	}
	
	// Start overall timing
	QElapsedTimer overallTimer;
	overallTimer.start();
	
	// Log the detection
	LOG(QString("Detected %1 new file(s)").arg(filePaths.size()));
	qint64 logTime = overallTimer.elapsed();
	LOG(QString("[TIMING] Initial log: %1 ms [window.cpp]").arg(logTime));
	
	// Perform single batch query to retrieve all existing hashes and status
	QElapsedTimer batchQueryTimer;
	batchQueryTimer.start();
	QMap<QString, AniDBApi::FileHashInfo> hashInfoMap = adbapi->batchGetLocalFileHashes(filePaths);
	qint64 batchQueryTime = batchQueryTimer.elapsed();
	LOG(QString("[TIMING] batchGetLocalFileHashes() for %1 files: %2 ms [window.cpp]")
		.arg(filePaths.size()).arg(batchQueryTime));
	
	// Add all files to hasher table with pre-loaded hash data
	// Disable table updates during bulk insertion for performance
	hashes->setUpdatesEnabled(false);
	
	QElapsedTimer insertTimer;
	insertTimer.start();
	for (const QString &filePath : filePaths) {
		QFileInfo fileInfo(filePath);
		QString preloadedHash = hashInfoMap.contains(filePath) ? hashInfoMap[filePath].hash : QString();
		hashesinsertrow(fileInfo, Qt::Unchecked, preloadedHash);
	}
	
	// Re-enable table updates after bulk insertion
	hashes->setUpdatesEnabled(true);
	
	qint64 insertTime = insertTimer.elapsed();
	LOG(QString("[TIMING] hashesinsertrow() loop for %1 files: %2 ms [window.cpp]")
		.arg(filePaths.size()).arg(insertTime));
	
	// Directory watcher always auto-starts the hasher for detected files
	if (!hasherThreadPool->isRunning()) {
		// Set settings for auto-hash if logged in
		if (adbapi->LoggedIn()) {
			addtomylist->setChecked(true);
			markwatched->setCheckState(Qt::Unchecked);  // no change (tristate: Unchecked=no change, PartiallyChecked=unwatched, Checked=watched)
			hasherFileState->setCurrentIndex(1);  // Internal (HDD)
		}
		
		// Separate files with existing hashes from those that need hashing
		// Check ALL files with progress="0" or "1", not just newly added ones
		int filesToHashCount = 0;
		QList<QPair<int, QString>> filesWithHashes; // row index, file path
		
		for (int i = 0; i < hashes->rowCount(); i++) {
			QString progress = hashes->item(i, 1)->text();
			QString filePath = hashes->item(i, 2)->text();
			QString existingHash = hashes->item(i, 9)->text();
			
			// Process files with progress="0" or progress="1" (already hashed but not yet API-processed)
			if (progress == "0" || progress == "1") {
				// Check if file has pending API calls (tags in columns 5 or 6)
				// Tags are "?" initially, set to actual tag when API call is queued, and "0" when completed/not needed
				QString fileTag = hashes->item(i, 5)->text();
				QString mylistTag = hashes->item(i, 6)->text();
				bool hasPendingAPICalls = (!fileTag.isEmpty() && fileTag != "?" && fileTag != "0") || 
				                          (!mylistTag.isEmpty() && mylistTag != "?" && mylistTag != "0");
				
				if (!existingHash.isEmpty()) {
					// Skip files with pending API calls to avoid duplicate processing
					if (!hasPendingAPICalls) {
						filesWithHashes.append(qMakePair(i, filePath));
					}
				} else {
					// File needs to be hashed
					// Note: If progress="1" but no hash, this is an inconsistent state
					if (progress == "1") {
						LOG(QString("Warning: File at row %1 has progress=1 but no hash - inconsistent state").arg(i));
					}
					filesToHashCount++;
				}
			}
		}
		
		// Queue files with existing hashes for deferred processing to prevent UI freeze
		// Instead of processing them all synchronously (which blocks the UI), we queue them
		// and process in small batches using a timer
		for (const auto& pair : filesWithHashes) {
			int rowIndex = pair.first;
			QString filePath = pair.second;
			QString filename = hashes->item(rowIndex, 0)->text();
			QString hexdigest = hashes->item(rowIndex, 9)->text();
			
			// Get file size
			QFileInfo fileInfo(filePath);
			qint64 fileSize = fileInfo.size();
			
			// Queue for deferred processing
			HashedFileInfo info;
			info.rowIndex = rowIndex;
			info.filePath = filePath;
			info.filename = filename;
			info.hexdigest = hexdigest;
			info.fileSize = fileSize;
			info.useUserSettings = false;  // Use auto-watcher defaults
			info.addToMylist = true;  // Auto-watcher always adds to mylist when logged in
			info.markWatchedState = Qt::Unchecked;  // Default for auto-watcher
			info.fileState = 1;  // Internal (HDD)
			pendingHashedFilesQueue.append(info);
		}
		
		// Start timer to process queued files in batches (keeps UI responsive)
		if (!filesWithHashes.isEmpty()) {
			LOG(QString("Queued %1 already-hashed file(s) for deferred processing").arg(filesWithHashes.size()));
			hashedFilesProcessingTimer->start();
		}
		
		// Start hashing for files without existing hashes
		if (filesToHashCount > 0) {
			// Calculate total hash parts for progress tracking
			QStringList filesToHash = getFilesNeedingHash();
			setupHashingProgress(filesToHash);
			
			// Start hashing all detected files that need hashing
			buttonstart->setEnabled(false);
			buttonclear->setEnabled(false);
			hasherThreadPool->start();
			
			if (adbapi->LoggedIn()) {
				LOG(QString("Auto-hashing %1 file(s) - will be added to MyList as HDD unwatched").arg(filesToHashCount));
			} else {
				LOG(QString("Auto-hashing %1 file(s) - login to add to MyList").arg(filesToHashCount));
			}
		}
	} else {
		LOG("Files added to hasher. Hasher is busy - click Start to hash queued files.");
	}
	
	// Log total time for the entire function
	qint64 totalTime = overallTimer.elapsed();
	LOG(QString("[TIMING] onWatcherNewFilesDetected() TOTAL: %1 ms [window.cpp]").arg(totalTime));
}

// Stub method for selecting preferred file from a list of files for the same episode
// This will be used for future playback feature
// Preference options: video resolution, group name, quality
FileTreeWidgetItem* Window::selectPreferredFile(const QList<FileTreeWidgetItem*>& files, const FilePreference& pref)
{
	if(files.isEmpty())
	{
		return nullptr;
	}
	
	// If only one file, return it
	if(files.size() == 1)
	{
		return files.first();
	}
	
	// TODO: Implement full preference-based selection logic:
	// 1. Filter by FilePreference::preferredResolution (exact match or closest)
	// 2. Filter by FilePreference::preferredGroup (case-insensitive substring match)
	// 3. Apply FilePreference::preferHigherQuality to select highest/lowest quality
	// 4. Consider file state (prefer HDD over CD/DVD, avoid Deleted)
	// 5. Rank by multiple criteria with weighted scoring system
	// For now, return the first video file with basic preference matching
	
	FileTreeWidgetItem* bestFile = nullptr;
	
	// First pass: try to find a video file
	for(FileTreeWidgetItem* file : files)
	{
		if(file->getFileType() == FileTreeWidgetItem::Video)
		{
			// Basic preference matching (stub implementation)
			if(!bestFile)
			{
				bestFile = file;
			}
			
			// Prefer matching resolution if specified
			if(!pref.preferredResolution.isEmpty() && 
			   file->getResolution() == pref.preferredResolution)
			{
				bestFile = file;
				break;
			}
			
			// Prefer matching group if specified
			if(!pref.preferredGroup.isEmpty() && 
			   file->getGroupName().contains(pref.preferredGroup, Qt::CaseInsensitive))
			{
				bestFile = file;
				break;
			}
		}
	}
	
	// If no video file found, return first file
	if(!bestFile && !files.isEmpty())
	{
		bestFile = files.first();
	}
	
	return bestFile;
}

// Helper method to determine file type from filetype string
// Uses pattern matching to classify files into Video, Subtitle, Audio, or Other
FileTreeWidgetItem::FileType Window::determineFileType(const QString& filetype)
{
	// Common subtitle file extensions and patterns
	static const QStringList subtitlePatterns = {
		"sub", "srt", "ass", "ssa", "vtt", "idx", "smi", "sup"
	};
	
	// Common audio file extensions and patterns
	static const QStringList audioPatterns = {
		"audio", "mp3", "flac", "aac", "ogg", "wav", "m4a", "wma", "opus"
	};
	
	// Common video file extensions and patterns
	static const QStringList videoPatterns = {
		"video", "mkv", "mp4", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg"
	};
	
	QString lowerFiletype = filetype.toLower();
	
	// Check subtitle patterns
	for(const QString& pattern : subtitlePatterns)
	{
		if(lowerFiletype.contains(pattern))
		{
			return FileTreeWidgetItem::Subtitle;
		}
	}
	
	// Check audio patterns
	for(const QString& pattern : audioPatterns)
	{
		if(lowerFiletype.contains(pattern))
		{
			return FileTreeWidgetItem::Audio;
		}
	}
	
	// Check video patterns (or empty filetype defaults to video)
	for(const QString& pattern : videoPatterns)
	{
		if(lowerFiletype.contains(pattern))
		{
			return FileTreeWidgetItem::Video;
		}
	}
	
	// Default to video for empty filetype
	if(filetype.isEmpty())
	{
		return FileTreeWidgetItem::Video;
	}
	
	// Everything else is Other
	return FileTreeWidgetItem::Other;
}

// Playback slot implementations

void Window::onMediaPlayerBrowseClicked()
{
	QString path = QFileDialog::getOpenFileName(this, "Select Media Player", 
		mediaPlayerPath->text(), "Executable Files (*.exe);;All Files (*)");
	
	if (!path.isEmpty()) {
		mediaPlayerPath->setText(path);
		PlaybackManager::setMediaPlayerPath(path);
		LOG(QString("Media player path set to: %1").arg(path));
	}
}

void Window::onPlayButtonClicked(const QModelIndex &index)
{
	if (!index.isValid()) {
		return;
	}
	
	// Get the item from the model index
	QTreeWidgetItem *item = mylistTreeWidget->itemFromIndex(index);
	if (!item) {
		return;
	}
	
	// Determine what type of item was clicked
	// Check if this is a file item (has a parent that has a parent)
	QTreeWidgetItem *parent = item->parent();
	if (parent && parent->parent()) {
		// This is a file item
		int lid = item->text(MYLIST_ID_COLUMN).toInt();
		startPlaybackForFile(lid);
	}
	// Check if this is an episode item (has a parent but no grandparent)
	else if (parent && !parent->parent()) {
		// This is an episode item - find the first video file and play it
		int childCount = item->childCount();
		for (int i = 0; i < childCount; i++) {
			QTreeWidgetItem *fileItem = item->child(i);
			FileTreeWidgetItem *file = dynamic_cast<FileTreeWidgetItem*>(fileItem);
			if (file && file->getFileType() == FileTreeWidgetItem::Video) {
				int lid = fileItem->text(MYLIST_ID_COLUMN).toInt();
				if (lid > 0) {
					startPlaybackForFile(lid);
					return;
				}
			}
		}
		LOG("Cannot play: no video file found for episode");
	}
	// Otherwise it's an anime item - find the first unwatched episode with a video file
	else if (!parent) {
		// This is an anime item
		int childCount = item->childCount();
		
		// First pass: find first unwatched episode
		for (int i = 0; i < childCount; i++) {
			QTreeWidgetItem *episodeItem = item->child(i);
			
			// Check if episode is watched (has checkmark)
			QString playIcon = episodeItem->text(PLAY_COLUMN);
			if (playIcon == "✓") {
				continue; // Skip watched episodes
			}
			
			// Find a video file in this unwatched episode
			int fileCount = episodeItem->childCount();
			for (int j = 0; j < fileCount; j++) {
				QTreeWidgetItem *fileItem = episodeItem->child(j);
				FileTreeWidgetItem *file = dynamic_cast<FileTreeWidgetItem*>(fileItem);
				if (file && file->getFileType() == FileTreeWidgetItem::Video) {
					int lid = fileItem->text(MYLIST_ID_COLUMN).toInt();
					if (lid > 0) {
						startPlaybackForFile(lid);
						return;
					}
				}
			}
		}
		
		// If no unwatched episodes found, fall back to first episode (for rewatching)
		for (int i = 0; i < childCount; i++) {
			QTreeWidgetItem *episodeItem = item->child(i);
			int fileCount = episodeItem->childCount();
			for (int j = 0; j < fileCount; j++) {
				QTreeWidgetItem *fileItem = episodeItem->child(j);
				FileTreeWidgetItem *file = dynamic_cast<FileTreeWidgetItem*>(fileItem);
				if (file && file->getFileType() == FileTreeWidgetItem::Video) {
					int lid = fileItem->text(MYLIST_ID_COLUMN).toInt();
					if (lid > 0) {
						startPlaybackForFile(lid);
						return;
					}
				}
			}
		}
		LOG("Cannot play: no video file found for anime");
	}
}

void Window::onPlaybackPositionUpdated(int lid, int position, int duration)
{
	LOG(QString("Playback position updated: LID %1, %2/%3s").arg(lid).arg(position).arg(duration));
	// The position is already saved by PlaybackManager, no need to do anything here
}

void Window::updatePlayButtonForItem(QTreeWidgetItem *item)
{
	if (!item) return;
	
	// Determine item type and update button accordingly
	QTreeWidgetItem *parent = item->parent();
	
	if (parent && parent->parent()) {
		// This is a file item
		FileTreeWidgetItem *file = dynamic_cast<FileTreeWidgetItem*>(item);
		if (file && file->getFileType() == FileTreeWidgetItem::Video) {
			bool viewed = (item->text(COL_VIEWED) == "Yes");
			// Check if file doesn't exist
			QString currentText = item->text(PLAY_COLUMN);
			if (currentText == "✗") {
				// Keep the X if file doesn't exist (sort key already set to 2)
				return;
			}
			item->setText(PLAY_COLUMN, viewed ? "✓" : "▶");
			item->setData(PLAY_COLUMN, Qt::UserRole, viewed ? 1 : 0); // Update sort key
		}
	}
	else if (parent && !parent->parent()) {
		// This is an episode item
		bool episodeViewed = false;
		bool hasVideoFile = false;
		for (int i = 0; i < item->childCount(); i++) {
			QTreeWidgetItem *fileItem = item->child(i);
			FileTreeWidgetItem *file = dynamic_cast<FileTreeWidgetItem*>(fileItem);
			if (file && file->getFileType() == FileTreeWidgetItem::Video) {
				hasVideoFile = true;
				if (fileItem->text(COL_VIEWED) == "Yes") {
					episodeViewed = true;
					break;
				}
			}
		}
		
		if (hasVideoFile) {
			item->setText(PLAY_COLUMN, episodeViewed ? "✓" : "▶");
			item->setData(PLAY_COLUMN, Qt::UserRole, episodeViewed ? 1 : 0); // Update sort key
		} else {
			item->setText(PLAY_COLUMN, "");
			item->setData(PLAY_COLUMN, Qt::UserRole, 2); // Update sort key (unavailable)
		}
		
		// Also update parent anime
		if (parent) {
			updatePlayButtonForItem(parent);
		}
	}
	else if (!parent) {
		// This is an anime item
		int totalEpisodes = 0;
		int viewedEpisodes = 0;
		
		for (int i = 0; i < item->childCount(); i++) {
			QTreeWidgetItem *episodeItem = item->child(i);
			bool episodeViewed = false;
			bool hasVideoFile = false;
			
			for (int j = 0; j < episodeItem->childCount(); j++) {
				QTreeWidgetItem *fileItem = episodeItem->child(j);
				FileTreeWidgetItem *file = dynamic_cast<FileTreeWidgetItem*>(fileItem);
				if (file && file->getFileType() == FileTreeWidgetItem::Video) {
					hasVideoFile = true;
					if (fileItem->text(COL_VIEWED) == "Yes") {
						episodeViewed = true;
						break;
					}
				}
			}
			
			if (hasVideoFile) {
				totalEpisodes++;
				if (episodeViewed) {
					viewedEpisodes++;
				}
			}
		}
		
		if (totalEpisodes > 0) {
			if (viewedEpisodes == totalEpisodes) {
				item->setText(PLAY_COLUMN, "✓");
				item->setData(PLAY_COLUMN, Qt::UserRole, 1); // Update sort key
			} else {
				item->setText(PLAY_COLUMN, "▶");
				item->setData(PLAY_COLUMN, Qt::UserRole, 0); // Update sort key
			}
		} else {
			item->setText(PLAY_COLUMN, "");
			item->setData(PLAY_COLUMN, Qt::UserRole, 2); // Update sort key (unavailable)
		}
	}
}

void Window::updatePlayButtonsInTree(QTreeWidgetItem *rootItem)
{
	if (!rootItem) {
		// Update all items in tree
		int topLevelCount = mylistTreeWidget->topLevelItemCount();
		for (int i = 0; i < topLevelCount; i++) {
			updatePlayButtonsInTree(mylistTreeWidget->topLevelItem(i));
		}
		return;
	}
	
	// Update this item
	updatePlayButtonForItem(rootItem);
	
	// Recursively update children
	for (int i = 0; i < rootItem->childCount(); i++) {
		updatePlayButtonsInTree(rootItem->child(i));
	}
}

void Window::onPlaybackCompleted(int lid)
{
	LOG(QString("Playback completed: LID %1 - Updating play buttons").arg(lid));
	
	// Remove from playing items
	m_playingItems.remove(lid);
	if (m_playingItems.isEmpty()) {
		m_animationTimer->stop();
	}
	
	// Find the file item with this LID and update its play button and parents
	QTreeWidgetItemIterator it(mylistTreeWidget);
	while (*it) {
		QTreeWidgetItem *item = *it;
		if (item->text(MYLIST_ID_COLUMN).toInt() == lid) {
			// Found the file - update it and its parents
			updatePlayButtonForItem(item);
			
			// Update episode parent if exists
			QTreeWidgetItem *episodeItem = item->parent();
			if (episodeItem) {
				updatePlayButtonForItem(episodeItem);
				
				// Update anime parent if exists
				QTreeWidgetItem *animeItem = episodeItem->parent();
				if (animeItem) {
					updatePlayButtonForItem(animeItem);
				}
			}
			break;
		}
		++it;
	}
}

void Window::onPlaybackStopped(int lid, int position)
{
	LOG(QString("Playback stopped: LID %1 at position %2s").arg(lid).arg(position));
	
	// Remove from playing items
	m_playingItems.remove(lid);
	if (m_playingItems.isEmpty()) {
		m_animationTimer->stop();
	}
}

void Window::onPlaybackStateChanged(int lid, bool isPlaying)
{
	if (isPlaying) {
		// Add to playing items with initial animation frame
		m_playingItems[lid] = 0;
		if (!m_animationTimer->isActive()) {
			m_animationTimer->start();
		}
		
		// Set initial animation frame immediately for file and parent items
		QTreeWidgetItemIterator it(mylistTreeWidget);
		while (*it) {
			QTreeWidgetItem *item = *it;
			if (item->data(0, Qt::UserRole + 1).toInt() == lid) {
				item->setText(COL_PLAY, "▶");
				
				// Also set initial frame for parent episode
				QTreeWidgetItem *episodeItem = item->parent();
				if (episodeItem) {
					if (!episodeItem->text(COL_PLAY).isEmpty() && episodeItem->text(COL_PLAY) != "✗") {
						episodeItem->setText(COL_PLAY, "▶");
					}
					
					// Also set initial frame for parent anime
					QTreeWidgetItem *animeItem = episodeItem->parent();
					if (animeItem) {
						if (!animeItem->text(COL_PLAY).isEmpty() && animeItem->text(COL_PLAY) != "✗") {
							animeItem->setText(COL_PLAY, "▶");
						}
					}
				}
				break;
			}
			++it;
		}
	} else {
		// Remove from playing items
		m_playingItems.remove(lid);
		if (m_playingItems.isEmpty()) {
			m_animationTimer->stop();
		}
		
		// Update the play button for this item and its parents
		QTreeWidgetItemIterator it(mylistTreeWidget);
		while (*it) {
			QTreeWidgetItem *item = *it;
			if (item->text(MYLIST_ID_COLUMN).toInt() == lid) {
				updatePlayButtonForItem(item);
				
				// Update parent episode
				QTreeWidgetItem *episodeItem = item->parent();
				if (episodeItem) {
					updatePlayButtonForItem(episodeItem);
					
					// Update parent anime
					QTreeWidgetItem *animeItem = episodeItem->parent();
					if (animeItem) {
						updatePlayButtonForItem(animeItem);
					}
				}
				break;
			}
			++it;
		}
	}
}

void Window::onAnimationTimerTimeout()
{
	// Temporarily disable sorting to prevent items from moving during animation
	bool sortingWasEnabled = mylistTreeWidget->isSortingEnabled();
	if (sortingWasEnabled) {
		mylistTreeWidget->setSortingEnabled(false);
	}
	
	// Update animation frames for all playing items and update their display
	for (auto it = m_playingItems.begin(); it != m_playingItems.end(); ++it) {
		int lid = it.key();
		int frame = (it.value() + 1) % 3;  // Cycle through 0, 1, 2
		it.value() = frame;
		
		// Animation frames: ▶ ▷ ▸ (different arrow styles)
		QString animationFrames[] = {"▶", "▷", "▸"};
		
		// Find the file item and update its play button with animation
		QTreeWidgetItemIterator itemIt(mylistTreeWidget);
		while (*itemIt) {
			QTreeWidgetItem *item = *itemIt;
			if (item->data(0, Qt::UserRole + 1).toInt() == lid) {
				// Update file item
				item->setText(COL_PLAY, animationFrames[frame]);
				
				// Also update parent episode item if it exists
				QTreeWidgetItem *episodeItem = item->parent();
				if (episodeItem) {
					// Only animate if the episode has a play button
					if (!episodeItem->text(COL_PLAY).isEmpty() && episodeItem->text(COL_PLAY) != "✗") {
						episodeItem->setText(COL_PLAY, animationFrames[frame]);
					}
					
					// Also update parent anime item if it exists
					QTreeWidgetItem *animeItem = episodeItem->parent();
					if (animeItem) {
						// Only animate if the anime has a play button
						if (!animeItem->text(COL_PLAY).isEmpty() && animeItem->text(COL_PLAY) != "✗") {
							animeItem->setText(COL_PLAY, animationFrames[frame]);
						}
					}
				}
				break;
			}
			++itemIt;
		}
	}
	
	// Re-enable sorting if it was enabled
	if (sortingWasEnabled) {
		mylistTreeWidget->setSortingEnabled(true);
	}
	
	// Trigger repaint of play column
	mylistTreeWidget->viewport()->update();
}

QString Window::getFilePathForPlayback(int lid)
{
	QSqlDatabase db = QSqlDatabase::database();
	if (!db.isOpen()) {
		return QString();
	}
	
	// First try to get the file path from local_files table
	QSqlQuery q(db);
	q.prepare("SELECT lf.path FROM mylist m "
			  "LEFT JOIN local_files lf ON m.local_file = lf.id "
			  "WHERE m.lid = ?");
	q.addBindValue(lid);
	
	if (q.exec() && q.next()) {
		QString path = q.value(0).toString();
		if (!path.isEmpty()) {
			return path;
		}
	}
	
	// If not found, try to get storage path from mylist table
	q.prepare("SELECT storage FROM mylist WHERE lid = ?");
	q.addBindValue(lid);
	
	if (q.exec() && q.next()) {
		QString storage = q.value(0).toString();
		if (!storage.isEmpty()) {
			return storage;
		}
	}
	
	return QString();
}

int Window::getPlaybackResumePosition(int lid)
{
	QSqlDatabase db = QSqlDatabase::database();
	if (!db.isOpen()) {
		return 0;
	}
	
	QSqlQuery q(db);
	q.prepare("SELECT playback_position FROM mylist WHERE lid = ?");
	q.addBindValue(lid);
	
	if (q.exec() && q.next()) {
		return q.value(0).toInt();
	}
	
	return 0;
}

void Window::startPlaybackForFile(int lid)
{
	if (lid <= 0) {
		return;
	}
	
	QString filePath = getFilePathForPlayback(lid);
	if (!filePath.isEmpty()) {
		int resumePosition = getPlaybackResumePosition(lid);
		playbackManager->startPlayback(filePath, lid, resumePosition);
	} else {
		LOG(QString("Cannot play: file path not found for LID %1").arg(lid));
	}
}

// Toggle between card view and tree view
void Window::toggleMylistView()
{
	mylistUseCardView = !mylistUseCardView;
	
	if (mylistUseCardView) {
		mylistTreeWidget->hide();
		mylistCardScrollArea->show();
		mylistViewToggleButton->setText("Switch to Tree View");
		mylistSortComboBox->setEnabled(true);
		animeMetadataRequested.clear();  // Clear request tracking when switching to card view
		loadMylistAsCards();
	} else {
		mylistCardScrollArea->hide();
		mylistTreeWidget->show();
		mylistViewToggleButton->setText("Switch to Card View");
		mylistSortComboBox->setEnabled(false);
		loadMylistFromDatabase();
	}
}

// Sort cards based on selected criterion
void Window::sortMylistCards(int sortIndex)
{
	if (!mylistUseCardView || animeCards.isEmpty()) {
		return;
	}
	
	// Remove all cards from layout
	for (AnimeCard* const card : std::as_const(animeCards)) {
		mylistCardLayout->removeWidget(card);
	}
	
	// Sort based on criterion
	switch (sortIndex) {
		case 0: // Anime Title
			std::sort(animeCards.begin(), animeCards.end(), [this](const AnimeCard *a, const AnimeCard *b) {
				if (mylistSortAscending) {
					return a->getAnimeTitle() < b->getAnimeTitle();
				} else {
					return a->getAnimeTitle() > b->getAnimeTitle();
				}
			});
			break;
		case 1: // Type
			std::sort(animeCards.begin(), animeCards.end(), [this](const AnimeCard *a, const AnimeCard *b) {
				if (a->getAnimeType() == b->getAnimeType()) {
					return a->getAnimeTitle() < b->getAnimeTitle();
				}
				if (mylistSortAscending) {
					return a->getAnimeType() < b->getAnimeType();
				} else {
					return a->getAnimeType() > b->getAnimeType();
				}
			});
			break;
		case 2: // Aired Date
			std::sort(animeCards.begin(), animeCards.end(), [this](const AnimeCard *a, const AnimeCard *b) {
				aired airedA = a->getAired();
				aired airedB = b->getAired();
				
				// Handle invalid dates (put at end)
				if (!airedA.isValid() && !airedB.isValid()) {
					return a->getAnimeTitle() < b->getAnimeTitle();
				}
				if (!airedA.isValid()) {
					return false;  // a goes after b
				}
				if (!airedB.isValid()) {
					return true;   // a goes before b
				}
				
				// Use aired class comparison operators
				if (airedA == airedB) {
					return a->getAnimeTitle() < b->getAnimeTitle();
				}
				if (mylistSortAscending) {
					return airedA < airedB;
				} else {
					return airedA > airedB;
				}
			});
			break;
		case 3: // Episodes (Count)
			std::sort(animeCards.begin(), animeCards.end(), [this](const AnimeCard *a, const AnimeCard *b) {
				int episodesA = a->getNormalEpisodes() + a->getOtherEpisodes();
				int episodesB = b->getNormalEpisodes() + b->getOtherEpisodes();
				if (episodesA == episodesB) {
					return a->getAnimeTitle() < b->getAnimeTitle();
				}
				if (mylistSortAscending) {
					return episodesA < episodesB;
				} else {
					return episodesA > episodesB;
				}
			});
			break;
		case 4: // Completion %
			std::sort(animeCards.begin(), animeCards.end(), [this](const AnimeCard *a, const AnimeCard *b) {
				int totalEpisodesA = a->getNormalEpisodes() + a->getOtherEpisodes();
				int totalEpisodesB = b->getNormalEpisodes() + b->getOtherEpisodes();
				int viewedA = a->getNormalViewed() + a->getOtherViewed();
				int viewedB = b->getNormalViewed() + b->getOtherViewed();
				double completionA = (totalEpisodesA > 0) ? (double)viewedA / totalEpisodesA : 0.0;
				double completionB = (totalEpisodesB > 0) ? (double)viewedB / totalEpisodesB : 0.0;
				if (completionA == completionB) {
					return a->getAnimeTitle() < b->getAnimeTitle();
				}
				if (mylistSortAscending) {
					return completionA < completionB;
				} else {
					return completionA > completionB;
				}
			});
			break;
		case 5: // Last Played
			std::sort(animeCards.begin(), animeCards.end(), [this](const AnimeCard *a, const AnimeCard *b) {
				qint64 lastPlayedA = a->getLastPlayed();
				qint64 lastPlayedB = b->getLastPlayed();
				
				// Never played items (0) go to the end regardless of sort order
				if (lastPlayedA == 0 && lastPlayedB == 0) {
					return a->getAnimeTitle() < b->getAnimeTitle();
				}
				if (lastPlayedA == 0) {
					return false;  // a goes after b
				}
				if (lastPlayedB == 0) {
					return true;   // a goes before b
				}
				
				if (mylistSortAscending) {
					return lastPlayedA < lastPlayedB;
				} else {
					return lastPlayedA > lastPlayedB;
				}
			});
			break;
	}
	
	// Re-add cards to layout in sorted order
	for (AnimeCard* const card : std::as_const(animeCards)) {
		mylistCardLayout->addWidget(card);
	}
}

// Toggle sort order between ascending and descending
void Window::toggleSortOrder()
{
	mylistSortAscending = !mylistSortAscending;
	
	// Update button text
	if (mylistSortAscending) {
		mylistSortOrderButton->setText("↑ Asc");
	} else {
		mylistSortOrderButton->setText("↓ Desc");
	}
	
	// Re-sort with current criterion
	sortMylistCards(mylistSortComboBox->currentIndex());
}

// Load mylist data as cards
void Window::loadMylistAsCards()
{
	QElapsedTimer timer;
	timer.start();
	
	LOG(QString("[Timing] Starting loadMylistAsCards with MyListCardManager"));
	
	// Set the layout for the card manager
	cardManager->setCardLayout(mylistCardLayout);
	
	// Connect card manager to API update signals
	connect(adbapi, &myAniDBApi::notifyEpisodeUpdated, 
	        cardManager, &MyListCardManager::onEpisodeUpdated, Qt::UniqueConnection);
	connect(adbapi, &myAniDBApi::notifyAnimeUpdated,
	        cardManager, &MyListCardManager::onAnimeUpdated, Qt::UniqueConnection);
	
	// Load all cards through the manager
	cardManager->loadAllCards();
	
	// Get cards for backward compatibility with sorting code
	animeCards = cardManager->getAllCards();
	
	// Apply current sort
	qint64 startSort = timer.elapsed();
	sortMylistCards(mylistSortComboBox->currentIndex());
	qint64 sortElapsed = timer.elapsed() - startSort;
	LOG(QString("[Timing] Sorting cards took %1 ms").arg(sortElapsed));
	
	qint64 totalElapsed = timer.elapsed();
	LOG(QString("[Timing] Total loadMylistAsCards took %1 ms").arg(totalElapsed));
}

// Download poster image for an anime
void Window::downloadPosterForAnime(int aid, const QString &picname)
{
	if (picname.isEmpty()) {
		return;
	}
	
	// AniDB CDN URL for anime posters
	QString url = QString("http://img7.anidb.net/pics/anime/%1").arg(picname);
	
	LOG(QString("Downloading poster for anime %1 from %2").arg(aid).arg(url));
	
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::UserAgentHeader, "Usagi/1");
	
	QNetworkReply *reply = posterNetworkManager->get(request);
	posterDownloadRequests[reply] = aid;
}

// Handle poster download completion
void Window::onPosterDownloadFinished(QNetworkReply *reply)
{
	// Clean up reply when done
	reply->deleteLater();
	
	if (!posterDownloadRequests.contains(reply)) {
		return;
	}
	
	int aid = posterDownloadRequests.take(reply);
	
	if (reply->error() != QNetworkReply::NoError) {
		LOG(QString("Error downloading poster for anime %1: %2").arg(aid).arg(reply->errorString()));
		return;
	}
	
	QByteArray imageData = reply->readAll();
	if (imageData.isEmpty()) {
		LOG(QString("Empty poster image data for anime %1").arg(aid));
		return;
	}
	
	// Verify it's a valid image
	QPixmap poster;
	if (!poster.loadFromData(imageData)) {
		LOG(QString("Invalid poster image data for anime %1").arg(aid));
		return;
	}
	
	// Store in database
	QSqlDatabase db = QSqlDatabase::database();
	if (!validateDatabaseConnection(db, "onPosterDownloadFinished")) {
		return;
	}
	
	QSqlQuery query(db);
	query.prepare("UPDATE anime SET poster_image = :image WHERE aid = :aid");
	query.bindValue(":image", imageData);
	query.bindValue(":aid", aid);
	
	if (!query.exec()) {
		LOG(QString("Error storing poster for anime %1: %2").arg(aid).arg(query.lastError().text()));
		return;
	}
	
	LOG(QString("Poster downloaded and stored for anime %1").arg(aid));
	
	// Update the card if it exists
	for (AnimeCard* const card : std::as_const(animeCards)) {
		if (card->getAnimeId() == aid) {
			card->setPoster(poster);
			animeNeedingPoster.remove(aid);
			break;
		}
	}
}

void Window::onCardClicked(int aid)
{
	LOG(QString("Card clicked for anime ID: %1").arg(aid));
	// Could expand to show more details or navigate somewhere
}

void Window::onCardEpisodeClicked(int lid)
{
	LOG(QString("Episode clicked with LID: %1").arg(lid));
	// Start playback for the episode
	startPlaybackForFile(lid);
}

// Unknown files handling slots
void Window::onUnknownFileAnimeSearchChanged(int row)
{
    // This is handled by the lambda in unknownFilesInsertRow
    // Placeholder for future enhancements
}

void Window::onUnknownFileEpisodeSelected(int row)
{
    // This is handled by the lambda in unknownFilesInsertRow
    // Placeholder for future enhancements
}

void Window::onUnknownFileBindClicked(int row, const QString& epno)
{
    if(!unknownFilesData.contains(row))
    {
        LOG(QString("Error: Unknown file data not found for row %1").arg(row));
        return;
    }
    
    UnknownFileData &fileData = unknownFilesData[row];
    
    if(fileData.selectedAid <= 0)
    {
        LOG(QString("Error: Invalid anime selection for row %1").arg(row));
        QMessageBox::warning(this, "Invalid Selection", "Please select an anime before binding.");
        return;
    }
    
    if(epno.isEmpty())
    {
        LOG(QString("Error: Empty episode number for row %1").arg(row));
        QMessageBox::warning(this, "Invalid Episode", "Please enter an episode number.");
        return;
    }
    
    LOG(QString("Binding unknown file: %1 to anime %2, episode %3")
        .arg(fileData.filename)
        .arg(fileData.selectedAid)
        .arg(epno));
    
    // Use the settings from the UI
    int viewed = 0;
    if(markwatched->checkState() == Qt::Checked) {
        viewed = 1;
    } else if(markwatched->checkState() == Qt::PartiallyChecked) {
        viewed = 0;
    } else {
        viewed = -1; // no change
    }
    
    int state = hasherFileState->currentIndex();
    QString storageStr = storage->text();
    
    // Prepare the "other" field with file details
    // Note: The AniDB API has an undocumented length limit for the 'other' field
    // Testing shows ~100 chars works reliably, so we truncate to stay safe
    QString otherField = QString("File: %1\nHash: %2\nSize: %3")
        .arg(fileData.filename)
        .arg(fileData.hash)
        .arg(fileData.size);
    
    // Truncate if too long (limit to 100 chars to stay within API limits)
    if(otherField.length() > 100)
    {
        otherField = otherField.left(97) + "...";
        LOG(QString("Truncated 'other' field to 100 chars for API compatibility"));
    }
    
    // Add to mylist via API using generic parameter
    if(addtomylist->isChecked() && adbapi->LoggedIn())
    {
        LOG(QString("Adding unknown file to mylist using generic: aid=%1, epno=%2")
            .arg(fileData.selectedAid)
            .arg(epno));
        
        adbapi->MylistAddGeneric(fileData.selectedAid, epno, viewed, state, storageStr, otherField);
        
        // Mark the file in local_files as bound to anime (binding_status 1)
        adbapi->UpdateLocalFileBindingStatus(fileData.filepath, 1);
        
        // Reload mylist to show the newly bound file
        // Note: This happens immediately, before the API response. 
        // The actual entry will appear after the API confirms (210 response)
        // but we refresh the display to pick up the binding_status change
        if(mylistUseCardView) {
            loadMylistAsCards();
        } else {
            loadMylistFromDatabase();
        }
        
        // Remove from unknown files widget after successful binding
        unknownFiles->removeRow(row);
        unknownFilesData.remove(row);
        
        // Update row indices in unknownFilesData map
        QMap<int, UnknownFileData> newMap;
        for(auto it = unknownFilesData.begin(); it != unknownFilesData.end(); ++it)
        {
            int oldRow = it.key();
            if(oldRow > row) {
                newMap[oldRow - 1] = it.value();
            } else {
                newMap[oldRow] = it.value();
            }
        }
        unknownFilesData = newMap;
        
        // Hide the widget if no more unknown files
        if(unknownFiles->rowCount() == 0)
        {
            unknownFiles->hide();
            QWidget *unknownFilesLabel = this->findChild<QWidget*>("unknownFilesLabel");
            if(unknownFilesLabel)
            {
                unknownFilesLabel->hide();
            }
        }
        
        LOG(QString("Successfully bound unknown file to anime %1, episode %2")
            .arg(fileData.selectedAid)
            .arg(epno));
    }
    else
    {
        QMessageBox::warning(this, "Cannot Add", 
            "Please enable 'Add file(s) to MyList' and ensure you are logged in.");
    }
}

void Window::onUnknownFileNotAnimeClicked(int row)
{
    if(!unknownFilesData.contains(row))
    {
        LOG(QString("Error: Unknown file data not found for row %1").arg(row));
        return;
    }
    
    UnknownFileData &fileData = unknownFilesData[row];
    
    LOG(QString("Marking file as not anime: %1").arg(fileData.filename));
    
    // Mark the file in local_files as not anime (binding_status 2)
    adbapi->UpdateLocalFileBindingStatus(fileData.filepath, 2);
    
    // Remove from unknown files widget
    unknownFiles->removeRow(row);
    unknownFilesData.remove(row);
    
    // Update row indices in unknownFilesData map
    QMap<int, UnknownFileData> newMap;
    for(auto it = unknownFilesData.begin(); it != unknownFilesData.end(); ++it)
    {
        int oldRow = it.key();
        if(oldRow > row) {
            newMap[oldRow - 1] = it.value();
        } else {
            newMap[oldRow] = it.value();
        }
    }
    unknownFilesData = newMap;
    
    // Hide the widget if no more unknown files
    if(unknownFiles->rowCount() == 0)
    {
        unknownFiles->hide();
        QWidget *unknownFilesLabel = this->findChild<QWidget*>("unknownFilesLabel");
        if(unknownFilesLabel)
        {
            unknownFilesLabel->hide();
        }
    }
    
    LOG(QString("Successfully marked file as not anime: %1").arg(fileData.filename));
}

void Window::onUnknownFileRecheckClicked(int row)
{
    if(!unknownFilesData.contains(row))
    {
        LOG(QString("Error: Unknown file data not found for row %1").arg(row));
        return;
    }
    
    UnknownFileData &fileData = unknownFilesData[row];
    
    LOG(QString("Re-checking file against AniDB: %1").arg(fileData.filename));
    LOG(QString("Hash: %1, Size: %2").arg(fileData.hash).arg(fileData.size));
    
    // Check if file is already in hashes table
    bool fileInHashesTable = false;
    int hashesRow = -1;
    for(int i = 0; i < hashes->rowCount(); ++i)
    {
        QString hashPath = hashes->item(i, 2)->text();
        if(hashPath == fileData.filepath)
        {
            fileInHashesTable = true;
            hashesRow = i;
            break;
        }
    }
    
    // If file is not in hashes table, add it first
    if(!fileInHashesTable)
    {
        LOG(QString("File not in hashes table, adding: %1").arg(fileData.filename));
        QFileInfo fileInfo(fileData.filepath);
        hashesinsertrow(fileInfo, Qt::Unchecked, fileData.hash);
        hashesRow = hashes->rowCount() - 1;
    }
    else
    {
        LOG(QString("File already in hashes table at row %1").arg(hashesRow));
    }
    
    // Call MylistAdd API with the hash and size
    // The response will be handled by getNotifyMylistAdd
    QString tag = adbapi->MylistAdd(fileData.size, fileData.hash, 
                                     markwatched->checkState(), 
                                     hasherFileState->currentIndex(), 
                                     storage->text());
    
    // Store the tag in the hashes table for response matching
    if(hashesRow >= 0 && hashesRow < hashes->rowCount())
    {
        hashes->item(hashesRow, 6)->setText(tag);
        // Update progress indicator to show it's being checked
        hashes->item(hashesRow, 1)->setText("1"); // Checking
        LOG(QString("Sent re-check request with tag: %1").arg(tag));
    }
    
    LOG(QString("Re-check initiated for file: %1").arg(fileData.filename));
}
