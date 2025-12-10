#ifndef TRAYSETTINGSMANAGER_H
#define TRAYSETTINGSMANAGER_H

#include <QObject>
#include <QGroupBox>
#include <QCheckBox>
#include <QVBoxLayout>

class AniDBApi;          // Provides persistence for user settings
class TrayIconManager;   // Controls system tray icon visibility/behavior

/**
 * @class TraySettingsManager
 * @brief Encapsulates tray settings UI and persistence.
 */
class TraySettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit TraySettingsManager(QObject *parent = nullptr);
    ~TraySettingsManager() override;

    QGroupBox *getSettingsGroup() const { return m_settingsGroup; }

    void loadSettingsFromApi(AniDBApi *api, TrayIconManager *trayManager);
    void saveSettingsToApi(AniDBApi *api, TrayIconManager *trayManager);
    void applyAvailability(TrayIconManager *trayManager);

    bool isStartMinimizedEnabled() const;

private:
    QGroupBox *m_settingsGroup;
    QCheckBox *m_trayMinimizeToTray;
    QCheckBox *m_trayCloseToTray;
    QCheckBox *m_trayStartMinimized;
    QVBoxLayout *m_layout;
};

#endif // TRAYSETTINGSMANAGER_H
