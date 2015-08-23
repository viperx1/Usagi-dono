#include "main.h"
#include "window.h"
#include "hasherthread.h"

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
    safeclose = new QTimer;
    safeclose->setInterval(100);
    connect(safeclose, SIGNAL(timeout()), this, SLOT(safeClose()));

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
    pageNotifyParent = new QWidget;
    pageNotify = new QBoxLayout(QBoxLayout::TopToBottom, pageNotifyParent);
    pageSettingsParent = new QWidget;
    pageSettings = new QGridLayout(pageSettingsParent);
    pageLogParent = new QWidget;
    pageLog = new QBoxLayout(QBoxLayout::TopToBottom, pageLogParent);
	pageApiTesterParent = new QWidget;
	pageApiTester = new QBoxLayout(QBoxLayout::TopToBottom, pageApiTesterParent);

    layout->addWidget(tabwidget, 0, 0);

	// tabs
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

    pageHasher->addWidget(hashes, 0, 0);
    pageHasher->addLayout(pageHasherSettings);
    pageHasher->addLayout(progress);
    pageHasher->addWidget(hasherOutput, 0, 0);

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
    pageHasherSettings->addWidget(renameto, 3, 0, Qt::AlignRight);
	pageHasherSettings->addLayout(layout1, 2, 1, 1, 6);
	pageHasherSettings->addLayout(layout2, 3, 1, 1, 6);
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
    connect(loginbutton, SIGNAL(clicked()), this, SLOT(ButtonLoginClick()));

    // end
    this->setLayout(layout);


    adbapi->CreateSocket();
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
            QFileInfo file = files.first();
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
				QFileInfo file = directory_walker.next();
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
			QFileInfo file = directory_walker.next();
			hashesinsertrow(file, renameto->checkState());
		}
    }}
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
    qDebug()<<__FILE__<<__LINE__<<"loggedin="<<loggedin;
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
			hashes->item(i, 0)->setBackgroundColor(yellow.toRgb());
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
				qSort(selrows.begin(), selrows.end());
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
	emit notifyLogAppend(msg);
//	window->logAppend(msg);
}

void Window::getNotifyMylistAdd(QString tag, int code)
{
    qDebug()<<__FILE__<<__LINE__<<"getNotifyMylistAdd()"<<tag<<code;
	for(int i=0; i<hashes->rowCount(); i++)
	{
        qDebug()<<__FILE__<<__LINE__<<"hashes->item(i, 5)->text() == tag"<<hashes->item(i, 5)->text()<<"=="<<tag;
        if(hashes->item(i, 5)->text() == tag || hashes->item(i, 6)->text() == tag)
		{
			QColor green_light; green_light.setRgb(0, 255, 0);
			QColor green_dark; green_dark.setRgb(0, 140, 0);
            QColor red; red.setRgb(255, 0, 0);
            if(code == 310) // already in mylist
            {
                hashes->item(i, 0)->setBackgroundColor(green_light.toRgb());
                hashes->item(i, 1)->setText("2");
                qDebug()<<"310-2";
                return;
            }
            if(code == 320)
            {
                hashes->item(i, 0)->setBackgroundColor(red.toRgb());
                hashes->item(i, 1)->setText("4"); // no such file
                qDebug()<<"320-4";
                return;
            }
            else if(code == 311 || code == 210)
			{
				hashes->item(i, 0)->setBackgroundColor(green_dark.toRgb());
				hashes->item(i, 1)->setText("3");
                qDebug()<<"311/210-3";
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
    qDebug()<<__FILE__<<__LINE__<<"getNotifyLoggedIn";
    loginbutton->setText(QString("Logout - logged in with tag %1 and code %2").arg(tag).arg(code));
}

void Window::getNotifyLoggedOut(QString tag, int code)
{
    qDebug()<<__FILE__<<__LINE__<<"getNotifyLoggedOut";
    loginbutton->setText(QString("Login - logged out with tag %1 and code %2").arg(tag).arg(code));
}

void Window::hashesinsertrow(QFileInfo file, Qt::CheckState ren)
{
	QTableWidgetItem *item1 = new QTableWidgetItem(QTableWidgetItem(QString(file.fileName())));
	QColor colorgray;
	colorgray.setRgb(230, 230, 230);
	item1->setBackgroundColor(colorgray.toRgb());
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
