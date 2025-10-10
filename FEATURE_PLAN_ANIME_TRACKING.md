# Usagi-dono Feature Enhancement Plan: Anime Tracking & Playback

## Executive Summary

This document outlines a comprehensive plan to enhance Usagi-dono from a basic AniDB client into a full-featured anime tracker and playback manager. The core goal is to make MyList function as an active playlist/tracker for currently watching anime, with seamless integration for downloading, organizing, and playing anime files.

## Current State Analysis

### Existing Functionality
- Basic AniDB API integration (auth, mylist add, file queries)
- ED2K file hashing
- SQLite database with tables for: mylist, anime, file, episode, group
- Qt6-based GUI with tabs: Hasher, Mylist, Notify, Settings, Log, ApiTester
- Basic mylist viewing via QTreeWidget

### Database Schema (Existing)
```sql
mylist: lid, fid, eid, aid, gid, date, state, viewed, viewdate, storage, source, other, filestate
anime: aid, eptotal, eplast, year, type, relaidlist, relaidtype, category, nameromaji, namekanji, nameenglish, nameother, nameshort, synonyms
file: fid, aid, eid, gid, lid, othereps, isdepr, state, size, ed2k, md5, sha1, crc, quality, source, codec_audio, bitrate_audio, codec_video, bitrate_video, resolution, filetype, lang_dub, lang_sub, length, description, airdate, filename
episode: eid, name, nameromaji, namekanji, rating, votecount
group: gid, name, shortname
```

## Feature Design: Enhanced MyList as Active Tracker

### 1. MyList UI/UX Redesign

#### 1.1 Hierarchical Tree View
**Current:** Basic QTreeWidget with minimal functionality
**Proposed:** Multi-level tree structure

```
📁 Currently Watching
  📺 Anime Title 1 (EP 3/12 watched)
    ✅ Episode 1 - "Title" [Watched] [File: /path/to/file]
    ✅ Episode 2 - "Title" [Watched] [File: /path/to/file]
    ▶️  Episode 3 - "Title" [Next Episode] [File: /path/to/file]
    📥 Episode 4 - "Title" [Downloading 45%]
    ⬇️  Episode 5 - "Title" [In Queue]
    ⚠️  Episode 6 - "Title" [Not Available]
  📺 Anime Title 2 (EP 1/24 watched)
    
📁 Plan to Watch
📁 Completed
📁 On Hold
📁 Dropped
```

**Visual Indicators:**
- ✅ Watched episode (green checkmark)
- ▶️ Next unwatched episode (play icon, highlighted)
- 📥 Currently downloading (progress indicator)
- ⬇️ Queued for download (download icon)
- ⚠️ Missing/not available (warning icon)
- 🎬 Currently playing (dynamic indicator)

#### 1.2 Context Menu Actions

**Right-click on Anime (parent node):**
- ▶️ Watch Next Episode
- 📋 View Anime Details
- 🔄 Refresh from AniDB
- 📥 Download Missing Episodes
- 🗂️ Open Anime Folder
- 🏷️ Change Watch Status (Currently Watching, Plan to Watch, etc.)
- ⭐ Set Priority (High/Medium/Low)
- 🔖 Add Notes/Tags
- 🗑️ Remove from MyList

**Right-click on Episode (child node):**
- ▶️ Play Episode
- ▶️ Play from Beginning
- ⏯️ Resume (if partially watched)
- ✅ Mark as Watched
- ❌ Mark as Unwatched
- 📥 Download Episode
- 📋 Episode Details
- 🗂️ Open File Location
- 📊 File Info (quality, codec, etc.)
- 🔄 Re-download (from stored torrent)
- 🗑️ Remove from Disk

#### 1.3 Double-Click Behavior
- **Double-click anime:** Expand/collapse episode list
- **Double-click episode:** Play episode (if file exists) or open download dialog

#### 1.4 Visual Enhancements
- Color-coding by watch status
- Progress bars for partially watched episodes
- Thumbnail/poster art for anime (optional, cached from AniDB)
- Episode air date display
- File size and quality indicators
- Badge for new episodes available

### 2. Playback Integration

#### 2.1 Video Player Options

**Option A: External Player (Recommended for MVP)**
- Integration with existing video players (VLC, MPV, MPC-HC)
- Configure preferred player in settings
- Pass file path and subtitle options
- Track playback time via file modification or player API
- Resume functionality by storing last played position

