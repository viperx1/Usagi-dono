#include "main.h"
#include "window.h"
#include "animeutils.h"
#include "hasherthreadpool.h"
#include "hasherthread.h"
#include "crashlog.h"
#include "logger.h"
#include "aired.h"
#include "directorywatchermanager.h"
#include "autofetchmanager.h"
#include "traysettingsmanager.h"
#include <QElapsedTimer>
#include <QThread>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QSet>
#include <QApplication>
#include <QSettings>
#include <QTextStream>
#include <QStyle>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QMenu>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QWindowStateChangeEvent>
#include <QSplitter>
#include <algorithm>
#include <functional>
#include <memory>
#include <QDirIterator>

#ifdef Q_OS_WIN
#include <windows.h>
#include "resource.h"  // Windows resource IDs
#endif

HasherThreadPool *hasherThreadPool = nullptr;
// adbapi is now defined in anidbapi.cpp (core library)
extern Window *window;

// Worker implementations

QList<int> MylistLoaderWorker::executeQuery(QSqlDatabase &db)
{
    LOG("Background thread: Loading mylist anime IDs...");
    
    QList<int> aids;
    
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
    
    LOG(QString("Background thread: Loaded %1 mylist anime IDs").arg(aids.size()));
    
    return aids;
}

QPair<QStringList, QMap<QString, int>> AnimeTitlesLoaderWorker::executeQuery(QSqlDatabase &db)
{
    LOG("Background thread: Loading anime titles cache...");
    
    QStringList tempTitles;
    QMap<QString, int> tempTitleToAid;
    
    QSqlQuery query(db);
    query.exec("SELECT DISTINCT aid, title FROM anime_titles ORDER BY title");
    
    while (query.next()) {
        int aid = query.value(0).toInt();
        QString title = query.value(1).toString();
        QString displayText = QString("%1: %2").arg(aid).arg(title);
        tempTitles << displayText;
        tempTitleToAid[displayText] = aid;
    }
    
    LOG(QString("Background thread: Loaded %1 anime titles").arg(tempTitles.size()));
    
    return QPair<QStringList, QMap<QString, int>>(tempTitles, tempTitleToAid);
}

QList<LocalFileInfo> UnboundFilesLoaderWorker::executeQuery(QSqlDatabase &db)
{
    LOG("Background thread: Loading unbound files...");
    
    QList<LocalFileInfo> tempUnboundFiles;
    
    // Query unbound files using same criteria as AniDBApi::getUnboundFiles
    // binding_status = 0 (not_bound) AND status = 3 (not in anidb)
    QSqlQuery query(db);
    query.prepare("SELECT `path`, `filename`, `ed2k_hash` FROM `local_files` WHERE `binding_status` = 0 AND `status` = 3 AND `ed2k_hash` IS NOT NULL AND `ed2k_hash` != ''");
    
    if (!query.exec()) {
        LOG(QString("Background thread: Failed to query unbound files: %1").arg(query.lastError().text()));
    } else {
        while (query.next()) {
            QString filepath = query.value(0).toString();
            QString filename = query.value(1).toString();
            QString hash = query.value(2).toString();
            
            // Use QFileInfo to get filename if not in database
            if (filename.isEmpty()) {
                QFileInfo qFileInfo(filepath);
                filename = qFileInfo.fileName();
            }
            
            // LocalFileInfo constructor will get size if file exists
            LocalFileInfo fileInfo(filename, filepath, hash, 0);
            
            // Get file size if file exists
            QFileInfo qFileInfo(filepath);
            if (qFileInfo.exists()) {
                fileInfo.setSize(qFileInfo.size());
            }
            
            tempUnboundFiles.append(fileInfo);
        }
    }
    
    LOG(QString("Background thread: Loaded %1 unbound files").arg(tempUnboundFiles.size()));
    
    return tempUnboundFiles;
}


