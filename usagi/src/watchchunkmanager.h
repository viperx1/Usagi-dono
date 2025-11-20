#ifndef WATCHCHUNKMANAGER_H
#define WATCHCHUNKMANAGER_H

#include <QObject>
#include <QSet>
#include <QMap>

/**
 * @brief Manages chunk-based watch tracking for media files
 * 
 * This class implements chunk-based tracking where each file is divided into
 * 1-minute chunks. It tracks which chunks have been watched and determines
 * when a file should be marked as "locally watched" based on watch percentage.
 * 
 * This tracking is separate from AniDB's viewed status and helps prevent
 * accidentally marking episodes as watched when only briefly viewed.
 */
class WatchChunkManager : public QObject
{
    Q_OBJECT
    
public:
    explicit WatchChunkManager(QObject *parent = nullptr);
    ~WatchChunkManager();
    
    /**
     * @brief Record that a chunk has been watched
     * @param lid MyList ID
     * @param chunkIndex The chunk index (position / CHUNK_SIZE_SECONDS)
     */
    void recordChunk(int lid, int chunkIndex);
    
    /**
     * @brief Clear all chunks for a specific file
     * @param lid MyList ID
     */
    void clearChunks(int lid);
    
    /**
     * @brief Get the set of watched chunks for a file
     * @param lid MyList ID
     * @return Set of chunk indices that have been watched
     */
    QSet<int> getWatchedChunks(int lid) const;
    
    /**
     * @brief Calculate watch percentage for a file
     * @param lid MyList ID
     * @param durationSeconds Total duration of the file in seconds
     * @return Percentage of chunks watched (0-100)
     */
    double calculateWatchPercentage(int lid, int durationSeconds) const;
    
    /**
     * @brief Check if a file should be marked as watched
     * @param lid MyList ID
     * @param durationSeconds Total duration of the file in seconds
     * @return true if watch criteria are met
     */
    bool shouldMarkAsWatched(int lid, int durationSeconds) const;
    
    /**
     * @brief Update local watched status in database
     * @param lid MyList ID
     * @param watched Whether the file is locally watched
     */
    void updateLocalWatchedStatus(int lid, bool watched);
    
    /**
     * @brief Get local watched status from database
     * @param lid MyList ID
     * @return true if marked as locally watched
     */
    bool getLocalWatchedStatus(int lid) const;
    
    /**
     * @brief Get the chunk size in seconds (default: 60)
     */
    static int getChunkSizeSeconds() { return CHUNK_SIZE_SECONDS; }
    
    /**
     * @brief Get the minimum watch percentage to mark as watched (default: 80%)
     */
    static double getMinWatchPercentage() { return MIN_WATCH_PERCENTAGE; }
    
    /**
     * @brief Get the minimum watch time in seconds (default: 300 = 5 minutes)
     */
    static int getMinWatchTimeSeconds() { return MIN_WATCH_TIME_SECONDS; }
    
signals:
    /**
     * @brief Emitted when a file is marked as locally watched
     * @param lid MyList ID
     */
    void fileMarkedAsWatched(int lid);
    
private:
    void loadChunksFromDatabase(int lid);
    void saveChunkToDatabase(int lid, int chunkIndex);
    int getTotalChunks(int durationSeconds) const;
    
    // Cache of loaded chunks to avoid repeated database queries
    mutable QMap<int, QSet<int>> m_cachedChunks;
    
    // Configuration constants
    static const int CHUNK_SIZE_SECONDS = 60;           // 1 minute chunks
    static const int MIN_WATCH_TIME_SECONDS = 300;      // 5 minutes minimum
    static constexpr double MIN_WATCH_PERCENTAGE = 80.0; // 80% of file must be watched
};

#endif // WATCHCHUNKMANAGER_H
