# Missing Anime Data Request Implementation

## Overview
This feature automatically detects and requests missing related anime data from the AniDB API when displaying cards with series chains enabled. When an anime in the user's list has a prequel or sequel that is not in the database, the system will automatically request that anime's data from AniDB.

## Problem Statement
Previously, when displaying anime with series chains:
1. Only anime already in the database could be shown in chains
2. If a related anime (prequel/sequel) was missing from the database, it would not appear in the chain
3. Users had to manually request missing anime data
4. Chains would be incomplete without manual intervention

## Solution
The implementation adds automatic detection and API requests for missing related anime:

1. **Detection Phase**: During chain expansion (`preloadRelationDataForChainExpansion`), the system identifies related anime that are referenced but not in the database
2. **Request Phase**: Missing anime are requested from the AniDB API via the `missingAnimeDataDetected` signal
3. **Update Phase**: When API responses arrive, the data is reloaded into cache and chains are rebuilt
4. **Display Phase**: The display is refreshed to show the newly arrived anime in their proper chain position

## Technical Implementation

### 1. Detection of Missing Anime
**File**: `usagi/src/mylistcardmanager.cpp`
**Function**: `preloadRelationDataForChainExpansion()`

```cpp
// After loading relation data from database
QSet<int> loadedAids;  // Track which AIDs were found
// ... database query ...

// Detect which AIDs were NOT found in the database
QSet<int> missingAids = aidsToLoad - loadedAids;

if (!missingAids.isEmpty()) {
    QList<int> missingAidList(missingAids.begin(), missingAids.end());
    emit missingAnimeDataDetected(missingAidList);
}
```

### 2. Signal Definition
**File**: `usagi/src/mylistcardmanager.h`

```cpp
signals:
    // Emitted when missing anime data is detected during chain expansion
    void missingAnimeDataDetected(const QList<int>& aids);
```

### 3. API Request Handler
**File**: `usagi/src/window.cpp`
**Location**: Window constructor

```cpp
// Connect missing anime data detected signal to request from API
connect(cardManager, &MyListCardManager::missingAnimeDataDetected, this, [this](const QList<int>& aids) {
    LOG(QString("[Window] Missing anime data detected for %1 anime during chain expansion").arg(aids.size()));
    if (adbapi) {
        for (int aid : aids) {
            LOG(QString("[Window] Requesting missing anime data from AniDB API for aid=%1").arg(aid));
            adbapi->Anime(aid);
        }
    } else {
        LOG(QString("[Window] ERROR: adbapi is null, cannot request anime data"));
    }
});
```

### 4. Cache Update on API Response
**File**: `usagi/src/mylistcardmanager.cpp`
**Function**: `onAnimeUpdated()`

```cpp
void MyListCardManager::onAnimeUpdated(int aid)
{
    LOG(QString("[MyListCardManager] Anime updated for aid=%1").arg(aid));
    
    // Reload this anime's data into the cache from database
    QList<int> aidList{aid};
    preloadCardCreationData(aidList);
    
    // ... rest of update logic ...
}
```

### 5. Chain Rebuild and Display Refresh
**File**: `usagi/src/window.cpp`
**Function**: `getNotifyAnimeUpdated()`

```cpp
if (filterSidebar && filterSidebar->getShowSeriesChain() && watchSessionManager) {
    checkAndRequestChainRelations(aid);
    
    // Refresh with throttling (max once per second)
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    const qint64 REFRESH_THROTTLE_MS = 1000;
    
    if (currentTime - lastChainRefreshTime >= REFRESH_THROTTLE_MS) {
        lastChainRefreshTime = currentTime;
        applyMylistFilters();
    }
}
```

## Flow Diagram

```
User enables series chains
        ↓
applyMylistFilters() called
        ↓
cardManager->setAnimeIdList(aids, chainMode=true)
        ↓
buildChainsFromCache()
        ↓
preloadRelationDataForChainExpansion()
        ↓
[Queries DB for related anime]
        ↓
Detects missing anime (referenced but not in DB)
        ↓
emit missingAnimeDataDetected(missingAids)
        ↓
Window receives signal
        ↓
For each missing aid: adbapi->Anime(aid)
        ↓
API requests sent to AniDB
        ↓
[Wait for API responses...]
        ↓
API response arrives → getNotifyAnimeUpdated(aid)
        ↓
onAnimeUpdated(aid) → reloads data into cache
        ↓
checkAndRequestChainRelations(aid) → requests deeper related anime
        ↓
applyMylistFilters() [throttled] → rebuilds chains and refreshes display
        ↓
Chains now include newly arrived anime
```

## Performance Optimizations

### 1. Throttling Chain Refreshes
**Problem**: Multiple anime might be fetched in quick succession, causing excessive chain rebuilds.

**Solution**: Throttle refreshes to maximum once per second using `lastChainRefreshTime` timestamp.

**Benefits**:
- Reduces UI lag during batch API responses
- Prevents repeated expensive chain building operations
- Maintains responsiveness while still updating the display

### 2. Bulk API Requests
**Problem**: Requesting anime one at a time would be slow for large chains.

**Solution**: Detect all missing anime at once and request them in parallel.

**Benefits**:
- Faster chain completion
- Better use of network bandwidth
- Reduced total wait time for users

## Testing

A comprehensive test suite was added in `tests/test_missing_anime_data_request.cpp`:

### Test Cases:
1. **testDetectMissingPrequel**: Verifies detection of missing prequel anime
2. **testDetectMissingSequel**: Verifies detection of missing sequel anime
3. **testDetectMultipleMissingAnime**: Verifies detection of multiple missing related anime
4. **testNoSignalWhenAllAnimePresent**: Ensures no false positives when all anime are present
5. **testAnimeDataReloadOnUpdate**: Verifies cache is updated when API response arrives

## Future Improvements

1. **Batch Refresh Timer**: Instead of throttling to once per second, use a timer that triggers a single refresh after all pending API responses are received
2. **Progress Indicator**: Show users when related anime are being fetched
3. **Retry Logic**: Automatically retry failed API requests
4. **Cache Persistence**: Save fetched related anime data to avoid re-requesting on next startup

## Migration Notes

This is a backward-compatible change:
- Existing functionality is not affected when chain mode is disabled
- No database schema changes required
- No settings or configuration changes needed
- Users will automatically benefit from the feature when using series chains

## Debugging

To debug missing anime detection:
1. Enable debug logging in MyListCardManager
2. Look for log messages with `[DEBUG]` prefix in `preloadRelationDataForChainExpansion`
3. Check for "Missing anime data detected" messages in Window
4. Monitor API requests being sent to AniDB

## Code Review Feedback Addressed

1. **QSet::values() Conversion**: Changed from `missingAids.values()` to explicit iterator-based conversion `QList<int>(missingAids.begin(), missingAids.end())` for better clarity and to avoid implementation-specific behavior.

2. **Throttling**: Added 1-second throttle on `applyMylistFilters()` calls to prevent performance issues during batch anime updates.

3. **Test Data Format**: Fixed relation list string construction in tests to use proper CSV format `'123','456'` instead of malformed `'123''456'`.

## Related Files

- `usagi/src/mylistcardmanager.h` - Signal definition
- `usagi/src/mylistcardmanager.cpp` - Detection and cache update logic
- `usagi/src/window.h` - Throttle timestamp field
- `usagi/src/window.cpp` - Signal connection and refresh logic
- `tests/test_missing_anime_data_request.cpp` - Test suite
- `tests/CMakeLists.txt` - Test build configuration