Window::Window()
{
	qRegisterMetaType<ed2k::ed2kfilestruct>("ed2k::ed2kfilestruct");
	qRegisterMetaType<Qt::HANDLE>("Qt::HANDLE");
	
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
	// m_hashingProgress is default-constructed, will be reset when hashing starts
	totalHashParts = 0;
	completedHashParts = 0;
	
	// Initialize UI color constants (use Qt's predefined yellow color)
	m_hashedFileColor = QColor(Qt::yellow);
	
	// Initialize anime titles cache flag
	animeTitlesCacheLoaded = false;
	
    // Initialize window state for tray restore
    windowStateBeforeHide = Qt::WindowNoState;
    exitingFromTray = false;
    playbackManager = nullptr;
    watchSessionManager = nullptr;
    directoryWatcherManager = nullptr;
    autoFetchManager = nullptr;
    traySettingsManager = nullptr;
	
    safeclose = new QTimer;
    safeclose->setInterval(100);
    connect(safeclose, SIGNAL(timeout()), this, SLOT(safeClose()));

    // Connect to application aboutToQuit signal to handle external termination
    connect(qApp, &QApplication::aboutToQuit, this, &Window::onApplicationAboutToQuit);

    // Timer for startup initialization (delayed to ensure UI is ready)
    startupTimer = new QTimer(this);
    startupTimer->setSingleShot(true);
    startupTimer->setInterval(1000);
    connect(startupTimer, SIGNAL(timeout()), this, SLOT(startupInitialization()));

    // main window
    this->setWindowTitle("Usagi");
    
    // Set window icon (use usagi.png if available)
    this->setWindowIcon(loadUsagiIcon());
    
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

	// tabs - Mylist first as default tab
    tabwidget->addTab(pageMylistParent, "Anime");
    tabwidget->addTab(pageHasherParent, "Hasher");
    tabwidget->addTab(pageNotifyParent, "Notify");
    tabwidget->addTab(pageSettingsParent, "Settings");
    tabwidget->addTab(pageLogParent, "Log");
	tabwidget->addTab(pageApiTesterParent, "ApiTester");

    // page hasher - Create HasherCoordinator to manage all hasher UI and logic
    hasherCoordinator = new HasherCoordinator(adbapi, pageHasherParent);
    hashes = hasherCoordinator->getHashesTable();  // Get reference to hashes table for compatibility
    
    // Create UnknownFilesManager to manage all unknown files UI and logic
    unknownFilesManager = new UnknownFilesManager(adbapi, hasherCoordinator, this);
    
    // Create the main hasher layout in the requested order:
    // 1. hashes (fixed minimum - resizable)
    // 2. unknown files (fixed minimum - resizable)
    // 3. all hasher control (not resizable)
    // 4. (collapse button for thread progress bars) total progress bar
    // 5. thread progress bars
    // 6. ed2k links (fixed size ~ 6 lines)
    
    // Create a splitter for resizable sections (hashes + unknown files)
    QSplitter *topSplitter = new QSplitter(Qt::Vertical);
    topSplitter->addWidget(hasherCoordinator->getHashesTable());
    topSplitter->addWidget(unknownFilesManager->getContainerWidget());
    topSplitter->setStretchFactor(0, 3);  // Hashes table gets more space
    topSplitter->setStretchFactor(1, 1);  // Unknown files gets less space
    
    // Create collapse button for thread progress bars
    QPushButton *collapseThreadProgressButton = new QPushButton("▼");
    collapseThreadProgressButton->setMaximumWidth(30);
    collapseThreadProgressButton->setCheckable(true);
    collapseThreadProgressButton->setChecked(false);  // Initially not collapsed (visible)
    
    // Create container for thread progress bars
    QWidget *threadProgressContainer = new QWidget();
    QVBoxLayout *threadProgressLayout = new QVBoxLayout(threadProgressContainer);
    threadProgressLayout->setContentsMargins(0, 0, 0, 0);
    for (QProgressBar *bar : hasherCoordinator->getThreadProgressBars()) {
        threadProgressLayout->addWidget(bar);
    }
    
    // Create total progress bar layout with collapse button
    QHBoxLayout *totalProgressLayout = new QHBoxLayout();
    totalProgressLayout->addWidget(collapseThreadProgressButton);
    totalProgressLayout->addWidget(hasherCoordinator->getTotalProgressBar());
    totalProgressLayout->addWidget(hasherCoordinator->getTotalProgressLabel());
    
    // Add everything to the main hasher page layout
    pageHasher->addWidget(topSplitter, 1);  // Resizable section
    pageHasher->addLayout(hasherCoordinator->getHasherSettings());  // Control section (not resizable)
    pageHasher->addLayout(totalProgressLayout);  // Total progress with collapse button
    pageHasher->addWidget(threadProgressContainer);  // Thread progress bars
    pageHasher->addWidget(hasherCoordinator->getHasherOutput());  // ED2K links (fixed size)
    
    // Connect collapse button
    connect(collapseThreadProgressButton, &QPushButton::toggled, this, [collapseThreadProgressButton, threadProgressContainer](bool checked) {
        threadProgressContainer->setVisible(!checked);
        collapseThreadProgressButton->setText(checked ? "▶" : "▼");
    });
    
    // Hide unknown files section initially (it's already hidden in the manager, but hiding the container too)
    unknownFilesManager->getContainerWidget()->hide();
    
    // Connect HasherCoordinator signals
    connect(hasherCoordinator, &HasherCoordinator::hashingFinished, this, &Window::hasherFinished);
    connect(hasherCoordinator, &HasherCoordinator::logMessage, this, &Window::getNotifyLogAppend);
    
    // Connect UnknownFilesManager signals
    connect(unknownFilesManager, &UnknownFilesManager::logMessage, this, &Window::getNotifyLogAppend);
    connect(unknownFilesManager, &UnknownFilesManager::fileNeedsHashing, this, [this](const QFileInfo& fileInfo, Qt::CheckState renameState, const QString& preloadedHash) {
        hashesinsertrow(fileInfo, renameState, preloadedHash);
    });
    
    // Connect hasher thread pool signals to HasherCoordinator
    connect(hasherThreadPool, &HasherThreadPool::requestNextFile, hasherCoordinator, &HasherCoordinator::provideNextFileToHash);
    connect(hasherThreadPool, &HasherThreadPool::notifyPartsDone, hasherCoordinator, &HasherCoordinator::onProgressUpdate);
    connect(hasherThreadPool, &HasherThreadPool::notifyFileHashed, hasherCoordinator, &HasherCoordinator::onFileHashed);
    connect(hasherThreadPool, &HasherThreadPool::finished, hasherCoordinator, &HasherCoordinator::onHashingFinished);
    
    // Connect AniDBApi signals
    connect(this, SIGNAL(notifyStopHasher()), adbapi, SLOT(getNotifyStopHasher()));
    connect(adbapi, SIGNAL(notifyLogAppend(QString)), this, SLOT(getNotifyLogAppend(QString)));
	connect(adbapi, SIGNAL(notifyMylistAdd(QString,int)), this, SLOT(getNotifyMylistAdd(QString,int)));
	
	// Connect unified Logger to log tab using modern Qt5+ syntax for type safety
	connect(Logger::instance(), &Logger::logMessage, this, &Window::getNotifyLogAppend);

    // page mylist (card view only)
    mylistSortAscending = false;  // Default to descending (newest first for aired date)
    lastInMyListState = true;  // Initialize to true (default is "In MyList only")
    allAnimeTitlesLoaded = false;  // All anime titles are not loaded initially
    
    // Initialize card manager for efficient card lifecycle management
    cardManager = new MyListCardManager(this);
    
    // Connect card manager signals to window slots
    connect(cardManager, &MyListCardManager::allCardsLoaded, this, [this](int count) {
        mylistStatusLabel->setText(QString("MyList Status: Loaded %1 anime").arg(count));
        animeCards = cardManager->getAllCards();  // Update legacy list for backward compatibility
    });
    
    // Connect progress update signal to status label
    connect(cardManager, &MyListCardManager::progressUpdate, this, [this](const QString& message) {
        mylistStatusLabel->setText(QString("MyList Status: %1").arg(message));
    });
    
    // Connect signal for brand new anime added to mylist (after initial load)
    connect(cardManager, &MyListCardManager::newAnimeAdded, this, [this](int aid) {
        LOG(QString("[Window] New anime aid=%1 added to mylist, auto-starting session").arg(aid));
        if (watchSessionManager) {
            watchSessionManager->onNewAnimeAdded(aid);
        }
    });
    
    connect(cardManager, &MyListCardManager::cardCreated, this, [this](int /*aid*/, AnimeCard *card) {
        // Connect individual card signals
        connect(card, &AnimeCard::cardClicked, this, &Window::onCardClicked);
        connect(card, &AnimeCard::episodeClicked, this, &Window::onCardEpisodeClicked);
        connect(card, &AnimeCard::playAnimeRequested, this, &Window::onPlayAnimeFromCard);
        connect(card, &AnimeCard::resetWatchSessionRequested, this, &Window::onResetWatchSession);
        
        // Connect session and file marking signals to WatchSessionManager
        connect(card, &AnimeCard::startSessionFromEpisodeRequested, this, [this](int lid) {
            LOG(QString("[Window] Starting session from file lid=%1").arg(lid));
            if (watchSessionManager) {
                watchSessionManager->startSessionFromFile(lid);
            }
        });
        connect(card, &AnimeCard::deleteFileRequested, this, [this](int lid) {
            LOG(QString("[Window] Delete file requested for lid=%1").arg(lid));
            // Show confirmation dialog
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Delete File",
                "Are you sure you want to delete this file?\n\n"
                "This will:\n"
                "- Delete the file from your disk\n"
                "- Remove it from your local database\n"
                "- Mark it as deleted in AniDB\n\n"
                "This action cannot be undone.",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            
            if (reply == QMessageBox::Yes) {
                if (watchSessionManager) {
                    watchSessionManager->deleteFile(lid, true);
                }
            }
        });
    });
    
    connect(cardManager, &MyListCardManager::cardUpdated, this, [](int) {
        // Card was updated in place - no need to resort the entire list
    });
    
    // Connect episode data request signal to fetch missing episode data
    connect(cardManager, &MyListCardManager::episodeDataRequested, this, [this](int eid) {
        LOG(QString("[Window] episodeDataRequested signal received for eid=%1").arg(eid));
        if (adbapi) {
            LOG(QString("[Window] Requesting episode data from AniDB API for eid=%1").arg(eid));
            adbapi->Episode(eid);
        } else {
            LOG(QString("[Window] ERROR: adbapi is null, cannot request episode data for eid=%1").arg(eid));
        }
    });
    
    // Connect file API update signal to update watched status on AniDB
    connect(cardManager, &MyListCardManager::fileNeedsApiUpdate, this, [this](int lid, int size, QString ed2khash, int viewed) {
        LOG(QString("[Window] Updating file watched status on AniDB for lid=%1, viewed=%2").arg(lid).arg(viewed));
        if (adbapi) {
            // Query state and storage from database to pass to UpdateFile
            QSqlDatabase db = QSqlDatabase::database();
            if (db.isOpen()) {
                QSqlQuery q(db);
                q.prepare("SELECT state, storage FROM mylist WHERE lid = ?");
                q.addBindValue(lid);
                if (q.exec() && q.next()) {
                    int state = q.value(0).toInt();
                    QString storage = q.value(1).toString();
                    adbapi->UpdateFile(size, ed2khash, viewed, state, storage);
                }
            }
        }
    });
    
    // Create horizontal layout for sidebar and card view
    QHBoxLayout *mylistContentLayout = new QHBoxLayout();
    
    // Create filter sidebar (now includes sorting controls)
    filterSidebar = new MyListFilterSidebar(this);
    connect(filterSidebar, &MyListFilterSidebar::filterChanged, this, [this]() {
        applyMylistFilters();
        // Re-apply sorting after filtering to preserve sort order
        sortMylistCards(filterSidebar->getSortIndex());
        // Save filter settings
        saveMylistSorting();
    });
    connect(filterSidebar, &MyListFilterSidebar::sortChanged, this, [this]() {
        sortMylistCards(filterSidebar->getSortIndex());
        // Save sort settings
        saveMylistSorting();
    });
    connect(filterSidebar, &MyListFilterSidebar::collapseRequested, this, &Window::onToggleFilterBarClicked);
    
    // Wrap filter sidebar in a scroll area for vertical scrolling
    filterSidebarScrollArea = new QScrollArea(this);
    filterSidebarScrollArea->setWidget(filterSidebar);
    filterSidebarScrollArea->setWidgetResizable(true);
    filterSidebarScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    filterSidebarScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    filterSidebarScrollArea->setMinimumWidth(220);  // Account for scrollbar
    filterSidebarScrollArea->setMaximumWidth(320);
    
    mylistContentLayout->addWidget(filterSidebarScrollArea);
    
    // Create toggle button to show filter sidebar when collapsed
    // Place it at the top by using a vertical layout with stretch
    QVBoxLayout *expandButtonLayout = new QVBoxLayout();
    toggleFilterBarButton = new QPushButton("▶");
    toggleFilterBarButton->setMaximumWidth(30);
    toggleFilterBarButton->setMaximumHeight(30);
    toggleFilterBarButton->setToolTip("Show filter sidebar");
    toggleFilterBarButton->setVisible(false);  // Hidden by default
    connect(toggleFilterBarButton, &QPushButton::clicked, this, &Window::onToggleFilterBarClicked);
    expandButtonLayout->addWidget(toggleFilterBarButton);
    expandButtonLayout->addStretch();  // Push button to the top
    mylistContentLayout->addLayout(expandButtonLayout);
    
    // Create a vertical layout for the card view
    QVBoxLayout *cardViewLayout = new QVBoxLayout();
    
    // Card view (only view mode available)
    // Use VirtualFlowLayout for efficient virtual scrolling
    mylistCardScrollArea = new QScrollArea(this);
    mylistCardScrollArea->setWidgetResizable(true);
    mylistCardScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mylistCardScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Create virtual flow layout for efficient rendering of many cards
    mylistVirtualLayout = new VirtualFlowLayout(this);
    mylistVirtualLayout->setSpacing(10, 10);
    mylistVirtualLayout->setItemSize(AnimeCard::getCardSize());
    mylistCardScrollArea->setWidget(mylistVirtualLayout);
    mylistVirtualLayout->setScrollArea(mylistCardScrollArea);
    
    // Note: Legacy FlowLayout (mylistCardLayout) is no longer used with virtual scrolling
    mylistCardContainer = nullptr;
    mylistCardLayout = nullptr;
    
    cardViewLayout->addWidget(mylistCardScrollArea, 1);  // Give card area stretch factor of 1
    mylistContentLayout->addLayout(cardViewLayout, 1);  // Give card view layout stretch factor of 1
    
    pageMylist->addLayout(mylistContentLayout);
    
    // Add progress status label
    mylistStatusLabel = new QLabel("MyList Status: Ready");
    mylistStatusLabel->setAlignment(Qt::AlignCenter);
    mylistStatusLabel->setStyleSheet("padding: 5px; background-color: #f0f0f0;");
    pageMylist->addWidget(mylistStatusLabel);

    // Note: Hasher signals now connected in HasherCoordinator constructor
    connect(this, SIGNAL(notifyStopHasher()), adbapi, SLOT(getNotifyStopHasher()));
    connect(adbapi, SIGNAL(notifyLogAppend(QString)), this, SLOT(getNotifyLogAppend(QString)));
	connect(adbapi, SIGNAL(notifyMylistAdd(QString,int)), this, SLOT(getNotifyMylistAdd(QString,int)));
	
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
    // Note: All other hasher UI setup is now handled by HasherCoordinator

    // page settings - reorganized with group boxes for better visual grouping
    
    // Create a scroll area for the settings page
    QScrollArea *settingsScrollArea = new QScrollArea(pageSettingsParent);
    settingsScrollArea->setWidgetResizable(true);
    settingsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    settingsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    QWidget *settingsContainer = new QWidget();
    QVBoxLayout *settingsMainLayout = new QVBoxLayout(settingsContainer);
    settingsMainLayout->setSpacing(10);
    settingsMainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Login Settings Group
    QGroupBox *loginGroup = new QGroupBox("Login Credentials");
    QGridLayout *loginLayout = new QGridLayout(loginGroup);
    labelLogin = new QLabel("Username:");
    editLogin = new QLineEdit;
	editLogin->setText(adbapi->getUsername());
    labelPassword = new QLabel("Password:");
    editPassword = new QLineEdit;
	editPassword->setText(adbapi->getPassword());
    editPassword->setEchoMode(QLineEdit::Password);
    loginLayout->addWidget(labelLogin, 0, 0);
    loginLayout->addWidget(editLogin, 0, 1);
    loginLayout->addWidget(labelPassword, 1, 0);
    loginLayout->addWidget(editPassword, 1, 1);
    settingsMainLayout->addWidget(loginGroup);
    
    // Directory Watcher Group
    directoryWatcherManager = new DirectoryWatcherManager(adbapi, this);
    settingsMainLayout->addWidget(directoryWatcherManager->getSettingsGroup());
    
    // Auto-fetch Group
    autoFetchManager = new AutoFetchManager(adbapi, this);
    settingsMainLayout->addWidget(autoFetchManager->getSettingsGroup());
    
    // Playback Group
    QGroupBox *playbackGroup = new QGroupBox("Playback");
    QHBoxLayout *playbackLayout = new QHBoxLayout(playbackGroup);
    mediaPlayerPath = new QLineEdit;
    mediaPlayerBrowseButton = new QPushButton("Browse...");
    playbackLayout->addWidget(new QLabel("Media Player:"));
    playbackLayout->addWidget(mediaPlayerPath, 1);
    playbackLayout->addWidget(mediaPlayerBrowseButton);
    settingsMainLayout->addWidget(playbackGroup);
    
    // Session Manager Group
    QGroupBox *sessionGroup = new QGroupBox("Session Manager");
    QGridLayout *sessionLayout = new QGridLayout(sessionGroup);
    sessionAheadBufferSpinBox = new QSpinBox();
    sessionAheadBufferSpinBox->setMinimum(1);
    sessionAheadBufferSpinBox->setMaximum(20);
    sessionAheadBufferSpinBox->setValue(3);
    sessionAheadBufferSpinBox->setToolTip("Number of episodes to keep ready for uninterrupted viewing.\n"
                                           "This value applies to all anime with active sessions.");
    
    sessionThresholdTypeComboBox = new QComboBox();
    sessionThresholdTypeComboBox->addItem("Fixed (GB)", 0);
    sessionThresholdTypeComboBox->addItem("Percentage (%)", 1);
    sessionThresholdTypeComboBox->setToolTip("Type of threshold for automatic file cleanup");
    
    sessionThresholdValueSpinBox = new QDoubleSpinBox();
    sessionThresholdValueSpinBox->setMinimum(1.0);
    sessionThresholdValueSpinBox->setMaximum(1000.0);
    sessionThresholdValueSpinBox->setValue(50.0);
    sessionThresholdValueSpinBox->setSuffix(" GB");
    sessionThresholdValueSpinBox->setToolTip("When free space drops below this value, files will be marked for deletion");
    
    sessionAutoMarkDeletionCheckbox = new QCheckBox("Auto-mark for deletion");
    sessionAutoMarkDeletionCheckbox->setToolTip("Automatically mark watched files for deletion when disk space is low");
    
    sessionLayout->addWidget(new QLabel("Episodes ahead:"), 0, 0);
    sessionLayout->addWidget(sessionAheadBufferSpinBox, 0, 1);
    sessionLayout->addWidget(new QLabel("Deletion threshold:"), 1, 0);
    sessionLayout->addWidget(sessionThresholdTypeComboBox, 1, 1);
    sessionLayout->addWidget(new QLabel("Threshold value:"), 2, 0);
    sessionLayout->addWidget(sessionThresholdValueSpinBox, 2, 1);
    sessionLayout->addWidget(sessionAutoMarkDeletionCheckbox, 3, 0, 1, 2);
    settingsMainLayout->addWidget(sessionGroup);
    
    // File Deletion Group
    QGroupBox *deletionGroup = new QGroupBox("File Deletion");
    QVBoxLayout *deletionLayout = new QVBoxLayout(deletionGroup);
    sessionEnableAutoDeletionCheckbox = new QCheckBox("Enable automatic file deletion");
    sessionEnableAutoDeletionCheckbox->setToolTip("When enabled, files marked for deletion will be automatically deleted");
    sessionEnableAutoDeletionCheckbox->setChecked(false);  // Default: disabled for safety
    
    sessionForceDeletePermissionsCheckbox = new QCheckBox("Force delete (change permissions)");
    sessionForceDeletePermissionsCheckbox->setToolTip("Attempt to remove read-only attribute before deletion (Windows)");
    sessionForceDeletePermissionsCheckbox->setChecked(false);  // Default: disabled for safety
    
    deletionLayout->addWidget(sessionEnableAutoDeletionCheckbox);
    deletionLayout->addWidget(sessionForceDeletePermissionsCheckbox);
    settingsMainLayout->addWidget(deletionGroup);
    
    // System Tray Group
    traySettingsManager = new TraySettingsManager(this);
    settingsMainLayout->addWidget(traySettingsManager->getSettingsGroup());
    
    // Auto-start Group
    QGroupBox *autoStartGroup = new QGroupBox("Application Startup");
    QVBoxLayout *autoStartLayout = new QVBoxLayout(autoStartGroup);
    autoStartEnabled = new QCheckBox("Start with operating system");
    autoStartEnabled->setToolTip("Automatically start the application when you log in");
    autoStartLayout->addWidget(autoStartEnabled);
    settingsMainLayout->addWidget(autoStartGroup);
    
    // File Marking Preferences Group
    QGroupBox *fileMarkingGroup = new QGroupBox("File Marking Preferences");
    QGridLayout *fileMarkingLayout = new QGridLayout(fileMarkingGroup);
    
    QLabel *audioLangLabel = new QLabel("Preferred Audio Languages:");
    audioLangLabel->setToolTip("Comma-separated list of preferred audio languages (e.g., japanese,english)\n"
                                "Files matching these languages will be prioritized for keeping.");
    QLineEdit *preferredAudioLanguagesEdit = new QLineEdit();
    preferredAudioLanguagesEdit->setObjectName("preferredAudioLanguagesEdit");
    preferredAudioLanguagesEdit->setText(adbapi->getPreferredAudioLanguages());
    preferredAudioLanguagesEdit->setPlaceholderText("japanese,english");
    
    QLabel *subLangLabel = new QLabel("Preferred Subtitle Languages:");
    subLangLabel->setToolTip("Comma-separated list of preferred subtitle languages (e.g., english,none)\n"
                              "Files matching these languages will be prioritized for keeping.");
    QLineEdit *preferredSubtitleLanguagesEdit = new QLineEdit();
    preferredSubtitleLanguagesEdit->setObjectName("preferredSubtitleLanguagesEdit");
    preferredSubtitleLanguagesEdit->setText(adbapi->getPreferredSubtitleLanguages());
    preferredSubtitleLanguagesEdit->setPlaceholderText("english,none");
    
    QCheckBox *preferHighestVersionCheckbox = new QCheckBox("Prefer highest version");
    preferHighestVersionCheckbox->setObjectName("preferHighestVersionCheckbox");
    preferHighestVersionCheckbox->setChecked(adbapi->getPreferHighestVersion());
    preferHighestVersionCheckbox->setToolTip("When multiple versions of the same episode exist, prefer the highest version");
    
    QCheckBox *preferHighestQualityCheckbox = new QCheckBox("Prefer highest quality");
    preferHighestQualityCheckbox->setObjectName("preferHighestQualityCheckbox");
    preferHighestQualityCheckbox->setChecked(adbapi->getPreferHighestQuality());
    preferHighestQualityCheckbox->setToolTip("Prefer files with higher quality and resolution");
    
    QLabel *bitrateLabel = new QLabel("Baseline Bitrate (Mbps):");
    bitrateLabel->setToolTip("Baseline bitrate in Mbps for 1080p content (e.g., 3.5).\n"
                             "Bitrate for other resolutions is automatically calculated:\n"
                             "bitrate = baseline × (resolution_megapixels / 2.07)\n"
                             "This ensures consistent quality across different resolutions.");
    QDoubleSpinBox *preferredBitrateSpinBox = new QDoubleSpinBox();
    preferredBitrateSpinBox->setObjectName("preferredBitrateSpinBox");
    preferredBitrateSpinBox->setRange(0.5, 50.0);
    preferredBitrateSpinBox->setSingleStep(0.5);
    preferredBitrateSpinBox->setDecimals(1);
    preferredBitrateSpinBox->setValue(adbapi->getPreferredBitrate());
    preferredBitrateSpinBox->setSuffix(" Mbps");
    
    QLabel *resolutionLabel = new QLabel("Preferred Resolution:");
    resolutionLabel->setToolTip("Preferred resolution for file selection (e.g., 1080p, 1440p, 4K).\n"
                                "Files closer to this resolution will be prioritized when multiple files exist.");
    QComboBox *preferredResolutionCombo = new QComboBox();
    preferredResolutionCombo->setObjectName("preferredResolutionCombo");
    preferredResolutionCombo->addItems({"480p", "720p", "1080p", "1440p", "4K", "8K"});
    preferredResolutionCombo->setEditable(true);
    preferredResolutionCombo->setCurrentText(adbapi->getPreferredResolution());
    
    fileMarkingLayout->addWidget(audioLangLabel, 0, 0);
    fileMarkingLayout->addWidget(preferredAudioLanguagesEdit, 0, 1);
    fileMarkingLayout->addWidget(subLangLabel, 1, 0);
    fileMarkingLayout->addWidget(preferredSubtitleLanguagesEdit, 1, 1);
    fileMarkingLayout->addWidget(preferHighestVersionCheckbox, 2, 0, 1, 2);
    fileMarkingLayout->addWidget(preferHighestQualityCheckbox, 3, 0, 1, 2);
    fileMarkingLayout->addWidget(bitrateLabel, 4, 0);
    fileMarkingLayout->addWidget(preferredBitrateSpinBox, 4, 1);
    fileMarkingLayout->addWidget(resolutionLabel, 5, 0);
    fileMarkingLayout->addWidget(preferredResolutionCombo, 5, 1);
    
    settingsMainLayout->addWidget(fileMarkingGroup);
    
    // Hasher Filter Group
    QGroupBox *hasherFilterGroup = new QGroupBox("Hasher File Filter");
    QVBoxLayout *hasherFilterLayout = new QVBoxLayout(hasherFilterGroup);
    
    QLabel *hasherFilterLabel = new QLabel("File masks to ignore (comma-separated):");
    hasherFilterLabel->setToolTip("Files matching these patterns will be ignored when adding for hashing.\n"
                                   "Use wildcards like *.!qB for incomplete downloads, *.tmp for temporary files.\n"
                                   "Examples: *.!qB,*.tmp,*.part");
    
    QLineEdit *hasherFilterMasksEdit = new QLineEdit();
    hasherFilterMasksEdit->setObjectName("hasherFilterMasksEdit");
    hasherFilterMasksEdit->setText(adbapi->getHasherFilterMasks());
    hasherFilterMasksEdit->setPlaceholderText("*.!qB,*.tmp,*.part");
    
    hasherFilterLayout->addWidget(hasherFilterLabel);
    hasherFilterLayout->addWidget(hasherFilterMasksEdit);
    
    settingsMainLayout->addWidget(hasherFilterGroup);
    
    // Action Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonSaveSettings = new QPushButton("Save Settings");
    buttonRequestMylistExport = new QPushButton("Request MyList Export");
    buttonLayout->addWidget(buttonSaveSettings);
    buttonLayout->addWidget(buttonRequestMylistExport);
    buttonLayout->addStretch();
    settingsMainLayout->addLayout(buttonLayout);
    
    // Add stretch at the end to push everything to the top
    settingsMainLayout->addStretch();
    
    // Set the container as the scroll area widget
    settingsScrollArea->setWidget(settingsContainer);
    
    // Add scroll area to the settings page layout
    pageSettings->addWidget(settingsScrollArea, 0, 0, 1, 3);
    pageSettings->setRowStretch(0, 1);
    pageSettings->setColumnStretch(0, 1);

    // page settings - signals
    connect(buttonSaveSettings, SIGNAL(clicked()), this, SLOT(saveSettings()));
    connect(buttonRequestMylistExport, SIGNAL(clicked()), this, SLOT(requestMylistExportManually()));
    connect(mediaPlayerBrowseButton, SIGNAL(clicked()), this, SLOT(onMediaPlayerBrowseClicked()));
    connect(directoryWatcherManager, &DirectoryWatcherManager::newFilesDetected,
            this, &Window::onWatcherNewFilesDetected);
    
    // Session manager settings signals - update WatchSessionManager when settings change
    connect(sessionAheadBufferSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (watchSessionManager) {
            watchSessionManager->setAheadBuffer(value);
        }
    });
    connect(sessionThresholdTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        // Use index directly: 0=FixedGB, 1=Percentage
        if (watchSessionManager) {
            watchSessionManager->setDeletionThresholdType(static_cast<DeletionThresholdType>(index));
        }
        // Update suffix based on type
        if (index == 0) {
            sessionThresholdValueSpinBox->setSuffix(" GB");
        } else {
            sessionThresholdValueSpinBox->setSuffix(" %");
        }
    });
    connect(sessionThresholdValueSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        if (watchSessionManager) {
            watchSessionManager->setDeletionThresholdValue(value);
        }
    });
    connect(sessionAutoMarkDeletionCheckbox, &QCheckBox::clicked, this, [this](bool checked) {
        if (watchSessionManager) {
            watchSessionManager->setAutoMarkDeletionEnabled(checked);
        }
    });
    connect(sessionEnableAutoDeletionCheckbox, &QCheckBox::clicked, this, [this](bool checked) {
        if (watchSessionManager) {
            watchSessionManager->setActualDeletionEnabled(checked);
        }
    });
    connect(sessionForceDeletePermissionsCheckbox, &QCheckBox::clicked, this, [this](bool checked) {
        if (watchSessionManager) {
            watchSessionManager->setForceDeletePermissionsEnabled(checked);
        }
    });

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
    connect(loginbutton, SIGNAL(clicked()), this, SLOT(ButtonLoginClick()));
    
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
    connect(playbackManager, &PlaybackManager::fileMarkedAsLocallyWatched,
            this, &Window::onFileMarkedAsLocallyWatched);
    
    // Initialize watch session manager
    watchSessionManager = new WatchSessionManager(this);
    LOG("[Window] WatchSessionManager initialized");
    if (directoryWatcherManager) {
        directoryWatcherManager->setWatchSessionManager(watchSessionManager);
    }
    
    // Connect card manager to watch session manager for file marks
    cardManager->setWatchSessionManager(watchSessionManager);
    
    // Load session settings from WatchSessionManager into Settings tab UI
    sessionAheadBufferSpinBox->blockSignals(true);
    sessionThresholdTypeComboBox->blockSignals(true);
    sessionThresholdValueSpinBox->blockSignals(true);
    sessionAutoMarkDeletionCheckbox->blockSignals(true);
    sessionEnableAutoDeletionCheckbox->blockSignals(true);
    sessionForceDeletePermissionsCheckbox->blockSignals(true);
    
    sessionAheadBufferSpinBox->setValue(watchSessionManager->getAheadBuffer());
    int thresholdType = static_cast<int>(watchSessionManager->getDeletionThresholdType());
    sessionThresholdTypeComboBox->setCurrentIndex(thresholdType);
    if (thresholdType == 0) {
        sessionThresholdValueSpinBox->setSuffix(" GB");
    } else {
        sessionThresholdValueSpinBox->setSuffix(" %");
    }
    sessionThresholdValueSpinBox->setValue(watchSessionManager->getDeletionThresholdValue());
    sessionAutoMarkDeletionCheckbox->setChecked(watchSessionManager->isAutoMarkDeletionEnabled());
    sessionEnableAutoDeletionCheckbox->setChecked(watchSessionManager->isActualDeletionEnabled());
    sessionForceDeletePermissionsCheckbox->setChecked(watchSessionManager->isForceDeletePermissionsEnabled());
    
    sessionAheadBufferSpinBox->blockSignals(false);
    sessionThresholdTypeComboBox->blockSignals(false);
    sessionThresholdValueSpinBox->blockSignals(false);
    sessionAutoMarkDeletionCheckbox->blockSignals(false);
    sessionEnableAutoDeletionCheckbox->blockSignals(false);
    sessionForceDeletePermissionsCheckbox->blockSignals(false);
    
    
    // Connect WatchSessionManager deleteFileRequested signal to perform actual deletion
    connect(watchSessionManager, &WatchSessionManager::deleteFileRequested, this, [this](int lid, bool deleteFromDisk) {
        LOG(QString("[Window] Delete file requested for lid=%1, deleteFromDisk=%2").arg(lid).arg(deleteFromDisk));
        if (adbapi) {
            // Get the aid before deletion for the callback
            int aid = 0;
            QSqlDatabase db = QSqlDatabase::database();
            if (db.isOpen()) {
                QSqlQuery q(db);
                q.prepare("SELECT aid FROM mylist WHERE lid = ?");
                q.addBindValue(lid);
                if (q.exec() && q.next()) {
                    aid = q.value(0).toInt();
                }
            }
            
            // Perform deletion and check result
            QString result = adbapi->deleteFileFromMylist(lid, deleteFromDisk);
            bool success = !result.isEmpty();
            
            // Notify WatchSessionManager of the result
            if (watchSessionManager) {
                watchSessionManager->onFileDeletionResult(lid, aid, success);
            }
        }
    });
    
    // Connect WatchSessionManager fileDeleted signal to refresh UI
    connect(watchSessionManager, &WatchSessionManager::fileDeleted, this, [this](int lid, int aid) {
        LOG(QString("[Window] File deleted: lid=%1, aid=%2 - refreshing card").arg(lid).arg(aid));
        Q_UNUSED(aid);
        // Refresh only the card containing the deleted file
        if (cardManager) {
            QSet<int> lids;
            lids.insert(lid);
            cardManager->refreshCardsForLids(lids);
        }
    });
    
    // Setup animation timer for play button animation
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(300);  // 300ms between animation frames
    connect(m_animationTimer, &QTimer::timeout, this, &Window::onAnimationTimerTimeout);
    
    // Initialize timer for deferred processing of already-hashed files
    hashedFilesProcessingTimer = new QTimer(this);
    hashedFilesProcessingTimer->setSingleShot(false);
    hashedFilesProcessingTimer->setInterval(HASHED_FILES_TIMER_INTERVAL);
    // Note: processPendingHashedFiles is now handled by HasherCoordinator
    // hashedFilesProcessingTimer is kept for backward compatibility but not used
    
    // Initialize background loading threads
    mylistLoadingThread = nullptr;
    animeTitlesLoadingThread = nullptr;
    unboundFilesLoadingThread = nullptr;
    
    // Load directory watcher settings from database
    if (directoryWatcherManager) {
        directoryWatcherManager->loadSettingsFromApi();
    }
    
    // Load auto-fetch settings from database
    if (autoFetchManager) {
        autoFetchManager->loadSettingsFromApi();
    }
    
    // Load media player path from settings
    QString playerPath = PlaybackManager::getMediaPlayerPath();
    mediaPlayerPath->setText(playerPath);
    
    adbapi->CreateSocket();

    // Automatically load mylist on startup (using timer for delayed initialization)
    startupTimer->start();

    // Auto-start directory watcher if enabled
    if (directoryWatcherManager) {
        directoryWatcherManager->applyStartupBehavior();
    }
    
    // Initialize system tray manager
    trayIconManager = new TrayIconManager(loadUsagiIcon(), this);
    connect(trayIconManager, &TrayIconManager::showHideRequested, 
            this, &Window::onTrayShowHideRequested);
    connect(trayIconManager, &TrayIconManager::exitRequested, 
            this, &Window::onTrayExitRequested);
    connect(trayIconManager, &TrayIconManager::logMessage, 
            this, &Window::getNotifyLogAppend);
    
    if (traySettingsManager) {
        traySettingsManager->applyAvailability(trayIconManager);
        traySettingsManager->loadSettingsFromApi(adbapi, trayIconManager);
        
        if (traySettingsManager->isStartMinimizedEnabled() && trayIconManager->isTrayIconVisible()) {
            this->hide();
            LOG("Application started minimized to tray");
        }
    }
    
    // Load filter bar visibility setting
    bool filterBarVisible = adbapi->getFilterBarVisible();
    filterSidebarScrollArea->setVisible(filterBarVisible);
    toggleFilterBarButton->setVisible(!filterBarVisible);
    
    // Load auto-start setting
    autoStartEnabled->setChecked(adbapi->getAutoStartEnabled());

    // end
    this->setLayout(layout);


}

