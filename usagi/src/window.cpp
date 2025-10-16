#include "main.h"
#include "window.h"
#include "hasherthread.h"
#include "crashlog.h"

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
	
	// Initialize notification tracking
	expectedNotificationsToCheck = 0;
	notificationsCheckedWithoutExport = 0;
	isCheckingNotifications = false;
	
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

    layout->addWidget(tabwidget, 0, Qt::AlignTop);

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

    pageHasher->addWidget(hashes, 0, Qt::AlignTop);
    pageHasher->addLayout(pageHasherSettings);
    pageHasher->addLayout(progress);
    pageHasher->addWidget(hasherOutput, 0, Qt::AlignTop);

    // page mylist
    mylistTreeWidget = new QTreeWidget(this);
    mylistTreeWidget->setColumnCount(6);
    mylistTreeWidget->setHeaderLabels(QStringList() << "Anime" << "Episode" << "State" << "Viewed" << "Storage" << "Mylist ID");
    mylistTreeWidget->setColumnWidth(0, 300);
    mylistTreeWidget->setColumnWidth(1, 150);
    mylistTreeWidget->setColumnWidth(2, 100);
    mylistTreeWidget->setColumnWidth(3, 80);
    mylistTreeWidget->setColumnWidth(4, 200);
    mylistTreeWidget->setColumnWidth(5, 80);
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
    connect(this, SIGNAL(hashFiles(QStringList)), &hasherThread, SLOT(getFiles(QStringList)));
    connect(&hasherThread, SIGNAL(sendHash(QString)), hasherOutput, SLOT(append(QString)));
    connect(&hasherThread, SIGNAL(finished()), this, SLOT(hasherFinished()));
    connect(adbapi, SIGNAL(notifyPartsDone(int,int)), this, SLOT(getNotifyPartsDone(int,int)));
    connect(adbapi, SIGNAL(notifyFileHashed(ed2k::ed2kfilestruct)), this, SLOT(getNotifyFileHashed(ed2k::ed2kfilestruct)));
    connect(buttonstop, SIGNAL(clicked()), this, SLOT(ButtonHasherStopClick()));
    connect(this, SIGNAL(notifyStopHasher()), adbapi, SLOT(getNotifyStopHasher()));
    connect(adbapi, SIGNAL(notifyLogAppend(QString)), this, SLOT(getNotifyLogAppend(QString)));
	connect(adbapi, SIGNAL(notifyMylistAdd(QString,int)), this, SLOT(getNotifyMylistAdd(QString,int)));
	connect(markwatched, SIGNAL(stateChanged(int)), this, SLOT(markwatchedStateChanged(int)));

    // page hasher - hashes
	hashes->setColumnCount(9);
    hashes->setRowCount(0);
    hashes->setRowHeight(0, 20);
    hashes->verticalHeader()->setDefaultSectionSize(20);
    hashes->setSelectionBehavior(QAbstractItemView::SelectRows);
    hashes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hashes->verticalHeader()->hide();
    hashes->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    hashes->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	hashes->setHorizontalHeaderLabels((QStringList() << "Filename" << "Progress" << "path" << "LF" << "LL" << "RF" << "RL" << "Ren" << "FP"));
    hashes->setColumnWidth(0, 600);
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
	progress->addWidget(progressTotal);

    // page settings
    labelLogin = new QLabel("Login:");
    editLogin = new QLineEdit;
	editLogin->setText(adbapi->getUsername());
    labelPassword = new QLabel("Password:");
    editPassword = new QLineEdit;
	editPassword->setText(adbapi->getPassword());
    editPassword->setEchoMode(QLineEdit::Password);
    buttonSaveSettings = new QPushButton("Save");

    pageSettings->addWidget(labelLogin, 0, 0);
    pageSettings->addWidget(editLogin, 0, 1);
    pageSettings->addWidget(labelPassword, 1, 0);
    pageSettings->addWidget(editPassword, 1, 1);
    pageSettings->setRowStretch(2, 1000);
    pageSettings->addWidget(buttonSaveSettings, 3, 0);

    pageSettings->setColumnStretch(2, 100);
    pageSettings->setRowStretch(5, 100);

	// page settings - signals
    connect(buttonSaveSettings, SIGNAL(clicked()), this, SLOT(saveSettings()));

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
    connect(loginbutton, SIGNAL(clicked()), this, SLOT(ButtonLoginClick()));

    // end
    this->setLayout(layout);


    adbapi->CreateSocket();
    
    // Automatically load mylist on startup (using timer for delayed initialization)
    startupTimer->start();
}

