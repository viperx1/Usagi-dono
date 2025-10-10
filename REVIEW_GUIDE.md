# Quick Start: Anime Tracking Feature Plan Review

This document provides a quick overview of the planning documents created for enhancing Usagi-dono's anime tracking functionality.

## Documents Created

### 1. FEATURE_PLAN_ANIME_TRACKING.md
**Main feature design document** - Read this first!

**Key Sections:**
- **Executive Summary**: High-level overview of the enhancement vision
- **Current State Analysis**: What exists now in Usagi-dono
- **MyList UI/UX Redesign**: Detailed mockups and interaction patterns
- **Playback Integration**: How to watch anime from within Usagi
- **Torrent Integration**: qBittorrent API integration for downloads
- **Smart File Organization**: Auto-organize downloaded files
- **Watch Status & Filtering**: MyAnimeList-style tracking
- **Notification System**: Stay informed about new episodes
- **Statistics & Insights**: Track your watching habits
- **Implementation Roadmap**: 7 phases from MVP to full feature set
- **Open Questions**: Items needing your input/decision

### 2. TECHNICAL_DESIGN_NOTES.md
**Technical implementation guide** - For developers implementing the features

**Key Sections:**
- **Database Migration Strategy**: How to add new tables safely
- **Code Architecture Patterns**: Class designs for key components
- **qBittorrent Web API**: Detailed API integration examples
- **File Organization**: Template system and matching algorithms
- **Performance Considerations**: Indexing, caching, lazy loading
- **Error Handling**: Graceful degradation patterns
- **Testing Strategies**: Unit and integration test approaches

## Quick Feature Summary

### What You Requested

✅ **MyList as Active Tracker/Playlist**
- Tree view showing anime with episodes nested underneath
- Visual indicators for watched/unwatched/next episode
- Right-click "Watch Now" on anime
- Expand anime to see episodes, click play on next one

✅ **qBittorrent Integration (Optional)**
- Direct control of qBittorrent from Usagi
- Monitor download progress
- Auto-organize completed downloads

✅ **Torrent Storage**
- Store .torrent files in database
- Quick redownloads without searching again

### What I Proposed (Beyond Original Request)

📺 **Enhanced Playback**
- Resume from where you left off
- Auto-mark as watched when 85% complete
- Prompt to play next episode

📁 **Smart File Organization**
- Auto-rename using AniDB data
- Customizable folder templates
- Fuzzy matching for downloaded files

📊 **Watch Status Tracking**
- Categories: Currently Watching, Plan to Watch, Completed, etc.
- Progress tracking (3/12 episodes watched)
- Personal ratings and notes

🔔 **Notifications**
- New episode aired
- Download completed
- Weekly watch summary

📈 **Statistics**
- Watch time tracking
- Genre preferences
- Completion rates

## Visual Mockup of Proposed UI

```
MyList Tab
├─ 📺 Currently Watching (3)
│  ├─ 📺 Anime Title 1 (EP 3/12 watched)
│  │  ├─ ✅ Episode 1 - "Title" [Watched] [/path/to/file.mkv]
│  │  ├─ ✅ Episode 2 - "Title" [Watched] [/path/to/file.mkv]
│  │  ├─ ▶️  Episode 3 - "Title" [Next Episode] [/path/to/file.mkv]  ← Highlighted
│  │  ├─ 📥 Episode 4 - "Title" [Downloading 45%]
│  │  └─ ⬇️  Episode 5 - "Title" [In Queue]
│  └─ 📺 Anime Title 2 (EP 1/24 watched)
├─ 📋 Plan to Watch (5)
├─ ✅ Completed (24)
└─ ⏸️ On Hold (2)

Right-click on "Anime Title 1" shows:
┌──────────────────────────────┐
│ ▶️  Watch Next Episode        │
│ 📋 View Anime Details         │
│ 🔄 Refresh from AniDB         │
│ 📥 Download Missing Episodes  │
│ 🗂️ Open Anime Folder          │
│ 🏷️ Change Watch Status        │
│ 🗑️ Remove from MyList         │
└──────────────────────────────┘

Right-click on "Episode 3" shows:
┌──────────────────────────────┐
│ ▶️  Play Episode              │
│ ⏯️ Resume (from 12:34)        │
│ ✅ Mark as Watched            │
│ 📋 Episode Details            │
│ 🗂️ Open File Location         │
│ 📊 File Info                  │
└──────────────────────────────┘
```

## Implementation Approach

### Phased Rollout (Recommended)

**Phase 1: Core Playback (MVP) - 2-3 weeks**
Goal: Basic watch & track functionality
- Enhanced tree view
- Play episodes with external player
- Track watched status
- "Watch next episode"

**Phase 2-3: File & Torrent Management - 4-5 weeks**
- Smart file organization
- qBittorrent integration
- Download monitoring

**Phase 4-7: Polish & Advanced - 5-7 weeks**
- UI enhancements
- Notifications
- Statistics
- Optimization

### Alternative: Ultra-Minimal MVP (1-2 weeks)

If the full plan seems too ambitious:
1. Show anime with episodes in tree view
2. Double-click episode → launch VLC
3. Context menu: "Mark as watched"
4. Highlight next unwatched episode
5. Done!

Then iterate from there based on feedback.

## Questions for You

Before implementation begins, please consider:

1. **Player Priority**: External player (VLC/MPV) sufficient, or want embedded player?

2. **Torrent Search**: Should Usagi include torrent search (Nyaa.si integration)?

3. **Cloud Sync**: Important feature or nice-to-have?

4. **MAL/AniList**: Should we support import/export with these services?

5. **Scope**: Start with minimal MVP or go for fuller feature set?

6. **UI Style**: The mockups shown - does this match your vision?

7. **Priority Features**: Which features are must-have vs nice-to-have?

## Next Steps

1. ✅ Review planning documents (you are here!)
2. ⏳ Provide feedback on features, priorities, open questions
3. ⏳ Decide on implementation approach (phased vs minimal MVP)
4. ⏳ Create detailed task breakdown for first phase
5. ⏳ Begin implementation

## Key Files to Review

1. **FEATURE_PLAN_ANIME_TRACKING.md** - Complete feature design (50+ pages)
2. **TECHNICAL_DESIGN_NOTES.md** - Technical implementation details (40+ pages)
3. **This file** - Quick overview and getting started guide

## Contact/Feedback

Please provide feedback on:
- Overall vision and direction
- Priority of different features
- Answers to open questions
- Timeline and scope preferences
- Any concerns or alternative ideas

---

**Created:** 2024
**Status:** Awaiting Review and Feedback
**Next:** Proceed to implementation after approval
