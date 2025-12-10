#include "traysettingsmanager.h"

#include <QVBoxLayout>

#include "anidbapi.h"
#include "trayiconmanager.h"

namespace {
constexpr const char *kTooltipMinimize = "Minimize the application to system tray instead of taskbar";
constexpr const char *kTooltipClose = "Hide to system tray when closing the window instead of exiting";
constexpr const char *kTooltipStartMinimized = "Start the application minimized to system tray";
constexpr const char *kTooltipUnavailable = "System tray not available on this platform";
}

TraySettingsManager::TraySettingsManager(QObject *parent)
    : QObject(parent)
{
    QWidget *parentWidget = qobject_cast<QWidget*>(parent); // Expected to be a QWidget (Window); falls back to nullptr if not.
    m_settingsGroup = new QGroupBox("System Tray", parentWidget);
    m_trayMinimizeToTray = new QCheckBox("Minimize to tray", m_settingsGroup);
    m_trayCloseToTray = new QCheckBox("Close to tray", m_settingsGroup);
    m_trayStartMinimized = new QCheckBox("Start minimized to tray", m_settingsGroup);
    m_layout = new QVBoxLayout(m_settingsGroup);
    m_trayMinimizeToTray->setToolTip(kTooltipMinimize);
    m_trayCloseToTray->setToolTip(kTooltipClose);
    m_trayStartMinimized->setToolTip(kTooltipStartMinimized);

    m_layout->addWidget(m_trayMinimizeToTray);
    m_layout->addWidget(m_trayCloseToTray);
    m_layout->addWidget(m_trayStartMinimized);

}

TraySettingsManager::~TraySettingsManager() = default;

void TraySettingsManager::loadSettingsFromApi(AniDBApi *api, TrayIconManager *trayManager)
{
    if (!api || !trayManager) {
        return;
    }

    const bool minimizeToTray = api->getTrayMinimizeToTray();
    const bool closeToTray = api->getTrayCloseToTray();
    const bool startMinimized = api->getTrayStartMinimized();

    m_trayMinimizeToTray->setChecked(minimizeToTray);
    m_trayCloseToTray->setChecked(closeToTray);
    m_trayStartMinimized->setChecked(startMinimized);

    trayManager->setMinimizeToTrayEnabled(minimizeToTray);
    trayManager->setCloseToTrayEnabled(closeToTray);
    trayManager->setStartMinimizedEnabled(startMinimized);
}

void TraySettingsManager::saveSettingsToApi(AniDBApi *api, TrayIconManager *trayManager)
{
    if (!api || !trayManager) {
        return;
    }

    const bool minimizeToTray = m_trayMinimizeToTray->isChecked();
    const bool closeToTray = m_trayCloseToTray->isChecked();
    const bool startMinimized = m_trayStartMinimized->isChecked();

    api->setTrayMinimizeToTray(minimizeToTray);
    api->setTrayCloseToTray(closeToTray);
    api->setTrayStartMinimized(startMinimized);

    trayManager->setMinimizeToTrayEnabled(minimizeToTray);
    trayManager->setCloseToTrayEnabled(closeToTray);
    trayManager->setStartMinimizedEnabled(startMinimized);
}

void TraySettingsManager::applyAvailability(TrayIconManager *trayManager)
{
    if (!trayManager) {
        return;
    }

    if (!trayManager->isSystemTrayAvailable()) {
        m_trayMinimizeToTray->setEnabled(false);
        m_trayMinimizeToTray->setToolTip(kTooltipUnavailable);
        m_trayCloseToTray->setEnabled(false);
        m_trayCloseToTray->setToolTip(kTooltipUnavailable);
        m_trayStartMinimized->setEnabled(false);
        m_trayStartMinimized->setToolTip(kTooltipUnavailable);
    } else {
        auto ensureEnabled = [](QCheckBox *box, const char *tooltip) {
            if (!box->isEnabled() || box->toolTip() == kTooltipUnavailable) {
                box->setEnabled(true);
                box->setToolTip(tooltip);
            }
        };

        ensureEnabled(m_trayMinimizeToTray, kTooltipMinimize);
        ensureEnabled(m_trayCloseToTray, kTooltipClose);
        ensureEnabled(m_trayStartMinimized, kTooltipStartMinimized);
    }
}

bool TraySettingsManager::isStartMinimizedEnabled() const
{
    return m_trayStartMinimized->isChecked();
}