Window::~Window()
{
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

void Window::ButtonHasherStartClick()
{
	QFileInfo file;
	int totalparts = 0;
	QStringList files;
	for(int i=0; i<hashes->rowCount(); i++)
	{
		if(hashes->item(i, 1)->text() == "0")
		{
			files.append(hashes->item(i, 2)->text());
			file.setFile(files.back());
			double a = file.size();
			double b = a/102400;
			double c = ceil(b);
			totalparts += c;
		}
//		logOutput->append(QString::number(totalparts));
	}
	progressTotal->setValue(0);
	progressTotal->setMaximum(totalparts-1);
//	logOutput->append(QString::number(totalparts));
	buttonstart->setEnabled(0);
	buttonclear->setEnabled(0);
	emit hashFiles(files);
//	adbapi->Auth();
}

void Window::ButtonHasherStopClick()
{
	buttonstart->setEnabled(1);
	buttonclear->setEnabled(1);
	progressTotal->setValue(0);
	progressTotal->setMaximum(1);
	progressFile->setValue(0);
	progressFile->setMaximum(1);
	emit notifyStopHasher();
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
    qDebug() << logMsg;
    getNotifyLogAppend(logMsg);
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
	progressFile->setMaximum(total);
	progressFile->setValue(done);
	progressTotal->setValue(progressTotal->value()+1);
//	logOutput->append(QString::number(progressTotal->value()));
}

void Window::getNotifyFileHashed(ed2k::ed2kfilestruct data)
{
	QString tag;
	for(int i=0; i<hashes->rowCount(); i++)
	{
		if(hashes->item(i, 0)->text() == data.filename)
		{
            QColor yellow; yellow.setRgb(255, 255, 0);
//			hashes->item(i, 0)->setBackgroundColor(yellow.toRgb());
            hashes->item(i, 0)->setBackground(yellow.toRgb());
			QTableWidgetItem *itemprogress = new QTableWidgetItem(QTableWidgetItem(QString("1")));
		    hashes->setItem(i, 1, itemprogress);
		    getNotifyLogAppend(QString("File hashed: %1").arg(data.filename));
			if(addtomylist->checkState() > 0)
			{
				std::bitset<2> li(adbapi->LocalIdentify(data.size, data.hexdigest));
				hashes->item(i, 3)->setText(QString((li[0])?"1":"0")); // LF
				if(li[0] == 0)
				{
					tag = adbapi->File(data.size, data.hexdigest);
					hashes->item(i, 5)->setText(tag);
				}
				else
				{
					hashes->item(i, 5)->setText("0");
				}

				hashes->item(i, 4)->setText(QString((li[1])?"1":"0")); // LL
				if(li[1] == 0)
				{
					tag = adbapi->MylistAdd(data.size, data.hexdigest, markwatched->checkState(), hasherFileState->currentIndex(), storage->text());
					hashes->item(i, 6)->setText(tag);
				}
				else
				{
					hashes->item(i, 6)->setText("0");
				}
//				adbapi->File(data.size, data.hexdigest);
//				QTableWidgetItem *itemtag = new QTableWidgetItem(QTableWidgetItem(tag));
//				hashes->setItem(i, 3, itemtag);
			}
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
	buttonstart->setEnabled(1);
	buttonclear->setEnabled(1);
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
	
	// Also write to persistent log file
	CrashLog::logMessage(str);
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
	qDebug()<<editLogin->text()<<editPassword->text();
	adbapi->setUsername(editLogin->text());
	adbapi->setPassword(editPassword->text());
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

void myAniDBApi::Debug(QString msg)
{
	// Output to console (for development and debugging)
	qDebug() << msg;
	// Also emit signal to update Log tab in UI
	emit notifyLogAppend(msg);
//	window->logAppend(msg);
}

void Window::getNotifyMylistAdd(QString tag, int code)
{
    QString logMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " getNotifyMylistAdd() tag=" + tag + " code=" + QString::number(code);
    qDebug() << logMsg;
    getNotifyLogAppend(logMsg);
	for(int i=0; i<hashes->rowCount(); i++)
	{
        QString itemLogMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " hashes->item(i, 5)->text() == tag " + hashes->item(i, 5)->text() + " == " + tag;
        qDebug() << itemLogMsg;
        getNotifyLogAppend(itemLogMsg);
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
                qDebug() << msg310;
                getNotifyLogAppend(msg310);
                return;
            }
            if(code == 320)
            {
                hashes->item(i, 0)->setBackground(red.toRgb());
                hashes->item(i, 1)->setText("4"); // no such file
                QString msg320 = "320-4";
                qDebug() << msg320;
                getNotifyLogAppend(msg320);
                return;
            }
            else if(code == 311 || code == 210)
			{
                hashes->item(i, 0)->setBackground(green_dark.toRgb());
				hashes->item(i, 1)->setText("3");
                QString msg311 = "311/210-3";
                qDebug() << msg311;
                getNotifyLogAppend(msg311);
				if(renameto->checkState() > 0)
				{
					// TODO: rename
				}
				return;
			}
		}
	}
}

void Window::getNotifyLoggedIn(QString tag, int code)
{
    QString logMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " [Window] Login notification received - Tag: " + tag + " Code: " + QString::number(code);
    qDebug() << logMsg;
    getNotifyLogAppend(logMsg);
    loginbutton->setText(QString("Logout - logged in with tag %1 and code %2").arg(tag).arg(code));
	
	// Enable notifications after successful login
	adbapi->NotifyEnable();
	logOutput->append("Notifications enabled");
}

void Window::getNotifyLoggedOut(QString tag, int code)
{
    QString logMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " [Window] getNotifyLoggedOut";
    qDebug() << logMsg;
    getNotifyLogAppend(logMsg);
    loginbutton->setText(QString("Login - logged out with tag %1 and code %2").arg(tag).arg(code));
}

void Window::getNotifyMessageReceived(int nid, QString message)
{
	QString logMsg = QString(__FILE__) + " " + QString::number(__LINE__) + " [Window] Notification received: " + QString::number(nid) + " " + message;
	qDebug() << logMsg;
	getNotifyLogAppend(logMsg);
	logOutput->append(QString("Notification %1 received").arg(nid));
	
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
		isDownloadingExport = true;
		logOutput->append(QString("MyList export link found: %1").arg(exportUrl));
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
					
					logOutput->append(QString("Export downloaded to: %1").arg(tempPath));
					mylistStatusLabel->setText("MyList Status: Parsing export...");
					
					// Parse the csv-adborg file
					int count = parseMylistCSVAdborg(tempPath);
					
					if(count > 0)
					{
						logOutput->append(QString("Successfully imported %1 mylist entries").arg(count));
						mylistStatusLabel->setText(QString("MyList Status: %1 entries loaded").arg(count));
						loadMylistFromDatabase();  // Refresh the display
					}
					else
					{
						logOutput->append("No entries imported from notification export");
						mylistStatusLabel->setText("MyList Status: Import failed");
					}
					
					// Clean up temporary file
					QFile::remove(tempPath);
				}
				else
				{
					logOutput->append("Error: Cannot save export file");
					mylistStatusLabel->setText("MyList Status: Download failed");
				}
			}
			else
			{
				logOutput->append(QString("Error downloading export: %1").arg(reply->errorString()));
				mylistStatusLabel->setText("MyList Status: Download failed");
			}
			
			reply->deleteLater();
			manager->deleteLater();
		});
	}
	else
	{
		logOutput->append("No mylist export link found in notification");
		
		// Track notifications checked without finding export
		if(isCheckingNotifications)
		{
			notificationsCheckedWithoutExport++;
			
			// If we've checked all expected notifications and found no export, request one
			if(notificationsCheckedWithoutExport >= expectedNotificationsToCheck)
			{
				logOutput->append(QString("Checked %1 notifications with no export link found - requesting new export").arg(expectedNotificationsToCheck));
				mylistStatusLabel->setText("MyList Status: Requesting export...");
				
				// Request MYLISTEXPORT with csv-adborg template
				adbapi->MylistExport("csv-adborg");
				
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
	logOutput->append(QString("Starting to check %1 notifications for mylist export link").arg(count));
}

void Window::getNotifyExportQueued(QString tag)
{
	// 217 EXPORT QUEUED - Export request accepted
	logOutput->append(QString("MyList export queued successfully (Tag: %1)").arg(tag));
	mylistStatusLabel->setText("MyList Status: Export queued - waiting for notification...");
	// AniDB will send a notification when the export is ready
	// The notification will contain the download link
}

void Window::getNotifyExportAlreadyInQueue(QString tag)
{
	// 318 EXPORT ALREADY IN QUEUE - Cannot queue another export
	logOutput->append(QString("MyList export already in queue (Tag: %1) - waiting for current export to complete").arg(tag));
	mylistStatusLabel->setText("MyList Status: Export already queued - waiting...");
	// No need to take action - wait for the existing export notification
}

void Window::getNotifyExportNoSuchTemplate(QString tag)
{
	// 317 EXPORT NO SUCH TEMPLATE - Invalid template name
	logOutput->append(QString("ERROR: MyList export template not found (Tag: %1)").arg(tag));
	mylistStatusLabel->setText("MyList Status: Export failed - invalid template");
	// This should not happen with "csv-adborg" template, but log for debugging
}

void Window::hashesinsertrow(QFileInfo file, Qt::CheckState ren)
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
//	hashes->setItem(hashes->rowCount()-1, 9, item10);
}

void Window::loadMylistFromDatabase()
{
	mylistTreeWidget->clear();
	
	// Query the database for mylist entries joined with anime and episode data
	QSqlDatabase db = QSqlDatabase::database();
	QString query = "SELECT m.lid, m.aid, m.eid, m.state, m.viewed, m.storage, "
					"a.nameromaji, a.nameenglish, a.eptotal, "
					"e.name as episode_name "
					"FROM mylist m "
					"LEFT JOIN anime a ON m.aid = a.aid "
					"LEFT JOIN episode e ON m.eid = e.eid "
					"ORDER BY a.nameromaji, m.eid";
	
	QSqlQuery q(db);
	
	if(!q.exec(query))
	{
		logOutput->append("Error loading mylist: " + q.lastError().text());
		return;
	}
	
	QMap<int, QTreeWidgetItem*> animeItems; // aid -> tree item
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
		
		// Use English name if romaji is empty
		if(animeName.isEmpty() && !animeNameEnglish.isEmpty())
		{
			animeName = animeNameEnglish;
		}
		
		// If anime name is still empty, use aid
		if(animeName.isEmpty())
		{
			animeName = QString("Anime #%1").arg(aid);
		}
		
		// Get or create the anime parent item
		QTreeWidgetItem *animeItem;
		if(animeItems.contains(aid))
		{
			animeItem = animeItems[aid];
		}
		else
		{
			animeItem = new QTreeWidgetItem(mylistTreeWidget);
			animeItem->setText(0, animeName);
			animeItem->setData(0, Qt::UserRole, aid); // Store aid
			animeItems[aid] = animeItem;
			mylistTreeWidget->addTopLevelItem(animeItem);
		}
		
		// Create episode item as child of anime
		QTreeWidgetItem *episodeItem = new QTreeWidgetItem(animeItem);
		episodeItem->setText(0, ""); // Empty for episode child
		
		// Build episode display string
		QString episodeDisplay = QString("Episode %1").arg(eid);
		if(!episodeName.isEmpty())
		{
			episodeDisplay += QString(" - %1").arg(episodeName);
		}
		episodeItem->setText(1, episodeDisplay);
		
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
		episodeItem->setText(2, stateStr);
		
		// Viewed: 0=no, 1=yes
		episodeItem->setText(3, viewed ? "Yes" : "No");
		episodeItem->setText(4, storage);
		episodeItem->setText(5, QString::number(lid));
		
		episodeItem->setData(0, Qt::UserRole, eid); // Store eid
		episodeItem->setData(0, Qt::UserRole + 1, lid); // Store lid
		
		totalEntries++;
	}
	
	// Expand all anime items
	mylistTreeWidget->expandAll();
	
	logOutput->append(QString("Loaded %1 mylist entries for %2 anime").arg(totalEntries).arg(animeItems.size()));
	mylistStatusLabel->setText(QString("MyList Status: %1 entries loaded").arg(totalEntries));
}

