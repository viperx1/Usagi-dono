#include "progresstracker.h"
#include <QMutexLocker>
#include <QtMath>
#include <algorithm>

ProgressTracker::ProgressTracker(int totalUnits)
    : m_totalUnits(totalUnits)
    , m_completedUnits(0)
    , m_started(false)
{
}

void ProgressTracker::start()
{
    QMutexLocker locker(&m_mutex);
    m_timer.start();
    m_lastEtaUpdate.start();
    m_started = true;
    m_completedUnits = 0;
    m_history.clear();
    m_taskProgress.clear();
    
    // Add initial snapshot
    addSnapshot(0);
}

void ProgressTracker::reset(int totalUnits)
{
    QMutexLocker locker(&m_mutex);
    m_totalUnits = totalUnits;
    m_completedUnits = 0;
    m_started = false;
    m_history.clear();
    m_taskProgress.clear();
}

void ProgressTracker::updateProgress(int completedUnits, int taskId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_started) {
        return;
    }
    
    // Update total completed units
    m_completedUnits = completedUnits;
    
    // Update task-specific progress if tracking tasks
    if (taskId >= 0) {
        m_taskProgress[taskId] = completedUnits;
    }
    
    // Add snapshot if enough time has passed
    qint64 elapsed = m_timer.elapsed();
    if (m_history.isEmpty() || elapsed - m_history.last().timestamp >= MIN_SNAPSHOT_INTERVAL) {
        addSnapshot(completedUnits);
    }
}

void ProgressTracker::addProgress(int deltaUnits, int taskId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_started) {
        return;
    }
    
    // Update total completed units
    m_completedUnits += deltaUnits;
    
    // Update task-specific progress if tracking tasks
    if (taskId >= 0) {
        m_taskProgress[taskId] = m_taskProgress.value(taskId, 0) + deltaUnits;
    }
    
    // Add snapshot if enough time has passed
    qint64 elapsed = m_timer.elapsed();
    if (m_history.isEmpty() || elapsed - m_history.last().timestamp >= MIN_SNAPSHOT_INTERVAL) {
        addSnapshot(m_completedUnits);
    }
}

double ProgressTracker::getProgressPercent() const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_totalUnits <= 0) {
        return 0.0;
    }
    
    return (static_cast<double>(m_completedUnits) / m_totalUnits) * 100.0;
}

qint64 ProgressTracker::getETA() const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_started || m_completedUnits <= 0 || m_totalUnits <= 0) {
        return -1;
    }
    
    int remaining = m_totalUnits - m_completedUnits;
    if (remaining <= 0) {
        return 0;
    }
    
    double speed = calculateSpeed();
    if (speed <= 0.0) {
        return -1;  // Cannot estimate
    }
    
    // ETA = remaining units / speed (units per second)
    double etaSeconds = remaining / speed;
    return static_cast<qint64>(etaSeconds * 1000.0);
}

QString ProgressTracker::getETAString() const
{
    qint64 eta = getETA();
    
    if (eta < 0) {
        return "Calculating...";
    }
    
    if (eta == 0) {
        return "Complete";
    }
    
    return formatDuration(eta);
}

bool ProgressTracker::isComplete() const
{
    QMutexLocker locker(&m_mutex);
    return m_completedUnits >= m_totalUnits;
}

qint64 ProgressTracker::getElapsedTime() const
{
    QMutexLocker locker(&m_mutex);
    return m_started ? m_timer.elapsed() : 0;
}

int ProgressTracker::getTotalUnits() const
{
    QMutexLocker locker(&m_mutex);
    return m_totalUnits;
}

int ProgressTracker::getCompletedUnits() const
{
    QMutexLocker locker(&m_mutex);
    return m_completedUnits;
}

int ProgressTracker::getRemainingUnits() const
{
    QMutexLocker locker(&m_mutex);
    return std::max(0, m_totalUnits - m_completedUnits);
}

double ProgressTracker::getSpeed() const
{
    QMutexLocker locker(&m_mutex);
    return calculateSpeed();
}

bool ProgressTracker::shouldUpdateETA(int minIntervalMs) const
{
    QMutexLocker locker(&m_mutex);
    return m_started && m_lastEtaUpdate.elapsed() >= minIntervalMs;
}

void ProgressTracker::addSnapshot(int completedUnits)
{
    // Caller must hold mutex
    
    ProgressSnapshot snapshot;
    snapshot.timestamp = m_timer.elapsed();
    snapshot.completedUnits = completedUnits;
    
    m_history.append(snapshot);
    
    // Limit history size
    while (m_history.size() > MAX_HISTORY_SIZE) {
        m_history.removeFirst();
    }
}

double ProgressTracker::calculateSpeed() const
{
    // Caller must hold mutex
    
    if (m_history.size() < 2) {
        return 0.0;  // Not enough data
    }
    
    // Use linear regression on recent history for more stable speed estimate
    // Simple approach: calculate average speed over available history
    
    const ProgressSnapshot& first = m_history.first();
    const ProgressSnapshot& last = m_history.last();
    
    qint64 timeDiff = last.timestamp - first.timestamp;
    if (timeDiff <= 0) {
        return 0.0;
    }
    
    int unitsDiff = last.completedUnits - first.completedUnits;
    if (unitsDiff <= 0) {
        return 0.0;
    }
    
    // Speed in units per second
    double speed = (static_cast<double>(unitsDiff) / timeDiff) * 1000.0;
    return speed;
}

QString ProgressTracker::formatDuration(qint64 milliseconds)
{
    if (milliseconds < 1000) {
        return "< 1s";
    }
    
    qint64 seconds = milliseconds / 1000;
    
    if (seconds < 60) {
        return QString("%1s").arg(seconds);
    }
    
    qint64 minutes = seconds / 60;
    seconds = seconds % 60;
    
    if (minutes < 60) {
        if (seconds > 0) {
            return QString("%1m %2s").arg(minutes).arg(seconds);
        }
        return QString("%1m").arg(minutes);
    }
    
    qint64 hours = minutes / 60;
    minutes = minutes % 60;
    
    if (hours < 24) {
        if (minutes > 0) {
            return QString("%1h %2m").arg(hours).arg(minutes);
        }
        return QString("%1h").arg(hours);
    }
    
    qint64 days = hours / 24;
    hours = hours % 24;
    
    if (hours > 0) {
        return QString("%1d %2h").arg(days).arg(hours);
    }
    return QString("%1d").arg(days);
}
