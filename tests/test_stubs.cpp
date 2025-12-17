/**
 * Minimal stubs for test_hasher_card_update_signal
 * 
 * This file provides stub implementations of global variables and classes
 * that are normally defined in window.cpp but are not needed for the test's
 * core functionality.
 */

#include "test_hashes_stub.h"
#include "../usagi/src/hasherthreadpool.h"
#include <QEvent>

// Global hasher thread pool pointer (normally defined in window.cpp)
// This stub is needed because HasherCoordinator references this global variable
HasherThreadPool *hasherThreadPool = nullptr;

// Stub implementation of hashes_::event
// This provides the minimal implementation needed for the vtable
bool hashes_::event(QEvent *e)
{
    // For test purposes, just forward to base class
    if (!e) {
        return false;
    }
    return QTableWidget::event(e);
}
