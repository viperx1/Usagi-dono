#include "hashercoordinator.h"
#include "window.h"
#include "anidbapi.h"
#include "hasherthreadpool.h"
#include "logger.h"
#include <QFileDialog>
#include <QListView>
#include <QTreeView>
#include <QDirIterator>
#include <QFileInfo>
#include <QAbstractItemView>
#include <QEvent>
#include <QKeyEvent>

// External hasher thread pool
extern HasherThreadPool *hasherThreadPool;

HasherCoordinator::HasherCoordinator(AniDBApi *adbapi, QWidget *parent)
    : QObject(parent)
    , m_adbapi(adbapi)
    , m_hasherThreadPool(hasherThreadPool)
    , m_totalHashParts(0)
    , m_completedHashParts(0)
{
    m_hashedFileColor = QColor(Qt::yellow);
    
    // Create timer for deferred processing
    m_hashedFilesProcessingTimer = new QTimer(this);
    m_hashedFilesProcessingTimer->setInterval(HASHED_FILES_TIMER_INTERVAL);
    connect(m_hashedFilesProcessingTimer, &QTimer::timeout, this, &HasherCoordinator::processPendingHashedFiles);
    
    createUI(parent);
    setupConnections();
}

HasherCoordinator::~HasherCoordinator()
{
    // Widgets are deleted by Qt parent-child relationship
}

void HasherCoordinator::createUI(QWidget *parent)
{
    // Create main page widget
    m_pageHasherParent = new QWidget(parent);
    m_pageHasher = new QBoxLayout(QBoxLayout::TopToBottom, m_pageHasherParent);
    
    // Create hasher widgets
    m_pageHasherSettings = new QGridLayout;
    m_button1 = new QPushButton("Add files...");
    m_button2 = new QPushButton("Add directories...");
    m_button3 = new QPushButton("Last directory");
    m_hashes = new hashes_();
    m_hasherOutput = new QTextEdit;
    m_hasherFileState = new QComboBox;
    m_addToMyList = new QCheckBox("Add file(s) to MyList");
    m_markWatched = new QCheckBox("Mark watched (no change)");
    QLabel *label1 = new QLabel("Set state:");
    m_buttonStart = new QPushButton("Start");
    m_buttonStop = new QPushButton("Stop");
    m_buttonClear = new QPushButton("Clear");
    m_moveTo = new QCheckBox("Move to:");
    m_renameTo = new QCheckBox("Rename to:");
    m_moveToDir = new QLineEdit;
    m_renameToPattern = new QLineEdit;
    m_storage = new QLineEdit;
    
    QBoxLayout *layout1 = new QBoxLayout(QBoxLayout::LeftToRight);
    QBoxLayout *layout2 = new QBoxLayout(QBoxLayout::LeftToRight);
    QPushButton *movetodirbutton = new QPushButton("...");
    QPushButton *patternhelpbutton = new QPushButton("?");
    QBoxLayout *progress = new QBoxLayout(QBoxLayout::TopToBottom);
    
    // Create progress bars for each thread
    int numThreads = m_hasherThreadPool->threadCount();
    for (int i = 0; i < numThreads; ++i) {
        QProgressBar *threadProgress = new QProgressBar;
        threadProgress->setFormat(QString("Thread %1: %p%").arg(i));
        m_threadProgressBars.append(threadProgress);
        progress->addWidget(threadProgress);
    }
    
    m_progressTotal = new QProgressBar;
    m_progressTotalLabel = new QLabel;
    QBoxLayout *progressTotalLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    progressTotalLayout->addWidget(m_progressTotal);
    progressTotalLayout->addWidget(m_progressTotalLabel);
    
    // Setup hashes table
    m_hashes->setColumnCount(10);
    QStringList headers;
    headers << "Filename" << "Status" << "Path" << "Size" << "Audio" << "File" << "MyList" << "Move" << "Rename" << "Hash";
    m_hashes->setHorizontalHeaderLabels(headers);
    m_hashes->hideColumn(2); // Hide path column
    m_hashes->hideColumn(9); // Hide hash column
    m_hashes->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_hashes->horizontalHeader()->setStretchLastSection(true);
    
    // Add widgets to layout
    m_pageHasher->addWidget(m_hashes, 1);
    m_pageHasher->addLayout(m_pageHasherSettings);
    m_pageHasher->addLayout(progress);
    m_pageHasher->addWidget(m_hasherOutput, 0, Qt::AlignTop);
    
    // Setup hasher settings layout
    layout1->addWidget(m_moveTo);
    layout1->addWidget(m_moveToDir, 1);
    layout1->addWidget(movetodirbutton);
    layout2->addWidget(m_renameTo);
    layout2->addWidget(m_renameToPattern, 1);
    layout2->addWidget(patternhelpbutton);
    
    m_pageHasherSettings->addWidget(m_button1, 0, 0);
    m_pageHasherSettings->addWidget(m_button2, 0, 1);
    m_pageHasherSettings->addWidget(m_button3, 0, 2);
    m_pageHasherSettings->addWidget(label1, 1, 0);
    m_pageHasherSettings->addWidget(m_hasherFileState, 1, 1, 1, 2);
    m_pageHasherSettings->addWidget(m_addToMyList, 2, 0, 1, 3);
    m_pageHasherSettings->addWidget(m_markWatched, 3, 0, 1, 3);
    m_pageHasherSettings->addLayout(layout1, 4, 0, 1, 3);
    m_pageHasherSettings->addLayout(layout2, 5, 0, 1, 3);
    m_pageHasherSettings->addWidget(m_buttonStart, 6, 0);
    m_pageHasherSettings->addWidget(m_buttonStop, 6, 1);
    m_pageHasherSettings->addWidget(m_buttonClear, 6, 2);
    m_pageHasherSettings->addLayout(progressTotalLayout, 7, 0, 1, 3);
    
    // Setup file state combo box
    m_hasherFileState->addItem("Unknown");
    m_hasherFileState->addItem("On HDD");
    m_hasherFileState->addItem("On CD");
    m_hasherFileState->addItem("Deleted");
    m_hasherFileState->setCurrentIndex(1); // Default to "On HDD"
    
    // Set default values
    m_moveTo->setChecked(false);
    m_renameTo->setChecked(false);
    m_markWatched->setChecked(true);
    m_markWatched->setCheckState(Qt::PartiallyChecked);
}

