#ifndef WATCHSESSIONMANAGER_H
#define WATCHSESSIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QList>
#include <QPair>
#include <tuple>
#include "sessioninfo.h"

/**
 * @brief Deletion threshold type for automatic file cleanup
 */
enum class DeletionThresholdType {
    FixedGB = 0,        // Fixed amount in GB
    Percentage = 1      // Percentage of total drive space
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
    
    // Allow test class to access private methods
    friend class TestBitratePreferences;
    
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
    
    /**
     * @brief Preload relation data for multiple anime into cache
     * @param aids List of anime IDs to preload relation data for
     * 
     * This is used for optimization when series chain view is enabled,
     * to avoid lazy loading during filtering/sorting operations.
     */
    void preloadRelationData(const QList<int>& aids);
    
    // ========== File Marking ==========
    
    /**
     * @brief Calculate the deletion score for a file
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
    int calculateDeletionScore(int lid) const;
    
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
     * @brief Delete the next eligible file based on space threshold
     * 
     * Calculates which file should be deleted on-demand based on:
     * - Current available disk space vs threshold
     * - Deletion scores (lower score = delete first)
     * - Gap protection (won't delete files that create gaps)
     * 
     * Deletes only one file at a time. Caller should wait for
     * API confirmation before calling this again.
     * 
     * @param deleteFromDisk If true, delete the physical file from disk
     * @return true if a file was deleted, false if no eligible files found
     */
    bool deleteNextEligibleFile(bool deleteFromDisk = true);
    
    /**
     * @brief Check if deletion is needed based on available space
     * 
     * Compares current available space with configured threshold.
     * 
     * @return true if space is below threshold and deletion is needed
     */
    bool isDeletionNeeded() const;

    /**
     * @brief Trigger file deletion when space is low (simplified from marking system)
     * 
     * Checks if space is below threshold and triggers deleteNextEligibleFile() if needed.
     * This replaces the complex marking logic with simple on-demand deletion triggering.
     */
    void autoMarkFilesForDeletion();
    
    /**
     * @brief Placeholder for download marking (removed)
     * 
     * This method is now a no-op. Download management should be handled elsewhere.
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
    // Static regex for episode number extraction (shared across functions)
    static const QRegularExpression s_epnoNumericRegex;
    
    // Cache for anime relations
    mutable QMap<int, QList<QPair<int, QString>>> m_relationsCache; // aid -> [(related_aid, relation_type), ...]
    
    // Cache for prequel lookups (optimization to avoid redundant chain traversal)
    mutable QMap<int, int> m_prequelCache; // aid -> original prequel aid
    
    // Cache for series chains (optimization to avoid redundant chain building)
    mutable QMap<int, QList<int>> m_seriesChainCache; // aid -> chain of aids
    
    // Active sessions by anime ID
    QMap<int, SessionInfo> m_sessions;
    
    // Track failed deletions to avoid retrying immediately
    QSet<int> m_failedDeletions;
    
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
    int getFileVersion(int lid) const;  // Get file version from state bits (bits 2-5 as flags: v2=4, v3=8, v4=16, v5=32, none=v1)
    int getFileCountForEpisode(int lid) const;  // Get number of local files for same episode
    int getHigherVersionFileCount(int lid) const;  // Count local files with higher version for same episode
    void loadSettings();
    void saveSettings();
    void ensureTablesExist();
    
    // Helper methods for new marking criteria
    bool matchesPreferredAudioLanguage(int lid) const;
    bool matchesPreferredSubtitleLanguage(int lid) const;
    int getQualityScore(const QString& quality) const;  // Convert AniDB quality string to numeric score
    QString getFileQuality(int lid) const;
    QString getFileAudioLanguage(int lid) const;
    QString getFileSubtitleLanguage(int lid) const;
    int getFileRating(int lid) const;  // Get anime rating for this file
    int getFileGroupId(int lid) const;  // Get group ID for this file
    int getGroupStatus(int gid) const;  // Get group status (0=unknown, 1=ongoing, 2=stalled, 3=disbanded)
    int getFileBitrate(int lid) const;  // Get video bitrate for this file (in Kbps)
    QString getFileResolution(int lid) const;  // Get resolution for this file
    QString getFileCodec(int lid) const;  // Get video codec for this file (e.g., "H.264", "H.265", "AV1")
    double getCodecEfficiency(const QString& codec) const;  // Get codec compression efficiency (H.264=1.0, H.265=0.5, AV1=0.35)
    double calculateExpectedBitrate(const QString& resolution, const QString& codec) const;  // Calculate expected bitrate for resolution and codec
    double calculateBitrateScore(double actualBitrate, const QString& resolution, const QString& codec, int fileCount) const;  // Calculate bitrate distance penalty
    
    // Gap detection and series continuity helpers
    bool wouldCreateGap(int lid, const QSet<int>& deletedEpisodes) const;  // Check if deleting this file would create an episode gap
    bool isLastFileForEpisode(int lid) const;  // Check if this is the only remaining file for its episode
    int getEpisodeIdForFile(int lid) const;  // Get episode ID (aid+epno combination) for gap tracking
    
    // Helper method to find active session info across series chain
    // Returns (sessionAid, episodeOffsetForRequestedAnime, sessionEpisodeOffset)
    std::tuple<int, int, int> findActiveSessionInSeriesChain(int aid) const;
    int getTotalEpisodesForAnime(int aid) const;
    
    /**
     * @brief Clean up database status for a file that no longer exists on disk
     * 
     * When a file is discovered to be missing from disk, this method triggers
     * the cleanup process by emitting deleteFileRequested with deleteFromDisk=false.
     * This ensures both local database and AniDB API are updated to reflect the file's absence.
     * 
     * @param lid MyList ID of the missing file
     */
    void cleanupMissingFileStatus(int lid);
    
