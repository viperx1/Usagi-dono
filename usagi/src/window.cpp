#include "main.h"
#include "window.h"
#include "animeutils.h"
#include "hasherthreadpool.h"
#include "hasherthread.h"
#include "crashlog.h"
#include "logger.h"
#include "playbuttondelegate.h"
#include "aired.h"
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
#include <algorithm>
#include <functional>
#include <memory>
#include <QDirIterator>

HasherThreadPool *hasherThreadPool = nullptr;
// adbapi is now defined in anidbapi.cpp (core library)
extern Window *window;
settings_struct settings;

// Worker implementations

void MylistLoaderWorker::doWork()
{
    LOG("Background thread: Loading mylist anime IDs...");
    
    QList<int> aids;
    
    {
        // Create separate database connection for this thread
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "MylistThread");
        db.setDatabaseName(m_dbName);
        
        if (!db.open()) {
            LOG("Background thread: Failed to open database for mylist");
            QSqlDatabase::removeDatabase("MylistThread");
            emit finished(QList<int>());
            return;
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
    
    emit finished(aids);
}

void AnimeTitlesLoaderWorker::doWork()
{
    LOG("Background thread: Loading anime titles cache...");
    
    QStringList tempTitles;
    QMap<QString, int> tempTitleToAid;
    
    {
        // We need to use a separate database connection for this thread
        // Qt SQL requires each thread to have its own connection
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "AnimeTitlesThread");
        db.setDatabaseName(m_dbName);
        
        if (!db.open()) {
            LOG("Background thread: Failed to open database for anime titles");
            QSqlDatabase::removeDatabase("AnimeTitlesThread");
            emit finished(QStringList(), QMap<QString, int>());
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
    
    emit finished(tempTitles, tempTitleToAid);
}

void UnboundFilesLoaderWorker::doWork()
{
    LOG("Background thread: Loading unbound files...");
    
    QList<UnboundFileData> tempUnboundFiles;
    
    {
        // Create separate database connection for this thread
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "UnboundFilesThread");
        db.setDatabaseName(m_dbName);
        
        if (!db.open()) {
            LOG("Background thread: Failed to open database for unbound files");
            QSqlDatabase::removeDatabase("UnboundFilesThread");
            emit finished(QList<UnboundFileData>());
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
    
    emit finished(tempUnboundFiles);
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
	totalHashParts = 0;
	completedHashParts = 0;
	
	// Initialize UI color constants (use Qt's predefined yellow color)
	m_hashedFileColor = QColor(Qt::yellow);
	
	// Initialize anime titles cache flag
	animeTitlesCacheLoaded = false;
	
	// Initialize window state for tray restore
	windowStateBeforeHide = Qt::WindowNoState;
	
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

	// tabs - Mylist first as default tab
    tabwidget->addTab(pageMylistParent, "Anime");
    tabwidget->addTab(pageHasherParent, "Hasher");
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

    // page mylist (card view only)
    mylistSortAscending = false;  // Default to descending (newest first for aired date)
    lastInMyListState = true;  // Initialize to true (default is "In MyList only")
    allAnimeTitlesLoaded = false;  // All anime titles are not loaded initially
    
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
    
    // Connect signal for brand new anime added to mylist (after initial load)
    connect(cardManager, &MyListCardManager::newAnimeAdded, this, [this](int aid) {
        LOG(QString("[Window] New anime aid=%1 added to mylist, auto-starting session").arg(aid));
        if (watchSessionManager) {
            watchSessionManager->onNewAnimeAdded(aid);
        }
    });
    
    connect(cardManager, &MyListCardManager::cardCreated, this, [this](int aid, AnimeCard *card) {
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
        connect(card, &AnimeCard::markFileForDownloadRequested, this, [this](int lid) {
            LOG(QString("[Window] Marking file lid=%1 for download").arg(lid));
            if (watchSessionManager) {
                watchSessionManager->setFileMarkType(lid, FileMarkType::ForDownload);
            }
        });
        connect(card, &AnimeCard::markFileForDeletionRequested, this, [this](int lid) {
            LOG(QString("[Window] Marking file lid=%1 for deletion").arg(lid));
            if (watchSessionManager) {
                watchSessionManager->setFileMarkType(lid, FileMarkType::ForDeletion);
            }
        });
        connect(card, &AnimeCard::clearFileMarkRequested, this, [this](int lid) {
            LOG(QString("[Window] Clearing file mark for lid=%1").arg(lid));
            if (watchSessionManager) {
                watchSessionManager->setFileMarkType(lid, FileMarkType::None);
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
    });
    connect(filterSidebar, &MyListFilterSidebar::sortChanged, this, [this]() {
        sortMylistCards(filterSidebar->getSortIndex());
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
    QGroupBox *watcherGroup = new QGroupBox("Directory Watcher");
    QVBoxLayout *watcherLayout = new QVBoxLayout(watcherGroup);
    watcherEnabled = new QCheckBox("Enable Directory Watcher");
    watcherAutoStart = new QCheckBox("Auto-start on application launch");
    watcherStatusLabel = new QLabel("Status: Not watching");
    QHBoxLayout *watcherDirLayout = new QHBoxLayout();
    watcherDirectory = new QLineEdit;
    watcherBrowseButton = new QPushButton("Browse...");
    watcherDirLayout->addWidget(new QLabel("Watch Directory:"));
    watcherDirLayout->addWidget(watcherDirectory, 1);
    watcherDirLayout->addWidget(watcherBrowseButton);
    watcherLayout->addWidget(watcherEnabled);
    watcherLayout->addLayout(watcherDirLayout);
    watcherLayout->addWidget(watcherAutoStart);
    watcherLayout->addWidget(watcherStatusLabel);
    settingsMainLayout->addWidget(watcherGroup);
    
    // Auto-fetch Group
    QGroupBox *autoFetchGroup = new QGroupBox("Auto-fetch");
    QVBoxLayout *autoFetchLayout = new QVBoxLayout(autoFetchGroup);
    autoFetchEnabled = new QCheckBox("Automatically download anime titles and other data on startup");
    autoFetchLayout->addWidget(autoFetchEnabled);
    settingsMainLayout->addWidget(autoFetchGroup);
    
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
    QGroupBox *trayGroup = new QGroupBox("System Tray");
    QVBoxLayout *trayLayout = new QVBoxLayout(trayGroup);
    trayMinimizeToTray = new QCheckBox("Minimize to tray");
    trayMinimizeToTray->setToolTip("Minimize the application to system tray instead of taskbar");
    trayCloseToTray = new QCheckBox("Close to tray");
    trayCloseToTray->setToolTip("Hide to system tray when closing the window instead of exiting");
    trayStartMinimized = new QCheckBox("Start minimized to tray");
    trayStartMinimized->setToolTip("Start the application minimized to system tray");
    trayLayout->addWidget(trayMinimizeToTray);
    trayLayout->addWidget(trayCloseToTray);
    trayLayout->addWidget(trayStartMinimized);
    settingsMainLayout->addWidget(trayGroup);
    
    // Auto-start Group
    QGroupBox *autoStartGroup = new QGroupBox("Application Startup");
    QVBoxLayout *autoStartLayout = new QVBoxLayout(autoStartGroup);
    autoStartEnabled = new QCheckBox("Start with operating system");
    autoStartEnabled->setToolTip("Automatically start the application when you log in");
    autoStartLayout->addWidget(autoStartEnabled);
    settingsMainLayout->addWidget(autoStartGroup);
    
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
    connect(watcherEnabled, SIGNAL(stateChanged(int)), this, SLOT(onWatcherEnabledChanged(int)));
    connect(watcherBrowseButton, SIGNAL(clicked()), this, SLOT(onWatcherBrowseClicked()));
    connect(mediaPlayerBrowseButton, SIGNAL(clicked()), this, SLOT(onMediaPlayerBrowseClicked()));
    
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
    connect(playbackManager, &PlaybackManager::fileMarkedAsLocallyWatched,
            this, &Window::onFileMarkedAsLocallyWatched);
    
    // Initialize watch session manager
    watchSessionManager = new WatchSessionManager(this);
    LOG("[Window] WatchSessionManager initialized");
    
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
    
    // Connect WatchSessionManager signals to refresh cards when markings change
    connect(watchSessionManager, &WatchSessionManager::markingsUpdated, this, [this](const QSet<int>& updatedLids) {
        LOG(QString("[Window] File markings updated for %1 files, refreshing affected cards").arg(updatedLids.size()));
        // Trigger targeted card refresh to show updated markings
        if (cardManager) {
            if (updatedLids.isEmpty()) {
                // Fallback: if no specific lids provided, refresh all (shouldn't happen normally)
                cardManager->refreshAllCards();
            } else {
                // Only refresh cards containing the updated lids
                cardManager->refreshCardsForLids(updatedLids);
            }
        }
    });
    
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
    
    // Initialize play button delegate (kept for card view compatibility)
    playButtonDelegate = new PlayButtonDelegate(this);
    
    // Setup animation timer for play button animation
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(300);  // 300ms between animation frames
    connect(m_animationTimer, &QTimer::timeout, this, &Window::onAnimationTimerTimeout);
    
    // Initialize timer for deferred processing of already-hashed files
    hashedFilesProcessingTimer = new QTimer(this);
    hashedFilesProcessingTimer->setSingleShot(false);
    hashedFilesProcessingTimer->setInterval(HASHED_FILES_TIMER_INTERVAL);
    connect(hashedFilesProcessingTimer, &QTimer::timeout, this, &Window::processPendingHashedFiles);
    
    // Initialize background loading threads
    mylistLoadingThread = nullptr;
    animeTitlesLoadingThread = nullptr;
    unboundFilesLoadingThread = nullptr;
    
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
        // Sync WatchSessionManager's watched path with directory watcher
        if (watchSessionManager) {
            watchSessionManager->setWatchedPath(watcherDir);
        }
    } else if (watcherEnabledSetting && !watcherDir.isEmpty()) {
        // If watcher is enabled but auto-start is not, just update status
        watcherStatusLabel->setText("Status: Enabled (not auto-started)");
        // Still set the path for WatchSessionManager even if watcher is not auto-started
        if (watchSessionManager) {
            watchSessionManager->setWatchedPath(watcherDir);
        }
    } else if (watcherEnabledSetting && watcherDir.isEmpty()) {
        // If watcher is enabled but no directory is set
        watcherStatusLabel->setText("Status: Enabled (no directory set)");
    }
    
    // Initialize system tray
    createTrayIcon();
    loadTraySettings();
    
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
    
    // Apply current filters first, then sorting
    // (sorting must happen after filtering to preserve sort order on filtered results)
    applyMylistFilters();
    
    // Apply sorting after filtering
    restoreMylistSorting();
    sortMylistCards(filterSidebar->getSortIndex());
    
    mylistStatusLabel->setText(QString("MyList Status: %1 anime (virtual scrolling)").arg(aids.size()));
    LOG(QString("[Virtual Scrolling] Ready to display %1 anime").arg(aids.size()));
    
    // Mark initial loading as complete so new anime can be detected
    cardManager->setInitialLoadComplete();
    
    // Perform initial scan for file marking after mylist is loaded
    if (watchSessionManager) {
        LOG("[Window] Mylist loaded, triggering initial file marking scan");
        watchSessionManager->performInitialScan();
    }
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
}

// Called when unbound files loading finishes (in UI thread)
void Window::onUnboundFilesLoadingFinished(const QList<UnboundFileData> &files)
{
    LOG(QString("Background loading: Unbound files loaded, adding %1 files to UI...").arg(files.size()));
    
    if (files.isEmpty()) {
        LOG("No unbound files found");
        return;
    }
    
    // Disable updates during bulk insertion for performance
    unknownFiles->setUpdatesEnabled(false);
    
    // Add each unbound file to the unknown files widget
    for (const UnboundFileData& fileData : std::as_const(files)) {
        unknownFilesInsertRow(fileData.filename, fileData.filepath, fileData.hash, fileData.size);
    }
    
    // Re-enable updates after bulk insertion
    unknownFiles->setUpdatesEnabled(true);
    
    LOG(QString("Successfully added %1 unbound files to UI").arg(files.size()));
}

void Window::saveMylistSorting()
{
    // Card view sorting is handled by filterSidebar
    // Save the current sort settings
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return;
    }
    
    int sortIndex = filterSidebar->getSortIndex();
    
    QSqlQuery q(db);
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_card_sort_index', ?)");
    q.addBindValue(sortIndex);
    q.exec();
    
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES ('mylist_card_sort_ascending', ?)");
    q.addBindValue(filterSidebar->getSortAscending() ? 1 : 0);
    q.exec();
    
    LOG(QString("Saved mylist card sorting: index=%1, ascending=%2").arg(sortIndex).arg(filterSidebar->getSortAscending()));
}

void Window::restoreMylistSorting()
{
    // Sorting is now handled by the filter sidebar
    // This function is kept for backward compatibility but does nothing
    // as the sidebar initializes with default values
    LOG(QString("restoreMylistSorting: Sorting is now managed by filter sidebar"));
}

void Window::onMylistSortChanged(int column, Qt::SortOrder order)
{
    Q_UNUSED(column);
    Q_UNUSED(order);
    // This slot was for tree view sorting - no longer used with card view only
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
    if (closeToTrayEnabled && QSystemTrayIcon::isSystemTrayAvailable() && trayIcon && trayIcon->isVisible()) {
        this->hide();
        event->ignore();
        LOG("Window close intercepted, hidden to tray");
        
        // Show balloon notification once to inform user
        static bool notificationShown = false;
        if (!notificationShown) {
            trayIcon->showMessage("Usagi-dono", 
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
    if(!adbapi->LoggedIn() || waitforlogout.elapsed() > 5000)
    {
        event->accept();
        LOG("Window close accepted, application exiting");
    }
}

void Window::changeEvent(QEvent *event)
{
    // Handle minimize to tray
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized() && minimizeToTrayEnabled && QSystemTrayIcon::isSystemTrayAvailable() && trayIcon && trayIcon->isVisible()) {
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
	adbapi->setWatcherEnabled(watcherEnabled->isChecked());
	adbapi->setWatcherDirectory(watcherDirectory->text());
	adbapi->setWatcherAutoStart(watcherAutoStart->isChecked());
	
	// Save auto-fetch settings to database
	adbapi->setAutoFetchEnabled(autoFetchEnabled->isChecked());
	
	// Save media player path
	PlaybackManager::setMediaPlayerPath(mediaPlayerPath->text());
	
	// Save tray settings
	saveTraySettings();
	
	// Save auto-start settings
	bool autoStartWasEnabled = adbapi->getAutoStartEnabled();
	bool autoStartNowEnabled = autoStartEnabled->isChecked();
	adbapi->setAutoStartEnabled(autoStartNowEnabled);
	
	// Update auto-start registration if changed
	if (autoStartWasEnabled != autoStartNowEnabled) {
		setAutoStartEnabled(autoStartNowEnabled);
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
                    updateOrAddMylistEntry(lid);
                    
                    // Trigger deletion mechanism as file was updated (space may have changed)
                    if (watchSessionManager && watchSessionManager->isAutoMarkDeletionEnabled()) {
                        watchSessionManager->autoMarkFilesForDeletion();
                    }
                }
                else
                {
                    // Even if we couldn't find the mylist entry in the local database,
                    // we know the file is in AniDB's mylist (310 response), so update binding_status
                    // to prevent the file from reappearing in unknown files list on restart
                    adbapi->UpdateLocalFileBindingStatus(localPath, 1); // 1 = bound_to_anime
                    adbapi->UpdateLocalFileStatus(localPath, 2); // 2 = in anidb
                    LOG(QString("Updated binding_status directly for path=%1 (mylist entry not found locally)").arg(localPath));
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
					
					// Trigger deletion mechanism as new files were added (space may have decreased)
					if (watchSessionManager && watchSessionManager->isAutoMarkDeletionEnabled()) {
						watchSessionManager->autoMarkFilesForDeletion();
					}
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
	
	// Update card manager (tree view has been removed)
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
					
					// Use card manager to download poster (tree view removed)
					if (cardManager) {
						cardManager->updateCardPoster(aid, picname);
					}
					
					needsPosterDownload = true;
					LOG(QString("[Timing] Triggered poster download for AID %1").arg(aid));
				}
			}
		}
	}
	
	// Update card view (tree view has been removed)
	if (cardManager) {
		// Use card manager for efficient update
		cardManager->onAnimeUpdated(aid);
		// Backward compatibility: update animeCards list
		animeCards = cardManager->getAllCards();
		
		// If not just triggering poster download, may need to resort
		if (!needsPosterDownload) {
			sortMylistCards(filterSidebar->getSortIndex());
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
	// Update or add a single mylist entry using card manager (tree view removed)
	if (cardManager) {
		cardManager->updateOrAddMylistEntry(lid);
		// Backward compatibility: update animeCards list
		animeCards = cardManager->getAllCards();
	}
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
	// Always use card view (tree view has been removed)
	animeMetadataRequested.clear();  // Clear request tracking for fresh load
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

void Window::onWatcherEnabledChanged(int state)
{
	if (state == Qt::Checked) {
		QString dir = watcherDirectory->text();
		if (!dir.isEmpty() && QDir(dir).exists()) {
			directoryWatcher->startWatching(dir);
			watcherStatusLabel->setText("Status: Watching " + dir);
			LOG("Directory watcher started: " + dir);
			// Sync WatchSessionManager's watched path with directory watcher
			if (watchSessionManager) {
				watchSessionManager->setWatchedPath(dir);
			}
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
		
		// Sync WatchSessionManager's watched path with directory watcher
		if (watchSessionManager) {
			watchSessionManager->setWatchedPath(dir);
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

void Window::updatePlayButtonForItem(QTreeWidgetItem *item)
{
	// Tree view has been removed - this function is now a no-op
	Q_UNUSED(item);
}

void Window::updatePlayButtonsInTree(QTreeWidgetItem *rootItem)
{
	// Tree view has been removed - this function is now a no-op
	Q_UNUSED(rootItem);
}

void Window::onPlaybackCompleted(int lid)
{
	LOG(QString("Playback completed: LID %1 - Updating play buttons").arg(lid));
	
	// Remove from playing items
	m_playingItems.remove(lid);
	if (m_playingItems.isEmpty()) {
		m_animationTimer->stop();
	}
	
	// Update tree view and anime card
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
	
	// Update tree view and anime card
	updateUIForWatchedFile(lid);
}

void Window::updateUIForWatchedFile(int lid)
{
	// Update anime card (tree view has been removed)
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

// Toggle between card view and tree view
// Sort cards based on selected criterion
void Window::sortMylistCards(int sortIndex)
{
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
	
	// Sort based on criterion
	// Use cached data when card widgets don't exist (virtual scrolling)
	switch (sortIndex) {
		case 0: // Anime Title
			std::sort(animeIds.begin(), animeIds.end(), [&cardsMap, &getCachedData, sortAscending](int aidA, int aidB) {
				AnimeCard* a = cardsMap.value(aidA);
				AnimeCard* b = cardsMap.value(aidB);
				
				// Get data from cards or cache
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden;
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden;
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName;
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName;
				
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
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden;
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden;
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName;
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName;
				QString typeA = a ? a->getAnimeType() : getCachedData(aidA).typeName;
				QString typeB = b ? b->getAnimeType() : getCachedData(aidB).typeName;
				
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
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden;
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden;
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName;
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName;
				
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
					airedA = aired(cachedA.startDate, cachedA.endDate);
				}
				if (b) {
					airedB = b->getAired();
				} else {
					MyListCardManager::CachedAnimeData cachedB = getCachedData(aidB);
					airedB = aired(cachedB.startDate, cachedB.endDate);
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
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden;
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden;
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName;
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName;
				
				// Hidden cards always go to the bottom
				if (hiddenA != hiddenB) {
					return hiddenB;  // non-hidden comes before hidden
				}
				
				int episodesA, episodesB;
				if (a) {
					episodesA = a->getNormalEpisodes() + a->getOtherEpisodes();
				} else {
					MyListCardManager::CachedAnimeData cachedA = getCachedData(aidA);
					episodesA = cachedA.stats.normalEpisodes + cachedA.stats.otherEpisodes;
				}
				if (b) {
					episodesB = b->getNormalEpisodes() + b->getOtherEpisodes();
				} else {
					MyListCardManager::CachedAnimeData cachedB = getCachedData(aidB);
					episodesB = cachedB.stats.normalEpisodes + cachedB.stats.otherEpisodes;
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
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden;
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden;
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName;
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName;
				
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
					totalEpisodesA = cachedA.stats.normalEpisodes + cachedA.stats.otherEpisodes;
					viewedA = cachedA.stats.normalViewed + cachedA.stats.otherViewed;
				}
				if (b) {
					totalEpisodesB = b->getNormalEpisodes() + b->getOtherEpisodes();
					viewedB = b->getNormalViewed() + b->getOtherViewed();
				} else {
					MyListCardManager::CachedAnimeData cachedB = getCachedData(aidB);
					totalEpisodesB = cachedB.stats.normalEpisodes + cachedB.stats.otherEpisodes;
					viewedB = cachedB.stats.normalViewed + cachedB.stats.otherViewed;
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
				bool hiddenA = a ? a->isHidden() : getCachedData(aidA).isHidden;
				bool hiddenB = b ? b->isHidden() : getCachedData(aidB).isHidden;
				QString titleA = a ? a->getAnimeTitle() : getCachedData(aidA).animeName;
				QString titleB = b ? b->getAnimeTitle() : getCachedData(aidB).animeName;
				
				// Hidden cards always go to the bottom
				if (hiddenA != hiddenB) {
					return hiddenB;  // non-hidden comes before hidden
				}
				
				qint64 lastPlayedA = a ? a->getLastPlayed() : getCachedData(aidA).lastPlayed;
				qint64 lastPlayedB = b ? b->getLastPlayed() : getCachedData(aidB).lastPlayed;
				
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
	cardManager->setAnimeIdList(animeIds);
	
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
	
	// Preload all data for cards
	if (!aids.isEmpty()) {
		mylistStatusLabel->setText(QString("MyList Status: Preloading data for %1 anime...").arg(aids.size()));
		cardManager->preloadCardCreationData(aids);
		LOG("[Window] Card data preload complete");
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
	
	// Apply current filters first, then sorting
	// (sorting must happen after filtering to preserve sort order on filtered results)
	applyMylistFilters();
	
	// Apply sorting after filtering
	restoreMylistSorting();
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
	AnimeAlternativeTitles currentTitles;
	
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
				animeAlternativeTitlesCache[currentAid] = currentTitles;
			}
			
			// Start new anime
			currentAid = aid;
			currentTitles.titles.clear();
			
			// Add romaji and english names from anime table
			if (!romaji.isEmpty()) {
				currentTitles.titles.append(romaji);
			}
			if (!english.isEmpty() && english != romaji) {
				currentTitles.titles.append(english);
			}
			
			// Add other name from anime table
			if (!other.isEmpty()) {
				QStringList otherList = other.split("'", Qt::SkipEmptyParts);
				for (const QString &otherName : otherList) {
					QString trimmed = otherName.trimmed();
					if (!trimmed.isEmpty() && !currentTitles.titles.contains(trimmed, Qt::CaseInsensitive)) {
						currentTitles.titles.append(trimmed);
					}
				}
			}
			
			// Add short names from anime table
			if (!shortNames.isEmpty()) {
				QStringList shortList = shortNames.split("'", Qt::SkipEmptyParts);
				for (const QString &shortName : shortList) {
					QString trimmed = shortName.trimmed();
					if (!trimmed.isEmpty() && !currentTitles.titles.contains(trimmed, Qt::CaseInsensitive)) {
						currentTitles.titles.append(trimmed);
					}
				}
			}
			
			// Add synonyms from anime table
			if (!synonyms.isEmpty()) {
				QStringList synonymList = synonyms.split("'", Qt::SkipEmptyParts);
				for (const QString &synonym : synonymList) {
					QString trimmed = synonym.trimmed();
					if (!trimmed.isEmpty() && !currentTitles.titles.contains(trimmed, Qt::CaseInsensitive)) {
						currentTitles.titles.append(trimmed);
					}
				}
			}
		}
		
		// Add alternative title from anime_titles table if not already in list
		if (!title.isEmpty() && !currentTitles.titles.contains(title, Qt::CaseInsensitive)) {
			currentTitles.titles.append(title);
		}
	}
	
	// Save last anime's titles
	if (currentAid != -1) {
		animeAlternativeTitlesCache[currentAid] = currentTitles;
	}
	
	LOG(QString("[Window] Loaded alternative titles for %1 anime").arg(animeAlternativeTitlesCache.size()));
}

// Check if a card matches the search filter
bool Window::matchesSearchFilter(AnimeCard *card, const QString &searchText)
{
	return matchesSearchFilter(card->getAnimeId(), card->getAnimeTitle(), searchText);
}

// Check if an anime matches the search filter (using aid and title)
bool Window::matchesSearchFilter(int aid, const QString &animeName, const QString &searchText)
{
	if (searchText.isEmpty()) {
		return true;
	}
	
	// Check main title first
	if (animeName.contains(searchText, Qt::CaseInsensitive)) {
		return true;
	}
	
	// Check alternative titles from cache
	if (animeAlternativeTitlesCache.contains(aid)) {
		const AnimeAlternativeTitles &titles = animeAlternativeTitlesCache[aid];
		for (const QString &title : titles.titles) {
			if (title.contains(searchText, Qt::CaseInsensitive)) {
				return true;
			}
		}
	}
	
	return false;
}

// Apply filters to mylist cards
void Window::applyMylistFilters()
{
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
	bool showMarkedForDeletion = filterSidebar->getShowMarkedForDeletion();
	QString adultContentFilter = filterSidebar->getAdultContentFilter();
	bool inMyListOnly = filterSidebar->getInMyListOnly();
	
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
	
	// Apply filters to determine which anime to show
	// Use cached data when card widgets don't exist (virtual scrolling)
	for (int aid : allAnimeIds) {
		bool visible = true;
		
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
			if (!cachedData.hasData) {
				// No card and no cached data - include it anyway (can't filter)
				filteredAnimeIds.append(aid);
				continue;
			}
		}
		
		// Get anime name for search filter
		QString animeName = card ? card->getAnimeTitle() : cachedData.animeName;
		
		// Apply search filter
		if (!searchText.isEmpty()) {
			visible = visible && matchesSearchFilter(aid, animeName, searchText);
		}
		
		// Apply type filter
		if (!typeFilter.isEmpty()) {
			QString animeType = card ? card->getAnimeType() : cachedData.typeName;
			visible = visible && (animeType == typeFilter);
		}
		
		// Apply completion filter
		if (!completionFilter.isEmpty()) {
			int normalEpisodes, normalViewed, totalEpisodes;
			
			if (card) {
				normalEpisodes = card->getNormalEpisodes();
				normalViewed = card->getNormalViewed();
				totalEpisodes = card->getTotalNormalEpisodes();
			} else {
				normalEpisodes = cachedData.stats.normalEpisodes;
				normalViewed = cachedData.stats.normalViewed;
				totalEpisodes = cachedData.eptotal > 0 ? cachedData.eptotal : cachedData.stats.totalNormalEpisodes;
			}
			
			if (completionFilter == "completed") {
				// Completed: all normal episodes viewed
				// For anime with known total episodes, check if all are viewed
				// For anime with unknown total (totalEpisodes == 0), check if all episodes in mylist are viewed
				if (totalEpisodes > 0) {
					visible = visible && (normalViewed >= totalEpisodes);
				} else {
					// Unknown total episodes: consider completed if user has episodes and all are viewed
					visible = visible && (normalEpisodes > 0 && normalViewed >= normalEpisodes);
				}
			} else if (completionFilter == "watching") {
				// Watching: some episodes viewed but not all
				// Must have at least one viewed episode
				// And either: total is unknown (totalEpisodes == 0) with more episodes available,
				//         or: total is known and not all are viewed
				if (totalEpisodes > 0) {
					visible = visible && (normalViewed > 0 && normalViewed < totalEpisodes);
				} else {
					// Unknown total episodes: watching if some viewed and not all in mylist are viewed
					visible = visible && (normalViewed > 0 && normalViewed < normalEpisodes);
				}
			} else if (completionFilter == "notstarted") {
				// Not started: no episodes viewed
				visible = visible && (normalViewed == 0);
			}
		}
		
		// Apply unwatched filter
		if (showOnlyUnwatched) {
			int normalEpisodes, normalViewed, otherEpisodes, otherViewed;
			
			if (card) {
				normalEpisodes = card->getNormalEpisodes();
				normalViewed = card->getNormalViewed();
				otherEpisodes = card->getOtherEpisodes();
				otherViewed = card->getOtherViewed();
			} else {
				normalEpisodes = cachedData.stats.normalEpisodes;
				normalViewed = cachedData.stats.normalViewed;
				otherEpisodes = cachedData.stats.otherEpisodes;
				otherViewed = cachedData.stats.otherViewed;
			}
			
			// Show only if there are unwatched episodes
			visible = visible && ((normalEpisodes > normalViewed) || (otherEpisodes > otherViewed));
		}
		
		// Apply marked for deletion filter
		if (showMarkedForDeletion) {
			// Check if this anime has any files marked for deletion
			bool hasMarkedFiles = false;
			if (watchSessionManager) {
				// Query all lids for this anime from mylist table
				QSqlDatabase db = QSqlDatabase::database();
				if (db.isOpen()) {
					QSqlQuery q(db);
					q.prepare("SELECT lid FROM mylist WHERE aid = ?");
					q.addBindValue(aid);
					if (q.exec()) {
						while (q.next()) {
							int lid = q.value(0).toInt();
							FileMarkType markType = watchSessionManager->getFileMarkType(lid);
							if (markType == FileMarkType::ForDeletion) {
								hasMarkedFiles = true;
								break;
							}
						}
					}
				}
			}
			visible = visible && hasMarkedFiles;
		}
		
		// Apply adult content filter
		if (adultContentFilter == "hide") {
			bool is18Restricted = card ? card->is18Restricted() : cachedData.is18Restricted;
			// Hide 18+ content (default)
			visible = visible && !is18Restricted;
		} else if (adultContentFilter == "showonly") {
			bool is18Restricted = card ? card->is18Restricted() : cachedData.is18Restricted;
			// Show only 18+ content
			visible = visible && is18Restricted;
		}
		// "ignore" means no filtering based on 18+ status
		
		if (visible) {
			filteredAnimeIds.append(aid);
		}
	}
	
	// Update the card manager with the filtered list
	cardManager->setAnimeIdList(filteredAnimeIds);
	
	// If using virtual scrolling, refresh the layout
	if (mylistVirtualLayout) {
		mylistVirtualLayout->refresh();
	}
	
	// For backward compatibility with non-virtual scrolling
	if (!mylistVirtualLayout && mylistCardLayout) {
		// Remove all cards from layout first
		for (AnimeCard *card : std::as_const(animeCards)) {
			mylistCardLayout->removeWidget(card);
			card->setVisible(false);
		}
		
		// Add only visible cards back to layout
		for (int aid : filteredAnimeIds) {
			AnimeCard* card = cardsMap.value(aid);
			if (card) {
				mylistCardLayout->addWidget(card);
				card->setVisible(true);
			}
		}
	}
	
	// Update status label
	mylistStatusLabel->setText(QString("MyList Status: Showing %1 of %2 anime").arg(filteredAnimeIds.size()).arg(totalCount));
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
        .arg(fileData.filename, fileData.hash)
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
        loadMylistAsCards();
        
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

void Window::createTrayIcon()
{
    // Check if system tray is available
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        LOG("System tray not available on this system");
        return;
    }
    
    // Create system tray icon menu
    trayIconMenu = new QMenu(this);
    
    QAction *showHideAction = new QAction("Show/Hide", this);
    connect(showHideAction, &QAction::triggered, this, &Window::onTrayShowHideAction);
    
    QAction *exitAction = new QAction("Exit", this);
    connect(exitAction, &QAction::triggered, this, &Window::onTrayExitAction);
    
    trayIconMenu->addAction(showHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(exitAction);
    
    // Create system tray icon
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    
    // Set icon (use application icon or a default icon)
    QIcon appIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    trayIcon->setIcon(appIcon);
    trayIcon->setToolTip("Usagi-dono");
    
    // Connect activation signal (for double-click)
    connect(trayIcon, &QSystemTrayIcon::activated, this, &Window::onTrayIconActivated);
    
    LOG("System tray icon created");
}

void Window::loadTraySettings()
{
    // Check if system tray is available
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        LOG("System tray not available, disabling tray features");
        // Disable tray checkboxes in UI
        if (trayMinimizeToTray) {
            trayMinimizeToTray->setEnabled(false);
            trayMinimizeToTray->setToolTip("System tray not available on this platform");
        }
        if (trayCloseToTray) {
            trayCloseToTray->setEnabled(false);
            trayCloseToTray->setToolTip("System tray not available on this platform");
        }
        if (trayStartMinimized) {
            trayStartMinimized->setEnabled(false);
            trayStartMinimized->setToolTip("System tray not available on this platform");
        }
        return;
    }
    
    // Load tray settings from database
    minimizeToTrayEnabled = adbapi->getTrayMinimizeToTray();
    closeToTrayEnabled = adbapi->getTrayCloseToTray();
    startMinimizedEnabled = adbapi->getTrayStartMinimized();
    
    // Update UI checkboxes
    trayMinimizeToTray->setChecked(minimizeToTrayEnabled);
    trayCloseToTray->setChecked(closeToTrayEnabled);
    trayStartMinimized->setChecked(startMinimizedEnabled);
    
    // Update tray icon visibility (this will show the tray icon if any tray feature is enabled)
    updateTrayIconVisibility();
    
    // If start minimized is enabled and tray icon is properly shown, minimize to tray on startup
    if (startMinimizedEnabled && trayIcon && trayIcon->isVisible()) {
        this->hide();
        LOG("Application started minimized to tray");
    }
    
    LOG(QString("Tray settings loaded: minimize=%1, close=%2, startMinimized=%3")
        .arg(minimizeToTrayEnabled).arg(closeToTrayEnabled).arg(startMinimizedEnabled));
}

void Window::saveTraySettings()
{
    adbapi->setTrayMinimizeToTray(trayMinimizeToTray->isChecked());
    adbapi->setTrayCloseToTray(trayCloseToTray->isChecked());
    adbapi->setTrayStartMinimized(trayStartMinimized->isChecked());
    
    // Update local flags
    minimizeToTrayEnabled = trayMinimizeToTray->isChecked();
    closeToTrayEnabled = trayCloseToTray->isChecked();
    startMinimizedEnabled = trayStartMinimized->isChecked();
    
    // Update tray icon visibility
    updateTrayIconVisibility();
    
    LOG("Tray settings saved");
}

void Window::updateTrayIconVisibility()
{
    // Only show tray icon if system tray is available and any tray feature is enabled
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    
    // Check if tray icon exists first
    if (!trayIcon) {
        return;
    }
    
    if (minimizeToTrayEnabled || closeToTrayEnabled || startMinimizedEnabled) {
        if (!trayIcon->isVisible()) {
            trayIcon->show();
            LOG("System tray icon shown");
        }
    } else {
        if (trayIcon->isVisible()) {
            trayIcon->hide();
            LOG("System tray icon hidden");
        }
    }
}

void Window::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        onTrayShowHideAction();
    }
}

void Window::onTrayShowHideAction()
{
    if (this->isVisible()) {
        // Store the current window state before hiding
        windowStateBeforeHide = this->windowState();
        this->hide();
        LOG("Window hidden to tray");
    } else {
        // Restore the previous window state
        this->setWindowState(windowStateBeforeHide);
        this->show();
        this->activateWindow();
        this->raise();
        // Force layout update to ensure content fills the window
        this->layout()->update();
        this->layout()->activate();
        LOG("Window shown from tray");
    }
}

void Window::onTrayExitAction()
{
    // Set a flag to bypass close-to-tray logic for this exit
    // Don't modify the user's closeToTrayEnabled setting
    bool originalCloseToTray = closeToTrayEnabled;
    closeToTrayEnabled = false;
    QApplication::quit();
    // Note: Application will exit, so no need to restore the flag
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
