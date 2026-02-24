# Design Analysis: Deletion Candidate Visualization

## Problem Statement

The current automatic file deletion system selects files for deletion based on a multi-factor scoring algorithm in `WatchSessionManager`, but provides no visibility into:
- Which files are candidates for deletion
- Why a file was chosen (score breakdown)
- When deletion will happen (proximity to threshold)

This makes it difficult to tune the system and leads to unexpected deletions of files the user wants to keep.

## Current Scoring Factors (for reference)

Each file starts at score 50 and is adjusted by:

| Factor | Adjustment | Notes |
|--------|-----------|-------|
| Hidden card | -50 | Anime not visible in current filter |
| Active session | +100 | Currently watching this series |
| In ahead buffer | +75 | Episode within keep-ahead window |
| Already watched | -5 | File has been viewed |
| Not watched | +50 | Protects unwatched files |
| Distance from current | -1 per episode | Primary factor; further = more deletable |
| Older revision | -1000 per newer version | Strongly favors deleting old versions |
| Preferred audio | +30 / -40 | Language preference match |
| Preferred subtitle | +20 / -20 | Subtitle preference match |
| Higher quality | +25 / -35 | Quality tier comparison |
| High/low anime rating | +15 / -15 | AniDB rating thresholds (800+/600-) |
| Active/stalled/disbanded group | +20 / -10 / -25 | Release group status |
| Bitrate deviation | -10 to -40 | Only with multiple files per episode |
| Codec age | +10 / -15 / -30 | Modern, old, or ancient codec |

Gap protection prevents deleting files that would create holes in episode sequences.

Lower score = higher deletion priority.

---

## Option 1: Inline Indicators on Anime Cards (Embedded in MyList)

### Description
Add visual markers directly to the episode/file tree within each `AnimeCard`. Files that are deletion candidates get a colored indicator (icon, background highlight, or badge) next to the file entry in the episode tree. The score and breakdown are shown in the tooltip or on click.

### Mockup
```
┌──────────────────────────────────┐
│ Poster       │ Title             │
│              │ Type: TV          │
│              │ Rating: 8.76      │
│              │ 🟢 Safe (score 95)│  ← card-level summary
├──────────────────────────────────┤
│ ▶ Ep 1 - Intro                  │
│   \ v1 1080p HEVC [Group] 🟢   │  ← green = safe
│ ▶ Ep 2 - Adventure              │
│   \ v1 720p AVC [Group]  🟡    │  ← yellow = at risk
│ ▶ Ep 3 - Battle                 │
│   \ v1 480p XviD [Group] 🔴    │  ← red = deletion imminent
│   \ v2 1080p HEVC [Group] 🟢   │  ← green = newer version safe
└──────────────────────────────────┘

Tooltip on 🔴:
  Score: -42
  ────────────────
  Base:               50
  Already watched:    -5
  Distance (47 eps): -47
  Ancient codec:     -30
  Low quality:       -35
  Preferred audio:   +30
  Not preferred sub: -20
  ────────────────
  Gap protected: No
```

### Pros
- **Zero navigation cost**: visible right where the user already looks at their collection
- **Per-file granularity**: shows exactly which files are at risk, not just which anime
- **Contextual**: user sees the deletion risk alongside episode info, watch state, and quality metadata that are already displayed
- **Low UI disruption**: reuses existing episode tree widget; only adds an icon column or background color
- **Tooltip provides detail on demand**: score breakdown without cluttering the card

### Cons
- **Visual noise**: adds color/icons to every file row even when user is not thinking about deletion
- **Limited space**: anime cards are 600×450px; adding another column to the tree compresses existing info
- **Mixed concerns**: the card's primary purpose is episode browsing and playback; deletion info may feel out of place
- **Filtering difficulty**: no easy way to see "all files at risk" across the entire collection
- **Performance**: recalculating scores for all visible files on every card refresh could add overhead in the virtual flow layout

