#include "trayiconmanager.h"
#include <QAction>
#include <QApplication>

TrayIconManager::TrayIconManager(const QIcon &icon, QObject *parent)
    : QObject(parent)
    , m_trayIcon(nullptr)
    , m_trayIconMenu(nullptr)
    , m_minimizeToTrayEnabled(false)
    , m_closeToTrayEnabled(false)
    , m_startMinimizedEnabled(false)
{
    // Create tray icon if system tray is available
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        createTrayIcon(icon);
    }
    // Note: No logging here since signals have no effect in constructor
    // The caller should check isSystemTrayAvailable() if needed
}

TrayIconManager::~TrayIconManager()
{
    // Qt parent-child relationship handles cleanup
}

void TrayIconManager::createTrayIcon(const QIcon &icon)
{
    // Create system tray icon menu
    m_trayIconMenu = new QMenu();
    
    QAction *showHideAction = new QAction("Show/Hide", this);
    connect(showHideAction, &QAction::triggered, this, &TrayIconManager::onShowHideAction);
    
    QAction *exitAction = new QAction("Exit", this);
    connect(exitAction, &QAction::triggered, this, &TrayIconManager::onExitAction);
    
    m_trayIconMenu->addAction(showHideAction);
    m_trayIconMenu->addSeparator();
    m_trayIconMenu->addAction(exitAction);
    
    // Create system tray icon
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayIconMenu);
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip("Usagi-dono");
    
    // Connect activation signal (for double-click)
    connect(m_trayIcon, &QSystemTrayIcon::activated, 
            this, &TrayIconManager::onTrayIconActivated);
    
    emit logMessage("System tray icon created");
}

void TrayIconManager::setMinimizeToTrayEnabled(bool enabled)
{
    m_minimizeToTrayEnabled = enabled;
    updateVisibility();
}

void TrayIconManager::setCloseToTrayEnabled(bool enabled)
{
    m_closeToTrayEnabled = enabled;
    updateVisibility();
}

void TrayIconManager::setStartMinimizedEnabled(bool enabled)
{
    m_startMinimizedEnabled = enabled;
    updateVisibility();
}

bool TrayIconManager::isSystemTrayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

bool TrayIconManager::isTrayIconVisible() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

void TrayIconManager::updateVisibility()
{
    // Only show tray icon if system tray is available and any tray feature is enabled
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    
    // Check if tray icon exists first
    if (!m_trayIcon) {
        return;
    }
    
    if (m_minimizeToTrayEnabled || m_closeToTrayEnabled || m_startMinimizedEnabled) {
        if (!m_trayIcon->isVisible()) {
            m_trayIcon->show();
            emit logMessage("System tray icon shown");
        }
    } else {
        if (m_trayIcon->isVisible()) {
            m_trayIcon->hide();
            emit logMessage("System tray icon hidden");
        }
    }
}

void TrayIconManager::showMessage(const QString &title, const QString &message,
                                  QSystemTrayIcon::MessageIcon icon,
                                  int millisecondsTimeoutHint)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(title, message, icon, millisecondsTimeoutHint);
    }
}

void TrayIconManager::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        onShowHideAction();
    }
}

void TrayIconManager::onShowHideAction()
{
    emit showHideRequested();
}

void TrayIconManager::onExitAction()
{
    emit exitRequested();
}