int Window::parseMylistCSVAdborg(const QString &tarGzPath)
{
	// Parse csv-adborg format mylist export (tar.gz containing anime/*.csv files)
	// Each CSV file contains pipe-delimited data with sections: anime info, FILES, GROUPS, GENREN
	int count = 0;
	QSqlDatabase db = QSqlDatabase::database();
	
	// Extract tar.gz to temporary directory
	QString tempDir = QDir::tempPath() + "/usagi_mylist_" + QString::number(QDateTime::currentMSecsSinceEpoch());
	QDir().mkpath(tempDir);
	
	// Use QProcess to extract tar.gz
	QProcess tarProcess;
	tarProcess.setWorkingDirectory(tempDir);
	tarProcess.start("tar", QStringList() << "-xzf" << tarGzPath);
	
	if(!tarProcess.waitForFinished(30000))  // 30 second timeout
	{
		logOutput->append("Error: Failed to extract tar.gz file (timeout)");
		QDir(tempDir).removeRecursively();
		return 0;
	}
	
	if(tarProcess.exitCode() != 0)
	{
		logOutput->append("Error: Failed to extract tar.gz file: " + tarProcess.readAllStandardError());
		QDir(tempDir).removeRecursively();
		return 0;
	}
	
	// Find all CSV files in anime/ subdirectory
	QDir animeDir(tempDir + "/anime");
	if(!animeDir.exists())
	{
		logOutput->append("Error: anime/ directory not found in tar.gz");
		QDir(tempDir).removeRecursively();
		return 0;
	}
	
	QStringList csvFiles = animeDir.entryList(QStringList() << "*.csv", QDir::Files);
	
	db.transaction();
	
	// Process each CSV file
	foreach(const QString &csvFile, csvFiles)
	{
		QString filePath = animeDir.absoluteFilePath(csvFile);
		QFile file(filePath);
		
		if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
			continue;
		
		QTextStream in(&file);
		QString content = in.readAll();
		file.close();
		
		// Parse the file - look for FILES section
		QStringList lines = content.split('\n');
		bool inFilesSection = false;
		QStringList fileHeaders;
		
		for(int i = 0; i < lines.size(); i++)
		{
			QString line = lines[i].trimmed();
			
			if(line.isEmpty())
				continue;
			
			// Check for FILES section marker
			if(line == "FILES")
			{
				inFilesSection = true;
				// Next line should be headers
				if(i + 1 < lines.size())
				{
					fileHeaders = lines[i + 1].split('|');
					i++;  // Skip the header line
				}
				continue;
			}
			
			// Check for next section (GROUPS, GENREN, etc.)
			if(line == "GROUPS" || line == "GENREN")
			{
				inFilesSection = false;
				continue;
			}
			
			// Parse file entries in FILES section
			if(inFilesSection && !fileHeaders.isEmpty())
			{
				QStringList fields = line.split('|');
				
				if(fields.size() < 20)  // Need at least 20 fields for complete data
					continue;
				
				// Extract fields from FILES section
				// AID|EID|GID|FID|size|ed2k|md5|sha1|crc|Length|Type|Quality|Resolution|vCodec|aCodec|sub|dub|isWatched|State|myState|FileType
				QString aid = fields[0].trimmed();
				QString eid = fields[1].trimmed();
				QString gid = fields[2].trimmed();
				QString fid = fields[3].trimmed();
				QString isWatched = fields.size() > 17 ? fields[17].trimmed() : "0";
				QString state = fields.size() > 18 ? fields[18].trimmed() : "0";
				QString myState = fields.size() > 19 ? fields[19].trimmed() : "0";
				
				// Validate required fields
				if(fid.isEmpty() || aid.isEmpty())
					continue;
				
				// Insert into database
				// We don't have lid from this format, so we'll use fid as a unique identifier
				QString q = QString("INSERT OR REPLACE INTO `mylist` "
					"(`lid`, `fid`, `eid`, `aid`, `gid`, `state`, `viewed`, `storage`) "
					"VALUES (%1, %2, %3, %4, %5, %6, %7, '')")
					.arg(fid)  // Use fid as lid since we don't have lid
					.arg(fid)
					.arg(eid.isEmpty() ? "0" : eid)
					.arg(aid)
					.arg(gid.isEmpty() ? "0" : gid)
					.arg(myState.isEmpty() ? "0" : myState)  // Use myState as state
					.arg(isWatched == "1" ? "1" : "0");  // viewed = isWatched
				
				QSqlQuery query(db);
				if(query.exec(q))
				{
					count++;
				}
			}
		}
	}
	
	db.commit();
	
	// Clean up temporary directory
	QDir(tempDir).removeRecursively();
	
	return count;
}