### Implementation Complexity
**Low-Medium**. Requires:
- New column or icon in `AnimeCard::addEpisode()` file items
- Score calculation call per file in `MyListCardManager::loadEpisodesForCardFromCache()`
- Tooltip builder for score breakdown
- Color thresholds (green/yellow/red) configurable in settings

---

## Option 2: Dedicated "Deletion" Tab

### Description
Add a new top-level tab (alongside MyList, Hasher, Settings, etc.) that shows a sorted table of all files ranked by deletion score. This tab is the single place for reviewing, overriding, and understanding deletion decisions.

### Mockup
```
[Hasher] [MyList] [Deletion] [Settings] [Log]
                      ▲ active

┌──────────────────────────────────────────────────────────┐
│ Deletion Candidates                    Space: 42/500 GB  │
│ Threshold: 50 GB │ Mode: Auto │ [▶ Run Now] [⏸ Pause]  │
├──────┬──────────────┬──────────┬────────┬───────────────┤
│Score │ File         │ Anime    │ Ep     │ Reason        │
├──────┼──────────────┼──────────┼────────┼───────────────┤
│ -942 │ show-01.avi  │ Show A   │ Ep 1   │ Old revision  │
│  -72 │ anime-03.mkv │ Anime B  │ Ep 3   │ Far, watched  │
│  -45 │ movie.mp4    │ Movie C  │ Ep 1   │ Ancient codec │
│  ...                                                     │
│  +95 │ new-01.mkv   │ New D    │ Ep 1   │ Active, unwat │
├──────────────────────────────────────────────────────────┤
│ ☑ Score breakdown panel (expandable)                     │
│   Base: 50 │ Watched: -5 │ Distance: -47 │ Codec: -30  │
│   Audio: +30 │ Sub: -20 │ Quality: -35 │ Gap: No       │
└──────────────────────────────────────────────────────────┘
```

### Pros
- **Complete overview**: see every file ranked by deletion priority in one place
- **Dedicated workspace**: user can focus on deletion tuning without context-switching
- **Rich detail**: plenty of screen real estate for score breakdowns, reason columns, filtering/sorting
- **Bulk operations**: easy to add "protect this file", "force delete", "exclude anime" actions
- **No impact on existing UI**: MyList cards remain clean and focused on their current purpose
- **Score comparison**: easy to see relative scores across different anime/files

### Cons
- **Discoverability**: user must know to check this tab; files might get deleted before the user looks
- **Navigation cost**: requires switching away from MyList to review deletion state
- **Disconnected context**: file deletion info is separated from the anime/episode context the user cares about
- **Maintenance overhead**: a new tab means a new manager class, new widget tree, new data pipeline
- **Redundant data**: duplicates much of what MyList already displays (anime name, episode, file details)
- **Possibly empty**: if auto-deletion is disabled or disk space is plentiful, the tab sits unused

### Implementation Complexity
**Medium-High**. Requires:
- New `DeletionCandidateManager` class with its own data model
- New tab page in `Window` with `QTableWidget` or `QTreeWidget`
- Score calculation for all files on tab activation
- Interaction with `WatchSessionManager` for score data
- UI for detail panel, protection overrides, threshold display

---

## Option 3: Filter/Sort Mode in Existing MyList Tab

### Description
Add a "Deletion Risk" sort option and/or filter to the existing `MyListFilterSidebar`. When activated, cards are reordered by their worst file score (lowest first), and deletion indicators appear on cards. This reuses the existing card infrastructure.

### Mockup
```
Filter Sidebar:                    Cards (sorted by deletion risk):
┌─────────────────┐    ┌─────────────────────────────────────┐
│ Sort: Deletion ▼│    │ 🔴 Show A (score: -942)             │
│                 │    │   Ep 1: v1 old.avi [-942] OLD REVN  │
│ ☑ Show at-risk  │    │   Ep 1: v2 new.mkv [+65]            │
│   only          │    ├─────────────────────────────────────┤
│                 │    │ 🟡 Anime B (score: -72)              │
│ Risk threshold: │    │   Ep 3: anime-03.mkv [-72]           │
│ [====|====] 0   │    │     watched, far from current        │
│                 │    ├─────────────────────────────────────┤
│ Space: 42/500GB │    │ 🟢 Anime C (score: +95)             │
│ Thresh: 50 GB   │    │   All files safe                     │
└─────────────────┘    └─────────────────────────────────────┘
```

