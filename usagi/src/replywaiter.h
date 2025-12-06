#ifndef REPLYWAITER_H
#define REPLYWAITER_H

#include <QElapsedTimer>

/**
 * @class ReplyWaiter
 * @brief Manages the state of waiting for network replies with timeout tracking
 * 
 * This class encapsulates the logic for tracking whether we're waiting for a reply
 * from a network operation and measuring how long we've been waiting. It provides
 * timeout detection and proper state management.
 * 
 * Renamed from _waitingForReply to follow proper C++ naming conventions.
 * 
 * SOLID Principles Applied:
 * - Single Responsibility: Only manages reply waiting state
 * - Encapsulation: Private state with controlled access
 * - Clear Interface: Simple methods for common operations
 */
class ReplyWaiter
{
public:
    /**
     * @brief Construct a ReplyWaiter in non-waiting state
     */
    ReplyWaiter();
    
    /**
     * @brief Check if currently waiting for a reply
     * @return true if waiting for reply
     */
    bool isWaiting() const { return m_isWaiting; }
    
    /**
     * @brief Get elapsed time since starting to wait
     * @return Milliseconds elapsed, or 0 if not waiting
     */
    qint64 elapsedMs() const;
    
    /**
     * @brief Start waiting for a reply (starts timer)
     */
    void startWaiting();
    
    /**
     * @brief Stop waiting for a reply (resets state)
     */
    void stopWaiting();
    
    /**
     * @brief Check if waiting has timed out
     * @param timeoutMs Timeout threshold in milliseconds
     * @return true if waiting and elapsed time exceeds timeout
     */
    bool hasTimedOut(qint64 timeoutMs) const;
    
    /**
     * @brief Reset to non-waiting state
     */
    void reset();
    
private:
    bool m_isWaiting;          ///< Whether currently waiting for reply
    QElapsedTimer m_timer;     ///< Timer for measuring wait duration
};

#endif // REPLYWAITER_H
