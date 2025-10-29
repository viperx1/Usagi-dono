#include "directorywatcher.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>

DirectoryWatcher::DirectoryWatcher(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_debounceTimer(new QTimer(this))
    , m_isWatching(false)
{
    // Set up debounce timer to avoid processing files immediately after detection
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(2000); // Wait 2 seconds after file change
    
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &DirectoryWatcher::onDirectoryChanged);
    connect(m_debounceTimer, &QTimer::timeout,
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
    
    // Do initial scan
    scanDirectory();
    
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
    
    // Scan for all files in the directory (no extension filtering - let API decide)
    QDirIterator it(m_watchedDirectory, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        
        // Skip if already processed
        if (m_processedFiles.contains(filePath)) {
            continue;
        }
        
        // Skip empty files
        if (fileInfo.size() == 0) {
            continue;
        }
        
        // Mark as processed to avoid duplicate processing
        m_processedFiles.insert(filePath);
        saveProcessedFile(filePath);
        
        // Emit signal for new file
        emit newFileDetected(filePath);
        qDebug() << "DirectoryWatcher: New file detected:" << filePath;
    }
}

void DirectoryWatcher::loadProcessedFiles()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    
    // Query all watched files from database
    query.exec("SELECT value FROM settings WHERE name LIKE 'watched_file_%'");
    
    while (query.next()) {
        QString filePath = query.value(0).toString();
        if (!filePath.isEmpty()) {
            m_processedFiles.insert(filePath);
        }
    }
    
    qDebug() << "DirectoryWatcher: Loaded" << m_processedFiles.size() << "processed files from database";
}

void DirectoryWatcher::saveProcessedFile(const QString &filePath)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);
    
    // Create a unique key for this file using hash of the path
    QString key = QString("watched_file_%1").arg(QString::number(qHash(filePath)));
    
    // Insert or replace the file path in the database
    QString sql = QString("INSERT OR REPLACE INTO settings (name, value) VALUES ('%1', '%2')")
        .arg(key)
        .arg(QString(filePath).replace("'", "''"));  // Escape single quotes
    
    query.exec(sql);
}
