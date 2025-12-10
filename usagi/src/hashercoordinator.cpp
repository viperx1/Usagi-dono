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
#include <QSplitter>

// External hasher thread pool
extern HasherThreadPool *hasherThreadPool;

HasherCoordinator::HasherCoordinator(AniDBApi *adbapi, QWidget *parent)
    : QObject(parent)
    , m_adbapi(adbapi)
    , m_totalHashParts(0)
    , m_completedHashParts(0)
    , m_hasherThreadPool(hasherThreadPool)
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
    m_hasherFileState->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_addToMyList = new QCheckBox("Add file(s) to MyList");
    m_addToMyList->setChecked(true);  // Enable by default
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
    
    // Create progress bars for each thread
    int numThreads = m_hasherThreadPool->threadCount();
    for (int i = 0; i < numThreads; ++i) {
        QProgressBar *threadProgress = new QProgressBar;
        threadProgress->setFormat(QString("Thread %1: %p%").arg(i));
        m_threadProgressBars.append(threadProgress);
    }
    
    m_progressTotal = new QProgressBar;
    m_progressTotalLabel = new QLabel;
    
    // Setup hashes table
    m_hashes->setColumnCount(10);
    QStringList headers;
    headers << "Filename" << "Status" << "Path" << "LF" << "LL" << "RF" << "RL" << "Move" << "Rename" << "Hash";
    m_hashes->setHorizontalHeaderLabels(headers);
    m_hashes->hideColumn(2); // Hide path column
    m_hashes->hideColumn(9); // Hide hash column
    m_hashes->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_hashes->horizontalHeader()->setStretchLastSection(true);
    
    // Set tooltips for column headers (including hidden columns for completeness)
    if (m_hashes->horizontalHeaderItem(0))
        m_hashes->horizontalHeaderItem(0)->setToolTip("Name of the file");
    if (m_hashes->horizontalHeaderItem(1))
        m_hashes->horizontalHeaderItem(1)->setToolTip("Hashing progress (0=pending, 1=completed)");
    if (m_hashes->horizontalHeaderItem(2))
        m_hashes->horizontalHeaderItem(2)->setToolTip("Full path to the file (hidden)");
    if (m_hashes->horizontalHeaderItem(3))
        m_hashes->horizontalHeaderItem(3)->setToolTip("LF (Local File): Whether file info is in local database (0=no, 1=yes)");
    if (m_hashes->horizontalHeaderItem(4))
        m_hashes->horizontalHeaderItem(4)->setToolTip("LL (Local List/MyList): Whether file is in your MyList (0=no, 1=yes)");
    if (m_hashes->horizontalHeaderItem(5))
        m_hashes->horizontalHeaderItem(5)->setToolTip("RF (Remote File): AniDB FILE command API tag");
    if (m_hashes->horizontalHeaderItem(6))
        m_hashes->horizontalHeaderItem(6)->setToolTip("RL (Remote List): AniDB MYLIST command API tag");
    if (m_hashes->horizontalHeaderItem(7))
        m_hashes->horizontalHeaderItem(7)->setToolTip("Whether to move the file");
    if (m_hashes->horizontalHeaderItem(8))
        m_hashes->horizontalHeaderItem(8)->setToolTip("Whether to rename the file");
    if (m_hashes->horizontalHeaderItem(9))
        m_hashes->horizontalHeaderItem(9)->setToolTip("ED2K hash of the file (hidden)");
    
    // Set minimum heights and size policies
    m_hashes->setMinimumHeight(100);
    m_hashes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // Allow table to expand
    
    // Set hasher output to fixed size (~6 lines)
    QFontMetrics fm(m_hasherOutput->font());
    int lineHeight = fm.lineSpacing();
    m_hasherOutput->setFixedHeight(lineHeight * 6 + 10);  // 6 lines + some padding
    
    // NOTE: Actual layout assembly happens in window.cpp to allow proper splitter arrangement
    // with unknown files widget. This just creates the widgets.
    
    // Setup hasher settings layout
    layout1->addWidget(m_moveTo);
    layout1->addWidget(m_moveToDir, 1);
    layout1->addWidget(movetodirbutton);
    layout2->addWidget(m_renameTo);
    layout2->addWidget(m_renameToPattern, 1);
    layout2->addWidget(patternhelpbutton);
    
    // Create horizontal layout for add file buttons (prevents stretching)
    QBoxLayout *addButtonsLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    addButtonsLayout->addWidget(m_button1);
    addButtonsLayout->addWidget(m_button2);
    addButtonsLayout->addWidget(m_button3);
    addButtonsLayout->addStretch(1);
    
    // Create horizontal layout for control buttons (prevents stretching)
    QBoxLayout *controlButtonsLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    controlButtonsLayout->addWidget(m_buttonStart);
    controlButtonsLayout->addWidget(m_buttonStop);
    controlButtonsLayout->addWidget(m_buttonClear);
    controlButtonsLayout->addStretch(1);
    
    // Create horizontal layout for file state (prevents stretching and centers)
    QBoxLayout *fileStateLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    fileStateLayout->addWidget(label1);
    fileStateLayout->addWidget(m_hasherFileState);
    fileStateLayout->addStretch(1);
    
    m_pageHasherSettings->addLayout(addButtonsLayout, 0, 0, 1, 3);
    m_pageHasherSettings->addLayout(fileStateLayout, 1, 0, 1, 3);
    m_pageHasherSettings->addWidget(m_addToMyList, 2, 0, 1, 3);
    m_pageHasherSettings->addWidget(m_markWatched, 3, 0, 1, 3);
    m_pageHasherSettings->addLayout(layout1, 4, 0, 1, 3);
    m_pageHasherSettings->addLayout(layout2, 5, 0, 1, 3);
    m_pageHasherSettings->addLayout(controlButtonsLayout, 6, 0, 1, 3);
    
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
    connect(m_markWatched, &QCheckBox::checkStateChanged, this, &HasherCoordinator::onMarkWatchedStateChanged);
    
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

