#ifndef WATCHSESSIONMANAGER_H
#define WATCHSESSIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QList>
#include <QPair>
#include <tuple>

/**
 * @brief Represents how a file is marked for queue management
 */
enum class FileMarkType {
    None = 0,           // No special marking
    ForDownload = 1,    // Marked for download (priority)
    ForDeletion = 2     // Marked for soft deletion (can be removed when space needed)
};

/**
 * @brief Deletion threshold type for automatic file cleanup
 */
enum class DeletionThresholdType {
    FixedGB = 0,        // Fixed amount in GB
    Percentage = 1      // Percentage of total drive space
};

/**
 * @brief Information about a file's marking status
 */
struct FileMarkInfo {
    int lid;                    // MyList ID
    int aid;                    // Anime ID
    FileMarkType markType;      // Current marking
    int markScore;              // Calculated score for deletion priority
    bool hasLocalFile;          // Whether file exists locally
    bool isWatched;             // Whether file has been watched (local_watched)
    bool isInActiveSession;     // Whether this file's anime has an active session
    
    FileMarkInfo() : lid(0), aid(0), markType(FileMarkType::None), 
                     markScore(0), hasLocalFile(false), isWatched(false), 
                     isInActiveSession(false) {}
};

/**
 * @brief Manages watch sessions and file marking for download/deletion
 * 
 * This class provides:
 * - Watch session tracking per anime (starting from first episode of original prequel)
 * - Ahead buffer management for continuous experience
 * - File marking for download/deletion based on calculated scores
 * - Settings for deletion thresholds
 * 
 * Key concepts:
 * - Session: A viewing session for an anime series that starts from the first episode
 *   of the original prequel and tracks progress
 * - Ahead buffer: Number of episodes to keep downloaded ahead of current position
 * - Mark score: Calculated value determining deletion priority (higher = more likely to delete)
 */
class WatchSessionManager : public QObject
{
    Q_OBJECT
    
public:
    explicit WatchSessionManager(QObject *parent = nullptr);
    virtual ~WatchSessionManager();
    
    // ========== Session Management ==========
    
    /**
     * @brief Start or resume a session for an anime
     * @param aid Anime ID
     * @param startEpisode Optional episode number to start from (0 = first unwatched)
     * @return true if session was started successfully
     */
    bool startSession(int aid, int startEpisode = 0);
    
    /**
     * @brief Start session from a specific file/episode (user-initiated)
     * @param lid MyList ID of the file to start from
     * @return true if session was started successfully
     */
    bool startSessionFromFile(int lid);
    
    /**
     * @brief End/pause the current session for an anime
     * @param aid Anime ID
     */
    void endSession(int aid);
    
    /**
     * @brief Check if an anime has an active session
     * @param aid Anime ID
     * @return true if session is active
     */
    bool hasActiveSession(int aid) const;
    
    /**
     * @brief Get the current episode position in the session
     * @param aid Anime ID
     * @return Current episode number (0 if no session)
     */
    int getCurrentSessionEpisode(int aid) const;
    
    /**
     * @brief Mark an episode as watched in the session
     * @param aid Anime ID
     * @param episodeNumber Episode number that was watched
     */
    void markEpisodeWatched(int aid, int episodeNumber);
    
    /**
     * @brief Get the original prequel anime ID for a series
     * @param aid Anime ID
     * @return Anime ID of the first anime in the series chain
     */
    int getOriginalPrequel(int aid) const;
    
    /**
     * @brief Get all related anime IDs (sequel/prequel chain)
     * @param aid Anime ID
     * @return List of anime IDs in order from prequel to sequel
     */
    QList<int> getSeriesChain(int aid) const;
    
    // ========== File Marking ==========
    
    /**
     * @brief Calculate the mark score for a file
     * 
     * Score factors:
     * - Is card hidden (-50 = less priority)
     * - Is session active (+100 for files in ahead buffer)
     * - Is already watched in session (-30 = can be deleted)
     * - Not yet watched (+50 = keep)
     * - Distance from current position (closer = higher priority)
     * 
     * @param lid MyList ID
     * @return Calculated score (higher = keep longer, lower = delete first)
     */
    int calculateMarkScore(int lid) const;
    
    /**
     * @brief Get the marking type for a file
     * @param lid MyList ID
     * @return Current mark type
     */
    FileMarkType getFileMarkType(int lid) const;
    
    /**
     * @brief Set the marking type for a file
     * @param lid MyList ID
     * @param markType The new mark type
     */
    void setFileMarkType(int lid, FileMarkType markType);
    
    /**
     * @brief Get full marking info for a file
     * @param lid MyList ID
     * @return FileMarkInfo structure with all details
     */
    FileMarkInfo getFileMarkInfo(int lid) const;
    
    /**
     * @brief Get all files marked for deletion, sorted by score
     * @return List of LIDs sorted by deletion priority (lowest score first)
     */
    QList<int> getFilesForDeletion() const;
    
