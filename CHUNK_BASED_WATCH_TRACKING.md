# Chunk-Based Watch Tracking Feature

## Overview

This feature implements intelligent watch tracking for local anime file playback. Instead of simply marking files as "watched" when the player reaches the end, it tracks playback in 1-minute chunks to provide more accurate watch status that prevents accidental marking.

## Problem Statement

The original issue identified a common problem:
> "i watched episodes 1-5, was about to start 6, but clicked play on 12 instead"

Without chunk-based tracking, even briefly viewing episode 12 could mark it as watched, causing confusion about which episodes were actually seen.

## Solution

The system now tracks watching in **1-minute chunks** and only marks files as "locally watched" when specific criteria are met:

### Watch Criteria

1. **For files longer than 5 minutes:**
   - At least 80% of 1-minute chunks must be watched
   - Example: A 20-minute file has 20 chunks; at least 16 must be watched

2. **For files 5 minutes or shorter:**
   - At least one chunk must be watched
   - Simpler threshold for short content

### Separate from AniDB Status

- **`local_watched`**: Local tracking based on chunk viewing (new)
- **`viewed`**: AniDB API status (existing, unchanged)

The two statuses are independent. You can:
- Mark files as watched locally without syncing to AniDB
- Sync AniDB status without affecting local tracking
- Use local status for session tracking while keeping AniDB in sync

## Database Schema

### New Tables

#### `watch_chunks`
Stores which 1-minute chunks have been watched for each file.

```sql
CREATE TABLE IF NOT EXISTS `watch_chunks`(
    `id` INTEGER PRIMARY KEY AUTOINCREMENT,
    `lid` INTEGER NOT NULL,
    `chunk_index` INTEGER NOT NULL,
    `watched_at` INTEGER NOT NULL,
    UNIQUE(`lid`, `chunk_index`)
);

CREATE INDEX IF NOT EXISTS `idx_watch_chunks_lid` ON `watch_chunks`(`lid`);
```

### Modified Tables

#### `mylist`
Added column for local watch status:

```sql
ALTER TABLE `mylist` ADD COLUMN `local_watched` INTEGER DEFAULT 0;
```

## Implementation

### Core Classes

#### `WatchChunkManager`
Manages chunk recording and watch status determination.

**Key Methods:**
- `recordChunk(lid, chunkIndex)` - Record that a chunk was watched
- `calculateWatchPercentage(lid, duration)` - Calculate percentage watched
- `shouldMarkAsWatched(lid, duration)` - Check if watch criteria met
- `updateLocalWatchedStatus(lid, watched)` - Update database status
- `getLocalWatchedStatus(lid)` - Query current status

**Configuration:**
- `CHUNK_SIZE_SECONDS = 60` - 1-minute chunks
- `MIN_WATCH_PERCENTAGE = 80.0` - 80% threshold
- `MIN_WATCH_TIME_SECONDS = 300` - 5-minute minimum for percentage check

#### Integration with `PlaybackManager`

The `PlaybackManager` now:
1. Creates a `WatchChunkManager` instance
2. Records chunks during playback every second
3. Checks watch criteria after each chunk
4. Automatically updates local_watched status

```cpp
// During playback (every second)
int chunkIndex = position / WatchChunkManager::getChunkSizeSeconds();
m_watchChunkManager->recordChunk(m_currentLid, chunkIndex);

// Check if should mark as watched
if (!m_watchChunkManager->getLocalWatchedStatus(m_currentLid)) {
    if (m_watchChunkManager->shouldMarkAsWatched(m_currentLid, duration)) {
        m_watchChunkManager->updateLocalWatchedStatus(m_currentLid, true);
    }
}
```

### UI Integration

#### Play Button Display

The play button now reflects local watch status:
- **▶** - Not locally watched (can play)
- **✓** - Locally watched (completed)
- **✗** - File not found

This is determined by querying `local_watched` from the database instead of the AniDB `viewed` field.

#### Episode/Anime Status

Episodes and anime items aggregate the local watch status of their child files:
- Episode marked with ✓ if all video files are locally watched
- Anime marked with ✓ if all episodes are locally watched

## Usage Examples

### Scenario 1: Normal Viewing
```
User plays episode 1 (24 minutes)
→ Watches from start to 20 minutes
→ 20/24 chunks = 83% watched
→ File marked as locally watched ✓
```