void HasherCoordinator::hashesInsertRow(QFileInfo file, Qt::CheckState /*renameState*/, const QString& preloadedHash)
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
    
    // LF, LL, RF, RL, Move, Rename columns - all start with "?"
    for(int i = 3; i < 9; i++)
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
    QList<int> rowsWithHashes; // Rows that already have hashes
    int filesToHashCount = 0;
    
    for(int i=0; i<m_hashes->rowCount(); i++)
    {
        QString progress = m_hashes->item(i, 1)->text();
        QString existingHash = m_hashes->item(i, 9)->text(); // Check hash column
        
        // Process files with progress="0" or progress="1" (already hashed but not yet API-processed)
        if(progress == "0" || progress == "1")
        {
            // Check if file has pending API calls (tags in columns 5 or 6)
            // Tags are "?" initially, set to actual tag when API call is queued, and "0" when completed/not needed
            QString fileTag = m_hashes->item(i, 5)->text();
            QString mylistTag = m_hashes->item(i, 6)->text();
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
        QString filename = m_hashes->item(rowIndex, 0)->text();
        QString filePath = m_hashes->item(rowIndex, 2)->text();
        QString hexdigest = m_hashes->item(rowIndex, 9)->text();
        
        LOG(QString("Queueing already-hashed file for processing: %1").arg(filename));
        
        // Get file size
        QFileInfo fileInfo(filePath);
        qint64 fileSize = fileInfo.size();
        
        // Queue for deferred processing using HashingTask class
        HashingTask task(filePath, filename, hexdigest, fileSize);
        task.setRowIndex(rowIndex);
        task.setUseUserSettings(true);
        task.setAddToMylist(m_addToMyList->checkState() > 0);
        task.setMarkWatchedState(m_markWatched->checkState());
        task.setFileState(m_hasherFileState->currentIndex());
        m_pendingHashedFilesQueue.append(task);
    }
    
    // Start timer to process queued files in batches (keeps UI responsive)
    if (!rowsWithHashes.isEmpty())
    {
        LOG(QString("Queued %1 already-hashed file(s) for deferred processing").arg(rowsWithHashes.size()));
        m_hashedFilesProcessingTimer->start();
    }
    
    // Start hashing for files without existing hashes
    if (filesToHashCount > 0)
    {
        // Calculate total hash parts for progress tracking
        QStringList filesToHash = getFilesNeedingHash();
        setupHashingProgress(filesToHash);
        
        m_buttonStart->setEnabled(false);
        m_buttonClear->setEnabled(false);
        m_hasherThreadPool->start();
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

void HasherCoordinator::stopHashing()
{
    m_buttonStart->setEnabled(true);
    m_buttonClear->setEnabled(true);
    m_progressTotal->setValue(0);
    m_progressTotal->setMaximum(1);
    m_progressTotal->setFormat("");
    m_progressTotalLabel->setText("");
    
    // Reset all thread progress bars
    for (QProgressBar* bar : m_threadProgressBars) {
        bar->setValue(0);
        bar->setMaximum(1);
    }
    
    // Reset progress of files that were assigned but not completed
    // Files with progress "0.1" were assigned to threads but stopped before completion
    // Reset them to "0" so they can be picked up again on next start
    for (int i = 0; i < m_hashes->rowCount(); i++)
    {
        QString progress = m_hashes->item(i, 1)->text();
        if (progress == "0.1")
        {
            m_hashes->item(i, 1)->setText("0");
        }
    }
    
    // Notify all worker threads to stop hashing
    // 1. First, notify ed2k instances in all worker threads to interrupt current hashing
    //    This sets a flag that ed2khash checks, causing it to return early
    m_hasherThreadPool->broadcastStopHasher();
    
    // 2. Then signal the thread pool to stop processing more files
    m_hasherThreadPool->stop();
    
    // 3. Don't wait here - let threads finish asynchronously to prevent UI freeze
    //    The onHashingFinished() slot will be called automatically when all threads complete
}

void HasherCoordinator::clearHasher()
{
    m_hashes->setRowCount(0);
}

void HasherCoordinator::onFileHashed(int /*threadId*/, ed2k::ed2kfilestruct fileData)
{
    for(int i=0; i<m_hashes->rowCount(); i++)
    {
        // Match by filename AND verify it's the file being hashed (progress starts with "0" - either "0" or "0.1" for assigned)
        // This prevents matching wrong file when there are multiple files with the same name
        QString progress = m_hashes->item(i, 1)->text();
        if(m_hashes->item(i, 0)->text() == fileData.filename && progress.startsWith("0"))
        {
            // Additional check: verify file size matches to ensure we have the right file
            QString filePath = m_hashes->item(i, 2)->text();
            QFileInfo fileInfo(filePath);
            if(!fileInfo.exists() || fileInfo.size() != fileData.size)
            {
                // Size mismatch or file doesn't exist - this is not the right file, continue searching
                continue;
            }
            
            // Mark as hashed in UI (use pre-allocated color object)
            m_hashes->item(i, 0)->setBackground(m_hashedFileColor);
            QTableWidgetItem *itemprogress = new QTableWidgetItem(QString("1"));
            m_hashes->setItem(i, 1, itemprogress);
            
            // Generate and output ed2k link (no encoding)
            QString ed2kLink = QString("ed2k://|file|%1|%2|%3|/")
                .arg(fileData.filename)
                .arg(fileData.size)
                .arg(fileData.hexdigest);
            m_hasherOutput->append(ed2kLink);
            
            emit logMessage(QString("File hashed: %1").arg(fileData.filename));
            
            // Update the hash column (column 9) in the UI to reflect the newly computed hash
            if (QTableWidgetItem* hashItem = m_hashes->item(i, 9))
            {
                hashItem->setText(fileData.hexdigest);
            }
            else
            {
                // Column 9 doesn't exist, create it
                QTableWidgetItem* newHashItem = new QTableWidgetItem(fileData.hexdigest);
                m_hashes->setItem(i, 9, newHashItem);
            }
            
            // Process file identification immediately instead of batching
            if (m_addToMyList->checkState() > 0)
            {
                // Update hash in database with status=1 immediately
                m_adbapi->updateLocalFileHash(filePath, fileData.hexdigest, 1);
                
                // Perform LocalIdentify immediately
                // Note: This is done per-file instead of batching to provide immediate feedback
                // LocalIdentify is a fast indexed database query, so the performance difference
                // is negligible compared to the user experience improvement of seeing results
                // immediately after each file is hashed rather than waiting for the entire batch
                std::bitset<2> li = m_adbapi->LocalIdentify(fileData.size, fileData.hexdigest);
                
                // Update UI with LocalIdentify results
                m_hashes->item(i, 3)->setText(QString((li[AniDBApi::LI_FILE_IN_DB])?"1":"0")); // File in database
                
                QString tag;
                if(li[AniDBApi::LI_FILE_IN_DB] == 0)
                {
                    tag = m_adbapi->File(fileData.size, fileData.hexdigest);
                    m_hashes->item(i, 5)->setText(tag);
                    // File info not in local DB yet - File() API call queued to fetch from AniDB
                }
                else
                {
                    m_hashes->item(i, 5)->setText("0");
                    // File is in local DB (previously fetched from AniDB)
                    // Update status to 2 (in anidb) to prevent re-detection
                    m_adbapi->UpdateLocalFileStatus(filePath, 2);
                }

                m_hashes->item(i, 4)->setText(QString((li[AniDBApi::LI_FILE_IN_MYLIST])?"1":"0")); // File in mylist
                if(li[AniDBApi::LI_FILE_IN_MYLIST] == 0)
                {
                    tag = m_adbapi->MylistAdd(fileData.size, fileData.hexdigest, m_markWatched->checkState(), m_hasherFileState->currentIndex(), m_storage->text());
                    m_hashes->item(i, 6)->setText(tag);
                    // Status will be updated when MylistAdd completes (via UpdateLocalPath)
                }
                else
                {
                    m_hashes->item(i, 6)->setText("0");
                    // File already in mylist - link the local_file and update status
                    m_adbapi->LinkLocalFileToMylist(fileData.size, fileData.hexdigest, filePath);
                }
            }
            else
            {
                LOG(QString("Skipping API processing for freshly hashed file: %1 (addToMylist=false)").arg(fileData.filename));
            }
            
            break; // Found the file, no need to continue
        }
    }
}

void HasherCoordinator::onProgressUpdate(int threadId, int total, int done)
{
    // Update individual thread progress bar
    if (threadId >= 0 && threadId < m_threadProgressBars.size()) {
        m_threadProgressBars[threadId]->setMaximum(total);
        m_threadProgressBars[threadId]->setValue(done);
    }
    
    // Update total progress
    // Calculate delta from last known progress for this thread
    int lastProgress = m_lastThreadProgress.value(threadId, 0);
    int delta = done - lastProgress;
    m_lastThreadProgress[threadId] = done;
    
    // Add delta to completed parts
    m_completedHashParts += delta;
    
    // Update total progress bar
    if (m_totalHashParts > 0) {
        m_progressTotal->setValue(m_completedHashParts);
        m_hashingProgress.updateProgress(delta);
    }
}

void HasherCoordinator::onHashingFinished()
{
    LOG("HasherCoordinator::onHashingFinished() - All hashing threads completed");
    
    m_buttonStart->setEnabled(true);
    m_buttonClear->setEnabled(true);
    
    // Reset progress bars
    m_progressTotal->setValue(0);
    m_progressTotal->setMaximum(1);
    m_progressTotal->setFormat("");
    m_progressTotalLabel->setText("");
    
    for (QProgressBar* bar : m_threadProgressBars) {
        bar->setValue(0);
        bar->setMaximum(1);
    }
    
    emit hashingFinished();
}

void HasherCoordinator::provideNextFileToHash()
{
    // Thread-safe file assignment: only one thread can request a file at a time
    QMutexLocker locker(&m_fileRequestMutex);
    
    // Look through the hashes widget for the next file that needs hashing (progress="0" and no hash)
    for(int i=0; i<m_hashes->rowCount(); i++)
    {
        QString progress = m_hashes->item(i, 1)->text();
        QString existingHash = m_hashes->item(i, 9)->text();
        
        if(progress == "0" && existingHash.isEmpty())
        {
            QString filePath = m_hashes->item(i, 2)->text();
            
            // Immediately mark this file as assigned to prevent other threads from picking it up
            QTableWidgetItem *itemProgressAssigned = new QTableWidgetItem(QString("0.1"));
            m_hashes->setItem(i, 1, itemProgressAssigned);
            
            m_hasherThreadPool->addFile(filePath);
            return;
        }
    }
    
    // No more files to hash, send empty string to signal completion
    m_hasherThreadPool->addFile(QString());
}

void HasherCoordinator::onMarkWatchedStateChanged(int state)
{
    switch(state)
    {
    case Qt::Unchecked:
        m_markWatched->setText("Mark watched (no change)");
        break;
    case Qt::PartiallyChecked:
        m_markWatched->setText("Mark watched (unwatched)");
        break;
    case Qt::Checked:
        m_markWatched->setText("Mark watched (watched)");
        break;
    }
}

void HasherCoordinator::setupHashingProgress(const QStringList &files)
{
    // Calculate total hash parts for all files
    m_totalHashParts = calculateTotalHashParts(files);
    m_completedHashParts = 0;
    m_lastThreadProgress.clear();
    
    // Reset progress bars
    m_progressTotal->setValue(0);
    m_progressTotal->setMaximum(m_totalHashParts);
    m_progressTotal->setFormat("%v/%m parts (%p%)");
    m_progressTotalLabel->setText(QString("Hashing %1 file(s)...").arg(files.size()));
    
    // Reset ProgressTracker
    m_hashingProgress.reset(m_totalHashParts);
    
    LOG(QString("Setup hashing progress: %1 files, %2 total parts").arg(files.size()).arg(m_totalHashParts));
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
    // Process files in small batches to keep UI responsive
    int processed = 0;
    
    while (!m_pendingHashedFilesQueue.isEmpty() && processed < HASHED_FILES_BATCH_SIZE) {
        HashingTask task = m_pendingHashedFilesQueue.takeFirst();
        processed++;
        
        // Mark as hashed in UI (use pre-allocated color object)
        m_hashes->item(task.rowIndex(), 0)->setBackground(m_hashedFileColor);
        m_hashes->item(task.rowIndex(), 1)->setText("1");
        
        // Update hash in database with status=1
        m_adbapi->updateLocalFileHash(task.filePath(), task.hash(), 1);
        
        // If adding to mylist, perform API calls (API methods handle auth internally)
        if (task.addToMylist()) {
            // Perform LocalIdentify
            std::bitset<2> li = m_adbapi->LocalIdentify(task.fileSize(), task.hash());
            
            // Update UI with LocalIdentify results
            m_hashes->item(task.rowIndex(), 3)->setText(QString((li[AniDBApi::LI_FILE_IN_DB])?"1":"0"));
            
            QString tag;
            if(li[AniDBApi::LI_FILE_IN_DB] == 0) {
                tag = m_adbapi->File(task.fileSize(), task.hash());
                m_hashes->item(task.rowIndex(), 5)->setText(tag);
                // File info not in local DB yet - File() API call queued to fetch from AniDB
                // Status will be updated to 2 when subsequent MylistAdd completes (via UpdateLocalPath)
            } else {
                m_hashes->item(task.rowIndex(), 5)->setText("0");
                // File is in local DB (previously fetched from AniDB)
                // Update status to 2 (in anidb) to prevent re-detection
                m_adbapi->UpdateLocalFileStatus(task.filePath(), 2);
            }

            m_hashes->item(task.rowIndex(), 4)->setText(QString((li[AniDBApi::LI_FILE_IN_MYLIST])?"1":"0"));
            if(li[AniDBApi::LI_FILE_IN_MYLIST] == 0) {
                // Use settings from the task
                int markWatched = task.useUserSettings() ? task.markWatchedState() : Qt::Unchecked;
                int fileState = task.useUserSettings() ? task.fileState() : 1;
                tag = m_adbapi->MylistAdd(task.fileSize(), task.hash(), markWatched, fileState, m_storage->text());
                m_hashes->item(task.rowIndex(), 6)->setText(tag);
                // Status will be updated when MylistAdd completes (via UpdateLocalPath)
            } else {
                m_hashes->item(task.rowIndex(), 6)->setText("0");
                // File already in mylist - no API call needed
                // Update status to 2 (in anidb) to prevent re-detection
                m_adbapi->UpdateLocalFileStatus(task.filePath(), 2);
            }
        } else {
            LOG(QString("Skipping API processing for already-hashed file: %1 (addToMylist=false)").arg(task.filename()));
        }
    }
    
    // Stop timer if queue is empty
    if (m_pendingHashedFilesQueue.isEmpty()) {
        m_hashedFilesProcessingTimer->stop();
        LOG("Finished processing all already-hashed files");
    }
}