    /**
     * @brief Actually delete a file that was marked for deletion
     * 
     * This performs the complete file deletion process:
     * - Deletes the physical file from disk
     * - Removes entry from local_files table
     * - Marks as deleted in mylist table (state=3)
     * - Sends update to AniDB API
     * 
     * @param lid MyList ID of the file to delete
     * @param deleteFromDisk If true, delete the physical file from disk
     * @return true if deletion was initiated successfully
     */
    bool deleteFile(int lid, bool deleteFromDisk = true);
    
    /**
     * @brief Delete all files currently marked for deletion
     * 
     * Processes all files in the deletion queue, deleting them one by one.
     * Files are deleted in order of their mark score (lowest first).
     * 
     * @param deleteFromDisk If true, delete the physical files from disk
     */
    void deleteMarkedFiles(bool deleteFromDisk = true);
    
    /**
     * @brief Get all files marked for download
     * @return List of LIDs marked for download
     */
    QList<int> getFilesForDownload() const;
    
    /**
      * @brief Automatically mark files for deletion based on available space
     * 
     * Uses the configured threshold settings to determine which files
     * should be soft-marked for deletion.
     */
    void autoMarkFilesForDeletion();
    
    /**
     * @brief Automatically mark files for download based on session progress
     * 
     * Marks files that should be downloaded based on:
     * - Active sessions
     * - Ahead buffer setting
     * - Current playback position
     */
    void autoMarkFilesForDownload();
    
    /**
     * @brief Auto-start sessions for existing anime with local files
     * 
     * This finds all anime IDs that have local files and creates sessions
     * for them if they don't already have one. This ensures WatchSessionManager
     * works for existing anime collections that were added before WatchSessionManager.
     */
    void autoStartSessionsForExistingAnime();
    
    /**
     * @brief Perform initial scan to mark files based on current state
     * 
     * This should be called after mylist data is loaded to mark files
     * that need download or can be deleted based on current settings.
     */
    void performInitialScan();
    
    /**
     * @brief Handle a new anime being added to mylist
     * 
     * Auto-starts a session for brand new anime when they are first added.
     * @param aid Anime ID
     */
    void onNewAnimeAdded(int aid);
    
    // ========== Settings ==========
    
    /**
     * @brief Get the ahead buffer size (episodes to keep ready)
     * @return Number of episodes
     */
    int getAheadBuffer() const;
    
    /**
     * @brief Set the ahead buffer size
     * @param episodes Number of episodes to keep ahead
     */
    void setAheadBuffer(int episodes);
    
    /**
     * @brief Get the deletion threshold type
     * @return Threshold type (FixedGB or Percentage)
     */
    DeletionThresholdType getDeletionThresholdType() const;
    
    /**
     * @brief Set the deletion threshold type
     * @param type New threshold type
     */
    void setDeletionThresholdType(DeletionThresholdType type);
    
    /**
     * @brief Get the deletion threshold value
     * @return Value in GB or percentage depending on type
     */
    double getDeletionThresholdValue() const;
    
    /**
     * @brief Set the deletion threshold value
     * @param value Value in GB or percentage
     */
    void setDeletionThresholdValue(double value);
    
    /**
     * @brief Check if auto-marking for deletion is enabled
     * @return true if enabled
     */
    bool isAutoMarkDeletionEnabled() const;
    
    /**
     * @brief Enable or disable auto-marking for deletion
     * @param enabled New state
     */
    void setAutoMarkDeletionEnabled(bool enabled);
    
    /**
     * @brief Get the path used for space monitoring
     * @return The monitored directory path
     */
    QString getWatchedPath() const;
    
    /**
     * @brief Set the path used for space monitoring
     * @param path Directory path to monitor for space calculations
     * 
     * This should typically be set to match the directory watcher's path
     * so that space monitoring applies to the correct drive.
     */
    void setWatchedPath(const QString& path);
    
    /**
     * @brief Check if actual file deletion is enabled
     * @return true if files will be actually deleted (not just marked)
     */
    bool isActualDeletionEnabled() const;
    
    /**
     * @brief Enable or disable actual file deletion
     * @param enabled New state
     */
    void setActualDeletionEnabled(bool enabled);
    
    /**
     * @brief Check if force delete permissions is enabled
     * @return true if read-only attribute will be removed before deletion
     */
    bool isForceDeletePermissionsEnabled() const;
    
    /**
     * @brief Enable or disable force delete permissions
     * @param enabled New state
     */
    void setForceDeletePermissionsEnabled(bool enabled);
    
    // ========== Persistence ==========
    
    /**
     * @brief Load session data from database
     */
    void loadFromDatabase();
    
