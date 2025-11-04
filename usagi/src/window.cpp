#include "main.h"
#include "window.h"
#include "hasherthread.h"
#include "crashlog.h"
#include "logger.h"
#include <QElapsedTimer>

HasherThread hasherThread;
myAniDBApi *adbapi;
extern Window *window;
settings_struct settings;


Window::Window()
{
	qRegisterMetaType<ed2k::ed2kfilestruct>("ed2k::ed2kfilestruct");
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
    progressFile = new QProgressBar;
    progressTotal = new QProgressBar;
    progressTotalLabel = new QLabel;
    QBoxLayout *progressTotalLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    progressTotalLayout->addWidget(progressTotal);
    progressTotalLayout->addWidget(progressTotalLabel);

    pageHasher->addWidget(hashes, 0, Qt::AlignTop);
    pageHasher->addLayout(pageHasherSettings);
    pageHasher->addLayout(progress);
    pageHasher->addWidget(hasherOutput, 0, Qt::AlignTop);

    // page mylist
    mylistTreeWidget = new QTreeWidget(this);
    mylistTreeWidget->setColumnCount(9);
    mylistTreeWidget->setHeaderLabels(QStringList() << "Anime" << "Episode" << "Episode Title" << "State" << "Viewed" << "Storage" << "Mylist ID" << "Type" << "Aired");
    mylistTreeWidget->setColumnWidth(0, 300);
    mylistTreeWidget->setColumnWidth(1, 80);
    mylistTreeWidget->setColumnWidth(2, 250);
    mylistTreeWidget->setColumnWidth(3, 100);
    mylistTreeWidget->setColumnWidth(4, 80);
    mylistTreeWidget->setColumnWidth(5, 200);
    mylistTreeWidget->setColumnWidth(6, 80);
    mylistTreeWidget->setColumnWidth(7, 100);
    mylistTreeWidget->setColumnWidth(8, 180);
    mylistTreeWidget->setAlternatingRowColors(true);
    mylistTreeWidget->setSortingEnabled(true);
    mylistTreeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pageMylist->addWidget(mylistTreeWidget);
    
    // Add progress status label
    mylistStatusLabel = new QLabel("MyList Status: Ready");
    mylistStatusLabel->setAlignment(Qt::AlignCenter);
    mylistStatusLabel->setStyleSheet("padding: 5px; background-color: #f0f0f0;");
    pageMylist->addWidget(mylistStatusLabel);
    
    mylistTreeWidget->show();

    // page hasher - signals
    connect(button1, SIGNAL(clicked()), this, SLOT(Button1Click()));
    connect(button2, SIGNAL(clicked()), this, SLOT(Button2Click()));
	connect(button3, SIGNAL(clicked()), this, SLOT(Button3Click()));
    connect(buttonstart, SIGNAL(clicked()), this, SLOT(ButtonHasherStartClick()));
    connect(buttonclear, SIGNAL(clicked()), this, SLOT(ButtonHasherClearClick()));
    connect(&hasherThread, SIGNAL(requestNextFile()), this, SLOT(provideNextFileToHash()));
    connect(&hasherThread, SIGNAL(sendHash(QString)), hasherOutput, SLOT(append(QString)));
    connect(&hasherThread, SIGNAL(finished()), this, SLOT(hasherFinished()));
    connect(adbapi, SIGNAL(notifyPartsDone(int,int)), this, SLOT(getNotifyPartsDone(int,int)));
    connect(adbapi, SIGNAL(notifyFileHashed(ed2k::ed2kfilestruct)), this, SLOT(getNotifyFileHashed(ed2k::ed2kfilestruct)));
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

	// page hasher - progress
	progress->addWidget(progressFile);
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
    
    pageSettings->setRowStretch(7, 1000);
    pageSettings->addWidget(buttonSaveSettings, 8, 0);
    pageSettings->addWidget(buttonRequestMylistExport, 9, 0);

    pageSettings->setColumnStretch(1, 100);
    pageSettings->setRowStretch(10, 100);

	// page settings - signals
    connect(buttonSaveSettings, SIGNAL(clicked()), this, SLOT(saveSettings()));
    connect(buttonRequestMylistExport, SIGNAL(clicked()), this, SLOT(requestMylistExportManually()));
    connect(watcherEnabled, SIGNAL(stateChanged(int)), this, SLOT(onWatcherEnabledChanged(int)));
    connect(watcherBrowseButton, SIGNAL(clicked()), this, SLOT(onWatcherBrowseClicked()));

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
	connect(mylistTreeWidget, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(onMylistItemExpanded(QTreeWidgetItem*)));
    connect(loginbutton, SIGNAL(clicked()), this, SLOT(ButtonLoginClick()));
    
    // Initialize directory watcher
    directoryWatcher = new DirectoryWatcher(this);
    connect(directoryWatcher, &DirectoryWatcher::newFilesDetected, 
            this, &Window::onWatcherNewFilesDetected);
    
    // Initialize timer for deferred processing of already-hashed files
    hashedFilesProcessingTimer = new QTimer(this);
    hashedFilesProcessingTimer->setSingleShot(false);
    hashedFilesProcessingTimer->setInterval(HASHED_FILES_TIMER_INTERVAL);
    connect(hashedFilesProcessingTimer, &QTimer::timeout, this, &Window::processPendingHashedFiles);
    
    // Load directory watcher settings from database
    bool watcherEnabledSetting = adbapi->getWatcherEnabled();
    QString watcherDir = adbapi->getWatcherDirectory();
    bool watcherAutoStartSetting = adbapi->getWatcherAutoStart();
    
    // Block signals while setting UI values to prevent premature slot activation
    watcherEnabled->blockSignals(true);
    watcherDirectory->blockSignals(true);
    watcherAutoStart->blockSignals(true);
    
    watcherEnabled->setChecked(watcherEnabledSetting);
    watcherDirectory->setText(watcherDir);
    watcherAutoStart->setChecked(watcherAutoStartSetting);
    
    // Restore signal connections
    watcherEnabled->blockSignals(false);
    watcherDirectory->blockSignals(false);
    watcherAutoStart->blockSignals(false);
    
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


    adbapi->CreateSocket();
    
    // Automatically load mylist on startup (using timer for delayed initialization)
    startupTimer->start();
}