void HasherCoordinator::setupConnections()
{
    connect(m_button1, &QPushButton::clicked, this, &HasherCoordinator::addFiles);
    connect(m_button2, &QPushButton::clicked, this, &HasherCoordinator::addDirectories);
    connect(m_button3, &QPushButton::clicked, this, &HasherCoordinator::addLastDirectory);
    connect(m_buttonStart, &QPushButton::clicked, this, &HasherCoordinator::startHashing);
    connect(m_buttonStop, &QPushButton::clicked, this, &HasherCoordinator::stopHashing);
    connect(m_buttonClear, &QPushButton::clicked, this, &HasherCoordinator::clearHasher);
    connect(m_markWatched, &QCheckBox::stateChanged, this, &HasherCoordinator::onMarkWatchedStateChanged);
    
    // Connect to hasher thread pool
    connect(m_hasherThreadPool, &HasherThreadPool::requestNextFile, this, &HasherCoordinator::provideNextFileToHash);
    connect(m_hasherThreadPool, &HasherThreadPool::notifyPartsDone, this, &HasherCoordinator::onProgressUpdate);
    connect(m_hasherThreadPool, &HasherThreadPool::notifyFileHashed, this, &HasherCoordinator::onFileHashed);
    connect(m_hasherThreadPool, &HasherThreadPool::finished, this, &HasherCoordinator::onHashingFinished);
}

void HasherCoordinator::addFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(nullptr, QString(), m_adbapi->getLastDirectory());
    if(!files.isEmpty())
    {
        m_adbapi->setLastDirectory(QFileInfo(files.first()).filePath());
        while(!files.isEmpty())
        {
            QFileInfo file = QFileInfo(files.first());
            files.pop_front();
            
            if (!shouldFilterFile(file.absoluteFilePath())) {
                hashesInsertRow(file, m_renameTo->checkState());
            }
        }
    }
}

void HasherCoordinator::addDirectories()
{
    m_hashes->setUpdatesEnabled(false);
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

    dialog.setDirectory(m_adbapi->getLastDirectory());

    if(dialog.exec())
    {
        files = dialog.selectedFiles();
        if(files.count() == 1)
        {
            m_adbapi->setLastDirectory(files.last());
        }
        while(!files.isEmpty())
        {
            QString dirPath = files.first();
            files.pop_front();
            addFilesFromDirectory(dirPath);
        }
    }
    m_hashes->setUpdatesEnabled(true);
}

void HasherCoordinator::addLastDirectory()
{
    m_hashes->setUpdatesEnabled(false);
    QStringList files;

    files.append(m_adbapi->getLastDirectory());
    if(!files.last().isEmpty())
    {
        while(!files.isEmpty())
        {
            QString dirPath = files.first();
            files.pop_front();
            addFilesFromDirectory(dirPath);
        }
    }
    m_hashes->setUpdatesEnabled(true);
}

void HasherCoordinator::addFilesFromDirectory(const QString &dirPath)
{
    QDirIterator directory_walker(dirPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    
    while(directory_walker.hasNext())
    {
        QFileInfo file = QFileInfo(directory_walker.next());
        
        if (!shouldFilterFile(file.absoluteFilePath())) {
            hashesInsertRow(file, m_renameTo->checkState());
        }
    }
}

bool HasherCoordinator::shouldFilterFile(const QString &filePath)
{
    updateFilterCache();
    
    if (m_cachedFilterRegexes.isEmpty()) {
        return false;
    }
    
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    
    for (const QRegularExpression &regex : m_cachedFilterRegexes) {
        if (regex.match(fileName).hasMatch()) {
            LOG(QString("File '%1' matches filter pattern, skipping").arg(fileName));
            return true;
        }
    }
    
    return false;
}

void HasherCoordinator::updateFilterCache()
{
    QString filterMasks = m_adbapi->getHasherFilterMasks();
    
    if (m_cachedFilterMasks == filterMasks) {
        return;
    }
    
    m_cachedFilterMasks = filterMasks;
    m_cachedFilterRegexes.clear();
    
    if (filterMasks.isEmpty()) {
        return;
    }
    
    QStringList masks = filterMasks.split(',', Qt::SkipEmptyParts);
    for (const QString &mask : masks) {
        QString trimmedMask = mask.trimmed();
        if (!trimmedMask.isEmpty()) {
            QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(trimmedMask));
            if (regex.isValid()) {
                m_cachedFilterRegexes.append(regex);
            } else {
                LOG(QString("Warning: Invalid filter mask pattern '%1', skipping").arg(trimmedMask));
            }
        }
    }
}

