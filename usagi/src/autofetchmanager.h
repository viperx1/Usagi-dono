#ifndef AUTOFETCHMANAGER_H
#define AUTOFETCHMANAGER_H

#include <QObject>
#include <QGroupBox>
#include <QCheckBox>

class AniDBApi;

/**
 * @class AutoFetchManager
 * @brief Encapsulates auto-fetch settings UI and persistence.
 *
 * Responsibilities:
 * - Build the auto-fetch settings group.
 * - Load and save the auto-fetch flag through AniDBApi.
 */
class AutoFetchManager : public QObject
{
    Q_OBJECT

public:
    explicit AutoFetchManager(AniDBApi *api, QObject *parent = nullptr);
    ~AutoFetchManager() override;

    QGroupBox *getSettingsGroup() const { return m_settingsGroup; }

    void loadSettingsFromApi();
    void saveSettingsToApi();

private:
    AniDBApi *m_api;
    QGroupBox *m_settingsGroup;
    QCheckBox *m_autoFetchEnabled;
};

#endif // AUTOFETCHMANAGER_H
