# System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           USAGI-DONO ARCHITECTURE                           │
│                          Enhanced Anime Tracker                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                              USER INTERFACE LAYER                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Hasher     │  │  MyList Tab  │  │  Downloads   │  │ Statistics   │  │
│  │     Tab      │  │  (Enhanced)  │  │     Tab      │  │     Tab      │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘  │
│         │                 │                  │                  │           │
│         │          ┌──────▼──────┐           │                  │           │
│         │          │ MyListWidget│           │                  │           │
│         │          │ (TreeView)  │           │                  │           │
│         │          └──────┬──────┘           │                  │           │
│         │                 │                  │                  │           │
│         │          ┌──────▼──────────┐       │                  │           │
│         │          │ Context Menus   │       │                  │           │
│         │          │ - Watch Now     │       │                  │           │
│         │          │ - Download      │       │                  │           │
│         │          │ - Mark Watched  │       │                  │           │
│         │          └─────────────────┘       │                  │           │
│         │                                    │                  │           │
└─────────┼────────────────────────────────────┼──────────────────┼───────────┘
          │                                    │                  │
          │                                    │                  │
┌─────────▼────────────────────────────────────▼──────────────────▼───────────┐
│                        APPLICATION LOGIC LAYER                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Playback   │  │   Torrent    │  │     File     │  │ Notification │  │
│  │   Manager    │  │   Manager    │  │  Organizer   │  │   Manager    │  │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  │
│         │                 │                  │                  │           │
│         │                 │                  │                  │           │
│  - Launch player    - Add torrents    - Rename files    - System tray     │
│  - Track position   - Monitor progress - Move files      - Notifications  │
│  - Mark watched     - Store .torrent   - Apply templates - Release alerts │
│  - Resume playback  - qBittorrent API  - Match anime     - Download done  │
│         │                 │                  │                  │           │
│         │                 │                  │                  │           │
│  ┌──────▼─────────────────▼──────────────────▼──────────────────▼───────┐ │
│  │                    MyListManager                                      │ │
│  │  - CRUD operations for MyList entries                                │ │
│  │  - Watch status management (Currently Watching, Completed, etc.)     │ │
│  │  - Progress tracking (3/12 episodes watched)                         │ │
│  │  - Filtering and sorting                                             │ │
│  └──────────────────────────────────────────┬─────────────────────────── │ │
│                                             │                             │
└─────────────────────────────────────────────┼─────────────────────────────┘
                                              │
                                              │
┌─────────────────────────────────────────────▼─────────────────────────────┐
│                          DATA ACCESS LAYER (DAO)                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  MyListDAO   │  │  AnimeDAO    │  │   FileDAO    │  │  EpisodeDAO  │  │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  │
│         │                 │                  │                  │           │
│         └─────────────────┴──────────────────┴──────────────────┘           │
│                                     │                                       │
│                              ┌──────▼──────┐                                │
│                              │   SQLite    │                                │
│                              │  Database   │                                │
│                              └──────┬──────┘                                │
│                                     │                                       │
└─────────────────────────────────────┼───────────────────────────────────────┘
                                      │
┌─────────────────────────────────────▼───────────────────────────────────────┐
│                              DATABASE SCHEMA                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐         │
│  │     mylist       │  │      anime       │  │      file        │         │
│  ├──────────────────┤  ├──────────────────┤  ├──────────────────┤         │
│  │ lid (PK)         │  │ aid (PK)         │  │ fid (PK)         │         │
│  │ fid              │  │ eptotal          │  │ aid              │         │
│  │ eid              │  │ nameromaji       │  │ eid              │         │
│  │ aid              │  │ nameenglish      │  │ size             │         │
│  │ viewed           │  │ year             │  │ ed2k             │         │
│  │ storage          │  │ type             │  │ filename         │         │
│  │ local_path (NEW) │  │ ...              │  │ local_path (NEW) │         │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘         │
│                                                                             │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐         │
│  │  anime_status    │  │playback_history  │  │stored_torrents   │         │
│  │      (NEW)       │  │      (NEW)       │  │      (NEW)       │         │
│  ├──────────────────┤  ├──────────────────┤  ├──────────────────┤         │
│  │ aid (PK)         │  │ id (PK)          │  │ id (PK)          │         │
│  │ status           │  │ eid              │  │ info_hash        │         │
│  │ user_rating      │  │ last_position    │  │ torrent_file     │         │
│  │ user_notes       │  │ completion_%     │  │ magnet_link      │         │
│  │ date_started     │  │ last_played      │  │ aid              │         │
│  │ ...              │  │ ...              │  │ eid              │         │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘         │
│                                                                             │
│  ┌──────────────────┐  ┌──────────────────┐                                │
│  │torrent_downloads │  │episode_releases  │                                │
│  │      (NEW)       │  │      (NEW)       │                                │
│  ├──────────────────┤  ├──────────────────┤                                │
│  │ id (PK)          │  │ id (PK)          │                                │
│  │ torrent_hash     │  │ aid              │                                │
│  │ status           │  │ eid              │                                │
│  │ progress         │  │ air_date         │                                │
│  │ download_speed   │  │ notified         │                                │
│  │ ...              │  │ ...              │                                │
│  └──────────────────┘  └──────────────────┘                                │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
          │                                            │
          │                                            │
