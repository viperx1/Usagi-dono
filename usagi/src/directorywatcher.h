#ifndef DIRECTORYWATCHER_H
#define DIRECTORYWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QTimer>
#include <QFuture>
#include <QMutex>

class DirectoryWatcher : public QObject
{
    Q_OBJECT
    
public:
    explicit DirectoryWatcher(QObject *parent = nullptr);
    ~DirectoryWatcher();
    
    // Start/stop watching
    void startWatching(const QString &directory);
    void stopWatching();
    bool isWatching() const;
    
    // Get current watched directory
    QString watchedDirectory() const;
    
signals:
    // Emitted when new files are detected and ready to be hashed
    void newFilesDetected(const QStringList &filePaths);
    
private slots:
    void onDirectoryChanged(const QString &path);
    void checkForNewFiles();
    void onScanComplete(const QStringList &newFiles);
    
private:
    QFileSystemWatcher *m_watcher;
    QString m_watchedDirectory;
    QSet<QString> m_processedFiles;
    QTimer *m_debounceTimer;
    QTimer *m_initialScanTimer;
    bool m_isWatching;
    QFuture<QStringList> m_scanFuture;
    QMutex m_mutex;
    
    void scanDirectory();
    QStringList scanDirectoryInBackground();
    void loadProcessedFiles();
    void saveProcessedFile(const QString &filePath);
};

#endif // DIRECTORYWATCHER_H