**Option B: Embedded Player (Future Enhancement)**
- Integrate Qt Multimedia or libVLC
- In-app playback with custom controls
- Direct integration with watch progress
- Picture-in-picture mode
- Pros: Better integration, seamless UX
- Cons: More complex, larger binary, codec dependencies

**Recommended Implementation:**
```
Settings → Playback
[x] Use external player
Player path: [/path/to/vlc.exe] [Browse...]
Arguments: [--fullscreen --sub-file=]
[ ] Embedded player (coming soon)

[x] Automatically mark as watched after 85% completion
[x] Prompt to play next episode after completion
[x] Resume from last position
```

#### 2.2 Playback Tracking

**Database Schema Addition:**
```sql
CREATE TABLE playback_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    eid INTEGER,
    fid INTEGER,
    last_position INTEGER,  -- seconds
    duration INTEGER,       -- total seconds
    last_played TIMESTAMP,
    play_count INTEGER DEFAULT 0,
    FOREIGN KEY (eid) REFERENCES episode(eid),
    FOREIGN KEY (fid) REFERENCES file(fid)
);
```

**Features:**
- Track when each episode was last played
- Store playback position for resume
- Count number of plays
- Calculate completion percentage
- Auto-mark as watched at configurable threshold (default 85%)

#### 2.3 "Watch Now" Workflow

```
User clicks "Watch Next Episode" on anime
  ↓
1. Identify next unwatched episode
  ↓
2. Check if file exists locally
  ↓
3a. If exists:
    - Launch player with file path
    - Update last_played timestamp
    - Create playback_history entry
    - Monitor for completion
  ↓
3b. If missing:
    - Show download dialog
    - Offer to download from stored torrent
    - Or search for torrent
    - Queue for download
  ↓
4. After playback:
    - Update watch status if > 85% watched
    - Show "Play Next Episode?" dialog
    - Increment episode counter
```

### 3. Torrent Integration

#### 3.1 qBittorrent Web API Integration

**Why qBittorrent:**
- Popular, open-source torrent client
- Well-documented Web API (REST)
- Supports remote control
- Cross-platform
- No licensing issues

**API Capabilities:**
- Add torrents (by file, magnet link, or URL)
- Monitor download progress
- Control torrent state (pause, resume, delete)
- Get torrent info (progress, speed, ETA)
- Set download location
- Manage categories/tags

**Implementation Steps:**

1. **Settings Configuration:**
```
Settings → qBittorrent Integration
[x] Enable qBittorrent integration
Host: [localhost] Port: [8080]
Username: [admin]
Password: [•••••••]
[Test Connection]

Default download path: [/path/to/anime/] [Browse...]
[x] Auto-organize by anime name
[x] Auto-import completed downloads to MyList
```

2. **Database Schema Addition:**
```sql
CREATE TABLE torrent_downloads (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    torrent_hash TEXT UNIQUE,
    aid INTEGER,
    eid INTEGER,
    torrent_name TEXT,
    download_path TEXT,
    status TEXT,  -- downloading, completed, paused, error
    progress REAL,  -- 0.0 to 1.0
    download_speed INTEGER,
    upload_speed INTEGER,
    eta INTEGER,  -- seconds
    added_date TIMESTAMP,
    completed_date TIMESTAMP,
    FOREIGN KEY (aid) REFERENCES anime(aid),
    FOREIGN KEY (eid) REFERENCES episode(eid)
);
```

3. **UI Integration:**
   - Download manager tab/panel
   - Progress indicators in MyList tree
   - Notification on download completion
   - Auto-refresh MyList when download completes

#### 3.2 Torrent Storage & Management

**Torrent Metadata Storage:**
```sql
CREATE TABLE stored_torrents (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    torrent_file BLOB,  -- .torrent file contents
    magnet_link TEXT,
    info_hash TEXT UNIQUE,
    aid INTEGER,
    eid INTEGER,  -- NULL for batch/season torrents
    name TEXT,
    size INTEGER,
    file_count INTEGER,
    uploader TEXT,
    tracker TEXT,
    added_date TIMESTAMP,
    FOREIGN KEY (aid) REFERENCES anime(aid),
    FOREIGN KEY (eid) REFERENCES episode(eid)
);
```

**Benefits:**
- Quick re-downloads without searching again
- Maintain seeding history
- Support for both individual episodes and batch torrents
- Version tracking (different fansub groups, qualities)

