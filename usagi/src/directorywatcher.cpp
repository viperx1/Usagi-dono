#include "directorywatcher.h"
#include "logger.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMutexLocker>
#include <QElapsedTimer>

// DirectoryScanWorker implementation
DirectoryScanWorker::DirectoryScanWorker(const QString &directory, const QSet<QString> &processedFiles, QObject *parent)
    : QObject(parent)
    , m_directory(directory)
    , m_processedFiles(processedFiles)
{
}

void DirectoryScanWorker::scan()
{
    QStringList newFiles;
    
    if (m_directory.isEmpty() || !QDir(m_directory).exists()) {
        emit scanComplete(newFiles);
        return;
    }
    
    // Scan for all files in the directory (no extension filtering - let API decide)
    QDirIterator it(m_directory, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    
    int totalFilesScanned = 0;
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        totalFilesScanned++;
        
        // Skip if already processed
        if (m_processedFiles.contains(filePath)) {
            continue;
        }
        
        // Skip empty files
        if (fileInfo.size() == 0) {
            continue;
        }
        
        newFiles.append(filePath);
    }
    
    emit scanComplete(newFiles);
}

// DirectoryWatcher implementation
DirectoryWatcher::DirectoryWatcher(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_debounceTimer(new QTimer(this))
    , m_initialScanTimer(new QTimer(this))
    , m_isWatching(false)
    , m_scanThread(nullptr)
    , m_scanInProgress(false)
{
    // Set up debounce timer to avoid processing files immediately after detection
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(2000); // Wait 2 seconds after file change
    
    // Set up initial scan timer to defer initial directory scan
    m_initialScanTimer->setSingleShot(true);
    m_initialScanTimer->setInterval(100); // Short delay to avoid UI freeze
    
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &DirectoryWatcher::onDirectoryChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &DirectoryWatcher::onFileChanged);
    connect(m_debounceTimer, &QTimer::timeout,
            this, &DirectoryWatcher::checkForNewFiles);
    connect(m_initialScanTimer, &QTimer::timeout,
            this, &DirectoryWatcher::checkForNewFiles);
}

DirectoryWatcher::~DirectoryWatcher()
{
    stopWatching();
    
    // Clean up scan thread if it exists
    if (m_scanThread) {
        if (m_scanThread->isRunning()) {
            m_scanThread->quit();
            m_scanThread->wait();
        }
        delete m_scanThread;
        m_scanThread = nullptr;
    }
}

void DirectoryWatcher::startWatching(const QString &directory)
{
    if (directory.isEmpty() || !QDir(directory).exists()) {
        LOG("DirectoryWatcher: Invalid directory: " + directory);
        return;
    }
    
    // Stop watching current directory if any
    stopWatching();
    
    m_watchedDirectory = directory;
    m_watcher->addPath(directory);
    m_isWatching = true;
    
    // Load previously processed files
    loadProcessedFiles();
    
    // Defer initial scan to avoid UI freeze
    m_initialScanTimer->start();
}

void DirectoryWatcher::stopWatching()
{
    if (!m_isWatching) {
        return;
    }
    
    QStringList dirs = m_watcher->directories();
    if (!dirs.isEmpty()) {
        m_watcher->removePaths(dirs);
    }
    
    QStringList files = m_watcher->files();
    if (!files.isEmpty()) {
        m_watcher->removePaths(files);
    }
    
    m_watchedDirectory.clear();
    m_isWatching = false;
    m_debounceTimer->stop();
    m_initialScanTimer->stop();
    
    LOG("DirectoryWatcher: Stopped watching");
}

bool DirectoryWatcher::isWatching() const
{
    return m_isWatching;
}

QString DirectoryWatcher::watchedDirectory() const
{
    return m_watchedDirectory;
}

void DirectoryWatcher::onDirectoryChanged(const QString &path)
{
    LOG(QString("DirectoryWatcher: Directory changed: %1").arg(path));
    
    // Restart debounce timer
    m_debounceTimer->start();
}

void DirectoryWatcher::onFileChanged(const QString &path)
{
    LOG(QString("DirectoryWatcher: File changed: %1").arg(path));
    
    // Restart debounce timer when files change
    m_debounceTimer->start();
}

void DirectoryWatcher::checkForNewFiles()
{
    if (!m_isWatching) {
        return;
    }
    
    scanDirectory();
}