    /**
     * @brief Save session data to database
     */
    void saveToDatabase();
    
public slots:
    /**
     * @brief Handle the result of a file deletion operation
     * 
     * Called by Window after adbapi->deleteFileFromMylist() completes.
     * Only emits fileDeleted signal if the deletion was successful.
     * 
     * @param lid MyList ID of the file
     * @param aid Anime ID the file belonged to
     * @param success True if the file was successfully deleted
     */
    void onFileDeletionResult(int lid, int aid, bool success);
    
signals:
    /**
     * @brief Emitted when a session state changes
     * @param aid Anime ID
     * @param isActive Whether session is now active
     */
    void sessionStateChanged(int aid, bool isActive);
    
    /**
     * @brief Emitted when file marking changes
     * @param lid MyList ID
     * @param markType New mark type
     */
    void fileMarkChanged(int lid, FileMarkType markType);
    
    /**
     * @brief Emitted when files should be refreshed due to marking changes
     * @param updatedLids Set of MyList IDs that were updated (empty means refresh all)
     */
    void markingsUpdated(const QSet<int>& updatedLids);
    
    /**
     * @brief Emitted when a file has been deleted
     * @param lid MyList ID of the deleted file
     * @param aid Anime ID the file belonged to
     */
    void fileDeleted(int lid, int aid);
    
    /**
     * @brief Emitted to request file deletion (handled by Window which has API access)
     * @param lid MyList ID of the file to delete
     * @param deleteFromDisk If true, delete the physical file from disk
     */
    void deleteFileRequested(int lid, bool deleteFromDisk);
    
private:
    // Session data
    struct SessionInfo {
        int aid;                // Anime ID
        int startAid;           // First anime in series (prequel)
        int currentEpisode;     // Current episode in session
        bool isActive;          // Whether session is active
        QSet<int> watchedEpisodes; // Episodes watched in this session
        
        SessionInfo() : aid(0), startAid(0), currentEpisode(0), isActive(false) {}
    };
    
    // Cache for anime relations
    mutable QMap<int, QList<QPair<int, QString>>> m_relationsCache; // aid -> [(related_aid, relation_type), ...]
    
    // Cache for prequel lookups (optimization to avoid redundant chain traversal)
    mutable QMap<int, int> m_prequelCache; // aid -> original prequel aid
    
    // Cache for series chains (optimization to avoid redundant chain building)
    mutable QMap<int, QList<int>> m_seriesChainCache; // aid -> chain of aids
    
    // Active sessions by anime ID
    QMap<int, SessionInfo> m_sessions;
    
    // File markings (lid -> mark info)
    mutable QMap<int, FileMarkInfo> m_fileMarks;
    
    // Settings
    int m_aheadBuffer;                          // Episodes to keep ahead
    DeletionThresholdType m_thresholdType;      // Threshold type
    double m_thresholdValue;                    // Threshold value
    bool m_autoMarkDeletionEnabled;             // Auto-mark enabled
    bool m_enableActualDeletion;                // Actually delete files (not just mark)
    bool m_forceDeletePermissions;              // Try to remove read-only before deletion
    QString m_watchedPath;                      // Path for space monitoring (uses directory watcher path)
    bool m_initialScanComplete;                 // True after performInitialScan() is called
    
    // Helper methods
    void loadAnimeRelations(int aid) const;
    int findPrequelAid(int aid, const QString& relationType) const;
    int getEpisodeNumber(int lid) const;
    int getAnimeIdForFile(int lid) const;
    bool isCardHidden(int aid) const;
    int getFileVersion(int lid) const;  // Get file version from state bits
    int getFileCountForEpisode(int lid) const;  // Get number of local files for same episode
    int getHigherVersionFileCount(int lid) const;  // Count local files with higher version for same episode
    void loadSettings();
    void saveSettings();
    void ensureTablesExist();
    
    // Helper method to find active session info across series chain
    // Returns (sessionAid, episodeOffsetForRequestedAnime, sessionEpisodeOffset)
    std::tuple<int, int, int> findActiveSessionInSeriesChain(int aid) const;
    int getTotalEpisodesForAnime(int aid) const;
    
    // AniDB relation type codes
    static const int RELATION_SEQUEL = 1;
    static const int RELATION_PREQUEL = 2;
    
    // Score calculation constants
    static const int SCORE_HIDDEN_CARD = -50;
    static const int SCORE_ACTIVE_SESSION = 100;  // Factor for being in active watching session
    static const int SCORE_IN_AHEAD_BUFFER = 75;
    static const int SCORE_ALREADY_WATCHED = -30;
    static const int SCORE_NOT_WATCHED = 50;
    static const int SCORE_DISTANCE_FACTOR = -1;  // Per episode away from current
    static const int SCORE_OLDER_REVISION = -1000;  // Per local file with higher version (older revisions more deletable)
    
    // Default settings
    static constexpr int DEFAULT_AHEAD_BUFFER = 3;
    static constexpr double DEFAULT_THRESHOLD_VALUE = 50.0; // 50 GB or 50%
    static constexpr int DEFAULT_EPISODE_COUNT = 12;  // Default episode count when unknown (typical anime cour)
};

#endif // WATCHSESSIONMANAGER_H
