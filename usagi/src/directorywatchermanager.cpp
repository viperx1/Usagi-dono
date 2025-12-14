#include "directorywatchermanager.h"

#include <QDir>
#include <QFileDialog>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "anidbapi.h"
#include "directorywatcher.h"
#include "logger.h"
#include "watchsessionmanager.h"

DirectoryWatcherManager::DirectoryWatcherManager(AniDBApi *api, QObject *parent)
    : QObject(parent),
      m_api(api),
      m_directoryWatcher(new DirectoryWatcher(this)),
      m_watchSessionManager(nullptr),
      m_settingsGroup(nullptr),
      m_watcherEnabled(nullptr),
      m_watcherAutoStart(nullptr),
      m_watcherStatusLabel(nullptr),
      m_watcherDirectory(nullptr),
      m_watcherBrowseButton(nullptr),
      m_pendingWatchPath()
{
    createUi();

    connect(m_watcherEnabled, &QCheckBox::checkStateChanged,
            this, &DirectoryWatcherManager::onWatcherEnabledChanged);
    connect(m_watcherBrowseButton, &QPushButton::clicked,
            this, &DirectoryWatcherManager::onWatcherBrowseClicked);
    connect(m_directoryWatcher, &DirectoryWatcher::newFilesDetected,
            this, &DirectoryWatcherManager::newFilesDetected);
}

DirectoryWatcherManager::~DirectoryWatcherManager() = default;

void DirectoryWatcherManager::createUi()
{
    m_settingsGroup = new QGroupBox("Directory Watcher");
    QVBoxLayout *watcherLayout = new QVBoxLayout(m_settingsGroup);

    m_watcherEnabled = new QCheckBox("Enable Directory Watcher");
    m_watcherAutoStart = new QCheckBox("Auto-start on application launch");
    m_watcherStatusLabel = new QLabel("Status: Not watching");

    QHBoxLayout *watcherDirLayout = new QHBoxLayout();
    m_watcherDirectory = new QLineEdit;
    m_watcherBrowseButton = new QPushButton("Browse...");

    watcherDirLayout->addWidget(new QLabel("Watch Directory:"));
    watcherDirLayout->addWidget(m_watcherDirectory, 1);
    watcherDirLayout->addWidget(m_watcherBrowseButton);

    watcherLayout->addWidget(m_watcherEnabled);
    watcherLayout->addLayout(watcherDirLayout);
    watcherLayout->addWidget(m_watcherAutoStart);
    watcherLayout->addWidget(m_watcherStatusLabel);
}

void DirectoryWatcherManager::loadSettingsFromApi()
{
    const bool watcherEnabledSetting = m_api ? m_api->getWatcherEnabled() : false;
    const QString watcherDir = m_api ? m_api->getWatcherDirectory() : QString();
    const bool watcherAutoStartSetting = m_api ? m_api->getWatcherAutoStart() : false;

    const QSignalBlocker blockEnabled(m_watcherEnabled);
    const QSignalBlocker blockDirectory(m_watcherDirectory);
    const QSignalBlocker blockAutoStart(m_watcherAutoStart);

    m_watcherEnabled->setChecked(watcherEnabledSetting);
    m_watcherDirectory->setText(watcherDir);
    m_watcherAutoStart->setChecked(watcherAutoStartSetting);

    // Keep WatchSessionManager in sync with current path even before auto-start.
    if (watcherEnabledSetting && !watcherDir.isEmpty()) {
        syncWatchSessionPath(watcherDir);
    }

    if (watcherEnabledSetting && watcherDir.isEmpty()) {
        setStatusText("Status: Enabled (no directory set)");
    } else {
        setStatusText("Status: Not watching");
    }
}

void DirectoryWatcherManager::saveSettingsToApi()
{
    if (!m_api) {
        return;
    }

    m_api->setWatcherEnabled(m_watcherEnabled->isChecked());
    m_api->setWatcherDirectory(m_watcherDirectory->text());
    m_api->setWatcherAutoStart(m_watcherAutoStart->isChecked());
}

void DirectoryWatcherManager::applyStartupBehavior()
{
    const bool watcherEnabledSetting = m_watcherEnabled->isChecked();
    const QString watcherDir = m_watcherDirectory->text();
    const bool watcherAutoStartSetting = m_watcherAutoStart->isChecked();

    if (watcherEnabledSetting && watcherAutoStartSetting && !watcherDir.isEmpty()) {
        startWatching(watcherDir);
    } else if (watcherEnabledSetting && !watcherDir.isEmpty()) {
        setStatusText("Status: Enabled (not auto-started)");
        syncWatchSessionPath(watcherDir);
    } else if (watcherEnabledSetting && watcherDir.isEmpty()) {
        setStatusText("Status: Enabled (no directory set)");
    } else {
        setStatusText("Status: Not watching");
    }
}

void DirectoryWatcherManager::setWatchSessionManager(WatchSessionManager *manager)
{
    m_watchSessionManager = manager;

    if (!m_pendingWatchPath.isEmpty()) {
        syncWatchSessionPath(m_pendingWatchPath);
        m_pendingWatchPath.clear();
    }
}

QString DirectoryWatcherManager::watchedDirectory() const
{
    return m_watcherDirectory->text();
}

void DirectoryWatcherManager::onWatcherEnabledChanged(Qt::CheckState state)
{
    if (state == Qt::CheckState::Checked) {
        const QString dir = m_watcherDirectory->text();
        if (!dir.isEmpty() && QDir(dir).exists()) {
            startWatching(dir);
        } else if (dir.isEmpty()) {
            setStatusText("Status: Enabled (no directory set)");
            LOG("Directory watcher enabled but no directory specified");
        } else {
            setStatusText("Status: Enabled (invalid directory)");
            LOG("Directory watcher enabled but directory is invalid: " + dir);
        }
    } else {
        m_directoryWatcher->stopWatching();
        setStatusText("Status: Not watching");
        LOG("Directory watcher stopped");
    }
}

void DirectoryWatcherManager::onWatcherBrowseClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        m_settingsGroup,
        "Select Directory to Watch",
        m_watcherDirectory->text().isEmpty() ? QDir::homePath() : m_watcherDirectory->text());

    if (dir.isEmpty()) {
        return;
    }

    m_watcherDirectory->setText(dir);

    if (m_watcherEnabled->isChecked()) {
        startWatching(dir);
    }

    syncWatchSessionPath(dir);
}

void DirectoryWatcherManager::startWatching(const QString &dir)
{
    m_directoryWatcher->startWatching(dir);
    setStatusText("Status: Watching " + dir);
    syncWatchSessionPath(dir);
}

void DirectoryWatcherManager::syncWatchSessionPath(const QString &dir)
{
    if (!m_watchSessionManager) {
        m_pendingWatchPath = dir;
        return;
    }

    m_pendingWatchPath.clear();
    m_watchSessionManager->setWatchedPath(dir);
}

void DirectoryWatcherManager::setStatusText(const QString &text)
{
    m_watcherStatusLabel->setText(text);
}
