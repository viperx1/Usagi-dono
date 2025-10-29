#include "directorywatcher.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QMutexLocker>

DirectoryWatcher::DirectoryWatcher(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_debounceTimer(new QTimer(this))
    , m_initialScanTimer(new QTimer(this))
    , m_isWatching(false)
{
    // Set up debounce timer to avoid processing files immediately after detection
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(2000); // Wait 2 seconds after file change
    
    // Set up initial scan timer to defer initial directory scan
    m_initialScanTimer->setSingleShot(true);
    m_initialScanTimer->setInterval(100); // Short delay to avoid UI freeze
    
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &DirectoryWatcher::onDirectoryChanged);
    connect(m_debounceTimer, &QTimer::timeout,
            this, &DirectoryWatcher::checkForNewFiles);
    connect(m_initialScanTimer, &QTimer::timeout,
            this, &DirectoryWatcher::checkForNewFiles);
}

DirectoryWatcher::~DirectoryWatcher()
{
    stopWatching();
}

void DirectoryWatcher::startWatching(const QString &directory)
{
    if (directory.isEmpty() || !QDir(directory).exists()) {
        qDebug() << "DirectoryWatcher: Invalid directory:" << directory;
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
    
    qDebug() << "DirectoryWatcher: Started watching" << directory;
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
    
    m_watchedDirectory.clear();
    m_isWatching = false;
    m_debounceTimer->stop();
    m_initialScanTimer->stop();
    
    qDebug() << "DirectoryWatcher: Stopped watching";
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
    Q_UNUSED(path);
    
    // Restart debounce timer
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
    if (m_scanFuture.isRunning()) {
        qDebug() << "DirectoryWatcher: Scan already in progress, skipping";
        return;
    }
    
    qDebug() << "DirectoryWatcher: Starting background directory scan";
    
    // Run scan in background thread
    m_scanFuture = QtConcurrent::run([this]() {
        return this->scanDirectoryInBackground();
    });
    
    // Set up a watcher for the future to handle completion
    QFutureWatcher<QStringList> *watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher]() {
        // Check if future is valid and not cancelled before getting result
        if (watcher->future().isValid() && !watcher->future().isCanceled()) {
            QStringList newFiles = watcher->result();
            onScanComplete(newFiles);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(m_scanFuture);
}

QStringList DirectoryWatcher::scanDirectoryInBackground()
{
    QStringList newFiles;
    QString directory;
    QSet<QString> processedFilesCopy;
    
    // Copy data under mutex protection
    {
        QMutexLocker locker(&m_mutex);
        directory = m_watchedDirectory;
        processedFilesCopy = m_processedFiles;
    }
    
    if (directory.isEmpty() || !QDir(directory).exists()) {
        return newFiles;
    }
    
    // Scan for all files in the directory (no extension filtering - let API decide)
    QDirIterator it(directory, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        
        // Skip if already processed
        if (processedFilesCopy.contains(filePath)) {
            continue;
        }
        
        // Skip empty files
        if (fileInfo.size() == 0) {
            continue;
        }
        
        newFiles.append(filePath);
    }
    
    return newFiles;
}

void DirectoryWatcher::onScanComplete(const QStringList &newFiles)
{
    if (newFiles.isEmpty()) {
        qDebug() << "DirectoryWatcher: No new files detected";
        return;
    }
    
    qDebug() << "DirectoryWatcher: Detected" << newFiles.size() << "new file(s)";
    
    // Mark files as processed
    {
        QMutexLocker locker(&m_mutex);
        for (const QString &filePath : newFiles) {
            m_processedFiles.insert(filePath);
        }
    }
    
    // Save to database if available
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isValid() && db.isOpen()) {
        for (const QString &filePath : newFiles) {
            saveProcessedFile(filePath);
        }
    }
    
    // Emit batch signal with all new files
    emit newFilesDetected(newFiles);
}

void DirectoryWatcher::loadProcessedFiles()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid() || !db.isOpen()) {
        qDebug() << "DirectoryWatcher: Database not available, cannot load processed files";
        return;
    }
    
    QSqlQuery query(db);
    
    // Query only files that have been hashed (status != 0)
    // Status 0 = not checked, Status 1 = in anidb, Status 2 = not in anidb
    if (!query.exec("SELECT path FROM local_files WHERE status != 0")) {
        qDebug() << "DirectoryWatcher: Failed to query local_files table:" << query.lastError().text();
        return;
    }
    
    while (query.next()) {
        QString filePath = query.value(0).toString();
        if (!filePath.isEmpty()) {
            m_processedFiles.insert(filePath);
        }
    }
    
    qDebug() << "DirectoryWatcher: Loaded" << m_processedFiles.size() << "hashed files from database";
}

void DirectoryWatcher::saveProcessedFile(const QString &filePath)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid() || !db.isOpen()) {
        qDebug() << "DirectoryWatcher: Database not available, cannot save processed file";
        return;
    }
    
    QSqlQuery query(db);
    
    QFileInfo fileInfo(filePath);
    QString filename = fileInfo.fileName();
    
    // Insert into local_files table with status=0 (not checked)
    // Use parameterized query to prevent SQL injection
    query.prepare("INSERT OR IGNORE INTO local_files (path, filename, status) VALUES (?, ?, 0)");
    query.addBindValue(filePath);
    query.addBindValue(filename);
    
    if (!query.exec()) {
        qDebug() << "DirectoryWatcher: Failed to save processed file:" << query.lastError().text();
    }
}
