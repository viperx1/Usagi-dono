# Watch Status Types in Usagi-dono

## Overview

This document clarifies the different types of watch statuses and related tracking fields used in Usagi-dono. There are **two distinct watch status types**, plus additional fields for playback tracking.

## Watch Status Types

### 1. AniDB API Watch Status

**Fields:**
- `viewed` (INTEGER) - Boolean indicating if watched on AniDB
- `viewdate` (INTEGER) - Unix timestamp when marked as viewed on AniDB

**Purpose:** Synchronize with AniDB's mylist to track which files are marked as watched on the AniDB server.

**Source:** Retrieved from AniDB API responses during mylist synchronization.

**Scope:** Server-side status that persists across devices and installations.

**Usage:**
- Used for AniDB API operations (MYLISTADD, MYLIST commands)
- Synced when fetching mylist from AniDB
- Updated when manually marking files as watched
- Displayed in tooltips and file info

**Code Example:**
```cpp
// When marking as watched, both statuses are set together
q.prepare("UPDATE mylist SET viewed = 1, local_watched = 1, viewdate = ? WHERE lid = ?");
```

### 2. Local Watch Status (Chunk-based Tracking)

**Fields:**
- `local_watched` (INTEGER) - Boolean indicating if watched locally

**Supporting Table:**
- `watch_chunks` - Tracks which 1-minute chunks have been watched

**Purpose:** Intelligent local tracking based on actual playback percentage to prevent accidental marking.

**Source:** Calculated from chunk-based playback tracking during media player usage.

**Scope:** Local application status specific to this installation.

**Logic:**
- Files longer than 5 minutes: Must watch at least 80% of chunks
- Files 5 minutes or shorter: Must watch at least 1 chunk

**Usage:**
- Controls play button display (▶ for unwatched, ✓ for watched)
- Determines which episode to play next
- Independent from AniDB API status
- Can be set without syncing to AniDB

**Code Example:**
```cpp
// Play button reflects local watch status
if (file.localWatched) {
    fileItem->setText(1, "✓"); // Checkmark for locally watched
} else {
    fileItem->setText(1, "▶"); // Play button for unwatched files
}
```

## Additional Tracking Fields (NOT Watch Status Types)

These fields track playback state but are **not** watch status types:

### Playback Position Tracking

**Fields:**
- `last_played` (INTEGER) - Unix timestamp of last playback session
- `playback_position` (INTEGER) - Last playback position in seconds
- `playback_duration` (INTEGER) - Total duration of file in seconds

**Purpose:** Resume playback from where the user left off.

**Scope:** Local playback state management.

**Important:** `last_played` is **NOT** a watch status. It only tracks when a file was last played for resume functionality.

## Coordination Between Watch Status Types

### When Manually Marking as Watched

When a user manually marks a file or episode as watched, **both** watch status types are set together:

```cpp
// From mylistcardmanager.cpp
q.prepare("UPDATE mylist SET viewed = 1, local_watched = 1, viewdate = ? WHERE lid = ?");
```

This ensures consistency between local UI state and AniDB sync state.

### When Playback Completes

When a file is played to completion (within 5 seconds of the end):

```cpp
// From playbackmanager.cpp
q.prepare("UPDATE mylist SET viewed = 1, local_watched = 1, viewdate = ? WHERE lid = ?");
```

Again, both statuses are set together.

### Automatic Chunk-based Marking

When chunk-based tracking determines a file should be marked as watched (80% threshold met):

```cpp
// From watchchunkmanager.cpp
q.prepare("UPDATE mylist SET local_watched = ? WHERE lid = ?");
```

In this case, **only** `local_watched` is set. The `viewed` status remains unchanged unless explicitly synced.

This allows users to:
1. Track local progress without affecting AniDB
2. Sync to AniDB when ready
3. Have different watch status on different devices

## Common Misconceptions

### ❌ "last_watched" Field

**This field does NOT exist** in the codebase. If you see references to "last_watched", it's likely a confusion with:
- `last_played` - Playback timestamp (not a watch status)
- `local_watched` - Local watch status (chunk-based tracking)

### ❌ "viewdate" as Local Status

`viewdate` is **not** a local timestamp. It is the timestamp from AniDB indicating when the file was marked as viewed on the AniDB server.

### ❌ "last_played" as Watch Status