Window::~Window()
{
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
					    .arg(titlesQuery.value("type").toString(),
					         titlesQuery.value("language").toString(),
					         titlesQuery.value("title").toString()));
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

// Note: updateFilterCache, shouldFilterFile, and addFilesFromDirectory methods
// have been moved to HasherCoordinator class

// Note: Button1Click, Button2Click, Button3Click methods
// have been moved to HasherCoordinator class

// Note: calculateTotalHashParts, setupHashingProgress, getFilesNeedingHash methods
// have been moved to HasherCoordinator class

// Note: ButtonHasherStartClick, ButtonHasherStopClick, provideNextFileToHash, ButtonHasherClearClick methods
// have been moved to HasherCoordinator class

// Note: markwatchedStateChanged method has been moved to HasherCoordinator class

// Note: getNotifyPartsDone method has been moved to HasherCoordinator class

// Note: getNotifyFileHashed method has been moved to HasherCoordinator class

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

// Note: markwatchedStateChanged method has been moved to HasherCoordinator class

// Note: getNotifyPartsDone method has been moved to HasherCoordinator class

// Note: getNotifyFileHashed method has been moved to HasherCoordinator class

void Window::safeClose()
{
    // If exiting from tray, quit the application directly after logout timeout
    if (exitingFromTray && (!adbapi->LoggedIn() || waitforlogout.elapsed() > LOGOUT_TIMEOUT_MS)) {
        LOG("Exiting from tray - quitting application");
        QApplication::quit();
        return;
    }
    
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
    QList<FileHashInfo> unboundFiles = adbapi->getUnboundFiles();
    
    if(unboundFiles.isEmpty())
    {
        LOG("No unbound files found");
        return;
    }
    
    LOG(QString("Found %1 unbound files, adding to unknown files widget").arg(unboundFiles.size()));
    
    // Load anime titles cache once before processing files
    loadAnimeTitlesCache();
    
    // Disable updates during bulk insertion for performance
    unknownFilesManager->setUpdatesEnabled(false);
    
    // Add each unbound file to the unknown files widget
    for(const FileHashInfo& fileInfo : std::as_const(unboundFiles))
    {
        QFileInfo qFileInfo(fileInfo.path());
        QString filename = qFileInfo.fileName();
        
        // Get file size from filesystem if available
        qint64 fileSize = 0;
        if(qFileInfo.exists())
        {
            fileSize = qFileInfo.size();
        }
        
        unknownFilesManager->insertFile(filename, fileInfo.path(), fileInfo.hash(), fileSize);
    }
    
    // Re-enable updates after bulk insertion
    unknownFilesManager->setUpdatesEnabled(true);
    
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
    
    // Load mylist anime IDs in background thread (if not already running)
    // The database query is done in background, but card creation happens in UI thread
    if (!mylistLoadingThread || !mylistLoadingThread->isRunning()) {
        mylistLoadingThread = new QThread(this);
        MylistLoaderWorker *mylistWorker = new MylistLoaderWorker(dbName);
        mylistWorker->moveToThread(mylistLoadingThread);
        
        connect(mylistLoadingThread, &QThread::started, mylistWorker, &MylistLoaderWorker::doWork);
        connect(mylistWorker, &MylistLoaderWorker::finished, this, &Window::onMylistLoadingFinished);
        connect(mylistWorker, &MylistLoaderWorker::finished, mylistLoadingThread, &QThread::quit);
        connect(mylistLoadingThread, &QThread::finished, mylistWorker, &QObject::deleteLater);
        
        mylistLoadingThread->start();
    }
    
    // Start anime titles cache loading in background thread (if not already loaded)
    if (!animeTitlesCacheLoaded && (!animeTitlesLoadingThread || !animeTitlesLoadingThread->isRunning())) {
        animeTitlesLoadingThread = new QThread(this);
        AnimeTitlesLoaderWorker *titlesWorker = new AnimeTitlesLoaderWorker(dbName);
        titlesWorker->moveToThread(animeTitlesLoadingThread);
        
        connect(animeTitlesLoadingThread, &QThread::started, titlesWorker, &AnimeTitlesLoaderWorker::doWork);
        connect(titlesWorker, &AnimeTitlesLoaderWorker::finished, this, &Window::onAnimeTitlesLoadingFinished);
        connect(titlesWorker, &AnimeTitlesLoaderWorker::finished, animeTitlesLoadingThread, &QThread::quit);
        connect(animeTitlesLoadingThread, &QThread::finished, titlesWorker, &QObject::deleteLater);
        
        animeTitlesLoadingThread->start();
    }
    
    // Start unbound files loading in background thread (if not already running)
    if (!unboundFilesLoadingThread || !unboundFilesLoadingThread->isRunning()) {
        unboundFilesLoadingThread = new QThread(this);
        UnboundFilesLoaderWorker *unboundWorker = new UnboundFilesLoaderWorker(dbName);
        unboundWorker->moveToThread(unboundFilesLoadingThread);
        
        connect(unboundFilesLoadingThread, &QThread::started, unboundWorker, &UnboundFilesLoaderWorker::doWork);
        connect(unboundWorker, &UnboundFilesLoaderWorker::finished, this, &Window::onUnboundFilesLoadingFinished);
        connect(unboundWorker, &UnboundFilesLoaderWorker::finished, unboundFilesLoadingThread, &QThread::quit);
        connect(unboundFilesLoadingThread, &QThread::finished, unboundWorker, &QObject::deleteLater);
        
        unboundFilesLoadingThread->start();
    }
}

