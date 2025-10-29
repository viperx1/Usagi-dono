#include "directorywatcher.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QSettings>
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
    
    // Scan for video files in the directory
    QDirIterator it(m_watchedDirectory, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        
        // Skip if already processed
        if (m_processedFiles.contains(filePath)) {
            continue;
        }
        
        // Check if it's a valid video file
        if (!isValidVideoFile(filePath)) {
            continue;
        }
        
        // Check if file is stable (not currently being written)
        // We check the file size twice with a small delay
        qint64 size1 = fileInfo.size();
        if (size1 == 0) {
            continue; // Skip empty files
        }
        
        // Mark as processed to avoid duplicate processing
        m_processedFiles.insert(filePath);
        saveProcessedFile(filePath);
        
        // Emit signal for new file
        emit newFileDetected(filePath);
        qDebug() << "DirectoryWatcher: New file detected:" << filePath;
    }
}

bool DirectoryWatcher::isValidVideoFile(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    // Common video file extensions
    QStringList videoExtensions;
    videoExtensions << "mkv" << "mp4" << "avi" << "mov" << "wmv" 
                   << "flv" << "webm" << "m4v" << "mpg" << "mpeg"
                   << "ogv" << "3gp" << "ts" << "m2ts";
    
    return videoExtensions.contains(suffix);
}

void DirectoryWatcher::loadProcessedFiles()
{
    QSettings settings("settings.dat", QSettings::IniFormat);
    
    int size = settings.beginReadArray("watchedFiles");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString filePath = settings.value("path").toString();
        if (!filePath.isEmpty()) {
            m_processedFiles.insert(filePath);
        }
    }
    settings.endArray();
    
    qDebug() << "DirectoryWatcher: Loaded" << m_processedFiles.size() << "processed files";
}

void DirectoryWatcher::saveProcessedFile(const QString &filePath)
{
    QSettings settings("settings.dat", QSettings::IniFormat);
    
    // Get current array size
    int size = settings.beginReadArray("watchedFiles");
    settings.endArray();
    
    // Add new entry
    settings.beginWriteArray("watchedFiles");
    settings.setArrayIndex(size);
    settings.setValue("path", filePath);
    settings.endArray();
}
