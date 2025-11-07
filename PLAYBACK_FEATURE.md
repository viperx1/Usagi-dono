# Playback Feature Documentation

## Overview

The playback feature allows users to play their anime files directly from the MyList view and automatically tracks playback progress. The system integrates with MPC-HC media player and monitors playback through its web interface.

## Features

### 1. Media Player Integration
- Supports MPC-HC media player (other players may be added in the future)
- Configurable player path in Settings tab
- Default path: `C:\Program Files (x86)\K-Lite Codec Pack\MPC-HC64\mpc-hc64_nvo.exe`
- Launches player with `/play` and `/close` flags for automatic playback

### 2. Playback Tracking
- Monitors playback position down to the second
- Polls MPC-HC web interface (http://localhost:13579/status.html) every second
- Saves position to database every 5 seconds
- Automatically marks files as watched when playback reaches within 5 seconds of end
- Updates `viewdate` field when file is marked as watched

### 3. Resume Capability
- Stores playback position in database (`playback_position` column)
- Stores total duration (`playback_duration` column)
- Tracks last played timestamp (`last_played` column)
- Automatically resumes from last position when playing previously watched files

### 4. User Interface

#### Play Button Column (Column 9)
Added new "Play" column to MyList tree widget showing playback status:

**File Level:**
- ▶ (play icon): Unwatched or partially watched video files
- ✓ (checkmark): Fully watched video files
- Empty: Non-video files (subtitles, audio tracks, etc.)

**Episode Level:**
- ▶ (play icon): Episode has unwatched video files
- ✓ (checkmark): All video files in episode are watched
- Empty: Episode has no video files

**Anime Level:**
- ▶ (play icon): Anime has unwatched episodes (normal episodes only)
- ✓ (checkmark): All normal episodes are fully watched
- Empty: Anime has no episodes in mylist

#### Settings Page
- "Media Player:" field with path to player executable
- "Browse..." button to select player executable
- Settings saved to database on "Save" button click

### 5. Playback Control

#### Starting Playback
Click the play button (column 9) on any row:

1. **File Row**: Plays that specific file
   - Resumes from last position if previously played
   - Only works for video files

2. **Episode Row**: Plays the first video file in that episode
   - Automatically selects the first video file
   - Skips non-video files (subtitles, audio)

3. **Anime Row**: Plays the first video file in the first episode
   - Starts from the first episode in the list
   - Useful for starting/continuing a series

#### During Playback
- Playback manager polls player status every second
- Position saved to database every 5 seconds
- Signals emitted:
  - `playbackPositionUpdated(lid, position, duration)`: Position update
  - `playbackCompleted(lid)`: Playback finished
  - `playbackStopped(lid, position)`: Player closed

#### File Path Resolution
The system attempts to find the file path in this order:
1. Check `local_files` table via `local_file` foreign key in mylist
2. Fall back to `storage` field in mylist table

## Database Schema Changes

### mylist Table
```sql
ALTER TABLE `mylist` ADD COLUMN `playback_position` INTEGER DEFAULT 0;
ALTER TABLE `mylist` ADD COLUMN `playback_duration` INTEGER DEFAULT 0;
ALTER TABLE `mylist` ADD COLUMN `last_played` INTEGER DEFAULT 0;
```

- `playback_position`: Last known playback position in seconds
- `playback_duration`: Total duration of the file in seconds
- `last_played`: Unix timestamp of last playback session

### settings Table
```sql
INSERT OR REPLACE INTO settings (name, value) VALUES ('media_player_path', '<path>');
```

## Implementation Details

### Classes

#### PlaybackManager
Main class handling playback functionality.

**Methods:**
- `startPlayback(filePath, lid, resumePosition)`: Launch player and start tracking
- `stopTracking()`: Stop polling and save final position
- `getMediaPlayerPath()`: Retrieve player path from settings (static)
- `setMediaPlayerPath(path)`: Save player path to settings (static)

**Signals:**
- `playbackPositionUpdated(int lid, int position, int duration)`
- `playbackCompleted(int lid)`
- `playbackStopped(int lid, int position)`

**Private Methods:**
- `checkPlaybackStatus()`: Timer slot for polling player
- `handleStatusReply()`: Process web interface response
- `savePlaybackPosition(position, duration)`: Update database

### MPC-HC Web Interface

The system parses the status response which has this format:
```javascript
OnStatus("filename.mkv", "Playing", 5298, "00:00:05", 1455038, "00:24:15", 0, 100, "D:\full\path\to\file.mkv")
```

**Parameters:**
1. Filename (short name)
2. State ("Playing", "Paused", "Stopped", "Wstrzymano" [Polish for Paused])
3. Position in milliseconds
4. Position as time string (HH:MM:SS)
5. Duration in milliseconds
6. Duration as time string (HH:MM:SS)
7. Muted (0/1)
8. Volume (0-100)
9. Full file path

The implementation extracts position and duration in milliseconds, converts to seconds, and saves to database.

## Configuration Requirements

### MPC-HC Setup
Users must enable the web interface in MPC-HC:
1. Open MPC-HC
2. Go to Options (O key or View > Options)
3. Navigate to Player > Web Interface
4. Check "Listen on port:" and ensure it's set to 13579
5. Click OK

### File Path Requirements
For playback to work, files must have valid paths stored in either:
- `local_files.path` (referenced via `mylist.local_file`)
- `mylist.storage`

## Error Handling

### Player Not Found
If the configured player executable doesn't exist:
- Error logged: "Playback error: Media player not found at: \<path\>"
- Returns false from `startPlayback()`

### File Not Found
If the file path doesn't exist:
- Error logged: "Playback error: File does not exist: \<path\>"
- Returns false from `startPlayback()`

### Web Interface Connection Failed
If unable to connect to player web interface:
- Retries for 5 consecutive failures
- After 5 failures, stops tracking
- Error logged: "Playback tracking stopped: Unable to connect to MPC-HC web interface"

### No Media Player Configured
If no player path is set:
- Error logged: "Playback error: Media player path not configured"
- Returns false from `startPlayback()`

## Future Enhancements

Potential improvements for future versions:
1. Support for additional media players (VLC, MPV, etc.)
2. Playback history view
3. Watch time statistics
4. Multiple file playback (playlist)
5. Keyboard shortcuts for play/pause/stop
6. In-app playback controls
7. Thumbnail preview on hover
8. Progress bar visualization in the tree
9. Auto-continue to next episode
10. Customizable completion threshold (currently hardcoded to 5 seconds before end)

## Testing

Test file: `tests/test_playbackmanager.cpp`

Tests included:
- Settings storage and retrieval
- Default path behavior
- Database integration

**Note:** Full integration testing requires Windows environment with MPC-HC installed.

## Code Files

- `usagi/src/playbackmanager.h` - PlaybackManager class declaration
- `usagi/src/playbackmanager.cpp` - PlaybackManager implementation
- `usagi/src/window.h` - Window class with playback UI elements
- `usagi/src/window.cpp` - Window class with playback handlers
- `usagi/src/anidbapi.cpp` - Database schema modifications
- `usagi/CMakeLists.txt` - Build configuration
- `tests/test_playbackmanager.cpp` - Unit tests

## Usage Example

1. Configure media player path in Settings tab
2. Navigate to MyList tab
3. Click the ▶ button next to any anime/episode/file
4. Player launches and starts playing
5. Playback position is automatically tracked
6. When playback completes, file is marked as watched
7. Checkmark (✓) appears in place of play button

## Known Limitations

1. Only supports MPC-HC media player currently
2. Requires web interface to be enabled in MPC-HC
3. Only tracks one playback session at a time
4. File path must be valid and accessible
5. No support for network/streaming URLs
6. No support for multi-file episodes
7. Progress tracking stops if player is closed