**Features:**
- Automatic storage when adding to MyList via hasher
- Manual torrent import
- Torrent search integration (Nyaa.si, etc.)
- Smart matching: associate torrents with anime/episodes

#### 3.3 Download Manager Tab

**UI Layout:**
```
Downloads Tab
┌─────────────────────────────────────────────────────┐
│ [+Add Torrent] [⏸Pause All] [▶️Resume All] [🔄Refresh]│
├─────────────────────────────────────────────────────┤
│ Name                    | Progress | Speed | ETA    │
│ Anime.S01E03.1080p.mkv | ████░░ 45% | 5MB/s | 2m │
│ Anime.S01E04.1080p.mkv | [Queued]   | -     | -  │
├─────────────────────────────────────────────────────┤
│ Details:                                            │
│   Anime: [Anime Title]                             │
│   Episode: 3                                        │
│   Size: 1.2 GB                                      │
│   Peers: 45 (23 seeds)                              │
│   Downloaded: 540 MB                                │
│   Upload: 120 MB                                    │
└─────────────────────────────────────────────────────┘
```

**Context Menu:**
- Pause/Resume
- Delete torrent (with/without files)
- Open download folder
- Copy magnet link
- Force re-check
- Set priority

### 4. Smart File Organization

#### 4.1 Automatic File Management

**Post-Download Actions:**
```
Settings → File Management
[x] Auto-organize downloaded files
Anime folder structure: 
    [Anime Name]/Season [N]/[Anime Name] - S[XX]E[YY] - [Episode Title].mkv

[x] Auto-rename using AniDB data
[x] Move completed downloads to organized location
[x] Keep original filename as alternate
[x] Create symlinks to original location

[x] Auto-add to MyList after download
[x] Auto-hash for AniDB verification
```

**Folder Templates:**
- `{anime_name}/{anime_name} - E{episode:02d} - {episode_title}.{ext}`
- `{anime_name}/Season {season}/{anime_name} - S{season:02d}E{episode:02d}.{ext}`
- `{anime_name_romaji}/{year}/[{group}] {anime_name} - {episode:02d}.{ext}`

#### 4.2 Smart Matching

**Challenges:**
- Downloaded files may not match AniDB exactly
- Different naming conventions (fansubs)
- Batch vs. individual episodes

**Solution: Fuzzy Matching Engine**
1. Parse filename for anime name, episode number, quality
2. Query AniDB database for similar titles
3. Offer suggestions to user
4. Learn from user corrections
5. Store mapping rules

```sql
CREATE TABLE filename_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    pattern TEXT,  -- regex or glob pattern
    aid INTEGER,
    confidence REAL,
    user_verified BOOLEAN,
    FOREIGN KEY (aid) REFERENCES anime(aid)
);
```

### 5. Enhanced Anime Information Display

#### 5.1 Anime Details Panel

**Triggered by:** Right-click → View Details, or double-click anime

**Display:**
```
┌─────────────────────────────────────┐
│ [Poster]  Anime Title (Romaji)    │
│  Image    Alternative Titles        │
│           English: Title            │
│           Japanese: タイトル         │
│                                     │
│ Type: TV Series                     │
│ Episodes: 12                        │
│ Status: Airing (3/12)              │
│ Year: 2024                          │
│ Season: Fall 2024                   │
│                                     │
│ Genres: Action, Drama, Fantasy      │
│ Rating: 8.5/10 (AniDB)             │
│                                     │
│ Synopsis:                           │
│ [Description text...]               │
│                                     │
│ Your Progress: 3/12 episodes       │
│ Your Rating: [★★★★☆]               │
│ Your Notes: [Text area]            │
│                                     │
│ [View on AniDB] [Update Info]      │
└─────────────────────────────────────┘
```

#### 5.2 Episode Details

**Triggered by:** Right-click episode → Episode Details

```
Episode 3: "Episode Title"
─────────────────────────────
Aired: 2024-01-15
Duration: 24m
Rating: 8.3/10

Synopsis:
[Episode description]

Files:
  ✅ [Group] Anime - 03 [1080p].mkv
     Size: 1.2 GB
     Quality: 1080p | x264 | AAC
     Languages: Japanese [ja] | English Sub [en]
     Added: 2024-01-16

Playback History:
  Last played: 2024-01-16 20:30
  Watch count: 1
  Completion: 100%
```

### 6. Watch Status & Filtering

#### 6.1 Status Categories