### Pros
- **Familiar UI**: reuses the card view and filter sidebar the user already knows
- **Contextual**: deletion risk is shown alongside all other anime info on the card
- **Filterable**: "show at-risk only" hides safe files and focuses attention on problems
- **Low disruption**: only visible when the user explicitly activates the sort/filter
- **Natural discovery**: appears alongside other sort options the user already uses
- **Minimal new code**: extends existing `AnimeFilter` and `MyListFilterSidebar` classes

### Cons
- **Card-level granularity only**: sorting works at the anime level (worst file score), but the real issue is per-file
- **Temporary view**: once the user switches sort back to name/rating, the deletion info disappears
- **Limited detail space**: score breakdown needs to fit within the existing card layout
- **Mixed metaphor**: MyList is for browsing the collection; adding "deletion mode" overloads its purpose
- **Score calculation on sort**: needs to compute scores for all anime to sort, which could be slow for large collections

### Implementation Complexity
**Low-Medium**. Requires:
- New sort option in `MyListFilterSidebar` ("Deletion Risk")
- New filter checkbox ("At-risk files only")
- Per-anime worst-score calculation in `AnimeFilter`
- Conditional deletion indicators on `AnimeCard` when sort mode is active
- Space/threshold display in sidebar

---

## Option 4: Notification/Warning Overlay on Cards

### Description
Only show deletion warnings when the system is approaching the deletion threshold or when auto-deletion is enabled. Cards with at-risk files get a dismissible warning banner. This is event-driven rather than always-visible.

### Mockup
```
┌──────────────────────────────────┐
│ ⚠ 3 files at risk of deletion   │  ← warning banner (card top)
│ Space: 42 GB / Threshold: 50 GB │
├──────────────────────────────────┤
│ Poster       │ Title             │
│              │ Type: TV          │
│              │ ...               │
├──────────────────────────────────┤
│ Ep 1 - Intro                    │
│   \ v1 480p XviD ⚠ Score: -45  │  ← only at-risk files marked
│ Ep 2 - Adventure                │
│   \ v1 1080p HEVC              │  ← safe files unmarked
└──────────────────────────────────┘
```

### Pros
- **Non-intrusive**: no visual noise when disk space is plentiful; warnings only appear when relevant
- **Urgency-appropriate**: warnings scale with actual deletion proximity
- **Action-oriented**: banner can include "Protect" / "Review" buttons for immediate action
- **Minimal cognitive load**: user only sees deletion info when it matters
- **Existing pattern**: the card already has a warning label (`m_warningLabel`) for missing metadata; this extends that concept

### Cons
- **Reactive only**: user cannot proactively review what would be deleted before space gets low
- **Threshold dependency**: if threshold is set very low, warnings may appear too late
- **Limited overview**: no way to see all at-risk files across the collection at once
- **Transient**: warnings disappear when space is freed, losing the tuning context
- **Incomplete picture**: doesn't help the user understand and tune the scoring weights

### Implementation Complexity
**Low**. Requires:
- Threshold proximity check on card refresh
- Conditional warning banner on `AnimeCard`
- Score calculation only for files approaching deletion
- Reuse of existing `m_warningLabel` pattern

---

## Option 5: Hybrid — Inline Indicators + Sidebar Summary Panel

### Description
Combine Options 1 and 3: add per-file risk indicators to the anime cards (always-visible but subtle), and add a collapsible "Deletion Summary" section to the filter sidebar showing aggregate stats and the next N files to be deleted.