void DirectoryWatcher::scanDirectory()
{
    if (m_watchedDirectory.isEmpty() || !QDir(m_watchedDirectory).exists()) {
        return;
    }
    
    // Check if a scan is already in progress
    if (m_scanInProgress) {
        LOG("DirectoryWatcher: Scan already in progress, skipping");
        return;
    }
    
    m_scanInProgress = true;
    
    // Copy data under mutex protection
    QSet<QString> processedFilesCopy;
    {
        QMutexLocker locker(&m_mutex);
        processedFilesCopy = m_processedFiles;
    }
    
    // Clean up old thread if it exists
    if (m_scanThread) {
        if (m_scanThread->isRunning()) {
            m_scanThread->quit();
            m_scanThread->wait();
        }
        delete m_scanThread;
    }
    
    // Create new thread and worker
    m_scanThread = new QThread(this);
    DirectoryScanWorker *worker = new DirectoryScanWorker(m_watchedDirectory, processedFilesCopy);
    worker->moveToThread(m_scanThread);
    
    // Connect signals
    connect(m_scanThread, &QThread::started, worker, &DirectoryScanWorker::scan);
    connect(worker, &DirectoryScanWorker::scanComplete, this, [this](const QStringList &newFiles) {
        m_scanInProgress = false;
        onScanComplete(newFiles);
    });
    connect(worker, &DirectoryScanWorker::scanComplete, m_scanThread, &QThread::quit);
    connect(m_scanThread, &QThread::finished, worker, &QObject::deleteLater);
    
    // Start the thread
    m_scanThread->start();
}

void DirectoryWatcher::onScanComplete(const QStringList &newFiles)
{
    if (newFiles.isEmpty()) {
        LOG("DirectoryWatcher: No new files detected");
        return;
    }
    
    LOG(QString("DirectoryWatcher: Detected %1 new file(s)").arg(newFiles.size()));
    
    // Mark files as processed
    LOG("DirectoryWatcher: Starting to add files to m_processedFiles set");
    {
        QMutexLocker locker(&m_mutex);
        for (const QString &filePath : newFiles) {
            m_processedFiles.insert(filePath);
        }
    }
    LOG("DirectoryWatcher: Finished adding files to m_processedFiles set");
    
    // Add files to watcher to monitor for changes
    QStringList currentlyWatchedFiles = m_watcher->files();
    for (const QString &filePath : newFiles) {
        if (!currentlyWatchedFiles.contains(filePath)) {
            m_watcher->addPath(filePath);
        }
    }
    
    // Save to database if available
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isValid() && db.isOpen()) {
        LOG(QString("DirectoryWatcher: Starting to save %1 files to database").arg(newFiles.size()));
        
        // Use transaction for batch insert - much faster than individual commits
        bool useTransaction = db.transaction();
        if (!useTransaction) {
            LOG("DirectoryWatcher: Failed to start transaction, will use individual commits: " + db.lastError().text());
        }
        
        for (const QString &filePath : newFiles) {
            saveProcessedFile(filePath);
        }
        
        if (useTransaction) {
            if (!db.commit()) {
                LOG("DirectoryWatcher: Failed to commit transaction: " + db.lastError().text());
            }
        }
        
        LOG("DirectoryWatcher: Finished saving files to database");
    }
    
    // Emit batch signal with all new files
    LOG(QString("DirectoryWatcher: Emitting newFilesDetected signal with %1 files").arg(newFiles.size()));
    emit newFilesDetected(newFiles);
}

void DirectoryWatcher::loadProcessedFiles()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid() || !db.isOpen()) {
        LOG("DirectoryWatcher: Database not available, cannot load processed files");
        return;
    }
    
    QSqlQuery query(db);
    
    // Query only files that have been checked by API (status >= 2)
    // Status: 0=not hashed, 1=hashed but not checked by API, 2=in anidb, 3=not in anidb
    // Files with status=1 need to be detected so they can be checked against API
    bool querySuccess = query.exec("SELECT path FROM local_files WHERE status >= 2");
    
    if (!querySuccess) {
        LOG(QString("DirectoryWatcher: Failed to query local_files table: %1")
                    .arg(query.lastError().text()));
        return;
    }
    
    while (query.next()) {
        QString filePath = query.value(0).toString();
        if (!filePath.isEmpty()) {
            m_processedFiles.insert(filePath);
        }
    }
}

void DirectoryWatcher::saveProcessedFile(const QString &filePath)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid() || !db.isOpen()) {
        LOG("DirectoryWatcher: Database not available, cannot save processed file");
        return;
    }
    
    QSqlQuery query(db);
    
    QFileInfo fileInfo(filePath);
    QString filename = fileInfo.fileName();
    qint64 fileSize = fileInfo.size();
    
    // Insert into local_files table with status=0 (not checked)
    // Include file size for duplicate detection
    // Use parameterized query to prevent SQL injection
    query.prepare("INSERT OR IGNORE INTO local_files (path, filename, file_size, status) VALUES (?, ?, ?, 0)");
    query.addBindValue(filePath);
    query.addBindValue(filename);
    query.addBindValue(fileSize);
    
    if (!query.exec()) {
        LOG("DirectoryWatcher: Failed to save processed file: " + query.lastError().text());
    }
}
