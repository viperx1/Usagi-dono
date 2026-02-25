#ifndef DIRECTORYWATCHERMANAGER_H
#define DIRECTORYWATCHERMANAGER_H

#include <QObject>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStringList>

class AniDBApi;
class DirectoryWatcher;
class WatchSessionManager;

/**
 * @class DirectoryWatcherManager
 * @brief Encapsulates directory watcher UI, persistence, and watcher lifecycle.
 *
 * Responsibilities:
 * - Build and expose the directory watcher settings UI group.
 * - Persist watcher settings through AniDBApi.
 * - Start/stop DirectoryWatcher based on UI state.
 * - Keep WatchSessionManager in sync with the watched path.
 * - Re-emit newFilesDetected to decouple Window from watcher implementation.
 */
class DirectoryWatcherManager : public QObject
{
    Q_OBJECT

public:
    explicit DirectoryWatcherManager(AniDBApi *api, QObject *parent = nullptr);
    ~DirectoryWatcherManager() override;

    QGroupBox *getSettingsGroup() const { return m_settingsGroup; }

    void loadSettingsFromApi();
    void saveSettingsToApi();
    void applyStartupBehavior();

    void setWatchSessionManager(WatchSessionManager *manager);

    QString watchedDirectory() const;

signals:
    void newFilesDetected(const QStringList &filePaths);
    void filesDeleted(const QStringList &filePaths);

public slots:
    void onWatcherEnabledChanged(Qt::CheckState state);
    void onWatcherBrowseClicked();

private:
    void createUi();
    void startWatching(const QString &dir);
    void syncWatchSessionPath(const QString &dir);
    void setStatusText(const QString &text);

    AniDBApi *m_api;
    DirectoryWatcher *m_directoryWatcher;
    WatchSessionManager *m_watchSessionManager;

    QGroupBox *m_settingsGroup;
    QCheckBox *m_watcherEnabled;
    QCheckBox *m_watcherAutoStart;
    QLabel *m_watcherStatusLabel;
    QLineEdit *m_watcherDirectory;
    QPushButton *m_watcherBrowseButton;

    QString m_pendingWatchPath;
};

#endif // DIRECTORYWATCHERMANAGER_H