### Mockup
```
Filter Sidebar:                          Cards:
┌─────────────────────┐    ┌───────────────────────────────┐
│ ▼ Deletion Summary  │    │ Poster │ Title                │
│   Space: 42/500 GB  │    │        │ ...                  │
│   Threshold: 50 GB  │    ├───────────────────────────────┤
│   Auto-delete: ON   │    │ Ep 1 - Intro                 │
│                     │    │   \ v1 1080p [Group] 🟢      │
│   Next to delete:   │    │ Ep 2 - Battle                │
│   1. show-01.avi    │    │   \ v1 720p [Group]  🟡      │
│      Score: -942    │    │ Ep 3 - End                   │
│      [Protect]      │    │   \ v1 480p [Group]  🔴      │
│   2. anime-03.mkv   │    └───────────────────────────────┘
│      Score: -72     │
│      [Protect]      │
│   3. movie.mp4      │
│      Score: -45     │
│      [Protect]      │
│                     │
│   [View all →]      │  ← opens detailed dialog/panel
│                     │
│ ▶ Status Filters    │
│ ▶ Type Filters      │
│ ▶ Sort Options      │
└─────────────────────┘
```

### Pros
- **Best of both worlds**: per-file indicators give contextual awareness; sidebar gives collection-wide overview
- **Always accessible**: sidebar is visible alongside cards without switching tabs
- **Progressive disclosure**: subtle icons for glanceability, sidebar for quick review, expand for full details
- **Actionable**: "Protect" buttons in sidebar let user intervene quickly on the most at-risk files
- **Space-efficient**: sidebar section collapses when not needed; icons are tiny
- **Integrates naturally**: the filter sidebar already has grouped sections; deletion summary fits the pattern

### Cons
- **Dual maintenance**: both card indicators and sidebar summary need to stay in sync
- **Sidebar space**: the filter sidebar is already populated; adding another section could require scrolling
- **Complexity**: two separate display mechanisms to implement and keep consistent
- **Score recalculation**: both indicators and sidebar need up-to-date scores; potential for inconsistency if not carefully managed

### Implementation Complexity
**Medium**. Requires:
- Per-file risk indicator in `AnimeCard::addEpisode()` (same as Option 1)
- New collapsible group in `MyListFilterSidebar` with `QListWidget` for top candidates
- Score cache in `WatchSessionManager` to avoid duplicate calculations
- Signal/slot connection between sidebar actions and card updates

---

## Recommendation

**Option 5 (Hybrid)** provides the best balance of visibility and detail. However, it can be implemented incrementally:

### Phase 1: Sidebar Summary Panel (from Option 3/5)
Add a "Deletion Queue" collapsible section to `MyListFilterSidebar` showing:
- Current disk space vs. threshold
- Auto-deletion status (enabled/disabled)
- Top 5-10 files sorted by deletion priority with score, anime name, episode, and primary reason
- "Protect" action per file

This gives immediate collection-wide visibility with minimal UI disruption. It naturally fits the sidebar pattern and provides the most critical missing information: **what will be deleted next and why**.

### Phase 2: Inline Card Indicators (from Option 1/5)
Add per-file colored risk icons (🟢🟡🔴) to the episode tree in anime cards. Tooltip shows score breakdown. This is only visible when the card is expanded, keeping it unobtrusive.

### Phase 3: Score Breakdown Dialog
Add a detailed score breakdown dialog accessible from both the sidebar (click on a candidate) and the card (click on risk icon). Shows all scoring factors with values, explains gap protection, and allows per-file protection overrides.

---

## Score Breakdown Display Format

For any UI element that shows score details, the following information should be displayed:

```
File: show-01.avi
Anime: Show A | Episode: 1 | Group: SubGroup
──────────────────────────────────────
Factor                        Score
──────────────────────────────────────
Base score                      +50
Watch status (watched)           -5
Distance (47 episodes away)     -47
Codec (XviD — ancient)          -30
Quality (low)                   -35
Audio (Japanese — preferred)    +30
Subtitle (none — not preferred) -20
Anime rating (5.2 — low)       -15
Group status (disbanded)        -25
──────────────────────────────────────
Total                           -97
Gap protection: No
──────────────────────────────────────
⚡ This file is #3 in deletion queue
```

This format makes every scoring decision transparent and gives the user the information needed to:
1. Understand why a file was chosen
2. Decide whether the scoring weights need tuning
3. Take manual action to protect specific files