Similar to MyAnimeList/AniList:
- 📺 **Currently Watching** (actively watching)
- 📋 **Plan to Watch** (want to watch)
- ✅ **Completed** (finished all episodes)
- ⏸️ **On Hold** (paused, will resume later)
- ❌ **Dropped** (abandoned)
- 🔄 **Rewatching** (watching again)

#### 6.2 MyList Filtering & Sorting

**Filter Options:**
```
MyList Tab
┌──────────────────────────────────────┐
│ Filter: [All ▼] Sort: [Last Watched ▼] │
│ Search: [_______________] [🔍]        │
├──────────────────────────────────────┤
│ [Tree view]                          │
└──────────────────────────────────────┘
```

**Filter by:**
- Watch status
- Has unwatched episodes
- Has missing episodes
- Currently downloading
- Year/Season
- Genre
- Rating (your rating or AniDB)

**Sort by:**
- Last watched (default for "Currently Watching")
- Alphabetical
- Next episode air date
- Date added
- Progress percentage
- Priority

### 7. Notification System

#### 7.1 Event Notifications

**Notification Types:**
- 📥 Download completed
- ✅ Episode marked as watched
- 🆕 New episode available (AniDB check)
- ⚠️ Download failed/stalled
- 🎬 Playback resumed
- 📊 Weekly watch summary

**Implementation:**
- System tray notifications (Qt QSystemTrayIcon)
- In-app notification panel (existing Notify tab enhancement)
- Optional sound effects
- Configurable notification preferences

#### 7.2 Episode Release Tracking

```sql
CREATE TABLE episode_releases (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    aid INTEGER,
    eid INTEGER,
    air_date TIMESTAMP,
    notified BOOLEAN DEFAULT 0,
    FOREIGN KEY (aid) REFERENCES anime(aid),
    FOREIGN KEY (eid) REFERENCES episode(eid)
);
```

**Features:**
- Check AniDB for new episodes on schedule
- Notify when new episode airs
- Auto-search for torrents (optional)
- Add to download queue (optional)

### 8. Statistics & Insights

#### 8.1 Watch Statistics Panel

**New tab: "Statistics"**

```
Your Anime Stats
─────────────────
Total Anime: 42
Total Episodes: 854
Episodes Watched: 612 (71.7%)

Watch Time: 245 hours (10.2 days)

This Week: 12 episodes (4.8 hours)
This Month: 48 episodes (19.2 hours)

Most Watched Genres:
  1. Action (142 episodes)
  2. Comedy (98 episodes)
  3. Drama (87 episodes)

Completion Rate: 78%

Currently Watching: 8 anime
Completed: 24 anime
Dropped: 3 anime
```

#### 8.2 Progress Charts

- Watch history timeline
- Episodes per day/week/month graph
- Completion rate over time
- Genre distribution pie chart

### 9. Advanced Features (Future)

#### 9.1 Multi-User Support
- Multiple user profiles
- Separate MyLists
- Shared anime library
- Individual watch progress

#### 9.2 Cloud Sync
- Sync MyList across devices
- Cloud backup of settings
- Integration with MAL/AniList APIs
- Import/export MyList

#### 9.3 Social Features
- Share watch status
- Friend recommendations
- Watch parties (synchronized playback)

#### 9.4 Smart Recommendations
- Based on watch history
- AniDB related anime
- Genre preferences
- Seasonal anime suggestions

#### 9.5 Subtitle Management
- Auto-download subtitles
- Multiple subtitle tracks
- Subtitle search integration
- Font/size/position customization

#### 9.6 Batch Operations
- Bulk download episodes
- Batch mark as watched
- Multi-anime status change
- Batch torrent operations

## Technical Architecture

### 10.1 Component Structure

```
┌─────────────────────────────────────┐
│           UI Layer (Qt6)            │
│  Window, MyListWidget, DetailsPanel │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│      Application Logic Layer        │
│  PlaybackManager, TorrentManager,   │
│  FileOrganizer, NotificationManager │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│       Data Access Layer (DAO)       │
│  MyListDAO, AnimeDAO, FileDAO       │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│     External Service Layer          │
│  AniDBApi, qBittorrentApi,          │
│  VideoPlayerApi                     │
└─────────────────────────────────────┘
```

### 10.2 New Classes/Modules

**PlaybackManager**
- Launch video player
- Track playback position
- Monitor completion
- Update watch status

**TorrentManager**
- Interface with qBittorrent Web API
- Monitor download progress
- Manage torrent queue
- Store torrent metadata

