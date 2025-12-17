/**
 * Minimal stubs for test_hasher_card_update_signal
 * 
 * This file provides stub implementations of global variables and classes
 * that are normally defined in window.cpp but are not needed for the test's
 * core functionality.
 */

#include "../usagi/src/hasherthreadpool.h"
#include "../usagi/src/window.h"
#include <QEvent>

// Global hasher thread pool pointer (normally defined in window.cpp)
HasherThreadPool *hasherThreadPool = nullptr;

// Stub implementation of hashes_::event
bool hashes_::event(QEvent *e)
{
    // For test purposes, just forward to base class
    if (!e) {
        return false;
    }
    return QTableWidget::event(e);
}