void HasherCoordinator::hashesInsertRow(QFileInfo file, Qt::CheckState renameState, const QString& preloadedHash)
{
    int row = m_hashes->rowCount();
    m_hashes->insertRow(row);
    
    QTableWidgetItem *item;
    
    // Filename
    item = new QTableWidgetItem(file.fileName());
    m_hashes->setItem(row, 0, item);
    
    // Status
    item = new QTableWidgetItem(preloadedHash.isEmpty() ? "0" : "1");
    m_hashes->setItem(row, 1, item);
    
    // Path
    item = new QTableWidgetItem(file.absoluteFilePath());
    m_hashes->setItem(row, 2, item);
    
    // Size
    item = new QTableWidgetItem(QString::number(file.size()));
    m_hashes->setItem(row, 3, item);
    
    // Audio, File, MyList, Move, Rename columns
    for(int i = 4; i < 9; i++)
    {
        item = new QTableWidgetItem("?");
        m_hashes->setItem(row, i, item);
    }
    
    // Hash column
    item = new QTableWidgetItem(preloadedHash);
    if(!preloadedHash.isEmpty())
    {
        item->setBackground(QBrush(m_hashedFileColor));
    }
    m_hashes->setItem(row, 9, item);
}

// Continuation in next message due to length...

void HasherCoordinator::startHashing()
{
    // TODO: Implement - extracted from Window::ButtonHasherStartClick
    LOG("HasherCoordinator::startHashing() - stub");
}

void HasherCoordinator::stopHashing()
{
    // TODO: Implement - extracted from Window::ButtonHasherStopClick
    LOG("HasherCoordinator::stopHashing() - stub");
}

void HasherCoordinator::clearHasher()
{
    m_hashes->setRowCount(0);
}

void HasherCoordinator::onFileHashed(int threadId, ed2k::ed2kfilestruct fileData)
{
    // TODO: Implement - extracted from Window::getNotifyFileHashed  
    LOG(QString("HasherCoordinator::onFileHashed() thread=%1 - stub").arg(threadId));
}

void HasherCoordinator::onProgressUpdate(int threadId, int total, int done)
{
    // TODO: Implement - extracted from Window::getNotifyPartsDone
    if (threadId >= 0 && threadId < m_threadProgressBars.size()) {
        m_threadProgressBars[threadId]->setMaximum(total);
        m_threadProgressBars[threadId]->setValue(done);
    }
}

void HasherCoordinator::onHashingFinished()
{
    // TODO: Implement - extracted from Window::hasherFinished
    LOG("HasherCoordinator::onHashingFinished() - stub");
    emit hashingFinished();
}

void HasherCoordinator::provideNextFileToHash()
{
    // TODO: Implement - extracted from Window::provideNextFileToHash
    LOG("HasherCoordinator::provideNextFileToHash() - stub");
}

void HasherCoordinator::onMarkWatchedStateChanged(int state)
{
    // TODO: Implement - extracted from Window::markwatchedStateChanged
    LOG(QString("HasherCoordinator::onMarkWatchedStateChanged() state=%1 - stub").arg(state));
}

void HasherCoordinator::setupHashingProgress(const QStringList &files)
{
    // TODO: Implement - extracted from Window::setupHashingProgress
    LOG("HasherCoordinator::setupHashingProgress() - stub");
}

int HasherCoordinator::calculateTotalHashParts(const QStringList &files)
{
    // TODO: Implement - extracted from Window::calculateTotalHashParts
    int totalparts = 0;
    for(const QString &filepath : files)
    {
        QFileInfo file(filepath);
        double a = file.size();
        double b = a/102400;
        double c = ceil(b);
        totalparts += c;
    }
    return totalparts;
}

QStringList HasherCoordinator::getFilesNeedingHash()
{
    // TODO: Implement - extracted from Window::getFilesNeedingHash
    QStringList files;
    for(int i = 0; i < m_hashes->rowCount(); i++)
    {
        QString progress = m_hashes->item(i, 1)->text();
        if(progress == "0")
        {
            files.append(m_hashes->item(i, 2)->text());
        }
    }
    return files;
}

void HasherCoordinator::processPendingHashedFiles()
{
    // TODO: Implement - extracted from Window::processPendingHashedFiles
    LOG("HasherCoordinator::processPendingHashedFiles() - stub");
}