**FileOrganizer**
- Rename/move files
- Apply naming templates
- Create folder structure
- Verify file integrity

**NotificationManager**
- Queue notifications
- Display system tray alerts
- Manage notification settings
- Log notification history

**MyListManager**
- CRUD operations for MyList entries
- Status management
- Progress tracking
- Smart filtering/sorting

### 10.3 Database Schema Enhancements

**New Tables:**
```sql
-- User watch status
CREATE TABLE anime_status (
    aid INTEGER PRIMARY KEY,
    status TEXT,  -- watching, plan_to_watch, completed, on_hold, dropped
    priority INTEGER,
    user_rating REAL,
    user_notes TEXT,
    date_started TIMESTAMP,
    date_completed TIMESTAMP,
    FOREIGN KEY (aid) REFERENCES anime(aid)
);

-- Playback history
CREATE TABLE playback_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    eid INTEGER,
    fid INTEGER,
    last_position INTEGER,
    duration INTEGER,
    last_played TIMESTAMP,
    play_count INTEGER DEFAULT 0,
    completion_percent REAL,
    FOREIGN KEY (eid) REFERENCES episode(eid),
    FOREIGN KEY (fid) REFERENCES file(fid)
);

-- Torrent downloads
CREATE TABLE torrent_downloads (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    torrent_hash TEXT UNIQUE,
    aid INTEGER,
    eid INTEGER,
    torrent_name TEXT,
    download_path TEXT,
    status TEXT,
    progress REAL,
    download_speed INTEGER,
    upload_speed INTEGER,
    eta INTEGER,
    added_date TIMESTAMP,
    completed_date TIMESTAMP,
    FOREIGN KEY (aid) REFERENCES anime(aid),
    FOREIGN KEY (eid) REFERENCES episode(eid)
);

-- Stored torrents
CREATE TABLE stored_torrents (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    torrent_file BLOB,
    magnet_link TEXT,
    info_hash TEXT UNIQUE,
    aid INTEGER,
    eid INTEGER,
    name TEXT,
    size INTEGER,
    file_count INTEGER,
    uploader TEXT,
    tracker TEXT,
    added_date TIMESTAMP,
    last_used TIMESTAMP,
    FOREIGN KEY (aid) REFERENCES anime(aid),
    FOREIGN KEY (eid) REFERENCES episode(eid)
);

-- Episode releases
CREATE TABLE episode_releases (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    aid INTEGER,
    eid INTEGER,
    air_date TIMESTAMP,
    notified BOOLEAN DEFAULT 0,
    FOREIGN KEY (aid) REFERENCES anime(aid),
    FOREIGN KEY (eid) REFERENCES episode(eid)
);

-- Filename mappings
CREATE TABLE filename_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    pattern TEXT,
    aid INTEGER,
    confidence REAL,
    user_verified BOOLEAN DEFAULT 0,
    created_date TIMESTAMP,
    FOREIGN KEY (aid) REFERENCES anime(aid)
);

-- Settings enhancements
-- (Add to existing settings table)
-- playback.external_player_path
-- playback.external_player_args
-- playback.auto_mark_watched_threshold
-- playback.prompt_next_episode
-- qbittorrent.enabled
-- qbittorrent.host
-- qbittorrent.port
-- qbittorrent.username
-- qbittorrent.password
-- file_management.auto_organize
-- file_management.folder_template
-- notification.enabled
-- notification.sound_enabled
```

## Implementation Roadmap

### Phase 1: Core Playback (MVP)
**Goal:** Basic watch tracking and playback
**Duration:** 2-3 weeks

1. ✅ Design plan (this document)
2. Enhanced MyList tree view
3. Episode list display under anime
4. External player integration
5. Basic playback tracking
6. Mark as watched functionality
7. "Watch next episode" feature

**Deliverable:** Can browse MyList, play episodes with external player, track watch progress

### Phase 2: File Organization
**Goal:** Smart file management
**Duration:** 2 weeks

1. File organizer implementation
2. Naming templates
3. Auto-rename/move functionality
4. Smart filename matching
5. Integration with hasher tab

**Deliverable:** Downloaded files automatically organized and added to MyList

### Phase 3: qBittorrent Integration
**Goal:** Direct torrent management
**Duration:** 2-3 weeks

1. qBittorrent Web API client
2. Download manager tab
3. Progress tracking
4. Auto-import on completion
5. Torrent metadata storage

