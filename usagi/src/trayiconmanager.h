#ifndef TRAYICONMANAGER_H
#define TRAYICONMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>
#include <QString>
#include <QRect>

/**
 * @class TrayIconManager
 * @brief Manages system tray icon functionality and settings
 * 
 * This class encapsulates all system tray related operations following SOLID principles:
 * - Single Responsibility: Only handles system tray functionality
 * - Open/Closed: Can be extended without modifying Window class
 * - Interface Segregation: Clean, focused interface
 * - Dependency Inversion: Communicates via signals
 * 
 * Responsibilities:
 * - System tray icon creation and lifecycle
 * - Tray menu management
 * - Tray settings (minimize to tray, close to tray, start minimized)
 * - Visibility management based on settings
 * - Event handling (activate, show/hide, exit)
 */
class TrayIconManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param icon The icon to use for the tray icon
     * @param parent Parent QObject
     */
    explicit TrayIconManager(const QIcon &icon, QObject *parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~TrayIconManager();
    
    // Settings accessors
    bool isMinimizeToTrayEnabled() const { return m_minimizeToTrayEnabled; }
    bool isCloseToTrayEnabled() const { return m_closeToTrayEnabled; }
    bool isStartMinimizedEnabled() const { return m_startMinimizedEnabled; }
    
    /**
     * @brief Set minimize to tray setting
     * @param enabled Whether to minimize to tray
     */
    void setMinimizeToTrayEnabled(bool enabled);
    
    /**
     * @brief Set close to tray setting
     * @param enabled Whether to close to tray
     */
    void setCloseToTrayEnabled(bool enabled);
    
    /**
     * @brief Set start minimized setting
     * @param enabled Whether to start minimized to tray
     */
    void setStartMinimizedEnabled(bool enabled);
    
    /**
     * @brief Check if system tray is available
     * @return True if system tray is available
     */
    bool isSystemTrayAvailable() const;
    
    /**
     * @brief Check if tray icon is visible
     * @return True if tray icon is visible
     */
    bool isTrayIconVisible() const;
    
    /**
     * @brief Update tray icon visibility based on settings
     * Shows tray icon only if any tray feature is enabled
     */
    void updateVisibility();
    
    /**
     * @brief Show balloon notification from tray icon
     * @param title Notification title
     * @param message Notification message
     * @param icon Icon to display
     * @param millisecondsTimeoutHint How long to show notification
     */
    void showMessage(const QString &title, const QString &message, 
                    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                    int millisecondsTimeoutHint = 3000);
    
    /**
     * @brief Get the tray icon object (for additional configuration if needed)
     * @return Pointer to QSystemTrayIcon
     */
    QSystemTrayIcon* getTrayIcon() const { return m_trayIcon; }

signals:
    /**
     * @brief Emitted when user wants to show/hide the main window from tray
     */
    void showHideRequested();
    
    /**
     * @brief Emitted when user wants to exit from tray menu
     */
    void exitRequested();
    
    /**
     * @brief Emitted for logging messages
     * @param message Log message
     */
    void logMessage(const QString &message);

private slots:
    /**
     * @brief Handle tray icon activation (double-click, etc.)
     * @param reason Activation reason
     */
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    
    /**
     * @brief Handle show/hide action from tray menu
     */
    void onShowHideAction();
    
    /**
     * @brief Handle exit action from tray menu
     */
    void onExitAction();

private:
    /**
     * @brief Create tray icon and menu
     * @param icon The icon to use
     */
    void createTrayIcon(const QIcon &icon);

    // Tray components
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayIconMenu;
    
    // Settings
    bool m_minimizeToTrayEnabled;
    bool m_closeToTrayEnabled;
    bool m_startMinimizedEnabled;
};

#endif // TRAYICONMANAGER_H
