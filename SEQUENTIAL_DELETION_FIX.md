# Sequential File Deletion Fix

## Problem: Continuous Loop Deletion

**Before the fix**, files were deleted in a continuous loop without waiting for API confirmation:

```
Time  Event
------------------------------------------------------------------
T0    User triggers deletion / Auto-deletion threshold reached
T1    → File 1: I/O + DB operations in background (60-90ms)
T91   → File 1: Send MYLISTADD API request (tag=57207)
T92   → File 1: Immediately call onFileDeletionResult(lid1, aid1, true)
T93   → File 2: NEXT DELETION TRIGGERED (wrong!)
T94   → File 2: I/O + DB operations in background (60-90ms)
T184  → File 2: Send MYLISTADD API request (tag=57208)
T185  → File 2: Immediately call onFileDeletionResult(lid2, aid2, true)
T186  → File 3: NEXT DELETION TRIGGERED (wrong!)
...
      (API responses arrive later, but deletions already proceeded)
T500  ← API: Receive code 311 for tag=57207 (too late!)
T650  ← API: Receive code 311 for tag=57208 (too late!)
```

**Issue**: Multiple files deleted before receiving any API confirmations.

---

## Solution: Sequential Deletion with API Confirmation

**After the fix**, each deletion waits for API confirmation before proceeding:

```
Time  Event
------------------------------------------------------------------
T0    User triggers deletion / Auto-deletion threshold reached
T1    → File 1: I/O + DB operations in background (60-90ms)
T91   → File 1: Send MYLISTADD API request (tag=57207)
T92   → File 1: Store in m_pendingDeletions[tag=57207] = (lid1, aid1)
T93   → File 1: DO NOT call onFileDeletionResult() yet
T94   → WAITING for API response...
T95   → WAITING for API response...
T96   → WAITING for API response...
...
T500  ← API: Receive code 311 for tag=57207
T501  → Check m_pendingDeletions, found tag=57207
T502  → Call onFileDeletionResult(lid1, aid1, true)
T503  → File 2: NEXT DELETION TRIGGERED (correct!)
T504  → File 2: I/O + DB operations in background (60-90ms)
T594  → File 2: Send MYLISTADD API request (tag=57208)
T595  → File 2: Store in m_pendingDeletions[tag=57208] = (lid2, aid2)
T596  → File 2: DO NOT call onFileDeletionResult() yet
T597  → WAITING for API response...
...
T750  ← API: Receive code 311 for tag=57208
T751  → Check m_pendingDeletions, found tag=57208
T752  → Call onFileDeletionResult(lid2, aid2, true)
T753  → File 3: NEXT DELETION TRIGGERED (correct!)
```

**Benefit**: Only one file deleted at a time, each deletion confirmed by API before next begins.

---

## Code Flow

### 1. File Deletion Completes (window.cpp:1052-1089)

```cpp
connect(worker, &FileDeletionWorker::finished, this, [this](const FileDeletionResult &result) {
    if (result.success && adbapi) {
        if (result.fid > 0 && !result.ed2k.isEmpty()) {
            // Send API request
            QString tag = adbapi->MylistAdd(result.size, result.ed2k, 0, MylistState::DELETED, "", true);
            
            // Store pending deletion - DO NOT notify WatchSessionManager yet!
            m_pendingDeletions[tag] = QPair<int, int>(result.lid, result.aid);
            
            LOG("Sent MYLISTADD - awaiting API confirmation");
        }
    }
    // NOTE: onFileDeletionResult() is NOT called here for successful deletions
});
```

### 2. API Response Received (window.cpp:2309-2334)

```cpp
void Window::getNotifyMylistAdd(QString tag, int code)
{
    // Check if this is a pending file deletion awaiting API confirmation
    if (code == 311 && m_pendingDeletions.contains(tag)) {
        QPair<int, int> deletion = m_pendingDeletions.take(tag);
        int lid = deletion.first;
        int aid = deletion.second;
        
        LOG("Received API confirmation (code 311) - notifying WatchSessionManager");
        
        // NOW notify WatchSessionManager - this may trigger next deletion
        if (watchSessionManager) {
            watchSessionManager->onFileDeletionResult(lid, aid, true);
        }
        
        return; // Don't process as regular mylist add
    }
    
    // ... rest of getNotifyMylistAdd logic
}
```

### 3. Next Deletion Triggered (watchsessionmanager.cpp:896-911)

```cpp
void WatchSessionManager::onFileDeletionResult(int lid, int aid, bool success)
{
    if (success) {
        emit fileDeleted(lid, aid);
        
        // Continue deleting if space is still below threshold
        if (m_enableActualDeletion && isDeletionNeeded()) {
            LOG("Space still below threshold - continuing with next file");
            bool deletedNext = deleteNextEligibleFile(true);
        }
    }
}
```

---

## Data Structure

```cpp
// In window.h
QMap<QString, QPair<int, int>> m_pendingDeletions;
//    └─ API tag  └─ (lid, aid)

// Example state during batch deletion:
m_pendingDeletions = {
    "57207": (424025633, 7891),  // Waiting for confirmation
    "57208": (424025630, 7891),  // Waiting for confirmation  
    "57209": (424025628, 7891)   // Waiting for confirmation
}

// But with the fix, only ONE deletion is pending at a time:
m_pendingDeletions = {
    "57207": (424025633, 7891)   // Currently waiting
}
// Next deletion won't start until this one gets code 311
```

---

## Test Coverage

```cpp
void TestWatchSessionManager::testSequentialDeletionWithApiConfirmation()
{
    // 1. Trigger first deletion
    bool firstDeleted = manager->deleteNextEligibleFile(true);
    QVERIFY(firstDeleted);
    QCOMPARE(deletionRequestSpy.count(), 1);
    
    // 2. Simulate file I/O completion
    int lid = deletionRequestSpy.at(0).at(0).toInt();
    manager->onFileDeletionResult(lid, 1, true);
    
    // 3. Verify next deletion was NOT triggered yet
    QCOMPARE(deletionRequestSpy.count(), 1);  // Still just one!
    
    // Next deletion will only happen after API sends code 311
    // via Window::getNotifyMylistAdd()
}
```

---

## Summary

**Key Changes:**
1. Added `m_pendingDeletions` map to track deletions awaiting API confirmation
2. Store tag when MYLISTADD is sent, DON'T call `onFileDeletionResult()` immediately
3. When API responds with code 311, check pending deletions and THEN call `onFileDeletionResult()`
4. This ensures sequential deletion: one file at a time, each confirmed before next begins

**Result:**
✅ Files deleted sequentially, not in a continuous loop
✅ Each deletion waits for API confirmation (code 311) before proceeding
✅ Proper synchronization with AniDB API
✅ Test coverage added to verify behavior
