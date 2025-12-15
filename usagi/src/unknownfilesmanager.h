#ifndef UNKNOWNFILESMANAGER_H
#define UNKNOWNFILESMANAGER_H

#include <QObject>
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCompleter>
#include <QMap>
#include <QStringList>
#include "localfileinfo.h"

// Forward declarations
class AniDBApi;
class HasherCoordinator;
class unknown_files_;

/**
 * @class UnknownFilesManager
 * @brief Manages unknown files (files not in AniDB database) display and operations
 * 
 * This class encapsulates all functionality related to unknown files:
 * - UI widget creation and management
 * - File insertion and display
 * - User actions (bind, mark as not anime, re-check, delete)
 * - Integration with AniDB API for file binding
 * 
 * SOLID Principles:
 * - Single Responsibility: Manages only unknown files functionality
 * - Open/Closed: Extensible through signals, closed for modification
 * - Dependency Inversion: Depends on abstractions (signals/slots)
 */
class UnknownFilesManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param api Pointer to AniDB API instance
     * @param hasherCoord Pointer to HasherCoordinator for hasher settings access
     * @param parent Parent QObject
     */
    explicit UnknownFilesManager(AniDBApi *api, HasherCoordinator *hasherCoord, QObject *parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~UnknownFilesManager() override;
    
    // UI Access Methods
    /**
     * @brief Get the unknown files container widget (includes label + table)
     * @return Pointer to the container widget
     */
    QWidget* getContainerWidget() const { return m_containerWidget; }
    
    /**
     * @brief Get the unknown files table widget (for direct access if needed)
     * @return Pointer to the table widget
     */
    unknown_files_* getTableWidget() const { return m_tableWidget; }
    
    /**
     * @brief Get unknown files data map (read-only access)
     * @return Const reference to the files data map
     */
    const QMap<int, LocalFileInfo>& getFilesData() const { return m_filesData; }
    
    // File Management Methods
    /**
     * @brief Insert a new unknown file row into the table
     * @param filename File name
     * @param filepath Full file path
     * @param hash File hash (ed2k)
     * @param size File size in bytes
     */
    void insertFile(const QString& filename, const QString& filepath, const QString& hash, qint64 size);
    
    /**
     * @brief Set anime titles cache for autocomplete
     * @param titles List of anime titles
     * @param titleToAid Map from title to anime ID
     */
    void setAnimeTitlesCache(const QStringList& titles, const QMap<QString, int>& titleToAid);
    
    /**
     * @brief Enable or disable updates for batch operations
     * @param enable True to enable updates, false to disable
     */
    void setUpdatesEnabled(bool enable);
    
    /**
     * @brief Remove file at given filepath from the unknown files list
     * @param filepath Full file path to remove
     * @param fromRow Starting row hint for search optimization (optional, -1 to search all)
     */
    void removeFileByPath(const QString& filepath, int fromRow = -1);

signals:
    /**
     * @brief Emitted when a log message should be displayed
     * @param message Log message text
     */
    void logMessage(const QString& message);
    
    /**
     * @brief Emitted when a file needs to be added to the hashes table
     * @param fileInfo QFileInfo for the file to add
     * @param renameState Rename checkbox state
     * @param preloadedHash Pre-computed hash (if available)
     */
    void fileNeedsHashing(const QFileInfo& fileInfo, Qt::CheckState renameState, const QString& preloadedHash);
    
    /**
     * @brief Emitted when the hasher should be started
     */
    void requestStartHasher();

private slots:
    /**
     * @brief Handle anime search text change
     * @param filepath File path identifier
     * @param searchText Current search text
     * @param episodeInput Episode input widget
     * @param bindButton Bind button widget
     */
    void onAnimeSearchChanged(const QString& filepath, const QString& searchText, 
                             QLineEdit* episodeInput, QPushButton* bindButton);
    
    /**
     * @brief Handle episode input text change
     * @param filepath File path identifier
     * @param episodeText Current episode text
     * @param bindButton Bind button widget
     */
    void onEpisodeInputChanged(const QString& filepath, const QString& episodeText, QPushButton* bindButton);
    
    /**
     * @brief Handle bind button click
     * @param row Table row index
     * @param epno Episode number/string
     */
    void onBindClicked(int row, const QString& epno);
    
    /**
     * @brief Handle "Not Anime" button click
     * @param row Table row index
     */
    void onNotAnimeClicked(int row);
    
    /**
     * @brief Handle "Re-check" button click
     * @param row Table row index
     */
    void onRecheckClicked(int row);
    
    /**
     * @brief Handle "Delete" button click
     * @param row Table row index
     */
    void onDeleteClicked(int row);

private:
    // Helper Methods
    /**
     * @brief Create and initialize the UI widgets
     */
    void createUI();
    
    /**
     * @brief Find row index by filepath (stable identifier)
     * @param filepath File path to find
     * @return Row index, or -1 if not found
     */
    int findRowByFilepath(const QString& filepath) const;
    
    /**
     * @brief Reindex the files data map after row removal
     * @param removedRow Row that was removed
     */
    void reindexFilesData(int removedRow);
    
    // Member Variables
    AniDBApi *m_api;                              // AniDB API instance
    HasherCoordinator *m_hasherCoordinator;       // Hasher coordinator for settings access
    
    // UI Widgets
    QWidget *m_containerWidget;                   // Container for label + table
    unknown_files_ *m_tableWidget;                // Main table widget
    QLabel *m_titleLabel;                         // Title label
    
    // Data Storage
    QMap<int, LocalFileInfo> m_filesData;         // Row index -> file data
    QStringList m_cachedAnimeTitles;              // Cached anime titles for autocomplete
    QMap<QString, int> m_cachedTitleToAid;        // Title -> AID mapping
};

#endif // UNKNOWNFILESMANAGER_H