Window::~Window()
{
    // Stop directory watcher on cleanup
    if (directoryWatcher) {
        directoryWatcher->stopWatching();
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
	progressTotal->setValue(0);
	progressTotal->setMaximum(totalHashParts > 0 ? totalHashParts : 1);
	progressTotal->setFormat("ETA: calculating...");
	progressTotalLabel->setText("0%");
	hashingTimer.start();
	lastEtaUpdate.start();
	lastUiUpdate.start(); // Initialize UI update throttling timer
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
			QString filePath = hashes->item(i, 2)->text();
			
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
		hasherThread.start();
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
	progressFile->setValue(0);
	progressFile->setMaximum(1);
	
	// Always notify ed2k to stop hashing, even if thread isn't running
	// This ensures any ongoing hash operation is interrupted
	emit notifyStopHasher();
	
	if (hasherThread.isRunning()) {
		hasherThread.stop();
		hasherThread.wait(); // Wait for thread to finish
	}
}

void Window::provideNextFileToHash()
{
	// Look through the hashes widget for the next file that needs hashing (progress="0" and no hash)
	for(int i=0; i<hashes->rowCount(); i++)
	{
		QString progress = hashes->item(i, 1)->text();
		QString existingHash = hashes->item(i, 9)->text();
		
		if(progress == "0" && existingHash.isEmpty())
		{
			QString filePath = hashes->item(i, 2)->text();
			hasherThread.addFile(filePath);
			return;
		}
	}
	
	// No more files to hash, send empty string to signal completion
	hasherThread.addFile(QString());
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

void Window::getNotifyPartsDone(int total, int done)
{
	completedHashParts++;
	
	// Throttle UI updates to prevent freezing - update at most every 100ms
	// This dramatically reduces the number of UI updates from thousands to ~10 per second
	// Always update on the last part to ensure progress bars reach 100%
	bool isLastPart = (done == total && completedHashParts == totalHashParts);
	bool shouldUpdate = (lastUiUpdate.elapsed() >= 100) || isLastPart;
	
	if (shouldUpdate) {
		progressFile->setMaximum(total);
		progressFile->setValue(done);
		progressTotal->setValue(completedHashParts);
		
		// Update percentage label
		int percentage = totalHashParts > 0 ? (completedHashParts * 100 / totalHashParts) : 0;
		progressTotalLabel->setText(QString("%1%").arg(percentage));
		
		// Calculate and display ETA - throttled to once per second to prevent UI freeze
		if (completedHashParts > 0 && totalHashParts > 0 && lastEtaUpdate.elapsed() >= 1000) {
			qint64 elapsedMs = hashingTimer.elapsed();
			double partsPerMs = static_cast<double>(completedHashParts) / elapsedMs;
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
		
		lastUiUpdate.restart();
	}
}

void Window::getNotifyFileHashed(ed2k::ed2kfilestruct data)
{
	for(int i=0; i<hashes->rowCount(); i++)
	{
		if(hashes->item(i, 0)->text() == data.filename)
		{
            // Mark as hashed in UI (use pre-allocated color object)
            hashes->item(i, 0)->setBackground(m_hashedFileColor);
			QTableWidgetItem *itemprogress = new QTableWidgetItem(QTableWidgetItem(QString("1")));
		    hashes->setItem(i, 1, itemprogress);
		    getNotifyLogAppend(QString("File hashed: %1").arg(data.filename));
			
			QString filePath = hashes->item(i, 2)->text();
			
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
					// File already in mylist - no API call needed
					// Update status to 2 (in anidb) to prevent re-detection
					adbapi->UpdateLocalFileStatus(filePath, 2);
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
    mylistStatusLabel->setText("MyList Status: Loading from database...");
    loadMylistFromDatabase();
    mylistStatusLabel->setText("MyList Status: Ready");
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
			if(!hasherThread.isRunning())
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
                adbapi->UpdateLocalPath(tag, localPath);
                
                // Refresh mylist widget to ensure it's up to date
                loadMylistFromDatabase();
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
				adbapi->UpdateLocalPath(tag, localPath);
				
				if(renameto->checkState() > 0)
				{
					// TODO: rename
				}
				// Refresh mylist widget to show the newly added entry
				loadMylistFromDatabase();
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
	QRegularExpression bbcodeRegex("\\[url=(https?://[^\\]]+\\.tgz)\\]");
	QRegularExpressionMatch bbcodeMatch = bbcodeRegex.match(message);
	
	if(bbcodeMatch.hasMatch())
	{
		exportUrl = bbcodeMatch.captured(1);  // Capture group 1 is the URL inside [url=...]
	}
	else
	{
		// Fallback to plain URL format
		QRegularExpression plainRegex("https?://[^\\s\\]]+\\.tgz");
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
	// When an anime item is expanded, queue EPISODE API requests for missing data
	if(!item)
		return;
	
	// Get the AID from the item (stored in UserRole)
	int aid = item->data(0, Qt::UserRole).toInt();
	if(aid == 0)
		return;  // Not an anime item (might be an episode child)
	
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
	updateEpisodeInTree(eid, aid);
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
						episodeItem->setText(1, episodeNumber.toDisplayString());
					}
					else
					{
						episodeItem->setText(1, "Unknown");
					}
					
					// Update episode name (Column 2)
					if(!episodeName.isEmpty())
					{
						episodeItem->setText(2, episodeName);
					}
					else
					{
						episodeItem->setText(2, "Unknown");
					}
					
					// Remove from tracking set since data has been loaded
					episodesNeedingData.remove(eid);
					
					LOG(QString("Updated episode in tree: EID %1, epno: %2, name: %3")
						.arg(eid).arg(episodeItem->text(1)).arg(episodeName));
					return;
				}
			}
		}
	}
	
	// If we get here, the episode item wasn't found in the tree
	LOG(QString("Episode item not found in tree for EID %1 (AID %2)").arg(eid).arg(aid));
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

void Window::loadMylistFromDatabase()
{
	mylistTreeWidget->clear();
	episodesNeedingData.clear();  // Clear tracking set
	
	// Query the database for mylist entries joined with anime and episode data
	// Also try to get anime name from anime_titles table if anime table is empty
	QSqlDatabase db = QSqlDatabase::database();
	
	// Validate database connection before proceeding
	if(!validateDatabaseConnection(db, "loadMylistFromDatabase"))
	{
		mylistStatusLabel->setText("MyList Status: Database Error");
		return;
	}
	
	QString query = "SELECT m.lid, m.aid, m.eid, m.state, m.viewed, m.storage, "
					"a.nameromaji, a.nameenglish, a.eptotal, "
					"e.name as episode_name, e.epno, "
					"(SELECT title FROM anime_titles WHERE aid = m.aid AND type = 1 LIMIT 1) as anime_title, "
					"a.eps, a.typename, a.startdate, a.enddate "
					"FROM mylist m "
					"LEFT JOIN anime a ON m.aid = a.aid "
					"LEFT JOIN episode e ON m.eid = e.eid "
					"ORDER BY a.nameromaji, m.eid";
	
	QSqlQuery q(db);
	
	if(!q.exec(query))
	{
		LOG("Error loading mylist: " + q.lastError().text());
		return;
	}
	
	QMap<int, QTreeWidgetItem*> animeItems; // aid -> tree item
	// Track statistics per anime
	QMap<int, int> animeEpisodeCount;  // aid -> count of episodes in mylist
	QMap<int, int> animeViewedCount;   // aid -> count of viewed episodes
	QMap<int, int> animeEpTotal;       // aid -> total primary type episodes (eptotal)
	QMap<int, int> animeEps;           // aid -> total normal episodes (eps)
	// Track normal (type 1) vs other types separately
	QMap<int, int> animeNormalEpisodeCount;    // aid -> count of normal episodes (type 1) in mylist
	QMap<int, int> animeOtherEpisodeCount;     // aid -> count of other type episodes (types 2-6) in mylist
	QMap<int, int> animeNormalViewedCount;     // aid -> count of normal episodes viewed
	QMap<int, int> animeOtherViewedCount;      // aid -> count of other type episodes viewed
	int totalEntries = 0;
	
	while(q.next())
	{
		int lid = q.value(0).toInt();
		int aid = q.value(1).toInt();
		int eid = q.value(2).toInt();
		int state = q.value(3).toInt();
		int viewed = q.value(4).toInt();
		QString storage = q.value(5).toString();
		QString animeName = q.value(6).toString();
		QString animeNameEnglish = q.value(7).toString();
		int epTotal = q.value(8).toInt();
		QString episodeName = q.value(9).toString();
		QString epno = q.value(10).toString();  // Episode number from database
		QString animeTitle = q.value(11).toString();  // From anime_titles table
		int eps = q.value(12).toInt();  // Normal episode count from database
		QString typeName = q.value(13).toString();  // Type name from database
		QString startDate = q.value(14).toString();  // Start date from database
		QString endDate = q.value(15).toString();  // End date from database
		
		// Use English name if romaji is empty
		if(animeName.isEmpty() && !animeNameEnglish.isEmpty())
		{
			animeName = animeNameEnglish;
		}
		
		// If anime name is still empty, try anime_titles table
		if(animeName.isEmpty() && !animeTitle.isEmpty())
		{
			animeName = animeTitle;
		}
		
		// If anime name is still empty, use aid
		if(animeName.isEmpty())
		{
			animeName = QString("Anime #%1").arg(aid);
		}
		
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
			animeItem->setData(0, Qt::UserRole, aid); // Store aid
			
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
		// Update on every row since all episodes have the same data
		if(!typeName.isEmpty() && animeItem->text(7).isEmpty())
		{
			animeItem->setText(7, typeName);
		}
		
		// Set Aired column (column 8) for anime parent item
		// Update on every row since all episodes have the same data
		if(!startDate.isEmpty() && animeItem->text(8).isEmpty())
		{
			aired airedDates(startDate, endDate);
			animeItem->setText(8, airedDates.toDisplayString());
			// Store the aired object for proper sorting by date
			animeItem->setAired(airedDates);
		}
		
		// Create episode item as child of anime
		EpisodeTreeWidgetItem *episodeItem = new EpisodeTreeWidgetItem(animeItem);
		episodeItem->setText(0, ""); // Empty for episode child
		
		// Column 1: Episode number using epno type
		int episodeType = 1;  // Default to normal episode
		if(!epno.isEmpty())
		{
			// Create epno object from string
			::epno episodeNumber(epno);
			
			// Store epno object in the item for sorting
			episodeItem->setEpno(episodeNumber);
			
			// Display formatted episode number (with leading zeros removed)
			episodeItem->setText(1, episodeNumber.toDisplayString());
			
			// Get the episode type (1 = normal, 2-6 = other types)
			episodeType = episodeNumber.type();
		}
		else
		{
			// Fallback to EID if episode number not available
			episodeItem->setText(1, "Loading...");
			episodesNeedingData.insert(eid);  // Track this episode for lazy loading
		}
		
		// Update statistics for this anime based on episode type
		animeEpisodeCount[aid]++;
		if(episodeType == 1)
		{
			// Normal episode (type 1)
			animeNormalEpisodeCount[aid]++;
			if(viewed)
			{
				animeNormalViewedCount[aid]++;
			}
		}
		else
		{
			// Other types (types 2-6: special, credit, trailer, parody, other)
			animeOtherEpisodeCount[aid]++;
			if(viewed)
			{
				animeOtherViewedCount[aid]++;
			}
		}
		
		if(viewed)
		{
			animeViewedCount[aid]++;
		}
		
		// Column 2: Episode title
		if(episodeName.isEmpty())
		{
			episodeName = "Loading...";
			episodesNeedingData.insert(eid);  // Track this episode for lazy loading
		}
		episodeItem->setText(2, episodeName);
		
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
		episodeItem->setText(3, stateStr);
		
		// Viewed: 0=no, 1=yes
		episodeItem->setText(4, viewed ? "Yes" : "No");
		episodeItem->setText(5, storage);
		episodeItem->setText(6, QString::number(lid));
		
		episodeItem->setData(0, Qt::UserRole, eid); // Store eid
		episodeItem->setData(0, Qt::UserRole + 1, lid); // Store lid
		
		totalEntries++;
	}
	
	// Update anime rows with aggregate statistics
	for(QMap<int, QTreeWidgetItem*>::iterator it = animeItems.begin(); it != animeItems.end(); ++it)
	{
		int aid = it.key();
		QTreeWidgetItem *animeItem = it.value();
		
		int episodesInMylist = animeEpisodeCount[aid];
		int viewedEpisodes = animeViewedCount[aid];
		int totalEpisodes = animeEpTotal[aid];
		int totalNormalEpisodes = animeEps[aid];
		int normalEpisodes = animeNormalEpisodeCount[aid];
		int otherEpisodes = animeOtherEpisodeCount[aid];
		int normalViewed = animeNormalViewedCount[aid];
		int otherViewed = animeOtherViewedCount[aid];
		
		// Column 1 (Episode): show format "A/B+C"
		// A = normal episodes in mylist, B = total normal episodes (from Eps), C = other types in mylist
		// When Eps is not available, show "?" to indicate unknown total
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
			// If eps is not available, show "?" to indicate unknown total instead of using same value
			if(otherEpisodes > 0)
			{
				episodeText = QString("%1/?+%2").arg(normalEpisodes).arg(otherEpisodes);
			}
			else
			{
				episodeText = QString("%1/?").arg(normalEpisodes);
			}
		}
		animeItem->setText(1, episodeText);
		
		// Column 4 (Viewed): show format "A/B+C"
		// A = normal episodes viewed, B = total normal episodes in mylist, C = other types viewed
		QString viewedText;
		if(otherEpisodes > 0)
		{
			viewedText = QString("%1/%2+%3").arg(normalViewed).arg(normalEpisodes).arg(otherViewed);
		}
		else
		{
			viewedText = QString("%1/%2").arg(normalViewed).arg(normalEpisodes);
		}
		animeItem->setText(4, viewedText);
	}
	
	// Keep anime items collapsed by default
	// (User can expand manually if needed)
	
	LOG(QString("Loaded %1 mylist entries for %2 anime").arg(totalEntries).arg(animeItems.size()));
	mylistStatusLabel->setText(QString("MyList Status: %1 entries loaded").arg(totalEntries));
	
	// Set default sort order to ascending by episode column (column 1)
	mylistTreeWidget->sortByColumn(1, Qt::AscendingOrder);
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
							.arg(currentAid).arg(animeQueryExec.lastError().text()));
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
							.arg(currentAid).arg(animeQueryExec.lastError().text()));
					}
				}
				
				// Always update typename, startdate, enddate from mylist export
				// This runs independently of the eptotal/eps check above
				if(!currentAid.isEmpty())
				{
					QSqlQuery animeQueryExec(db);
					// Always update typename, startdate, enddate (even if already set)
					animeQueryExec.prepare("UPDATE `anime` SET `typename` = :typename, "
						"`startdate` = :startdate, `enddate` = :enddate WHERE `aid` = :aid");
					animeQueryExec.bindValue(":typename", typeName.isEmpty() ? QVariant() : typeName);
					animeQueryExec.bindValue(":startdate", startDate.isEmpty() ? QVariant() : startDate);
					animeQueryExec.bindValue(":enddate", endDate.isEmpty() ? QVariant() : endDate);
					animeQueryExec.bindValue(":aid", currentAid.toInt());
					
					if(!animeQueryExec.exec())
					{
						LOG(QString("Warning: Failed to update anime metadata (aid=%1): %2")
							.arg(currentAid).arg(animeQueryExec.lastError().text()));
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
						.arg(currentEid)
						.arg(epNo_escaped)
						.arg(epName_escaped)
						.arg(epNameRomaji_escaped)
						.arg(epNameKanji_escaped);
					
					QSqlQuery episodeQueryExec(db);
					if(!episodeQueryExec.exec(episodeQuery))
					{
						LOG(QString("Warning: Failed to insert episode data (eid=%1): %2")
							.arg(currentEid).arg(episodeQueryExec.lastError().text()));
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
				
				// Insert into database with proper lid value
				QString q = QString("INSERT OR REPLACE INTO `mylist` "
					"(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) "
					"VALUES (%1, %2, %3, %4, %5, %6, %7, '%8')")
					.arg(lid)  // LId from File element
					.arg(fid.isEmpty() ? "0" : fid)  // Id from File element
					.arg(currentEid.isEmpty() ? "0" : currentEid)  // Id from Ep element
					.arg(currentAid)  // Id from Anime element
					.arg(gid.isEmpty() ? "0" : gid)
					.arg(myState.isEmpty() ? "0" : myState)  // MyState as state
					.arg(viewed)
					.arg(storage_escaped);
				
				QSqlQuery query(db);
				if(query.exec(q))
				{
					count++;
				}
				else
				{
					LOG(QString("Error inserting mylist entry (lid=%1): %2").arg(lid).arg(query.lastError().text()));
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
	if (!hasherThread.isRunning()) {
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
			hasherThread.start();
			
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