// Called when mylist loading finishes (in UI thread)
void Window::onMylistLoadingFinished(const QList<int> &aids)
{
    LOG(QString("Background loading: Mylist query complete with %1 anime, using virtual scrolling...").arg(aids.size()));
    
    // Store the mylist anime IDs for fast filtering later
    mylistAnimeIdSet.clear();
    for (int aid : aids) {
        mylistAnimeIdSet.insert(aid);
    }
    
    // Store the full unfiltered list of anime IDs (for filter reset)
    allAnimeIdsList = aids;
    
    // Clear existing cards
    cardManager->clearAllCards();
    
    // Set the virtual layout for virtual scrolling (this sets the item factory)
    cardManager->setVirtualLayout(mylistVirtualLayout);
    
    // Comprehensive preload of ALL data needed for card creation
    // This eliminates ALL SQL queries from createCard()
    if (!aids.isEmpty()) {
        LOG(QString("[Virtual Scrolling] Preloading comprehensive card data for %1 anime...").arg(aids.size()));
        cardManager->preloadCardCreationData(aids);
        LOG("[Virtual Scrolling] Comprehensive card data preload complete");
    }
    
    // Set the ordered anime ID list for virtual scrolling
    // The VirtualFlowLayout will request cards on-demand as user scrolls
    cardManager->setAnimeIdList(aids);
    
    // Update the virtual layout with the item count
    if (mylistVirtualLayout) {
        mylistVirtualLayout->setItemCount(aids.size());
        mylistVirtualLayout->refresh();
    }
    
    // Get all cards for backward compatibility (will be empty initially with virtual scrolling)
    animeCards = cardManager->getAllCards();
    
    // Reload alternative titles for filtering
    loadAnimeAlternativeTitlesForFiltering();
    
    // Restore filter settings first
    restoreMylistSorting();
    
    // Apply current filters, then sorting
    // (sorting must happen after filtering to preserve sort order on filtered results)
    applyMylistFilters();
    
    // Apply sorting after filtering
    LOG("[Window] Calling sortMylistCards()");
    sortMylistCards(filterSidebar->getSortIndex());
    LOG("[Window] sortMylistCards() returned");
    
    mylistStatusLabel->setText(QString("MyList Status: %1 anime (virtual scrolling)").arg(aids.size()));
    LOG(QString("[Virtual Scrolling] Ready to display %1 anime").arg(aids.size()));
    
    // Mark initial loading as complete so new anime can be detected
    LOG("[Window] Setting initial load complete");
    cardManager->setInitialLoadComplete();
    LOG("[Window] Initial load complete set");
    
    // Perform initial scan for file marking after mylist is loaded
    if (watchSessionManager) {
        LOG("[Window] Mylist loaded, triggering initial file marking scan");
        watchSessionManager->performInitialScan();
        LOG("[Window] Initial file marking scan triggered");
    }
    LOG("[Window] onMylistLoadingFinished complete");
}



// Called when anime titles loading finishes (in UI thread)
void Window::onAnimeTitlesLoadingFinished(const QStringList &titles, const QMap<QString, int> &titleToAid)
{
    QMutexLocker locker(&backgroundLoadingMutex);
    LOG("Background loading: Anime titles cache loaded successfully");
    animeTitlesCacheLoaded = true;
    
    // Store the data in member variables
    cachedAnimeTitles = titles;
    cachedTitleToAid = titleToAid;
    
    // Update unknown files manager cache
    unknownFilesManager->setAnimeTitlesCache(titles, titleToAid);
}

// Called when unbound files loading finishes (in UI thread)
void Window::onUnboundFilesLoadingFinished(const QList<LocalFileInfo> &files)
{
    LOG(QString("Background loading: Unbound files loaded, adding %1 files to UI...").arg(files.size()));
    
    if (files.isEmpty()) {
        LOG("No unbound files found");
        return;
    }
    
    // Disable updates during bulk insertion for performance
    unknownFilesManager->setUpdatesEnabled(false);
    
    // Add each unbound file to the unknown files widget
    for (const LocalFileInfo& fileInfo : std::as_const(files)) {
        unknownFilesManager->insertFile(fileInfo.filename(), fileInfo.filepath(), fileInfo.hash(), fileInfo.size());
    }
    
    // Re-enable updates after bulk insertion
    unknownFilesManager->setUpdatesEnabled(true);
    
    LOG(QString("Successfully added %1 unbound files to UI").arg(files.size()));
}

void Window::saveMylistSorting()
{
    // Save all filter sidebar settings (except search text)
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    QSqlQuery q(db);
    
    // Save sort settings
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_card_sort_index', ?)");
    q.addBindValue(filterSidebar->getSortIndex());
    q.exec();
    
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_card_sort_ascending', ?)");
    q.addBindValue(filterSidebar->getSortAscending() ? 1 : 0);
    q.exec();
    
    // Save filter settings
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_filter_type', ?)");
    q.addBindValue(filterSidebar->getTypeFilter());
    q.exec();
    
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_filter_completion', ?)");
    q.addBindValue(filterSidebar->getCompletionFilter());
    q.exec();
    
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_filter_unwatched', ?)");
    q.addBindValue(filterSidebar->getShowOnlyUnwatched() ? 1 : 0);
    q.exec();
    
    
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_filter_inmylist', ?)");
    q.addBindValue(filterSidebar->getInMyListOnly() ? 1 : 0);
    q.exec();
    
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_filter_serieschain', ?)");
    q.addBindValue(filterSidebar->getShowSeriesChain() ? 1 : 0);
    q.exec();
    
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_filter_adultcontent', ?)");
    q.addBindValue(filterSidebar->getAdultContentFilter());
    q.exec();
    
    // Log saved settings for debugging
    LOG(QString("Saved mylist sort settings: index=%1, ascending=%2")
        .arg(filterSidebar->getSortIndex())
        .arg(filterSidebar->getSortAscending()));
    LOG(QString("Saved mylist filter settings: type=%1, completion=%2, unwatched=%3")
        .arg(filterSidebar->getTypeFilter(), filterSidebar->getCompletionFilter())
        .arg(filterSidebar->getShowOnlyUnwatched()));
    LOG(QString("Saved mylist view settings: inmylist=%1, serieschain=%2, adult=%3")
        .arg(filterSidebar->getInMyListOnly())
        .arg(filterSidebar->getShowSeriesChain())
        .arg(filterSidebar->getAdultContentFilter()));
}

void Window::restoreMylistSorting()
{
    // Load all filter sidebar settings (except search text)
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        LOG("restoreMylistSorting: Database not open");
        return;
    }
    
    // Batch load all settings in a single query for better performance
    QMap<QString, QString> settings;
    QSqlQuery q(db);
    q.prepare("SELECT name, value FROM settings WHERE name LIKE 'mylist_%'");
    if (q.exec()) {
        while (q.next()) {
            settings[q.value(0).toString()] = q.value(1).toString();
        }
    }
    
    // Apply sort settings
    if (settings.contains("mylist_card_sort_index")) {
        int sortIndex = settings["mylist_card_sort_index"].toInt();
        filterSidebar->setSortIndex(sortIndex);
        LOG(QString("Restored sort index: %1").arg(sortIndex));
    }
    
    if (settings.contains("mylist_card_sort_ascending")) {
        bool sortAscending = settings["mylist_card_sort_ascending"].toInt() != 0;
        filterSidebar->setSortAscending(sortAscending);
        LOG(QString("Restored sort ascending: %1").arg(sortAscending));
    }
    
    // Apply filter settings
    if (settings.contains("mylist_filter_type")) {
        filterSidebar->setTypeFilter(settings["mylist_filter_type"]);
    }
    
    if (settings.contains("mylist_filter_completion")) {
        filterSidebar->setCompletionFilter(settings["mylist_filter_completion"]);
    }
    
    if (settings.contains("mylist_filter_unwatched")) {
        filterSidebar->setShowOnlyUnwatched(settings["mylist_filter_unwatched"].toInt() != 0);
    }
    
    
    if (settings.contains("mylist_filter_inmylist")) {
        filterSidebar->setInMyListOnly(settings["mylist_filter_inmylist"].toInt() != 0);
    }
    
    if (settings.contains("mylist_filter_serieschain")) {
        filterSidebar->setShowSeriesChain(settings["mylist_filter_serieschain"].toInt() != 0);
    }
    
    if (settings.contains("mylist_filter_adultcontent")) {
        filterSidebar->setAdultContentFilter(settings["mylist_filter_adultcontent"]);
    }
    
    LOG(QString("Restored %1 mylist filter settings from database").arg(settings.size()));
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
	
	// Note: All UI updates are handled by HasherCoordinator::onHashingFinished()
	// File identification happens immediately in HasherCoordinator::onFileHashed()
}

// Note: processPendingHashedFiles method has been moved to HasherCoordinator class

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
    setColumnWidth(3, 290);  // Increased to accommodate Re-check and Delete buttons
    // Removed setMaximumHeight(200) to allow table to expand vertically with splitter
    
    // Set context menu policy
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

void unknown_files_::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Execute file on double-click
    if (event->button() == Qt::LeftButton)
    {
        executeFile();
    }
    QTableWidget::mouseDoubleClickEvent(event);
}

void unknown_files_::contextMenuEvent(QContextMenuEvent *event)
{
    // Create context menu
    QMenu contextMenu(this);
    
    QAction *executeAction = contextMenu.addAction("Execute");
    QAction *openLocationAction = contextMenu.addAction("Open Location");
    
    QAction *selectedAction = contextMenu.exec(event->globalPos());
    
    if (selectedAction == executeAction)
    {
        executeFile();
    }
    else if (selectedAction == openLocationAction)
    {
        openFileLocation();
    }
}

void unknown_files_::executeFile()
{
    int row = currentRow();
    if (row < 0) return;
    
    // Get the file path from the window's unknownFilesManager->getFilesData()
    Window *window = qobject_cast<Window*>(this->window());
    if (!window) return;
    
    const QMap<int, LocalFileInfo>& filesData = window->getUnknownFilesManager()->getFilesData();
    if (filesData.contains(row))
    {
        QString filePath = filesData[row].filepath();
        if (!filePath.isEmpty())
        {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        }
    }
}

void unknown_files_::openFileLocation()
{
    int row = currentRow();
    if (row < 0) return;
    
    // Get the file path from the window's unknownFilesManager->getFilesData()
    Window *window = qobject_cast<Window*>(this->window());
    if (!window) return;
    
    const QMap<int, LocalFileInfo>& filesData = window->getUnknownFilesManager()->getFilesData();
    if (filesData.contains(row))
    {
        QString filePath = filesData[row].filepath();
        if (!filePath.isEmpty())
        {
            QFileInfo fileInfo(filePath);
            QString dirPath = fileInfo.absolutePath();
            QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
        }
    }
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
	a = QString("%1: %2").arg(t.toString(), str);
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
    // First check if we should close to tray
    if (trayIconManager->isCloseToTrayEnabled() && trayIconManager->isSystemTrayAvailable() && 
        trayIconManager->isTrayIconVisible()) {
        this->hide();
        event->ignore();
        LOG("Window close intercepted, hidden to tray");
        
        // Show balloon notification once to inform user
        static bool notificationShown = false;
        if (!notificationShown) {
            trayIconManager->showMessage("Usagi-dono", 
                                        "Application minimized to tray. Use tray menu to exit.",
                                        QSystemTrayIcon::Information, 3000);
            notificationShown = true;
        }
        return;
    }
    
    // Otherwise, handle normal close with logout
    if(adbapi->LoggedIn() && !safeclose->isActive())
	{
		adbapi->Logout();
        waitforlogout.start();
        safeclose->start();
        event->ignore();
	}
    else if(!adbapi->LoggedIn() || waitforlogout.elapsed() > LOGOUT_TIMEOUT_MS)
    {
        event->accept();
        LOG("Window close accepted, application exiting");
    }
    else
    {
        // Still waiting for logout to complete
        event->ignore();
    }
}

void Window::changeEvent(QEvent *event)
{
    // Handle minimize to tray
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized() && trayIconManager->isMinimizeToTrayEnabled() && 
            trayIconManager->isSystemTrayAvailable() && trayIconManager->isTrayIconVisible()) {
            // Save window state and geometry before hiding
            // Use QWindowStateChangeEvent to get the previous state (before minimize)
            auto *stateEvent = static_cast<QWindowStateChangeEvent *>(event);
            windowStateBeforeHide = stateEvent->oldState();
            // Use normalGeometry() which returns the correct geometry even when minimized
            windowGeometryBeforeHide = this->normalGeometry();
            
            // Hide window and show in tray
            this->hide();
            event->ignore();
            LOG("Window minimized to tray");
            return;
        }
    }
    
    QWidget::changeEvent(event);
}