`last_played` tracks **when** a file was played, not **if** it was watched. It's used for:
- Sorting by recently played
- Resume functionality
- Playback history

It does **not** indicate watch status.

## Database Schema

### mylist Table (relevant fields)

```sql
CREATE TABLE mylist (
    lid INTEGER PRIMARY KEY,
    fid INTEGER,
    eid INTEGER,
    aid INTEGER,
    -- AniDB API watch status
    viewed INTEGER,           -- Boolean: watched on AniDB
    viewdate INTEGER,         -- Timestamp: when marked viewed on AniDB
    
    -- Local watch status (added via ALTER TABLE)
    local_watched INTEGER DEFAULT 0,  -- Boolean: watched locally
    
    -- Playback tracking (NOT watch status)
    last_played INTEGER DEFAULT 0,    -- Timestamp: last playback session
    playback_position INTEGER DEFAULT 0,
    playback_duration INTEGER DEFAULT 0
);
```

### watch_chunks Table

```sql
CREATE TABLE watch_chunks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    lid INTEGER NOT NULL,
    chunk_index INTEGER NOT NULL,
    watched_at INTEGER NOT NULL,
    UNIQUE(lid, chunk_index)
);

CREATE INDEX idx_watch_chunks_lid ON watch_chunks(lid);
```

## Implementation Details

### Key Files

- **`watchchunkmanager.h/cpp`** - Manages chunk-based tracking
- **`playbackmanager.h/cpp`** - Integrates chunk tracking with playback
- **`mylistcardmanager.h/cpp`** - Handles manual watch marking
- **`animecard.h/cpp`** - Displays watch status in UI
- **`anidbapi.cpp`** - Syncs with AniDB API

### Class: WatchChunkManager

Manages the chunk-based watch tracking system:

```cpp
// Record that a 1-minute chunk was watched
void recordChunk(int lid, int chunkIndex);

// Calculate percentage of file watched
double calculateWatchPercentage(int lid, int durationSeconds) const;

// Check if file meets watch criteria (80% threshold)
bool shouldMarkAsWatched(int lid, int durationSeconds) const;

// Update local_watched status in database
void updateLocalWatchedStatus(int lid, bool watched);

// Get current local_watched status
bool getLocalWatchedStatus(int lid) const;
```

## Best Practices

### When to Use Each Status Type

**Use `viewed` (AniDB API status) when:**
- Syncing with AniDB server
- Updating mylist via API
- Checking server-side watch status
- Exporting to AniDB

**Use `local_watched` (chunk-based) when:**
- Determining play button state
- Finding next unwatched episode
- Local session tracking
- Preventing accidental marking

**Use `last_played` when:**
- Implementing resume functionality
- Sorting by recently played
- Displaying playback history

### Code Comments

When working with watch status fields, always clarify which type:

```cpp
// Good: Clear which status type
fileInfo.viewed = (viewed != 0);         // AniDB API watch status
fileInfo.localWatched = (localWatched != 0);  // Local chunk-based status

// Bad: Ambiguous
fileInfo.watched = (watched != 0);  // Which watch status?
```

## Testing

The chunk-based watch tracking system has comprehensive tests:

**Test File:** `tests/test_watchchunkmanager.cpp`

**Test Cases:**
- Chunk recording and retrieval
- Watch percentage calculation
- Short file threshold logic
- Long file 80% threshold
- Status update and retrieval
- Chunk caching performance

All tests verify that `local_watched` operates independently from `viewed`.

## Related Documentation

- **`CHUNK_BASED_WATCH_TRACKING.md`** - Detailed implementation of chunk-based tracking
- **`PLAYBACK_FEATURE.md`** - Playback manager and integration
- **`MYLIST_API_GUIDELINES.md`** - AniDB API usage

## Conclusion

Usagi-dono correctly implements **two distinct watch status types**:

1. **AniDB API status** (`viewed`/`viewdate`) - Server-side sync
2. **Local chunk-based status** (`local_watched`) - Client-side tracking

These are properly separated and coordinated. There is **no confusion** with "last_watched" or other interpretations. The `last_played` field is separate and used only for playback resume functionality.

This separation allows users to:
- Track local progress without affecting AniDB
- Prevent accidental marking from brief previews
- Sync to AniDB when ready
- Have independent watch status across devices
