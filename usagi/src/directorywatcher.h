#ifndef DIRECTORYWATCHER_H
#define DIRECTORYWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QString>
#include <QSet>
#include <QTimer>

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
    // Emitted when a new file is detected and ready to be hashed
    void newFileDetected(const QString &filePath);
    
private slots:
    void onDirectoryChanged(const QString &path);
    void checkForNewFiles();
    
private:
    QFileSystemWatcher *m_watcher;
    QString m_watchedDirectory;
    QSet<QString> m_processedFiles;
    QTimer *m_debounceTimer;
    bool m_isWatching;
    
    void scanDirectory();
    void loadProcessedFiles();
    void saveProcessedFile(const QString &filePath);
};

#endif // DIRECTORYWATCHER_H