**Deliverable:** Can download torrents directly from Usagi, monitor progress, auto-organize

### Phase 4: Enhanced UI/UX
**Goal:** Polish and usability
**Duration:** 2 weeks

1. Anime details panel
2. Episode details view
3. Context menus
4. Visual enhancements (icons, colors, progress bars)
5. Filtering and sorting
6. Search functionality

**Deliverable:** Beautiful, intuitive UI with rich information display

### Phase 5: Notifications & Automation
**Goal:** Proactive features
**Duration:** 1-2 weeks

1. Notification system
2. System tray integration
3. Episode release tracking
4. Auto-download new episodes (optional)

**Deliverable:** Notified of new episodes, downloads, completions

### Phase 6: Statistics & Advanced Features
**Goal:** Power user features
**Duration:** 2-3 weeks

1. Statistics panel
2. Watch history charts
3. Batch operations
4. Advanced filtering
5. Export/import functionality

**Deliverable:** Comprehensive tracking and bulk operations

### Phase 7: Polish & Optimization
**Goal:** Production ready
**Duration:** 1-2 weeks

1. Performance optimization
2. Bug fixes
3. User testing
4. Documentation
5. Tutorial/help system

**Deliverable:** Stable, documented, user-friendly application

## Conclusion

This plan transforms Usagi-dono from a basic AniDB client into a comprehensive anime management system. Key innovations:

1. **Active Tracker:** MyList becomes a living playlist, not just a static list
2. **Seamless Playback:** One-click access to next episode
3. **Integrated Downloads:** Direct qBittorrent control without leaving app
4. **Smart Organization:** Automatic file management based on AniDB data
5. **Rich Information:** Detailed anime/episode info at your fingertips
6. **Notifications:** Stay informed about new episodes and downloads

The phased approach allows for incremental development, with each phase delivering tangible value. The MVP (Phase 1) can be completed relatively quickly, providing core functionality, while later phases add polish and power-user features.

**Next Steps:**
1. Review and refine this plan
2. Get user feedback on priorities
3. Create detailed task breakdown for Phase 1
4. Begin implementation

## Open Questions for Discussion

1. **Player Choice:** Should we prioritize embedded player or is external player sufficient?
2. **Torrent Search:** Should Usagi include torrent search (Nyaa.si integration)?
3. **Subtitle Handling:** How important is automatic subtitle management?
4. **Cloud Sync:** Is this a must-have or nice-to-have?
5. **MAL/AniList Integration:** Should we support import/export with these services?
6. **Batch Torrents:** How should we handle season packs (multiple episodes in one torrent)?
7. **Multiple Versions:** How to manage multiple files per episode (different groups/qualities)?
8. **Streaming:** Any interest in streaming support (watching while downloading)?

## Alternative Approaches

### Simpler MVP Approach
If the full plan seems too ambitious:

**Ultra-Minimal MVP:**
1. Tree view: Show anime with episodes nested underneath
2. Double-click episode → launch VLC/MPV
3. Context menu: "Mark as watched"
4. Highlight next unwatched episode
5. Done.

**Pros:** Fast to implement, proves concept
**Cons:** Limited functionality, no downloads, no automation

### Integration Alternatives

**Instead of qBittorrent:**
- Generic torrent client (via command line)
- No integration (manual workflow)
- Web browser integration (open torrent sites)

**Instead of External Player:**
- Direct OS file association (double-click opens default player)
- Simple: no player integration needed
- Trade-off: no playback tracking

## References & Resources

### APIs & Documentation
- AniDB UDP API: https://wiki.anidb.net/UDP_API_Definition
- qBittorrent Web API: https://github.com/qbittorrent/qBittorrent/wiki/WebUI-API-(qBittorrent-4.1)
- Qt6 Documentation: https://doc.qt.io/qt-6/
- Nyaa.si (torrent site): https://nyaa.si/

### Similar Projects (for inspiration)
- Taiga (Windows anime tracker)
- Shoko Anime (anime library manager)
- Plex/Jellyfin (media servers with anime support)

### Technologies
- Qt6 (GUI framework) - already in use
- SQLite (database) - already in use
- QNetworkAccessManager (HTTP client for qBittorrent API)
- QProcess (external player launching)
- QSystemTrayIcon (notifications)

---

**Document Version:** 1.0
**Author:** AI Assistant
**Date:** 2024
**Status:** Draft for Review