### Scenario 2: Accidental Click
```
User accidentally clicks episode 12
→ Views for 30 seconds (0.5/24 chunks = 2%)
→ Below 80% threshold
→ File NOT marked as watched (stays as ▶)
```

### Scenario 3: Seeking Around
```
User watches episode with seeking:
→ Watches chunks: 0-5, 10-15, 20-24 (16 chunks)
→ 16/24 = 66% watched
→ Below 80% threshold
→ File NOT marked as watched
→ Continue watching chunks 6-9, 16-19 (8 more chunks)
→ Now 24/24 = 100% watched
→ File marked as locally watched ✓
```

### Scenario 4: Short Content
```
User plays 3-minute opening (OP)
→ Watches any part (at least 1 chunk)
→ Short file exception applies
→ File marked as locally watched ✓
```

## API Reference

### WatchChunkManager

#### Public Methods

```cpp
// Record a chunk as watched
void recordChunk(int lid, int chunkIndex);

// Clear all chunks for a file
void clearChunks(int lid);

// Get watched chunks
QSet<int> getWatchedChunks(int lid) const;

// Calculate watch percentage (0-100)
double calculateWatchPercentage(int lid, int durationSeconds) const;

// Check if should be marked as watched
bool shouldMarkAsWatched(int lid, int durationSeconds) const;

// Update local watched status
void updateLocalWatchedStatus(int lid, bool watched);

// Get local watched status
bool getLocalWatchedStatus(int lid) const;
```

#### Signals

```cpp
// Emitted when a file is marked as locally watched
void fileMarkedAsWatched(int lid);
```

#### Static Methods

```cpp
static int getChunkSizeSeconds();      // Returns 60
static double getMinWatchPercentage(); // Returns 80.0
static int getMinWatchTimeSeconds();   // Returns 300
```

## Testing

Comprehensive test suite in `tests/test_watchchunkmanager.cpp`:

- `testRecordChunk()` - Verify chunk recording
- `testGetWatchedChunks()` - Test chunk retrieval
- `testClearChunks()` - Test chunk clearing
- `testCalculateWatchPercentage()` - Test percentage calculation
- `testShouldMarkAsWatched_ShortFile()` - Test short file logic
- `testShouldMarkAsWatched_LongFile()` - Test 80% threshold
- `testShouldMarkAsWatched_InsufficientWatching()` - Test below threshold
- `testLocalWatchedStatus()` - Test status updates
- `testChunkCaching()` - Test performance optimization

All tests pass ✅

## Performance Considerations

### Chunk Caching

The `WatchChunkManager` caches loaded chunks in memory to avoid repeated database queries:

```cpp
mutable QMap<int, QSet<int>> m_cachedChunks;
```

- Chunks loaded once from database
- Updates added to cache
- Cache persists for application lifetime

### Database Efficiency

- Indexed on `lid` for fast lookups
- UNIQUE constraint prevents duplicate chunks
- INSERT OR IGNORE for idempotent chunk recording

## Future Enhancements

Potential improvements for future versions:

1. **Configurable thresholds** - Allow users to adjust 80% requirement
2. **Session management** - Track watching sessions separately
3. **Watch time statistics** - Show total watch time per anime
4. **Chunk decay** - Expire old chunks after a time period
5. **Export/import** - Backup and restore watch history
6. **Multi-user support** - Track chunks per user profile
7. **Analytics** - View watching patterns and habits

## Migration

For existing databases, the migration is automatic:
- New columns added with ALTER TABLE
- Existing files have `local_watched = 0` by default
- No data loss or corruption
- Forward and backward compatible

## Troubleshooting

### Files not marking as watched

Check:
1. MPC-HC web interface enabled (port 13579)
2. Playback actually occurring (check logs)
3. Watch percentage meets threshold (80% for long files)

### Play button not updating

The button updates when:
1. Playback completes
2. `updatePlayButtonForItem()` is called
3. MyList is reloaded

Force refresh by switching tabs or reloading mylist.

## See Also

- `PLAYBACK_FEATURE.md` - Original playback feature documentation
- `usagi/src/watchchunkmanager.h` - Header file
- `usagi/src/watchchunkmanager.cpp` - Implementation
- `usagi/src/playbackmanager.cpp` - Integration code
- `tests/test_watchchunkmanager.cpp` - Test suite
