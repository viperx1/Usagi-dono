#include "autofetchmanager.h"

#include <QVBoxLayout>
#include <QSignalBlocker>

#include "anidbapi.h"

AutoFetchManager::AutoFetchManager(AniDBApi *api, QObject *parent)
    : QObject(parent),
      m_api(api),
      m_settingsGroup(new QGroupBox("Auto-fetch")),
      m_autoFetchEnabled(new QCheckBox("Automatically download anime titles and other data on startup"))
{
    QVBoxLayout *layout = new QVBoxLayout(m_settingsGroup);
    layout->addWidget(m_autoFetchEnabled);
}

AutoFetchManager::~AutoFetchManager() = default;

void AutoFetchManager::loadSettingsFromApi()
{
    if (!m_api) {
        return;
    }

    const bool autoFetchEnabledSetting = m_api->getAutoFetchEnabled();
    const QSignalBlocker blockSignals(m_autoFetchEnabled);
    m_autoFetchEnabled->setChecked(autoFetchEnabledSetting);
}

void AutoFetchManager::saveSettingsToApi()
{
    if (!m_api) {
        return;
    }

    m_api->setAutoFetchEnabled(m_autoFetchEnabled->isChecked());
}