┌─────────▼────────────────────────────────────────────▼─────────────────────┐
│                       EXTERNAL SERVICES LAYER                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   AniDB      │  │ qBittorrent  │  │ Video Player │  │   System     │  │
│  │  UDP API     │  │   Web API    │  │ (VLC/MPV/    │  │ Notifications│  │
│  │              │  │              │  │   MPC-HC)    │  │   (Qt)       │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘  │
│         │                 │                  │                  │           │
│         │                 │                  │                  │           │
│  - AUTH/LOGIN       - Add torrent     - Play file      - System tray      │
│  - MYLISTADD        - Get status      - Resume from    - Toast messages   │
│  - FILE             - Pause/Resume      position       - Sound alerts     │
│  - ANIME            - Delete          - Track progress                    │
│                     - Monitor queue                                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════

                              DATA FLOW EXAMPLES

─────────────────────────────────────────────────────────────────────────────
  User clicks "Watch Next Episode" on anime
─────────────────────────────────────────────────────────────────────────────

  1. MyListWidget emits playNextEpisodeRequested(aid)
                    ↓
  2. PlaybackManager::playNextEpisode(aid)
     - Query database for next unwatched episode
     - Check if file exists locally
                    ↓
  3a. If file exists:
     - Launch external player with file path
     - Create playback_history entry
     - Monitor player process
     - On completion: mark as watched
                    ↓
  3b. If file missing:
     - Show download dialog
     - User provides torrent/magnet link
                    ↓
  4. TorrentManager::addTorrent(magnetLink, aid, eid)
     - Send to qBittorrent Web API
     - Create torrent_downloads entry
     - Start monitoring progress
                    ↓
  5. On download completion:
     - FileOrganizer::organizeFile(path, aid, eid)
     - Rename/move file using template
     - Update mylist with local_path
     - Emit notification
                    ↓
  6. User clicks "Watch" again → goes to step 3a

─────────────────────────────────────────────────────────────────────────────
  New file added via Hasher
─────────────────────────────────────────────────────────────────────────────

  1. User adds file in Hasher tab
                    ↓
  2. ED2K hash calculated
                    ↓
  3. AniDB FILE query
     - Retrieves aid, eid, anime name, episode info
                    ↓
  4. If "auto-organize" enabled:
     FileOrganizer::organizeFile(path, aid, eid)
     - Apply naming template
     - Move to organized location
                    ↓
  5. If "add to mylist" enabled:
     AniDB MYLISTADD
                    ↓
  6. Update local database:
     - Insert/update file table
     - Insert/update mylist table with local_path
                    ↓
  7. MyListWidget::refresh()
     - Show new entry in tree
     - Update episode count

─────────────────────────────────────────────────────────────────────────────
  Periodic torrent status refresh
─────────────────────────────────────────────────────────────────────────────

  1. TorrentManager timer (every 5 seconds)
                    ↓
  2. Query qBittorrent Web API
     - Get all torrents
                    ↓
  3. For each torrent:
     - Update torrent_downloads table
     - Emit downloadProgress signal
                    ↓
  4. If status changed to "completed":
     - Emit downloadCompleted signal
     - NotificationManager shows toast
     - FileOrganizer organizes file
     - Update mylist with local_path
                    ↓
  5. MyListWidget receives signal
     - Update tree item icon (📥 → ✅)
     - Update progress text

═══════════════════════════════════════════════════════════════════════════════

                           COMPONENT INTERACTIONS

┌───────────────┐         signals          ┌────────────────┐
│  MyListWidget ├────────────────────────► │ PlaybackManager│
└───────────────┘                          └────────────────┘
       │                                            │
       │ signals                                    │ signals
       ▼                                            ▼
┌───────────────┐                          ┌────────────────┐
│ TorrentManager│                          │ MyListManager  │
└───────┬───────┘                          └────────┬───────┘
        │                                           │
        │ signals                                   │ database
        ▼                                           ▼
┌───────────────┐                          ┌────────────────┐
│Notification   │◄──────signals───────────►│ SQLite Database│
│   Manager     │                          └────────────────┘
└───────────────┘                                   ▲
                                                    │
                                            database queries
                                                    │
                                           ┌────────┴───────┐
                                           │  FileOrganizer │
                                           └────────────────┘

All components communicate via Qt signals/slots for loose coupling

═══════════════════════════════════════════════════════════════════════════════
```

## Key Architectural Principles

### 1. Separation of Concerns
- **UI Layer**: Only handles display and user interaction
- **Logic Layer**: Business logic, workflow coordination
- **Data Layer**: Database operations, queries
- **External Layer**: Third-party integrations

### 2. Signal-Slot Communication
- Components communicate via Qt signals/slots
- Loose coupling, easy to test
- Event-driven architecture

### 3. Database-Centric
- SQLite as single source of truth
- All state persisted
- Transactional operations

### 4. Manager Pattern
- Each major feature has a Manager class
- Manager coordinates between UI and data
- Encapsulates external service interaction

### 5. Extensibility
- Easy to add new features (e.g., MAL sync)
- Plugin-like architecture for players, torrent clients
- Template system for file organization

### 6. Error Handling
- Graceful degradation
- User-friendly error messages
- Retry logic for network operations

### 7. Performance
- Lazy loading (episodes loaded on expand)
- Database indexing
- Caching frequently accessed data
- Background operations (torrent monitoring)

---

**See also:**
- FEATURE_PLAN_ANIME_TRACKING.md - Detailed feature specs
- TECHNICAL_DESIGN_NOTES.md - Implementation details
- REVIEW_GUIDE.md - Quick start guide