    // Episode ID multiplier for unique identification (aid * multiplier + epno)
    static constexpr int EPISODE_ID_MULTIPLIER = 100000;
    
    // AniDB relation type codes
    static const int RELATION_SEQUEL = 1;
    static const int RELATION_PREQUEL = 2;
    
    // Score calculation constants
    static const int SCORE_HIDDEN_CARD = -50;
    static const int SCORE_ACTIVE_SESSION = 100;  // Factor for being in active watching session
    static const int SCORE_IN_AHEAD_BUFFER = 75;
    static const int SCORE_ALREADY_WATCHED = -5;
    static const int SCORE_NOT_WATCHED = 50;
    static const int SCORE_DISTANCE_FACTOR = -1;  // Per episode away from current
    static const int SCORE_OLDER_REVISION = -1000;  // Per local file with higher version (older revisions more deletable)
    static const int SCORE_PREFERRED_AUDIO = 30;  // Bonus for matching preferred audio language
    static const int SCORE_PREFERRED_SUBTITLE = 20;  // Bonus for matching preferred subtitle language
    static const int SCORE_NOT_PREFERRED_AUDIO = -40;  // Penalty for not matching preferred audio language
    static const int SCORE_NOT_PREFERRED_SUBTITLE = -20;  // Penalty for not matching preferred subtitle language
    static const int SCORE_HIGHER_QUALITY = 25;  // Bonus for higher quality/resolution
    static const int SCORE_LOWER_QUALITY = -35;  // Penalty for lower quality/resolution
    static const int SCORE_HIGH_RATING = 15;  // Bonus for highly rated anime (rating >= 800)
    static const int SCORE_LOW_RATING = -15;  // Penalty for poorly rated anime (rating < 600)
    
    // Quality thresholds for scoring (based on AniDB quality field)
    static constexpr int QUALITY_HIGH_THRESHOLD = 60;  // Quality score above this is considered high (e.g., "high", "very high")
    static constexpr int QUALITY_LOW_THRESHOLD = 40;   // Quality score below this is considered low (e.g., "low", "very low")
    
    // Rating thresholds (on 0-1000 scale)
    static constexpr int RATING_HIGH_THRESHOLD = 800;  // 8.0/10 - excellent anime
    static constexpr int RATING_LOW_THRESHOLD = 600;   // 6.0/10 - below average anime
    
    // Group status scores
    static const int SCORE_ACTIVE_GROUP = 20;      // Bonus for files from active groups
    static const int SCORE_STALLED_GROUP = -10;    // Penalty for files from stalled groups
    static const int SCORE_DISBANDED_GROUP = -25;  // Penalty for files from disbanded groups
    
    // Bitrate distance penalties (only apply when fileCount > 1)
    static const int SCORE_BITRATE_CLOSE = -10;       // 10-30% from expected bitrate
    static const int SCORE_BITRATE_MODERATE = -25;    // 30-50% from expected bitrate
    static const int SCORE_BITRATE_FAR = -40;         // 50%+ from expected bitrate
    
    // Codec quality tier bonuses
    static const int SCORE_MODERN_CODEC = 10;         // Bonus for modern efficient codecs (H.265, AV1)
    static const int SCORE_OLD_CODEC = -15;           // Penalty for old inefficient codecs (MPEG-4, XviD)
    static const int SCORE_ANCIENT_CODEC = -30;       // Penalty for very old codecs (MPEG-2, H.263)
    
    // Default settings
    static constexpr int DEFAULT_AHEAD_BUFFER = 3;
    static constexpr double DEFAULT_THRESHOLD_VALUE = 50.0; // 50 GB or 50%
    static constexpr int DEFAULT_EPISODE_COUNT = 12;  // Default episode count when unknown (typical anime cour)
};

#endif // WATCHSESSIONMANAGER_H