void Window::saveSettings()
{
	LOG("Saving settings - username: " + editLogin->text());
	adbapi->setUsername(editLogin->text());
	adbapi->setPassword(editPassword->text());
    
    // Save directory watcher settings to database
    if (directoryWatcherManager) {
        directoryWatcherManager->saveSettingsToApi();
    }
	
    // Save auto-fetch settings to database
    if (autoFetchManager) {
        autoFetchManager->saveSettingsToApi();
    }
	
	// Save media player path
	PlaybackManager::setMediaPlayerPath(mediaPlayerPath->text());
	
    // Save tray settings and update tray manager
    if (traySettingsManager) {
        traySettingsManager->saveSettingsToApi(adbapi, trayIconManager);
        LOG("Tray settings saved");
    }
	
	// Save auto-start settings
	bool autoStartWasEnabled = adbapi->getAutoStartEnabled();
	bool autoStartNowEnabled = autoStartEnabled->isChecked();
	adbapi->setAutoStartEnabled(autoStartNowEnabled);
	
	// Update auto-start registration if changed
	if (autoStartWasEnabled != autoStartNowEnabled) {
		setAutoStartEnabled(autoStartNowEnabled);
	}
	
	// Save file marking preferences
	QLineEdit *audioEdit = this->findChild<QLineEdit*>("preferredAudioLanguagesEdit");
	if (audioEdit) {
		adbapi->setPreferredAudioLanguages(audioEdit->text());
	}
	
	QLineEdit *subtitleEdit = this->findChild<QLineEdit*>("preferredSubtitleLanguagesEdit");
	if (subtitleEdit) {
		adbapi->setPreferredSubtitleLanguages(subtitleEdit->text());
	}
	
	QCheckBox *versionCheckbox = this->findChild<QCheckBox*>("preferHighestVersionCheckbox");
	if (versionCheckbox) {
		adbapi->setPreferHighestVersion(versionCheckbox->isChecked());
	}
	
	QCheckBox *qualityCheckbox = this->findChild<QCheckBox*>("preferHighestQualityCheckbox");
	if (qualityCheckbox) {
		adbapi->setPreferHighestQuality(qualityCheckbox->isChecked());
	}
	
	QDoubleSpinBox *bitrateSpinBox = this->findChild<QDoubleSpinBox*>("preferredBitrateSpinBox");
	if (bitrateSpinBox) {
		adbapi->setPreferredBitrate(bitrateSpinBox->value());
	}
	
	QComboBox *resolutionCombo = this->findChild<QComboBox*>("preferredResolutionCombo");
	if (resolutionCombo) {
		adbapi->setPreferredResolution(resolutionCombo->currentText());
	}
	
	// Save hasher filter masks
	QLineEdit *hasherFilterMasksEdit = this->findChild<QLineEdit*>("hasherFilterMasksEdit");
	if (hasherFilterMasksEdit) {
		adbapi->setHasherFilterMasks(hasherFilterMasksEdit->text());
	}
	
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
                    LOG(QString("Updating anime card for lid=%1 after successful mylist add (code 310)").arg(lid));
                    updateOrAddMylistEntry(lid);
                    
                    // Note: Deletion mechanism now uses on-demand file selection
                    // autoMarkFilesForDeletion() is simplified to just trigger deletion when space is low
                    if (watchSessionManager && watchSessionManager->isAutoMarkDeletionEnabled()) {
                        watchSessionManager->autoMarkFilesForDeletion();
                    }
                }
                else
                {
                    LOG(QString("WARNING: UpdateLocalPath returned lid=%1 for path=%2 (code 310 - already in mylist). Card may not be created/updated.")
                        .arg(lid).arg(localPath));
                    
                    // Even if we couldn't find the mylist entry in the local database,
                    // we know the file is in AniDB's mylist (310 response), so update binding_status
                    // to prevent the file from reappearing in unknown files list on restart
                    adbapi->UpdateLocalFileBindingStatus(localPath, 1); // 1 = bound_to_anime
                    adbapi->UpdateLocalFileStatus(localPath, 2); // 2 = in anidb
                }
                
                // Remove from unknown files widget if present (re-check succeeded)
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
                QTableWidget *unknownFilesTable = unknownFilesManager->getTableWidget();
                bool alreadyExists = false;
                for(int row = 0; row < unknownFilesTable->rowCount(); ++row)
                {
                    QTableWidgetItem *item = unknownFilesTable->item(row, 0);
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
                    
                    unknownFilesManager->insertFile(filename, filepath, hash, fileSize);
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
				
				if(hasherCoordinator->getRenameTo()->checkState() > 0)
				{
					// TODO: rename
				}
				
				// Update only the specific mylist entry instead of reloading entire tree
				if(lid > 0)
				{
					LOG(QString("Updating anime card for lid=%1 after successful mylist add (code %2)").arg(lid).arg(code));
					updateOrAddMylistEntry(lid);
					
					// Note: Deletion mechanism now uses on-demand file selection
					if (watchSessionManager && watchSessionManager->isAutoMarkDeletionEnabled()) {
						watchSessionManager->autoMarkFilesForDeletion();
					}
				}
				else
				{
					LOG(QString("WARNING: UpdateLocalPath returned lid=%1 for path=%2 (code %3 - newly added). Card may not be created/updated.")
						.arg(lid).arg(localPath).arg(code));
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

void Window::getNotifyEpisodeUpdated(int eid, int aid)
{
	// Episode data was updated in the database, update only the specific episode item
	LOG(QString("Episode data received for EID %1 (AID %2), updating field...").arg(eid).arg(aid));
	
	if (cardManager) {
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
	
	// Update alternative titles cache for this anime
	updateAnimeAlternativeTitlesInCache(aid);
	
	if (cardManager) {
		cardManager->onAnimeUpdated(aid);
		// Backward compatibility: update animeCards list
		animeCards = cardManager->getAllCards();
		
		// Sort the cards
		sortMylistCards(filterSidebar->getSortIndex());
		
		// If series chain display is enabled, check for more missing relation data in this anime's chain
		// This continues building the chain as relation data arrives, but only for the updated anime
		if (filterSidebar && filterSidebar->getShowSeriesChain() && watchSessionManager) {
			LOG(QString("[Window] Series chain display enabled - checking chain for anime %1").arg(aid));
			checkAndRequestChainRelations(aid);
		}
	}
	
	qint64 totalElapsed = timer.elapsed();
	LOG(QString("[Timing] Total getNotifyAnimeUpdated for AID %1 took %2 ms").arg(aid).arg(totalElapsed));
}

void Window::updateEpisodeInTree(int eid, int aid)
{
	// Tree view has been removed - this function is now a no-op
	// Episode updates are handled by the card manager
	Q_UNUSED(eid);
	Q_UNUSED(aid);
}



void Window::updateOrAddMylistEntry(int lid)
{
	if (cardManager) {
		cardManager->updateOrAddMylistEntry(lid);
		// Backward compatibility: update animeCards list
		animeCards = cardManager->getAllCards();
	}
}

void Window::hashesinsertrow(QFileInfo file, Qt::CheckState ren, const QString& preloadedHash)
{
	// Delegate to HasherCoordinator which owns the hashes table and hasher logic
	hasherCoordinator->hashesInsertRow(file, ren, preloadedHash);
}

void Window::loadMylistFromDatabase()
{
	loadMylistAsCards();
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
	QMap<QString, FileHashInfo> hashInfoMap = adbapi->batchGetLocalFileHashes(filePaths);
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
		
		// Check if file should be filtered (delegate to HasherCoordinator)
		if (hasherCoordinator->shouldFilterFile(filePath)) {
			continue; // Skip this file
		}
		
		QString preloadedHash = hashInfoMap.contains(filePath) ? hashInfoMap[filePath].hash() : QString();
		hashesinsertrow(fileInfo, Qt::Unchecked, preloadedHash);
	}
	
	// Re-enable table updates after bulk insertion
	hashes->setUpdatesEnabled(true);
	
	qint64 insertTime = insertTimer.elapsed();
	LOG(QString("[TIMING] hashesinsertrow() loop for %1 files: %2 ms [window.cpp]")
		.arg(filePaths.size()).arg(insertTime));
	
	// Directory watcher always auto-starts the hasher for detected files
	if (!hasherThreadPool->isRunning()) {
		// Note: Hasher settings (addtomylist, markwatched, hasherFileState) are now in HasherCoordinator
		// They are set to defaults in HasherCoordinator constructor:
		// - addtomylist: checked
		// - markwatched: unchecked (no change)
		// - hasherFileState: 1 (Internal HDD)
		
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
			
			// Queue for deferred processing using new HashingTask class
			HashingTask task(filePath, filename, hexdigest, fileSize);
			task.setRowIndex(rowIndex);
			task.setUseUserSettings(false);  // Use auto-watcher defaults
			task.setAddToMylist(true);  // Auto-watcher always adds to mylist when logged in
			task.setMarkWatchedState(Qt::Unchecked);  // Default for auto-watcher
			task.setFileState(1);  // Internal (HDD)
			pendingHashedFilesQueue.append(task);
		}
		
		// Start timer to process queued files in batches (keeps UI responsive)
		if (!filesWithHashes.isEmpty()) {
			LOG(QString("Queued %1 already-hashed file(s) for deferred processing").arg(filesWithHashes.size()));
			hashedFilesProcessingTimer->start();
		}
		
		// Start hashing for files without existing hashes
		if (filesToHashCount > 0) {
			// Calculate total hash parts for progress tracking
			QStringList filesToHash = hasherCoordinator->getFilesNeedingHash();
			hasherCoordinator->setupHashingProgress(filesToHash);
			
			// Start hashing all detected files that need hashing
			hasherCoordinator->getButtonStart()->setEnabled(false);
			hasherCoordinator->getButtonClear()->setEnabled(false);
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
	// Tree view has been removed - play button clicks are handled by card view
	Q_UNUSED(index);
}

void Window::onPlaybackPositionUpdated(int lid, int position, int duration)
{
	LOG(QString("Playback position updated: LID %1, %2/%3s").arg(lid).arg(position).arg(duration));
	// The position is already saved by PlaybackManager, no need to do anything here
}


void Window::onPlaybackCompleted(int lid)
{
	LOG(QString("Playback completed: LID %1 - Updating play buttons").arg(lid));
	
	// Remove from playing items
	m_playingItems.remove(lid);
	if (m_playingItems.isEmpty()) {
		m_animationTimer->stop();
	}
	
	// Update anime card
	updateUIForWatchedFile(lid);
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

void Window::onFileMarkedAsLocallyWatched(int lid)
{
	LOG(QString("File marked as locally watched via chunk tracking: LID %1 - Updating UI").arg(lid));
	
	// Update anime card
	updateUIForWatchedFile(lid);
}

void Window::updateUIForWatchedFile(int lid)
{
	// Update anime card
	if (cardManager) {
		cardManager->updateOrAddMylistEntry(lid);
	}
}

void Window::onPlaybackStateChanged(int lid, bool isPlaying)
{
	// Tree view animation removed - just manage timer state for card view
	if (isPlaying) {
		// Add to playing items with initial animation frame
		m_playingItems[lid] = 0;
		if (!m_animationTimer->isActive()) {
			m_animationTimer->start();
		}
	} else {
		// Remove from playing items
		m_playingItems.remove(lid);
		if (m_playingItems.isEmpty()) {
			m_animationTimer->stop();
		}
	}
}

void Window::onAnimationTimerTimeout()
{
	// Tree view animation has been removed
	// Card view handles its own animation
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

// Sort cards based on selected criterion
void Window::sortMylistCards(int sortIndex)
{
	// Prevent concurrent sort operations to avoid race conditions
	QMutexLocker locker(&filterOperationsMutex);
	
	// Get the list of anime IDs from the card manager
	QList<int> animeIds = cardManager->getAnimeIdList();
	
	if (animeIds.isEmpty()) {
		// Fallback: try to get IDs from animeCards for backward compatibility
		if (animeCards.isEmpty()) {
			return;
		}
		// Build ID list from existing cards
		for (AnimeCard* card : std::as_const(animeCards)) {
			animeIds.append(card->getAnimeId());
		}
	}
	
	bool sortAscending = filterSidebar->getSortAscending();
	bool seriesChainEnabled = filterSidebar && filterSidebar->getShowSeriesChain();
	
	// When series chain is enabled, we apply nested sorting:
	// - Sort the series chains by the selected criterion (using first anime in chain as representative)
	// - Within each chain, maintain sequential order (prequel -> sequel)
	// This happens in applyMylistFilters(), but sortMylistCards() needs to re-sort when user changes criteria
	
	// For sorting, we need to access card data. Build a combined map from all sources.
	QMap<int, AnimeCard*> cardsMap;
	
	// First add cards from the card manager
	for (AnimeCard* card : cardManager->getAllCards()) {
		cardsMap[card->getAnimeId()] = card;
	}
	
	// Also include cards from the legacy animeCards list (for backward compatibility)
	for (AnimeCard* card : std::as_const(animeCards)) {
		if (!cardsMap.contains(card->getAnimeId())) {
			cardsMap[card->getAnimeId()] = card;
		}
	}
	
	// Helper lambda to get cached data for an anime
	// This allows sorting to work with virtual scrolling where cards may not exist
	auto getCachedData = [this](int aid) -> MyListCardManager::CachedAnimeData {
		return cardManager->getCachedAnimeData(aid);
	};
	
	// Handle nested sorting when series chain is enabled
	if (seriesChainEnabled) {
		LOG("[Window] Series chain enabled - delegating to MyListCardManager for chain sorting");
		
		// Map UI sortIndex to AnimeChain::SortCriteria
		AnimeChain::SortCriteria chainCriteria;
		switch (sortIndex) {
			case 0:  // Anime Title
				chainCriteria = AnimeChain::SortCriteria::ByRepresentativeTitle;
				break;
			case 1:  // Type
				chainCriteria = AnimeChain::SortCriteria::ByRepresentativeType;
				break;
			case 2:  // Aired Date
				chainCriteria = AnimeChain::SortCriteria::ByRepresentativeDate;
				break;
			case 3:  // Episodes (Count)
				chainCriteria = AnimeChain::SortCriteria::ByRepresentativeEpisodeCount;
				break;
			case 4:  // Completion %
				chainCriteria = AnimeChain::SortCriteria::ByRepresentativeCompletion;
				break;
			case 5:  // Last Played
				chainCriteria = AnimeChain::SortCriteria::ByRepresentativeLastPlayed;
				break;
			default:
				chainCriteria = AnimeChain::SortCriteria::ByRepresentativeDate;
				break;
		}
		
		LOG(QString("[Window] Sorting chains by criteria %1 (sortIndex=%2), ascending=%3")
			.arg(static_cast<int>(chainCriteria)).arg(sortIndex).arg(sortAscending));
		
		// Sort chains using the selected criteria and sort order
		cardManager->sortChains(chainCriteria, sortAscending);
		
		// Get the updated anime ID list from card manager (already reordered)
		animeIds = cardManager->getAnimeIdList();
		
		// Refresh the virtual layout to display the reordered items
		// This must be done AFTER sortChains returns, not from within sortChains,
		// to avoid re-entrancy issues during the sorting operation
		if (mylistVirtualLayout) {
			mylistVirtualLayout->refresh();
		}
		
		// Also update the legacy animeCards list order for backward compatibility
		animeCards.clear();
		for (int aid : animeIds) {
			AnimeCard* card = cardManager->getCard(aid);
			if (card) {
				animeCards.append(card);
			}
		}
		
		// If not using virtual scrolling (backward compatibility), update the regular flow layout
		if (!mylistVirtualLayout && mylistCardLayout) {
			LOG("[Window] Updating flow layout for non-virtual mode");
			// Remove all cards from layout
			for (AnimeCard* const card : std::as_const(animeCards)) {
				mylistCardLayout->removeWidget(card);
			}
			// Re-add cards to layout in sorted order
			for (AnimeCard* const card : std::as_const(animeCards)) {
				mylistCardLayout->addWidget(card);
			}
			LOG("[Window] Flow layout updated for non-virtual mode");
		}
		
		return;  // Done with chain sorting
	}
	
	// Regular sorting (no series chain grouping)
	// Sort based on criterion
	// Use cached data when card widgets don't exist (virtual scrolling)
	switch (sortIndex) {
		case 0: // Anime Title
			std::sort(animeIds.begin(), animeIds.end(), [&cardsMap, &getCachedData, sortAscending](int aidA, int aidB) {
				AnimeCard* a = cardsMap.value(aidA);
				AnimeCard* b = cardsMap.value(aidB);
				
				// Get data from cards or cache
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden();
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden();
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName();
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName();
				
				// Hidden cards always go to the bottom
				if (hiddenA != hiddenB) {
					return hiddenB;  // non-hidden comes before hidden
				}
				
				if (sortAscending) {
					return titleA < titleB;
				} else {
					return titleA > titleB;
				}
			});
			break;
		case 1: // Type
			std::sort(animeIds.begin(), animeIds.end(), [&cardsMap, &getCachedData, sortAscending](int aidA, int aidB) {
				AnimeCard* a = cardsMap.value(aidA);
				AnimeCard* b = cardsMap.value(aidB);
				
				// Get data from cards or cache
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden();
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden();
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName();
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName();
				QString typeA = a ? a->getAnimeType() : getCachedData(aidA).typeName();
				QString typeB = b ? b->getAnimeType() : getCachedData(aidB).typeName();
				
				// Hidden cards always go to the bottom
				if (hiddenA != hiddenB) {
					return hiddenB;  // non-hidden comes before hidden
				}
				
				if (typeA == typeB) {
					return titleA < titleB;
				}
				if (sortAscending) {
					return typeA < typeB;
				} else {
					return typeA > typeB;
				}
			});
			break;
		case 2: // Aired Date
			std::sort(animeIds.begin(), animeIds.end(), [&cardsMap, &getCachedData, sortAscending](int aidA, int aidB) {
				AnimeCard* a = cardsMap.value(aidA);
				AnimeCard* b = cardsMap.value(aidB);
				
				// Get data from cards or cache
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden();
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden();
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName();
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName();
				
				// Hidden cards always go to the bottom
				if (hiddenA != hiddenB) {
					return hiddenB;  // non-hidden comes before hidden
				}
				
				// Get aired dates
				aired airedA, airedB;
				if (a) {
					airedA = a->getAired();
				} else {
					MyListCardManager::CachedAnimeData cachedA = getCachedData(aidA);
					airedA = aired(cachedA.startDate(), cachedA.endDate());
				}
				if (b) {
					airedB = b->getAired();
				} else {
					MyListCardManager::CachedAnimeData cachedB = getCachedData(aidB);
					airedB = aired(cachedB.startDate(), cachedB.endDate());
				}
				
				// Handle invalid dates (put at end)
				if (!airedA.isValid() && !airedB.isValid()) {
					return titleA < titleB;
				}
				if (!airedA.isValid()) {
					return false;  // a goes after b
				}
				if (!airedB.isValid()) {
					return true;   // a goes before b
				}
				
				// Use aired class comparison operators
				if (airedA == airedB) {
					return titleA < titleB;
				}
				if (sortAscending) {
					return airedA < airedB;
				} else {
					return airedA > airedB;
				}
			});
			break;
		case 3: // Episodes (Count)
			std::sort(animeIds.begin(), animeIds.end(), [&cardsMap, &getCachedData, sortAscending](int aidA, int aidB) {
				AnimeCard* a = cardsMap.value(aidA);
				AnimeCard* b = cardsMap.value(aidB);
				
				// Get data from cards or cache
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden();
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden();
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName();
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName();
				
				// Hidden cards always go to the bottom
				if (hiddenA != hiddenB) {
					return hiddenB;  // non-hidden comes before hidden
				}
				
				int episodesA, episodesB;
				if (a) {
					episodesA = a->getNormalEpisodes() + a->getOtherEpisodes();
				} else {
					MyListCardManager::CachedAnimeData cachedA = getCachedData(aidA);
					episodesA = cachedA.stats().normalEpisodes() + cachedA.stats().otherEpisodes();
				}
				if (b) {
					episodesB = b->getNormalEpisodes() + b->getOtherEpisodes();
				} else {
					MyListCardManager::CachedAnimeData cachedB = getCachedData(aidB);
					episodesB = cachedB.stats().normalEpisodes() + cachedB.stats().otherEpisodes();
				}
				
				if (episodesA == episodesB) {
					return titleA < titleB;
				}
				if (sortAscending) {
					return episodesA < episodesB;
				} else {
					return episodesA > episodesB;
				}
			});
			break;
		case 4: // Completion %
			std::sort(animeIds.begin(), animeIds.end(), [&cardsMap, &getCachedData, sortAscending](int aidA, int aidB) {
				AnimeCard* a = cardsMap.value(aidA);
				AnimeCard* b = cardsMap.value(aidB);
				
				// Get data from cards or cache
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden();
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden();
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName();
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName();
				
				// Hidden cards always go to the bottom
				if (hiddenA != hiddenB) {
					return hiddenB;  // non-hidden comes before hidden
				}
				
				int totalEpisodesA, totalEpisodesB, viewedA, viewedB;
				if (a) {
					totalEpisodesA = a->getNormalEpisodes() + a->getOtherEpisodes();
					viewedA = a->getNormalViewed() + a->getOtherViewed();
				} else {
					MyListCardManager::CachedAnimeData cachedA = getCachedData(aidA);
					totalEpisodesA = cachedA.stats().normalEpisodes() + cachedA.stats().otherEpisodes();
					viewedA = cachedA.stats().normalViewed() + cachedA.stats().otherViewed();
				}
				if (b) {
					totalEpisodesB = b->getNormalEpisodes() + b->getOtherEpisodes();
					viewedB = b->getNormalViewed() + b->getOtherViewed();
				} else {
					MyListCardManager::CachedAnimeData cachedB = getCachedData(aidB);
					totalEpisodesB = cachedB.stats().normalEpisodes() + cachedB.stats().otherEpisodes();
					viewedB = cachedB.stats().normalViewed() + cachedB.stats().otherViewed();
				}
				
				double completionA = (totalEpisodesA > 0) ? static_cast<double>(viewedA) / totalEpisodesA : 0.0;
				double completionB = (totalEpisodesB > 0) ? static_cast<double>(viewedB) / totalEpisodesB : 0.0;
				if (completionA == completionB) {
					return titleA < titleB;
				}
				if (sortAscending) {
					return completionA < completionB;
				} else {
					return completionA > completionB;
				}
			});
			break;
		case 5: // Last Played
			std::sort(animeIds.begin(), animeIds.end(), [&cardsMap, &getCachedData, sortAscending](int aidA, int aidB) {
				AnimeCard* a = cardsMap.value(aidA);
				AnimeCard* b = cardsMap.value(aidB);
				
				// Get data from cards or cache
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden();
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden();
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName();
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName();
				
				// Hidden cards always go to the bottom
				if (hiddenA != hiddenB) {
					return hiddenB;  // non-hidden comes before hidden
				}
				
				qint64 lastPlayedA = a ? a->getLastPlayed() : getCachedData(aidA).lastPlayed();
				qint64 lastPlayedB = b ? b->getLastPlayed() : getCachedData(aidB).lastPlayed();
				
				// Never played items (0) go to the end regardless of sort order
				if (lastPlayedA == 0 && lastPlayedB == 0) {
					return titleA < titleB;
				}
				if (lastPlayedA == 0) {
					return false;  // a goes after b
				}
				if (lastPlayedB == 0) {
					return true;   // a goes before b
				}
				
				if (sortAscending) {
					return lastPlayedA < lastPlayedB;
				} else {
					return lastPlayedA > lastPlayedB;
				}
			});
			break;
	}
	
	// Update the card manager with the new order
	cardManager->setAnimeIdList(animeIds, false);  // Chain mode is disabled when in regular sorting mode
	
	// If using virtual scrolling, refresh the layout
	if (mylistVirtualLayout) {
		mylistVirtualLayout->refresh();
	}
	
	// Also update the legacy animeCards list order for backward compatibility
	animeCards.clear();
	for (int aid : animeIds) {
		AnimeCard* card = cardManager->getCard(aid);
		if (card) {
			animeCards.append(card);
		}
	}
	
	// If not using virtual scrolling (backward compatibility), update the regular flow layout
	if (!mylistVirtualLayout && mylistCardLayout) {
		// Remove all cards from layout
		for (AnimeCard* const card : std::as_const(animeCards)) {
			mylistCardLayout->removeWidget(card);
		}
		// Re-add cards to layout in sorted order
		for (AnimeCard* const card : std::as_const(animeCards)) {
			mylistCardLayout->addWidget(card);
		}
	}
}

// Toggle sort order between ascending and descending (deprecated - now handled by sidebar)
void Window::toggleSortOrder()
{
	// This function is no longer used as sorting is handled by the sidebar
	// Kept for backward compatibility
}

// Load mylist data as cards using virtual scrolling
void Window::loadMylistAsCards()
{
	LOG(QString("[Window] loadMylistAsCards - loading mylist directly"));
	
	// Set the layout for the card manager
	cardManager->setCardLayout(mylistCardLayout);
	
	// Set the virtual layout for virtual scrolling
	cardManager->setVirtualLayout(mylistVirtualLayout);
	
	// Connect card manager to API update signals
	connect(adbapi, &myAniDBApi::notifyEpisodeUpdated, 
	        cardManager, &MyListCardManager::onEpisodeUpdated, Qt::UniqueConnection);
	connect(adbapi, &myAniDBApi::notifyAnimeUpdated,
	        cardManager, &MyListCardManager::onAnimeUpdated, Qt::UniqueConnection);
	
	// Clear existing cards
	cardManager->clearAllCards();
	
	// Show status
	mylistStatusLabel->setText("MyList Status: Loading mylist anime...");
	
	// Query mylist anime IDs
	QSqlDatabase db = QSqlDatabase::database();
	if (!db.isOpen()) {
		LOG("[Window] Database not open");
		mylistStatusLabel->setText("MyList Status: Error - database not open");
		return;
	}
	
	QString query = "SELECT DISTINCT m.aid FROM mylist m ORDER BY m.aid";
	QSqlQuery q(db);
	
	if (!q.exec(query)) {
		LOG(QString("[Window] Error loading mylist: %1").arg(q.lastError().text()));
		mylistStatusLabel->setText("MyList Status: Error loading mylist");
		return;
	}
	
	QList<int> aids;
	while (q.next()) {
		int aid = q.value(0).toInt();
		aids.append(aid);
	}
	
	LOG(QString("[Window] Found %1 mylist anime").arg(aids.size()));
	
	// Store the mylist anime IDs for fast filtering later
	mylistAnimeIdSet.clear();
	for (int aid : aids) {
		mylistAnimeIdSet.insert(aid);
	}
	
	// Store the full unfiltered list of anime IDs (for filter reset)
	allAnimeIdsList = aids;
	
	// Preload data for ALL anime in database (not just mylist)
	// This enables proper chain building even when some anime in a chain are not in mylist
	QList<int> allAnimeIds;
	QSqlQuery allAnimeQuery(db);
	if (allAnimeQuery.exec("SELECT aid FROM anime")) {
		while (allAnimeQuery.next()) {
			allAnimeIds.append(allAnimeQuery.value(0).toInt());
		}
		LOG(QString("[Window] Preloading data for %1 total anime (including %2 in mylist)").arg(allAnimeIds.size()).arg(aids.size()));
		mylistStatusLabel->setText(QString("MyList Status: Preloading data for %1 anime...").arg(allAnimeIds.size()));
		cardManager->preloadCardCreationData(allAnimeIds);
		LOG("[Window] Card data preload complete");
	} else {
		LOG(QString("[Window] Error loading all anime: %1").arg(allAnimeQuery.lastError().text()));
		// Fallback to loading just mylist anime
		if (!aids.isEmpty()) {
			mylistStatusLabel->setText(QString("MyList Status: Preloading data for %1 anime...").arg(aids.size()));
			cardManager->preloadCardCreationData(aids);
			LOG("[Window] Card data preload complete (fallback to mylist only)");
		}
	}
	
	// Set the ordered anime ID list for virtual scrolling
	cardManager->setAnimeIdList(aids);
	
	// Update the virtual layout with the item count
	if (mylistVirtualLayout) {
		mylistVirtualLayout->setItemCount(aids.size());
		mylistVirtualLayout->refresh();
	}
	
	// Get all cards for backward compatibility (will be empty initially with virtual scrolling)
	animeCards = cardManager->getAllCards();
	
	// Reload alternative titles for filtering
	loadAnimeAlternativeTitlesForFiltering();
	
	// Restore filter settings first
	restoreMylistSorting();
	
	// Apply current filters, then sorting
	// (sorting must happen after filtering to preserve sort order on filtered results)
	applyMylistFilters();
	
	// Apply sorting after filtering
	sortMylistCards(filterSidebar->getSortIndex());
	
	mylistStatusLabel->setText(QString("MyList Status: %1 anime (virtual scrolling)").arg(aids.size()));
	LOG(QString("[Window] Mylist loaded: %1 anime").arg(aids.size()));
}

// Load alternative titles from anime_titles table for filtering
void Window::loadAnimeAlternativeTitlesForFiltering()
{
	animeAlternativeTitlesCache.clear();
	
	QSqlDatabase db = QSqlDatabase::database();
	if (!db.isOpen()) {
		LOG("[Window] Database not open for loading alternative titles");
		return;
	}
	
	// Check if we're in "In MyList only" mode
	bool inMyListOnly = filterSidebar->getInMyListOnly();
	
	// Query all alternative titles based on filter mode
	QString query;
	if (inMyListOnly) {
		// Only load titles for anime in mylist
		query = "SELECT DISTINCT at.aid, at.title, a.nameromaji, a.nameenglish, "
		        "a.nameother, a.nameshort, a.synonyms "
		        "FROM anime_titles at "
		        "LEFT JOIN anime a ON at.aid = a.aid "
		        "WHERE at.aid IN (SELECT DISTINCT aid FROM mylist) "
		        "ORDER BY at.aid";
	} else {
		// Load titles for all anime
		query = "SELECT DISTINCT at.aid, at.title, a.nameromaji, a.nameenglish, "
		        "a.nameother, a.nameshort, a.synonyms "
		        "FROM anime_titles at "
		        "LEFT JOIN anime a ON at.aid = a.aid "
		        "ORDER BY at.aid";
	}
	
	QSqlQuery q(db);
	if (!q.exec(query)) {
		LOG("[Window] Error loading alternative titles: " + q.lastError().text());
		return;
	}
	
	int currentAid = -1;
	QStringList currentTitles;
	
	while (q.next()) {
		int aid = q.value(0).toInt();
		QString title = q.value(1).toString();
		QString romaji = q.value(2).toString();
		QString english = q.value(3).toString();
		QString other = q.value(4).toString();
		QString shortNames = q.value(5).toString();
		QString synonyms = q.value(6).toString();
		
		if (aid != currentAid) {
			// Save previous anime's titles if any
			if (currentAid != -1) {
				animeAlternativeTitlesCache.addAnime(currentAid, currentTitles);
			}
			
			// Start new anime
			currentAid = aid;
			currentTitles.clear();
			
			// Add main titles from anime table using helper function
			addAnimeTitlesToList(currentTitles, romaji, english, other, shortNames, synonyms);
		}
		
		// Add alternative title from anime_titles table if not already in list
		if (!title.isEmpty() && !currentTitles.contains(title, Qt::CaseInsensitive)) {
			currentTitles.append(title);
		}
	}
	
	// Save last anime's titles
	if (currentAid != -1) {
		animeAlternativeTitlesCache.addAnime(currentAid, currentTitles);
	}
	
	LOG(QString("[Window] Loaded alternative titles for %1 anime").arg(animeAlternativeTitlesCache.size()));
}

// Helper function to parse and add anime titles from database fields to a title list
// Extracts titles from romaji, english, other, shortNames, and synonyms fields
void Window::addAnimeTitlesToList(QStringList& titles, const QString& romaji, const QString& english,
                                   const QString& other, const QString& shortNames, const QString& synonyms)
{
	// Add romaji and english names from anime table
	if (!romaji.isEmpty()) {
		titles.append(romaji);
	}
	if (!english.isEmpty() && english != romaji) {
		titles.append(english);
	}
	
	// Add other name from anime table
	if (!other.isEmpty()) {
		QStringList otherList = other.split("'", Qt::SkipEmptyParts);
		for (const QString &otherName : otherList) {
			QString trimmed = otherName.trimmed();
			if (!trimmed.isEmpty() && !titles.contains(trimmed, Qt::CaseInsensitive)) {
				titles.append(trimmed);
			}
		}
	}
	
	// Add short names from anime table
	if (!shortNames.isEmpty()) {
		QStringList shortList = shortNames.split("'", Qt::SkipEmptyParts);
		for (const QString &shortName : shortList) {
			QString trimmed = shortName.trimmed();
			if (!trimmed.isEmpty() && !titles.contains(trimmed, Qt::CaseInsensitive)) {
				titles.append(trimmed);
			}
		}
	}
	
	// Add synonyms from anime table
	if (!synonyms.isEmpty()) {
		QStringList synonymList = synonyms.split("'", Qt::SkipEmptyParts);
		for (const QString &synonym : synonymList) {
			QString trimmed = synonym.trimmed();
			if (!trimmed.isEmpty() && !titles.contains(trimmed, Qt::CaseInsensitive)) {
				titles.append(trimmed);
			}
		}
	}
}

// Update alternative titles for a single anime in the cache
// This is called when anime metadata is updated to keep the cache fresh
void Window::updateAnimeAlternativeTitlesInCache(int aid)
{
	if (aid <= 0) {
		return;
	}
	
	QSqlDatabase db = QSqlDatabase::database();
	if (!db.isOpen()) {
		LOG("[Window] Database not open for updating alternative titles");
		return;
	}
	
	// Query all titles for this specific anime
	// Using INNER JOIN since we filter on at.aid - anime without entries in anime_titles won't be found anyway
	QString query = "SELECT DISTINCT at.aid, at.title, a.nameromaji, a.nameenglish, "
	                "a.nameother, a.nameshort, a.synonyms "
	                "FROM anime_titles at "
	                "INNER JOIN anime a ON at.aid = a.aid "
	                "WHERE at.aid = ? "
	                "ORDER BY at.aid";
	
	QSqlQuery q(db);
	q.prepare(query);
	q.addBindValue(aid);
	
	if (!q.exec()) {
		LOG(QString("[Window] Error loading alternative titles for AID %1: %2").arg(aid).arg(q.lastError().text()));
		return;
	}
	
	QStringList titles;
	bool hasData = false;
	
	while (q.next()) {
		QString title = q.value(1).toString();
		QString romaji = q.value(2).toString();
		QString english = q.value(3).toString();
		QString other = q.value(4).toString();
		QString shortNames = q.value(5).toString();
		QString synonyms = q.value(6).toString();
		
		if (!hasData) {
			// First row - add main titles from anime table using helper function
			hasData = true;
			addAnimeTitlesToList(titles, romaji, english, other, shortNames, synonyms);
		}
		
		// Add alternative title from anime_titles table if not already in list
		if (!title.isEmpty() && !titles.contains(title, Qt::CaseInsensitive)) {
			titles.append(title);
		}
	}
	
	// Update cache with the titles (or remove if no data found)
	if (!titles.isEmpty()) {
		animeAlternativeTitlesCache.addAnime(aid, titles);
		LOG(QString("[Window] Updated alternative titles cache for AID %1 (%2 titles)").arg(aid).arg(titles.size()));
	} else {
		// No data found - remove from cache if present
		if (animeAlternativeTitlesCache.contains(aid)) {
			animeAlternativeTitlesCache.removeAnime(aid);
			LOG(QString("[Window] Removed AID %1 from alternative titles cache (no data)").arg(aid));
		}
	}
}
// Check and request missing relation data for a specific anime's chain
// This is called when anime data is updated to continue building the chain
void Window::checkAndRequestChainRelations(int aid)
{
	if (!watchSessionManager || !adbapi) {
		return;
	}
	
	QSqlDatabase db = QSqlDatabase::database();
	if (!db.isOpen()) {
		return;
	}
	
	// CONSOLIDATION: Use MyListCardManager's chain building instead of WatchSessionManager
	// This maintains SOLID principle - single responsibility for chain building logic
	// MyListCardManager is now the authoritative source for all chain operations
	// Build chain from this single anime (chains are always expanded)
	QList<AnimeChain> chains = cardManager->buildChainsFromAnimeIds(QList<int>() << aid);
	
	if (chains.isEmpty()) {
		return;
	}
	
	// Get the first chain (should only be one since we started with one anime)
	QList<int> chain = chains.first().getAnimeIds();
	
	if (chain.isEmpty()) {
		return;
	}
	
	QSet<int> animeNeedingRelationData;
	QSet<int> animeReferencedByOthers;
	QSqlQuery q(db);
	
	// Check all anime in this chain for referenced anime that might need data
	for (int chainAid : chain) {
		q.prepare("SELECT relaidlist, relaidtype FROM anime WHERE aid = ?");
		q.addBindValue(chainAid);
		if (q.exec() && q.next()) {
			QString relaidlist = q.value(0).toString();
			QString relaidtype = q.value(1).toString();
			
			// Parse the relation list to find referenced anime
			if (!relaidlist.isEmpty() && !relaidtype.isEmpty()) {
				QStringList aidList = relaidlist.split("'", Qt::SkipEmptyParts);
				QStringList typeList = relaidtype.split("'", Qt::SkipEmptyParts);
				
				int count = qMin(aidList.size(), typeList.size());
				for (int i = 0; i < count; i++) {
					int relAid = aidList[i].toInt();
					QString relType = typeList[i].toLower();
					// Track anime that are prequels or sequels (part of the chain)
					if (relAid > 0 && (relType == "1" || relType == "2" || 
					                    relType.contains("prequel") || relType.contains("sequel"))) {
						animeReferencedByOthers.insert(relAid);
					}
				}
			}
		}
	}
	
	// Now check which of the referenced anime are missing their relation data
	for (int refAid : std::as_const(animeReferencedByOthers)) {
		q.prepare("SELECT relaidlist, relaidtype FROM anime WHERE aid = ?");
		q.addBindValue(refAid);
		if (q.exec() && q.next()) {
			QString relaidlist = q.value(0).toString();
			QString relaidtype = q.value(1).toString();
			// If relation data is missing, request it
			if (relaidlist.isEmpty() || relaidtype.isEmpty()) {
				animeNeedingRelationData.insert(refAid);
			}
		} else {
			// Anime is referenced but not in database at all - request it
			animeNeedingRelationData.insert(refAid);
		}
	}
	
	// Request anime metadata for those with missing relation data
	if (!animeNeedingRelationData.isEmpty()) {
		LOG(QString("[Window] Requesting relation data for %1 referenced anime in chain of anime %2")
		    .arg(animeNeedingRelationData.size()).arg(aid));
		for (int requestAid : std::as_const(animeNeedingRelationData)) {
			adbapi->Anime(requestAid);
		}
	}
}

// Apply filters to mylist cards
void Window::applyMylistFilters()
{
	// Prevent concurrent filter operations to avoid race conditions
	QMutexLocker locker(&filterOperationsMutex);
	
	// Use the stored full unfiltered list of anime IDs (not the current filtered list)
	// This ensures that when filters are removed, previously hidden cards reappear
	QList<int> allAnimeIds = allAnimeIdsList;
	
	// Fallback for backward compatibility: if the full list wasn't initialized 
	// (shouldn't happen in normal operation since onMylistLoadingFinished or 
	// loadMylistAsCards sets it), try to build it from existing cards.
	// Note: This fallback may not work correctly if animeCards contains only
	// a subset of cards, but this is an edge case for legacy code paths.
	if (allAnimeIds.isEmpty() && !animeCards.isEmpty()) {
		for (AnimeCard* card : std::as_const(animeCards)) {
			allAnimeIds.append(card->getAnimeId());
		}
		// Store for future use
		allAnimeIdsList = allAnimeIds;
	}
	
	if (allAnimeIds.isEmpty()) {
		mylistStatusLabel->setText("MyList Status: No anime");
		return;
	}
	
	QString searchText = filterSidebar->getSearchText();
	QString typeFilter = filterSidebar->getTypeFilter();
	QString completionFilter = filterSidebar->getCompletionFilter();
	bool showOnlyUnwatched = filterSidebar->getShowOnlyUnwatched();
	bool inMyListOnly = filterSidebar->getInMyListOnly();
	QString adultContentFilter = filterSidebar->getAdultContentFilter();
	bool showSeriesChain = filterSidebar->getShowSeriesChain();
	
	// Check if we need to handle MyList filter change
	if (inMyListOnly != lastInMyListState) {
		lastInMyListState = inMyListOnly;
		
		// Check if we need to load all anime titles (only on first uncheck of "In My List")
		bool needsToLoadAllTitles = !inMyListOnly && !allAnimeTitlesLoaded;
		if (needsToLoadAllTitles) {
			// User unchecked "In My List" for the first time - need to load all anime titles
			LOG("[Window] First time showing all anime - loading all anime titles from database...");
			
			// Load all anime titles directly
			QSqlDatabase db = QSqlDatabase::database();
			if (!db.isOpen()) {
				LOG("[Window] Database not open");
				mylistStatusLabel->setText("MyList Status: Error - database not open");
				return;
			}
			
			QString query = "SELECT DISTINCT at.aid FROM anime_titles at "
			                "WHERE at.type = 1 AND at.language = 'x-jat' "
			                "ORDER BY at.aid";
			QSqlQuery q(db);
			
			if (!q.exec(query)) {
				LOG(QString("[Window] Error loading anime_titles: %1").arg(q.lastError().text()));
				mylistStatusLabel->setText("MyList Status: Error loading anime titles");
				return;
			}
			
			QList<int> aids;
			while (q.next()) {
				int aid = q.value(0).toInt();
				aids.append(aid);
			}
			
			LOG(QString("[Window] Found %1 anime titles, preloading data...").arg(aids.size()));
			
			// Preload all data
			if (!aids.isEmpty()) {
				mylistStatusLabel->setText(QString("MyList Status: Preloading data for %1 anime...").arg(aids.size()));
				cardManager->preloadCardCreationData(aids);
				LOG("[Window] Card data preload complete");
			}
			
			// Mark all anime titles as loaded
			allAnimeTitlesLoaded = true;
			
			// Store the full unfiltered list of all anime IDs
			allAnimeIdsList = aids;
			
			// Set up virtual scrolling with the full list of anime IDs
			cardManager->setVirtualLayout(mylistVirtualLayout);
			cardManager->setAnimeIdList(aids);
			
			// Update the virtual layout with the item count
			if (mylistVirtualLayout) {
				mylistVirtualLayout->setItemCount(aids.size());
				mylistVirtualLayout->refresh();
			}
			
			// Get all cards for backward compatibility
			animeCards = cardManager->getAllCards();
			
			// Reload alternative titles for filtering
			loadAnimeAlternativeTitlesForFiltering();
			
			// Update allAnimeIds for the rest of this function
			allAnimeIds = aids;
			
			mylistStatusLabel->setText(QString("MyList Status: %1 anime (virtual scrolling)").arg(aids.size()));
			LOG(QString("[Window] All anime titles loaded: %1 anime").arg(aids.size()));
			
			// Note: Sorting will be applied by the caller after this function returns
			// (sorting must happen after filtering to preserve sort order)
		}
		
		// Otherwise, we can filter from already-loaded cached data
		// No need to reload from database - all data is already in memory
		LOG(QString("[Window] Filtering by MyList state change: inMyListOnly=%1 (using cached data)").arg(inMyListOnly));
	}
	
	// Build a map of existing cards for quick lookup (for backward compatibility)
	QMap<int, AnimeCard*> cardsMap;
	for (AnimeCard* card : cardManager->getAllCards()) {
		cardsMap[card->getAnimeId()] = card;
	}
	
	// Also include animeCards list for backward compatibility
	for (AnimeCard* card : std::as_const(animeCards)) {
		if (!cardsMap.contains(card->getAnimeId())) {
			cardsMap[card->getAnimeId()] = card;
		}
	}
	
	QList<int> filteredAnimeIds;
	int totalCount = allAnimeIds.size();
	
	// Build composite filter using new filter classes
	CompositeFilter composite;
	
	// Add search filter if text is provided
	if (!searchText.isEmpty()) {
		composite.addFilter(new SearchFilter(searchText, &animeAlternativeTitlesCache));
	}
	
	// Add type filter if specified
	if (!typeFilter.isEmpty()) {
		composite.addFilter(new TypeFilter(typeFilter));
	}
	
	// Add completion filter if specified
	if (!completionFilter.isEmpty()) {
		composite.addFilter(new CompletionFilter(completionFilter));
	}
	
	// Add unwatched filter if enabled
	if (showOnlyUnwatched) {
		composite.addFilter(new UnwatchedFilter(true));
	}
	
	// Add adult content filter
	composite.addFilter(new AdultContentFilter(adultContentFilter));
	
	LOG(QString("[Window] Applying filters: %1").arg(composite.description()));
	
	// Apply filters to determine which anime to show
	// Use cached data when card widgets don't exist (virtual scrolling)
	for (int aid : allAnimeIds) {
		// Apply "In My List" filter first - this is a quick check using the set
		if (inMyListOnly && !mylistAnimeIdSet.contains(aid)) {
			// This anime is not in mylist, skip it
			continue;
		}
		
		// Try to get data from existing card first, then fall back to cached data
		AnimeCard* card = cardsMap.value(aid);
		MyListCardManager::CachedAnimeData cachedData;
		
		// Get cached data for filtering - essential for virtual scrolling
		if (!card) {
			cachedData = cardManager->getCachedAnimeData(aid);
			if (!cachedData.hasData()) {
				// No card and no cached data - skip it (can't apply filters properly)
				continue;
			}
		}
		
		// Create data accessor for unified access to card/cached data
		AnimeDataAccessor accessor(aid, card, cachedData);
		
		// Apply all filters at once using composite filter
		if (composite.matches(accessor)) {
			filteredAnimeIds.append(aid);
		}
	}
	
	// Update the card manager with the filtered list and chain mode flag
	// This will build chains if showSeriesChain is true and expand them as needed
	cardManager->setAnimeIdList(filteredAnimeIds, showSeriesChain);
	
	QList<int> displayAnimeIds = cardManager->getAnimeIdList();
	
	// FINAL VALIDATION: Ensure all anime in the final display list have card creation data preloaded
	QList<int> missingDataAnime;
	for (int aid : std::as_const(displayAnimeIds)) {
		if (!cardManager->hasCachedData(aid)) {
			missingDataAnime.append(aid);
		}
	}
	
	if (!missingDataAnime.isEmpty()) {
		LOG(QString("[Window] FINAL PRELOAD: Found %1 anime in display list without cached data, preloading now").arg(missingDataAnime.size()));
		cardManager->preloadCardCreationData(missingDataAnime);
	} else {
		LOG(QString("[Window] FINAL VALIDATION: All %1 anime in display list have cached data").arg(displayAnimeIds.size()));
	}
	
	// Update chain connections directly via card manager
	cardManager->updateSeriesChainConnections(showSeriesChain);
	
	// If using virtual scrolling, refresh the layout
	if (mylistVirtualLayout) {
		mylistVirtualLayout->refresh();
		// Trigger repaint for arrow drawing
		mylistVirtualLayout->update();
	}
	
	// For backward compatibility with non-virtual scrolling
	if (!mylistVirtualLayout && mylistCardLayout) {
		// Remove all cards from layout first
		for (AnimeCard *card : std::as_const(animeCards)) {
			mylistCardLayout->removeWidget(card);
			card->setVisible(false);
		}
		
		// Add only visible cards back to layout
		for (int aid : displayAnimeIds) {
			AnimeCard* card = cardsMap.value(aid);
			if (card) {
				mylistCardLayout->addWidget(card);
				card->setVisible(true);
			}
		}
	}
	
	// Update status label
	mylistStatusLabel->setText(QString("MyList Status: Showing %1 of %2 anime").arg(displayAnimeIds.size()).arg(totalCount));
}

// Download poster image for an anime

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

void Window::onPlayAnimeFromCard(int aid)
{
	LOG(QString("Play anime requested from card for anime ID: %1").arg(aid));
	
	// Find the first unwatched episode (based on local_watched) for this anime
	// Prefer highest version (newest file with highest lid) within each episode
	QSqlDatabase db = QSqlDatabase::database();
	if (!db.isOpen()) {
		LOG("Cannot play anime: Database not open");
		return;
	}
	
	QSqlQuery q(db);
	// Order by episode number, then by lid DESC to get newest files first
	q.prepare("SELECT m.lid, e.epno, m.local_watched, lf.path, m.eid "
	          "FROM mylist m "
	          "LEFT JOIN episode e ON m.eid = e.eid "
	          "LEFT JOIN local_files lf ON m.local_file = lf.id "
	          "WHERE m.aid = ? AND lf.path IS NOT NULL AND e.epno IS NOT NULL "
	          "ORDER BY e.epno, m.lid DESC");
	q.addBindValue(aid);
	
	if (q.exec()) {
		// Track which episodes we've seen to pick highest version file per episode
		QSet<int> seenEpisodes;
		int firstAvailableLid = 0;
		
		// Find first unwatched episode (with highest version file)
		while (q.next()) {
			int lid = q.value(0).toInt();
			int localWatched = q.value(2).toInt();
			QString localPath = q.value(3).toString();
			int eid = q.value(4).toInt();
			
			// Skip if we've already processed this episode (we want the first file, which is highest version due to DESC order)
			if (seenEpisodes.contains(eid)) {
				continue;
			}
			seenEpisodes.insert(eid);
			
			// Check if file exists
			if (!localPath.isEmpty() && QFile::exists(localPath)) {
				if (firstAvailableLid == 0) {
					firstAvailableLid = lid;
				}
				
				if (localWatched == 0) {
					// Found first unwatched episode with available file (highest version)
					LOG(QString("Playing first unwatched episode LID: %1, EID: %2 (highest version)").arg(lid).arg(eid));
					startPlaybackForFile(lid);
					return;
				}
			}
		}
		
		// If all episodes are watched, play the first available one (highest version)
		if (firstAvailableLid > 0) {
			LOG(QString("All episodes watched, playing first episode LID: %1 (highest version)").arg(firstAvailableLid));
			startPlaybackForFile(firstAvailableLid);
			return;
		}
	}
	
	LOG(QString("No playable episodes found for anime ID: %1 (files with episode data only)").arg(aid));
}

void Window::onResetWatchSession(int aid)
{
	LOG(QString("Reset watch session requested for anime ID: %1").arg(aid));
	
	// Clear local_watched status for all episodes of this anime
	QSqlDatabase db = QSqlDatabase::database();
	if (!db.isOpen()) {
		LOG("Cannot reset watch session: Database not open");
		return;
	}
	
	// Clear local_watched in mylist
	QSqlQuery q(db);
	q.prepare("UPDATE mylist SET local_watched = 0 WHERE aid = ?");
	q.addBindValue(aid);
	
	if (!q.exec()) {
		LOG(QString("Error resetting local_watched: %1").arg(q.lastError().text()));
		return;
	}
	
	// Clear watch chunks for all episodes of this anime
	QSqlQuery q2(db);
	q2.prepare("DELETE FROM watch_chunks WHERE lid IN (SELECT lid FROM mylist WHERE aid = ?)");
	q2.addBindValue(aid);
	
	if (!q2.exec()) {
		LOG(QString("Error clearing watch chunks: %1").arg(q2.lastError().text()));
		return;
	}
	
	LOG(QString("Watch session reset for anime ID: %1").arg(aid));
	
	// Update only the affected card to reflect the changes
	if (cardManager) {
		cardManager->updateCardAnimeInfo(aid);
	}
}

// ========== Filter Bar Toggle Implementation ==========

void Window::onToggleFilterBarClicked()
{
    bool isVisible = filterSidebarScrollArea->isVisible();
    filterSidebarScrollArea->setVisible(!isVisible);
    toggleFilterBarButton->setVisible(isVisible);
    
    // Save the state
    adbapi->setFilterBarVisible(!isVisible);
    LOG(QString("Filter bar visibility toggled: %1").arg(!isVisible ? "visible" : "hidden"));
}

// ========== System Tray Implementation ==========

QIcon Window::loadUsagiIcon()
{
    // Try to load from file system paths first
    // This works on all platforms and Qt versions
    // Ordered by reliability: application dir first, then relative paths
    QStringList iconPaths = {
        QCoreApplication::applicationDirPath() + "/usagi.ico",  // ICO file in app dir
        QCoreApplication::applicationDirPath() + "/usagi.png",  // PNG in app dir
        QCoreApplication::applicationDirPath() + "/../usagi.ico",  // ICO one level up from app dir
        QCoreApplication::applicationDirPath() + "/../usagi.png",  // PNG one level up from app dir
        "usagi/usagi.ico",     // ICO in usagi subdirectory (development)
        "usagi.ico",           // ICO in current directory
        "usagi.png",           // PNG in current directory
        "../usagi.ico",        // ICO in parent directory (if running from build dir)
        "../usagi.png",        // PNG in parent directory (if running from build dir)
        ":/usagi.png"          // Qt resource (if added to .qrc in future)
    };
    
    LOG(QString("Searching for icon. Application dir: %1").arg(QCoreApplication::applicationDirPath()));
    
    for (const QString &path : iconPaths) {
        // Handle Qt resource paths separately (they don't exist as files)
        if (path.startsWith(":/")) {
            QIcon icon(path);
            if (!icon.isNull()) {
                LOG(QString("Loaded icon from Qt resource: %1").arg(path));
                return icon;
            }
        } else {
            // Regular file system paths - check existence first to avoid unnecessary QIcon creation
            if (QFile::exists(path)) {
                QIcon icon(path);
                if (!icon.isNull()) {
                    LOG(QString("Loaded icon from: %1").arg(path));
                    return icon;
                } else {
                    LOG(QString("Icon file exists but failed to load: %1").arg(path));
                }
            }
        }
    }
    
    // Fall back to default icon if usagi icon not found
    LOG("Using default icon (usagi icon not found)");
    return QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
}

void Window::onTrayShowHideRequested()
{
    if (this->isVisible()) {
        // Store the current window state and geometry before hiding
        windowStateBeforeHide = this->windowState();
        // Use normalGeometry() which returns the correct geometry even when maximized
        windowGeometryBeforeHide = this->normalGeometry();
        this->hide();
        LOG("Window hidden to tray");
    } else {
        // Restore the previous window state and geometry
        // Only set geometry if it's valid (non-empty and has positive dimensions)
        if (windowGeometryBeforeHide.isValid() && !windowGeometryBeforeHide.isEmpty()) {
            this->setGeometry(windowGeometryBeforeHide);
        }
        this->setWindowState(windowStateBeforeHide);
        this->show();
        this->activateWindow();
        this->raise();
        LOG("Window shown from tray");
    }
}

void Window::onTrayExitRequested()
{
    // Set a flag to bypass close-to-tray logic for this exit
    exitingFromTray = true;
    
    // Temporarily disable close to tray for this exit
    trayIconManager->setCloseToTrayEnabled(false);
    
    // Send logout command if logged in
    if (adbapi->LoggedIn()) {
        LOG("Tray exit requested while logged in, sending LOGOUT command");
        adbapi->Logout();
        waitforlogout.start();
        safeclose->start();
    } else {
        // Not logged in, quit application directly
        LOG("Tray exit requested while not logged in, quitting application");
        QApplication::quit();
    }
}

void Window::onApplicationAboutToQuit()
{
    // This slot is called when the application is about to quit
    // This handles external termination (e.g., Qt Creator stop button, kill signals)
    // where closeEvent might be bypassed or ignored due to close-to-tray logic
    
    if (adbapi && adbapi->LoggedIn()) {
        LOG("Application terminating while logged in, sending LOGOUT command");
        adbapi->Logout();
        
        // Give a short time for the logout to be sent (event loop is still running)
        QEventLoop loop;
        QTimer::singleShot(200, &loop, &QEventLoop::quit);
        loop.exec();
    }
}

// ========== Auto-start Implementation ==========

void Window::registerAutoStart()
{
#ifdef Q_OS_WIN
    // Windows: Add registry entry in HKCU\Software\Microsoft\Windows\CurrentVersion\Run
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    QString appPath = QCoreApplication::applicationFilePath();
    appPath = QDir::toNativeSeparators(appPath);
    settings.setValue("Usagi-dono", "\"" + appPath + "\"");
    LOG(QString("Auto-start registered (Windows): %1").arg(appPath));
#elif defined(Q_OS_LINUX)
    // Linux: Create .desktop file in ~/.config/autostart/
    QString autostartDir = QDir::homePath() + "/.config/autostart";
    QDir dir;
    if (!dir.exists(autostartDir)) {
        dir.mkpath(autostartDir);
    }
    
    QString desktopFile = autostartDir + "/usagi-dono.desktop";
    QFile file(desktopFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        QString appPath = QCoreApplication::applicationFilePath();
        // Escape the path for shell execution (quote if contains spaces or special characters)
        if (appPath.contains(' ') || appPath.contains('\'') || appPath.contains('"')) {
            appPath = "\"" + appPath.replace("\"", "\\\"") + "\"";
        }
        out << "[Desktop Entry]\n";
        out << "Type=Application\n";
        out << "Name=Usagi-dono\n";
        out << "Exec=" << appPath << "\n";
        out << "X-GNOME-Autostart-enabled=true\n";
        file.close();
        LOG(QString("Auto-start registered (Linux): %1").arg(desktopFile));
    } else {
        LOG(QString("Failed to create auto-start file: %1").arg(desktopFile));
    }
#else
    LOG("Auto-start not supported on this platform");
#endif
}

void Window::unregisterAutoStart()
{
#ifdef Q_OS_WIN
    // Windows: Remove registry entry
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    settings.remove("Usagi-dono");
    LOG("Auto-start unregistered (Windows)");
#elif defined(Q_OS_LINUX)
    // Linux: Remove .desktop file
    QString desktopFile = QDir::homePath() + "/.config/autostart/usagi-dono.desktop";
    if (QFile::exists(desktopFile)) {
        QFile::remove(desktopFile);
        LOG(QString("Auto-start unregistered (Linux): %1").arg(desktopFile));
    }
#else
    LOG("Auto-start not supported on this platform");
#endif
}

bool Window::isAutoStartEnabled()
{
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return settings.contains("Usagi-dono");
#elif defined(Q_OS_LINUX)
    QString desktopFile = QDir::homePath() + "/.config/autostart/usagi-dono.desktop";
    return QFile::exists(desktopFile);
#else
    return false;
#endif
}

void Window::setAutoStartEnabled(bool enabled)
{
    if (enabled) {
        registerAutoStart();
    } else {
        unregisterAutoStart();
    }
}
