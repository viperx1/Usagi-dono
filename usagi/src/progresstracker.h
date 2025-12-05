#ifndef PROGRESSTRACKER_H
#define PROGRESSTRACKER_H

#include <QElapsedTimer>
#include <QList>
#include <QMutex>
#include <QPair>

/**
 * @brief ProgressTracker - Reusable class for tracking progress with ETA calculation
 * 
 * This class encapsulates progress tracking logic that was previously scattered
 * throughout the Window class. It provides:
 * - Thread-safe progress updates
 * - ETA calculation using moving average
 * - Automatic throttling of ETA updates
 * - Per-task progress tracking
 * 
 * Design improvements:
 * - Single Responsibility: Only tracks progress and calculates ETAs
 * - Encapsulation: Internal state hidden from users
 * - Reusability: Can be used for any progress-based operation
 * - Thread Safety: All operations are mutex-protected
 * 
 * Example usage:
 * @code
 *   ProgressTracker tracker(100);  // 100 total units
 *   tracker.start();
 *   
 *   // In worker thread:
 *   tracker.updateProgress(5);  // 5 more units completed
 *   
 *   // In UI update:
 *   qint64 eta = tracker.getETA();  // Get estimated time remaining
 *   double percent = tracker.getProgressPercent();
 * @endcode
 */
class ProgressTracker
{
public:
    /**
     * @brief Construct progress tracker
     * @param totalUnits Total number of units to track (e.g., file parts, bytes, etc.)
     */
    explicit ProgressTracker(int totalUnits = 0);
    
    /**
     * @brief Start timing (call before first progress update)
     */
    void start();
    
    /**
     * @brief Reset tracker for a new operation
     * @param totalUnits New total units
     */
    void reset(int totalUnits);
    
    /**
     * @brief Update progress with completed units
     * @param completedUnits Number of units completed so far
     * @param taskId Optional task identifier for per-task tracking (e.g., thread ID)
     */
    void updateProgress(int completedUnits, int taskId = -1);
    
    /**
     * @brief Add incremental progress
     * @param deltaUnits Number of additional units completed since last update
     * @param taskId Optional task identifier for per-task tracking
     */
    void addProgress(int deltaUnits, int taskId = -1);
    
    /**
     * @brief Get current progress as a percentage (0-100)
     * @return Progress percentage
     */
    double getProgressPercent() const;
    
    /**
     * @brief Get estimated time remaining in milliseconds
     * @return Milliseconds until completion, or -1 if cannot estimate
     * 
     * This uses a moving average of recent progress to provide a stable ETA
     * that adapts to changing speeds.
     */
    qint64 getETA() const;
    
    /**
     * @brief Get formatted ETA string
     * @return Human-readable ETA (e.g., "2h 15m", "45s", "< 1s")
     */
    QString getETAString() const;
    
    /**
     * @brief Check if operation is complete
     * @return true if all units are completed
     */
    bool isComplete() const;
    
    /**
     * @brief Get elapsed time since start in milliseconds
     * @return Milliseconds elapsed
     */
    qint64 getElapsedTime() const;
    
    /**
     * @brief Get total units to complete
     * @return Total units
     */
    int getTotalUnits() const;
    
    /**
     * @brief Get completed units
     * @return Units completed so far
     */
    int getCompletedUnits() const;
    
    /**
     * @brief Get remaining units
     * @return Units remaining
     */
    int getRemainingUnits() const;
    
    /**
     * @brief Get current processing speed
     * @return Units per second
     */
    double getSpeed() const;
    
    /**
     * @brief Check if ETA should be updated (throttled to avoid UI spam)
     * @param minIntervalMs Minimum milliseconds between ETA updates
     * @return true if enough time has passed since last ETA update
     */
    bool shouldUpdateETA(int minIntervalMs = 500) const;
    
private:
    // Progress snapshot for moving average calculation
    struct ProgressSnapshot {
        qint64 timestamp;      // Time in milliseconds
        int completedUnits;    // Units completed at this time
    };
    
    // Add a snapshot to history for ETA calculation
    void addSnapshot(int completedUnits);
    
    // Calculate speed from history (units per second)
    double calculateSpeed() const;
    
    // Format milliseconds as human-readable string
    static QString formatDuration(qint64 milliseconds);
    
    // Total units to track
    int m_totalUnits;
    
    // Currently completed units
    int m_completedUnits;
    
    // Timer for elapsed time tracking
    QElapsedTimer m_timer;
    
    // Timer for ETA update throttling
    mutable QElapsedTimer m_lastEtaUpdate;
    
    // Progress history for moving average
    QList<ProgressSnapshot> m_history;
    
    // Maximum history size (keep last N snapshots)
    static constexpr int MAX_HISTORY_SIZE = 20;
    
    // Minimum time between snapshots (ms)
    static constexpr int MIN_SNAPSHOT_INTERVAL = 100;
    
    // Track last progress per task ID (for delta calculation)
    QMap<int, int> m_taskProgress;
    
    // Mutex for thread safety
    mutable QMutex m_mutex;
    
    // Flag to indicate if tracking has started
    bool m_started;
};

#endif // PROGRESSTRACKER_H
