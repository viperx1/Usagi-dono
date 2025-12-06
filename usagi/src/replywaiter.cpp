#include "replywaiter.h"

ReplyWaiter::ReplyWaiter()
    : m_isWaiting(false)
{
}

qint64 ReplyWaiter::elapsedMs() const
{
    if (!m_isWaiting) {
        return 0;
    }
    return m_timer.elapsed();
}

void ReplyWaiter::startWaiting()
{
    m_isWaiting = true;
    m_timer.start();
}

void ReplyWaiter::stopWaiting()
{
    m_isWaiting = false;
}

bool ReplyWaiter::hasTimedOut(qint64 timeoutMs) const
{
    if (!m_isWaiting) {
        return false;
    }
    return m_timer.elapsed() > timeoutMs;
}

void ReplyWaiter::reset()
{
    m_isWaiting = false;
}
