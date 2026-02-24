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
  Base:                        50
  Already watched:             -5
  Distance (47 eps away):    -47
  Codec: XviD (ancient):     -30   expected: HEVC/AV1
  Quality: low (score 20):   -35   expected: ≥60
  Audio: Japanese:           +30   matches preferred
  Subtitle: none:            -20   preferred: English
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
──────────────────────────────────────────────────────────
Factor                        Score   Actual → Expected
──────────────────────────────────────────────────────────
Base score                      +50
Watch status (watched)           -5   viewed
Distance (47 episodes away)     -47   ep 1 → current ep 48
Codec (XviD — ancient)          -30   XviD → HEVC/AV1
Quality (low)                   -35   score 20 → ≥60
Audio (Japanese — preferred)    +30   Japanese = preferred
Subtitle (none — not preferred) -20   none → English
Anime rating (5.2 — low)       -15   520 → ≥800
Group status (disbanded)        -25   disbanded → active
──────────────────────────────────────────────────────────
Total                           -97
Gap protection: No
──────────────────────────────────────────────────────────
⚡ This file is #3 in deletion queue
```

This format makes every scoring decision transparent and gives the user the information needed to:
1. Understand why a file was chosen
2. Decide whether the scoring weights need tuning
3. Take manual action to protect specific files

---

# Alternative: Procedural Deletion Logic (replacing the scoring system)

## Why Consider a Procedural Alternative

The scoring system combines 17 weighted factors into a single integer. This has fundamental problems:

1. **Weight interactions are opaque.** A file that is unwatched (+50) but far away (-47) and has an ancient codec (-30) ends up at -27. The user cannot easily predict which combination of factors will tip a file into the danger zone because the weights were chosen by the developer, not derived from user priorities.

2. **Tuning is trial-and-error.** If the system deletes a file the user wanted to keep, the only recourse is to adjust one or more of the 17 constants — but changing one weight affects the relative ranking of every other file in the collection.

3. **No clear "why".** Even with a score breakdown tooltip, the user sees a list of numbers that add up to a total. They still have to mentally reason about "which of these factors was the decisive one."

4. **False precision.** Assigning +20 to "active group" vs. +15 to "high rating" implies a precision that does not exist. These numbers were not derived from empirical data.

A procedural approach replaces the single numeric score with a series of explicit rules that mirror how a human would think about the decision: "first delete duplicates, then delete watched episodes far from my current position, then delete low-quality files..." Each rule has a clear, explainable purpose.

---

## Design: Tiered Priority Cascade

Instead of calculating a score, files are classified into **deletion tiers** using a fixed sequence of rules. Files in a lower tier are always deleted before any file in a higher tier. Within each tier, files are ordered by a simple, single-dimension sort (e.g., distance from current episode).

### Tier Definitions

```
Tier 0 — UNCONDITIONAL DELETE (always safe to remove)
  Rule: File has a newer local revision for the same episode.
  Sort: Oldest revision first.
  Reason template: "Superseded by v{N} — this file: v{M} {res} {codec}, newer: v{N} {res} {codec}"
  Example: "Superseded by v2 — this file: v1 480p XviD, newer: v2 1080p HEVC"

Tier 1 — WATCHED, FAR, NO ACTIVE SESSION
  Rule: File is watched AND the anime has no active watch session
        AND the file is not in an anime the user explicitly protected.
  Sort: Largest file size first (reclaim space fastest).
  Reason template: "Watched, no active session — {codec}, {fileSize}, rating {rating}"
  Example: "Watched, no active session — XviD, 2.4 GB, rating 5.20"

Tier 2 — WATCHED, FAR FROM CURRENT POSITION
  Rule: File is watched AND distance from current episode > aheadBuffer
        AND anime HAS an active session.
  Sort: Distance from current episode descending (farthest first).
  Reason template: "Watched, {N} eps from current — ep {fileEp} → current ep {curEp}, {codec}, bitrate {actual}kbps (expected {expected}kbps)"
  Example: "Watched, 30 eps from current — ep 2 → current ep 32, H.264, bitrate 850kbps (expected 1200kbps)"

Tier 3 — UNWATCHED, LOW QUALITY DUPLICATE
  Rule: File is unwatched AND another file for the same episode exists
        with higher quality/resolution AND that other file is also local.
  Sort: Lower quality first.
  Reason template: "Lower quality duplicate — this file: {res} {codec} {bitrate}kbps, better: {res} {codec} {bitrate}kbps"
  Example: "Lower quality duplicate — this file: 480p XviD 600kbps, better: 1080p HEVC 1500kbps"

Tier 4 — UNWATCHED, FAR BEHIND CURRENT POSITION
  Rule: File is unwatched AND episode number < current episode - aheadBuffer
        (i.e., the user has already passed this episode).
  Sort: Distance behind current episode descending.
  Reason template: "Unwatched, {N} eps behind current — ep {fileEp} → current ep {curEp}, quality {qualityTier}"
  Example: "Unwatched, 15 eps behind current — ep 3 → current ep 18, quality low"

Tier 5 — PREFERENCE MISMATCH
  Rule: File does not match preferred audio language
        AND does not match preferred subtitle language
        AND another file for the same episode exists that does match.
  Sort: Least-matching first (no audio match + no sub match before just no sub match).
  Reason template: "Language mismatch — audio: {fileAudio} (preferred: {prefAudio}), sub: {fileSub} (preferred: {prefSub}); alternative has {altAudio}/{altSub}"
  Example: "Language mismatch — audio: Italian (preferred: Japanese), sub: none (preferred: English); alternative has Japanese/English"

── PROTECTION BOUNDARY ──
Files below this line are never auto-deleted.

Tier 6+ — PROTECTED (never auto-deleted)
  - Unwatched files in or ahead of the current position
  - Files in the ahead buffer
  - Only copy of an unwatched episode
  - Files the user explicitly protected
```

### Tier Assignment Pseudocode

```
function assignDeletionTier(file):

    // ── Absolute protections (cannot be overridden) ──
    if file.isExplicitlyProtected:
        return PROTECTED

    if wouldCreateGap(file):
        return PROTECTED

    // ── Tier 0: superseded revisions ──
    if hasNewerLocalRevision(file.episodeId, file.version):
        return Tier 0, sortKey = file.version  // oldest first

    // ── Tier 1: watched, no active session ──
    if file.isWatched AND NOT hasActiveSession(file.animeId):
        return Tier 1, sortKey = -file.sizeBytes  // largest first

    // ── Gather context for remaining tiers ──
    session = findActiveWatchSession(file.animeId)
    if session exists:
        distance = file.totalEpisodePosition - session.currentTotalPosition
    else:
        distance = NO_SESSION  // sentinel; file already handled by Tier 1

    // ── Tier 2: watched, far from current ──
    if file.isWatched AND distance > aheadBuffer:
        return Tier 2, sortKey = -abs(distance)  // farthest first

    // ── Tier 3: unwatched low-quality duplicate ──
    if NOT file.isWatched AND hasBetterLocalDuplicate(file):
        return Tier 3, sortKey = file.qualityScore  // worst quality first

    // ── Tier 4: unwatched, far behind ──
    if NOT file.isWatched AND distance < -aheadBuffer:
        return Tier 4, sortKey = distance  // most behind first

    // ── Tier 5: preference mismatch with better alternative ──
    // sortKey: 0 = neither audio nor sub match, 1 = only sub mismatch
    if NOT matchesPreferredAudio(file) AND NOT matchesPreferredSub(file):
        if hasBetterLanguageAlternative(file):
            return Tier 5, sortKey = 0  // worst match — delete first
    if NOT matchesPreferredSub(file) AND matchesPreferredAudio(file):
        if hasBetterLanguageAlternative(file):
            return Tier 5, sortKey = 1  // partial match — delete after full mismatches

    // ── Everything else is protected ──
    return PROTECTED
```

### Deletion Selection

```
function selectNextFileToDelete():
    if NOT isDeletionNeeded():
        return null

    candidates = getAllLocalFiles()
    for each file in candidates:
        file.tier, file.sortKey = assignDeletionTier(file)

    // Remove protected files
    candidates = candidates.filter(c => c.tier != PROTECTED)

    // Sort: tier ascending, then sortKey ascending within tier.
    // Each tier's sortKey is pre-computed so that ascending order
    // produces the intended deletion priority (e.g., Tier 1 uses
    // -sizeBytes so that the largest file sorts first).
    candidates.sort(by: tier ASC, sortKey ASC)

    // Return the first candidate (lowest tier, best sort position)
    if candidates is not empty:
        return candidates[0]
    return null
```

---

## Gap Protection (unchanged)

The gap protection logic (`wouldCreateGap()`) works identically in the procedural model. It is applied as an absolute protection before any tier assignment: if deleting a file would leave episodes on both sides without that episode, the file is classified as PROTECTED regardless of any other factor.

---

## Comparison: Scoring vs. Procedural

| Aspect | Scoring System | Procedural Cascade |
|--------|---------------|-------------------|
| **Decision transparency** | Shows a number; user must interpret which factors dominated | Shows a tier name and a single reason; immediately clear |
| **Predictability** | Score depends on all 17 factors interacting; hard to predict ranking changes when one factor changes | Tier depends on 1-3 conditions; user can reason "if I watch this episode, it moves from Tier 6 to Tier 2" |
| **Tuning** | Change one of 17 weights; affects every file globally | Reorder tiers or adjust tier conditions; localized impact |
| **Edge cases** | Unlikely edge cases hidden by additive math — e.g., enough small bonuses can override a major penalty | Each tier's conditions are explicit; edge cases are visible in the rule definitions |
| **Expressiveness** | Can represent fine-grained relative priorities (score 47 vs. 48) | Coarser; files in the same tier are distinguished only by the sort key |
| **New factors** | Add a constant and a `score +=` line | Add a new tier or add a condition to an existing tier; must decide its position in the cascade |
| **UI display** | Score breakdown table with 17 rows | Single tier label + reason string + sort position within tier |
| **Granularity within tier** | N/A (single dimension) | Limited to one sort key per tier; cannot express complex intra-tier ordering without splitting into sub-tiers |
| **Risk of "wrong" deletions** | Possible through unintuitive weight interactions | Possible through wrong tier ordering, but each mistake is isolated and easy to diagnose |
| **Code complexity** | One method with 17 `score +=` blocks | One method with 6 if/else blocks; simpler per-block but more branching |
| **Testability** | Assert exact score value; fragile if weights change | Assert tier assignment; stable across weight changes since there are no weights |

---

## UI Display for Procedural Approach

The procedural model simplifies the UI significantly because each file has a single, human-readable reason for its classification:

### Card-Level Display
```
Ep 1 - Intro
  \ v1 480p XviD [Group] 🔴 Superseded (v2 exists locally)
  \ v2 1080p HEVC [Group] 🟢 Protected (unwatched, in buffer)
Ep 5 - Journey
  \ v1 720p [Group]       🟡 Watched, 12 eps from current (ep 5 → current ep 17)
```

No score numbers needed. The tier name and reason are the complete explanation.

### Sidebar Display
```
┌─────────────────────────┐
│ ▼ Deletion Queue        │
│   Space: 42 / 500 GB    │
│   Threshold: 50 GB      │
│                         │
│   ── Will delete next ──│
│   1. show-01.avi        │
│      Superseded (v1→v2) │
│      [Protect]          │
│   2. anime-12.mkv       │
│      Watched, no session│
│      2.4 GB · XviD      │
│      [Protect]          │
│   3. anime-03.mkv       │
│      Watched, 47 eps    │
│      from current       │
│      (ep 3 → cur ep 50) │
│      [Protect]          │
└─────────────────────────┘
```

### Detail Dialog
```
File: show-01.avi
Anime: Show A | Episode: 1 | Group: SubGroup
────────────────────────────────────────────
Status: Tier 0 — SUPERSEDED REVISION
Reason: This file is v1; v2 exists locally for the same episode.
        v1 480p XviD → v2 1080p HEVC
────────────────────────────────────────────
Queue position: #1 (first to be deleted)
Gap protection: Not applicable (other file exists)
────────────────────────────────────────────
To keep this file: [Protect] or delete the v2 file.
```

vs. the scoring system's equivalent:
```
File: show-01.avi
Score: -1947
  Base: 50, Revision: -2000, Watched: -5, Distance: +8, ...
```

The procedural version answers "why is this file being deleted?" in one line. The scoring version answers "what is this file's numeric rank?" — which requires further interpretation.

---

## Handling Cases Where Scoring Outperforms Procedural

The main scenario where scoring is better is **fine-grained intra-tier ordering**. For example, within Tier 2 (watched, far from current), the scoring system would also factor in codec age, bitrate, and group status to decide between two files at the same distance. The procedural model sorts only by distance.

### Mitigation: Secondary Sort Keys

Each tier can define a secondary sort as a tiebreaker:

```
Tier 2 — WATCHED, FAR FROM CURRENT POSITION
  Primary sort: distance descending
  Tiebreaker: file size descending (reclaim more space first)

Tier 4 — UNWATCHED, FAR BEHIND
  Primary sort: distance behind descending
  Tiebreaker: quality ascending (delete worst quality first)
```

This keeps the model procedural (no additive scoring) while allowing two-level ordering within each tier. Adding more tiebreakers is possible but should be avoided — if a tier needs complex intra-ordering, it is better to split it into two tiers.

---

## Implementation Classes

The procedural approach maps cleanly to new classes while replacing the current scoring logic:

### DeletionTier (enum)
```
enum class DeletionTier {
    SupersededRevision = 0,    // Tier 0
    WatchedNoSession = 1,      // Tier 1
    WatchedFarFromCurrent = 2, // Tier 2
    UnwatchedLowQualDupe = 3,  // Tier 3
    UnwatchedFarBehind = 4,    // Tier 4
    PreferenceMismatch = 5,    // Tier 5
    Protected = 100            // Never deleted
};
```

### DeletionCandidate (struct)
```
struct DeletionCandidate {
    int lid;
    int aid;
    DeletionTier tier;
    QString reason;       // Human-readable with actual values:
                          // "Superseded by v2 — this file: v1 480p XviD, newer: v2 1080p HEVC"
                          // "Language mismatch — audio: Italian (preferred: Japanese), sub: none (preferred: English); alternative has Japanese/English"
    QString filePath;
    QString animeName;
    qint64 sortKey;       // Tier-specific ordering value
    bool gapProtected;
};
```

### DeletionClassifier (new class — replaces calculateDeletionScore)
Single responsibility: takes a file and returns its `DeletionTier` + reason.
```
class DeletionClassifier {
public:
    DeletionCandidate classify(int lid) const;
private:
    bool hasNewerLocalRevision(int eid, int version) const;
    bool hasBetterLocalDuplicate(int lid) const;
    bool hasBetterLanguageAlternative(int lid) const;
    // ...uses existing WatchSessionManager helpers for context
};
```

### DeletionQueue (new class — replaces deleteNextEligibleFile loop)
Single responsibility: maintains the ordered queue of candidates.
```
class DeletionQueue {
public:
    void rebuild();                          // Re-classify all files
    const DeletionCandidate* next() const;   // First non-protected candidate
    QList<DeletionCandidate> topN(int n) const; // For sidebar display
    void protect(int lid);                   // Move file to Protected tier
    void unprotect(int lid);                 // Re-classify file
private:
    QList<DeletionCandidate> m_candidates;   // Sorted by tier, then sortKey
    QSet<int> m_explicitlyProtected;         // User overrides
};
```

This separation means:
- `DeletionClassifier` can be unit-tested by providing mock database state and asserting tier assignments.
- `DeletionQueue` can be tested independently from the classifier.
- The UI (sidebar, card indicators) only needs to read from `DeletionQueue::topN()`.
- `WatchSessionManager` delegates to `DeletionQueue::next()` instead of implementing its own candidate loop.

---

## Migration Path

The procedural system can coexist with the scoring system during development:

1. **Phase 1**: Implement `DeletionClassifier` and `DeletionQueue` alongside the existing scoring code. Add a setting to switch between them ("Deletion strategy: Scoring / Procedural").
2. **Phase 2**: Wire the UI (sidebar, card indicators from the visualization options above) to the `DeletionQueue` API. Since the queue exposes tier+reason instead of a score, the UI naturally shows human-readable explanations.
3. **Phase 3**: Once the procedural system is validated, remove the scoring constants and `calculateDeletionScore()` method. The `DeletionClassifier` becomes the single source of truth.

---

## Summary: When to Prefer Each Approach

| Use Scoring If... | Use Procedural If... |
|--------------------|----------------------|
| You need fine-grained relative ordering across all factors simultaneously | You need each deletion decision to be explainable in one sentence |
| The weight values will be empirically tuned (e.g., via user feedback or A/B testing) | The rules should be immediately intuitive without tuning |
| New factors are added frequently and should blend smoothly with existing ones | New rules have clear priority relative to existing ones |
| The UI will show numeric scores and breakdowns | The UI will show tier names and reason strings |

---

# Hybrid Approach: Procedural Tiers + Intra-Tier Scoring + Manual Locks

## Motivation

The pure scoring system is hard to reason about (17 interacting weights). The pure procedural system is easy to reason about but loses the fine-grained ordering that scoring provides within a tier — when two watched files are both far from the current position, codec quality and bitrate matter for picking which one to delete first, and a single sort key cannot capture that.

The hybrid approach uses **procedural rules for tier assignment** (the "why") and **scoring for intra-tier ordering** (the "which one first"). Manual locks give the user a hard override that no algorithm can bypass.

---

## Manual Lock Mechanism

### What Can Be Locked

| Lock Target | Effect | Granularity |
|-------------|--------|-------------|
| **Anime lock** | All files for this anime are protected from auto-deletion | Entire anime (all episodes, all files) |
| **Episode lock** | All files for this specific episode are protected | Single episode (all its files) |

Locks are explicit user actions — they are not inferred or computed. A locked item stays locked until the user unlocks it.

### Database Schema

```sql
-- New column on mylist table — denormalized cache for fast lookups.
-- Value reflects the highest-priority lock covering this file:
--   0 = not locked
--   1 = locked at episode level
--   2 = locked at anime level (trumps episode-level)
-- When both anime and episode locks exist for the same file, the value is 2.
ALTER TABLE mylist ADD COLUMN deletion_locked INTEGER DEFAULT 0;

-- Separate table for user-created locks (applied to all current and future files)
CREATE TABLE deletion_locks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    aid INTEGER,              -- Non-null for anime lock
    eid INTEGER,              -- Non-null for episode lock
    locked_at INTEGER,        -- Unix timestamp
    reason TEXT,              -- Optional user note: "rewatching", "seeding", etc.
    CHECK ((aid IS NOT NULL AND eid IS NULL) OR (aid IS NULL AND eid IS NOT NULL)),
    UNIQUE(aid, eid)
);
CREATE INDEX idx_deletion_locks_aid ON deletion_locks(aid);
CREATE INDEX idx_deletion_locks_eid ON deletion_locks(eid);
```

Using a separate `deletion_locks` table rather than a column on `mylist` because:
- Anime-level locks apply to files that do not yet exist in mylist (future downloads).
- The lock is a user intent, not a file property — it belongs in its own table.
- The `mylist.deletion_locked` column is a denormalized cache for fast lookups during deletion candidate selection; it is updated whenever the `deletion_locks` table changes.

### Lock Propagation

When the user locks an **anime**:
1. Insert row into `deletion_locks` with `aid = X, eid = NULL`.
2. Update all existing mylist rows: `UPDATE mylist SET deletion_locked = 2 WHERE aid = X`.
3. Any future file added to this anime inherits `deletion_locked = 2`.

When the user locks an **episode**:
1. Insert row into `deletion_locks` with `aid = NULL, eid = Y`.
2. Update matching mylist rows, but only if not already covered by an anime lock: `UPDATE mylist SET deletion_locked = 1 WHERE eid = Y AND deletion_locked < 1`.
3. Any future file added to this episode inherits `deletion_locked = 1` (or 2 if its anime is also locked).

When the user unlocks:
1. Remove the `deletion_locks` row.
2. Recalculate `deletion_locked` for affected mylist rows (an episode might still be locked via an anime-level lock).

### UI for Locking

**Anime Card — Card-Level Context Menu:**
```
Right-click on card header:
  ┌───────────────────────────┐
  │ Fetch data                │
  │ Hide / Unhide             │
  │ ─────────────────────────── 
  │ 🔒 Lock anime (keep all) │  ← NEW
  │ 🔓 Unlock anime          │  ← NEW (shown when locked)
  └───────────────────────────┘
```

**Anime Card — Episode Context Menu:**
```
Right-click on episode row:
  ┌─────────────────────────────────┐
  │ Start session from here         │
  │ Mark episode as watched         │
  │ ───────────────────────────────── 
  │ 🔒 Lock episode (keep files)   │  ← NEW
  │ 🔓 Unlock episode              │  ← NEW (shown when locked)
  └─────────────────────────────────┘
```

**Visual Indicators:**
```
┌──────────────────────────────────┐
│ Poster       │ 🔒 Title          │  ← lock icon in title when anime is locked
│              │ Type: TV          │
├──────────────────────────────────┤
│ 🔒 Ep 1 - Intro                 │  ← lock icon on locked episodes
│   \ v1 1080p HEVC [Group]       │
│ Ep 2 - Adventure                │  ← no icon = not locked
│   \ v1 720p AVC [Group]         │
└──────────────────────────────────┘
```

---

## Hybrid Tier Definitions

The tiers use procedural rules for classification. Locks are the highest-priority protection. Within each deletable tier, a **reduced scoring formula** orders candidates — but only using factors relevant to that tier, not all 17.

```
── ABSOLUTE PROTECTIONS (checked first, in order) ──

Lock Check:
  Rule: File's anime or episode has an active lock in deletion_locks.
  Result: PROTECTED. No further evaluation.

Gap Check:
  Rule: Deleting this file would create a gap in the episode sequence
        (episodes exist both before and after, no other file covers this episode).
  Result: PROTECTED. No further evaluation.

── DELETION TIERS ──

Tier 0 — SUPERSEDED REVISION
  Rule: A newer local revision exists for the same episode.
  Intra-tier score:
    - file version (oldest first)
    - file size descending (free more space)
  Reason: "Superseded by v{N} — this file: v{M} {resolution} {codec}, newer: v{N} {resolution} {codec}"
  Example: "Superseded by v2 — this file: v1 480p XviD, newer: v2 1080p HEVC"

Tier 1 — WATCHED, NO ACTIVE SESSION
  Rule: File is watched AND anime has no active watch session.
  Intra-tier score:
    + anime rating (delete low-rated first)
    + file size descending (free more space)
    + codec age (delete ancient codecs first)
  Reason: "Watched, no active session — {codec}, {fileSize}, rating {rating}"
  Example: "Watched, no active session — XviD, 2.4 GB, rating 5.20"

Tier 2 — WATCHED, FAR FROM CURRENT
  Rule: File is watched AND anime has active session
        AND distance from current > aheadBuffer.
  Intra-tier score:
    + distance from current (farthest first)
    + codec age
    + bitrate deviation
  Reason: "Watched, {N} eps from current — ep {fileEp} → current ep {curEp}, {codec}, bitrate {actual}kbps (expected {expected}kbps)"
  Example: "Watched, 30 eps from current — ep 2 → current ep 32, H.264, bitrate 850kbps (expected 1200kbps)"

Tier 3 — UNWATCHED LOW-QUALITY DUPLICATE
  Rule: File is unwatched AND another local file for the same episode
        has strictly higher quality/resolution.
  Intra-tier score:
    + quality gap (biggest quality difference first)
    + bitrate deviation
  Reason: "Lower quality duplicate — this file: {resolution} {codec} {bitrate}kbps, better: {resolution} {codec} {bitrate}kbps"
  Example: "Lower quality duplicate — this file: 480p XviD 600kbps, better: 1080p HEVC 1500kbps"

Tier 4 — UNWATCHED, FAR BEHIND CURRENT
  Rule: File is unwatched AND episode is behind current position
        by more than aheadBuffer.
  Intra-tier score:
    + distance behind (most behind first)
    + quality (lower quality first)
  Reason: "Unwatched, {N} eps behind current — ep {fileEp} → current ep {curEp}, quality {qualityTier}"
  Example: "Unwatched, 15 eps behind current — ep 3 → current ep 18, quality low"

Tier 5 — PREFERENCE MISMATCH WITH ALTERNATIVE
  Rule: File does not match audio AND/OR subtitle preferences
        AND a better-matching local alternative exists for the same episode.
  Intra-tier score:
    + mismatch severity (no audio + no sub > no sub only)
    + quality (lower quality first among mismatches)
  Reason: "Language mismatch — audio: {fileAudio} (preferred: {prefAudio}), sub: {fileSub} (preferred: {prefSub}); alternative has {altAudio}/{altSub}"
  Example: "Language mismatch — audio: Italian (preferred: Japanese), sub: none (preferred: English); alternative has Japanese/English"

── PROTECTION BOUNDARY ──

Everything else — PROTECTED
  - Unwatched episodes at or ahead of current position
  - Episodes within the ahead buffer
  - Only local copy of an episode with no better alternative
  - Manually locked anime or episodes
```

### Why Scoring Inside Tiers Works Better Than Pure Procedural

In the pure procedural model, Tier 2 sorts only by distance. If two files are equidistant (e.g., episode 5 and episode 45 are both 20 episodes from current), the system has no way to prefer the XviD encode over the HEVC encode without splitting into sub-tiers.

With intra-tier scoring, each tier uses a **small, focused score** built from 2-4 factors relevant to that tier's context. This avoids the 17-factor soup of the full scoring system while still providing meaningful ordering.

### Tier Assignment + Intra-Tier Score Pseudocode

```
function classifyFile(file):

    // ── Absolute protections ──
    if isLocked(file.aid, file.eid):
        return { tier: PROTECTED, reason: lockReason(file) }

    if wouldCreateGap(file):
        return { tier: PROTECTED, reason: "Gap protection" }

    // ── Tier 0 ──
    if hasNewerLocalRevision(file.eid, file.version):
        newer = getNewestLocalRevision(file.eid)
        score = file.version * 1000 - file.sizeBytes / (1024*1024)
        return { tier: 0, score: score,
                 reason: "Superseded by v" + newer.version
                       + " — this file: v" + file.version + " " + file.resolution + " " + file.codec
                       + ", newer: v" + newer.version + " " + newer.resolution + " " + newer.codec }

    isWatched = file.viewed > 0 OR file.localWatched > 0

    // ── Tier 1 ──
    if isWatched AND NOT hasActiveSession(file.aid):
        rating = animeRating(file.aid)
        score = 0
        score -= rating / 100
        score -= file.sizeBytes / (1024*1024*1024)
        score -= codecAgePenalty(file)
        return { tier: 1, score: score,
                 reason: "Watched, no active session"
                       + " — " + file.codec + ", " + formatSize(file.sizeBytes)
                       + ", rating " + formatRating(rating) }

    // ── Gather session context ──
    session = findActiveWatchSession(file.aid)
    distance = NO_SESSION
    if session:
        distance = file.totalEpisodePosition - session.currentTotalPosition

    // ── Tier 2 ──
    if isWatched AND distance != NO_SESSION AND abs(distance) > aheadBuffer:
        score = -abs(distance) * 100            // farthest first
        score -= codecAgePenalty(file)
        score -= bitrateDeviation(file)
        expectedBitrate = calculateExpectedBitrate(file.resolution, file.codec)
        return { tier: 2, score: score,
                 reason: "Watched, " + abs(distance) + " eps from current"
                       + " — ep " + file.episodeNumber + " → current ep " + session.currentEpisode
                       + ", " + file.codec
                       + ", bitrate " + file.bitrate + "kbps"
                       + " (expected " + expectedBitrate + "kbps)" }

    // ── Tier 3 ──
    if NOT isWatched:
        betterDupe = findBetterLocalDuplicate(file)
        if betterDupe:
            qualityGap = betterDupe.qualityScore - file.qualityScore
            score = -qualityGap * 100 - bitrateDeviation(file)
            return { tier: 3, score: score,
                     reason: "Lower quality duplicate"
                           + " — this file: " + file.resolution + " " + file.codec + " " + file.bitrate + "kbps"
                           + ", better: " + betterDupe.resolution + " " + betterDupe.codec + " " + betterDupe.bitrate + "kbps" }

    // ── Tier 4 ──
    if NOT isWatched AND distance != NO_SESSION AND distance < -aheadBuffer:
        score = distance * 100                  // most behind first (distance is negative)
        score += qualityScore(file)             // lower quality → delete first
        return { tier: 4, score: score,
                 reason: "Unwatched, " + abs(distance) + " eps behind current"
                       + " — ep " + file.episodeNumber + " → current ep " + session.currentEpisode
                       + ", quality " + qualityTierLabel(file) }

    // ── Tier 5 ──
    audioMatch = matchesPreferredAudio(file)
    subMatch = matchesPreferredSub(file)
    if (NOT audioMatch OR NOT subMatch) AND hasBetterLanguageAlternative(file):
        alt = getBetterLanguageAlternative(file)
        mismatchSeverity = 0
        if NOT audioMatch: mismatchSeverity += 2
        if NOT subMatch: mismatchSeverity += 1
        score = -mismatchSeverity * 1000 + qualityScore(file)
        reasonParts = []
        if NOT audioMatch:
            reasonParts.append("audio: " + file.audioLang + " (preferred: " + preferredAudioLang + ")")
        if NOT subMatch:
            reasonParts.append("sub: " + file.subLang + " (preferred: " + preferredSubLang + ")")
        return { tier: 5, score: score,
                 reason: "Language mismatch — " + reasonParts.join(", ")
                       + "; alternative has " + alt.audioLang + "/" + alt.subLang }

    // ── Protected ──
    return { tier: PROTECTED, reason: protectionReason(file) }
```

### Deletion Selection

```
function selectNextFileToDelete():
    if NOT isDeletionNeeded():
        return null

    candidates = []
    for each file in getAllLocalFiles():
        result = classifyFile(file)
        if result.tier != PROTECTED:
            candidates.append(result)

    // Primary sort: tier ascending. Secondary sort: score ascending within tier.
    candidates.sort(by: tier ASC, score ASC)

    if candidates is not empty:
        return candidates[0]
    return null
```

---

## Lock Interaction with Deletion

### Lock Hierarchy

```
Anime lock (deletion_locks.aid = X, eid = NULL)
  └── Protects ALL episodes and ALL files for anime X
       └── Including episodes/files added after the lock was set

Episode lock (deletion_locks.aid = NULL, eid = Y)
  └── Protects ALL files for episode Y
       └── Including files added after the lock was set
```

An anime lock implies all its episodes are locked. If an anime is locked and the user also locks a specific episode, the episode lock is redundant but harmless. Unlocking the anime does NOT remove explicit episode locks — they are independent.

### Lock Resolution for a File

```
// Cache locked IDs at queue rebuild time to avoid per-file queries:
//   m_lockedAnimeIds = SELECT aid FROM deletion_locks WHERE aid IS NOT NULL
//   m_lockedEpisodeIds = SELECT eid FROM deletion_locks WHERE eid IS NOT NULL

function isLocked(aid, eid):
    if aid in m_lockedAnimeIds:
        return true
    if eid in m_lockedEpisodeIds:
        return true
    return false

function lockReason(file):
    if EXISTS in deletion_locks WHERE aid = file.aid AND eid IS NULL:
        lock = query(...)
        reason = "Anime locked"
        if lock.reason is not empty:
            reason += " (" + lock.reason + ")"
        return reason

    if EXISTS in deletion_locks WHERE eid = file.eid:
        lock = query(...)
        reason = "Episode locked"
        if lock.reason is not empty:
            reason += " (" + lock.reason + ")"
        return reason

    return ""
```

### Edge Cases

| Scenario | Behavior |
|----------|----------|
| User locks anime, then unlocks a single episode within it | Episode remains locked via anime lock. To unlock one episode, user must unlock the anime and lock remaining episodes individually — or we add an "exclude episode" feature later. |
| File is the only copy, episode is not locked, anime is not locked | Protected by gap check OR by "only copy of unwatched episode" rule. Lock is not needed. |
| User locks anime that has no files yet | Lock is stored in `deletion_locks`. When files are added, they inherit `deletion_locked = 2` via the propagation logic. |
| User deletes a file manually (context menu) while it is locked | Manual deletion bypasses auto-deletion locks. Locks only protect against the automatic deletion system. The UI should show a confirmation: "This file is locked against auto-deletion. Delete anyway?" |
| Disk space is critically low and all remaining files are locked | System logs a warning and does not delete. The user must manually unlock files or free space by other means. |

---

## Comparison: All Three Approaches

| Aspect | Pure Scoring | Pure Procedural | Hybrid + Locks |
|--------|-------------|----------------|----------------|
| **Tier assignment** | N/A (single score) | Procedural rules | Procedural rules |
| **Intra-tier ordering** | N/A (global score) | Single sort key | Focused scoring (2-4 factors per tier) |
| **User override** | None | Per-file protect | Anime lock + episode lock |
| **"Why this file?"** | Score breakdown (17 rows) | Tier name + reason | Tier name + reason + intra-tier rank |
| **Lock persistence** | N/A | Runtime only | Database-persisted; survives restart |
| **New file handling** | Scored on arrival | Classified on arrival | Classified on arrival; inherits locks |
| **UI complexity** | Score numbers + breakdown | Tier labels + reasons | Tier labels + reasons + lock icons + lock context menus |
| **Code complexity** | 1 method, 17 score lines | 1 method, 6 tier blocks | 1 classifier method + 1 lock table + propagation |
| **Edge case safety** | Weights may cancel out unpredictably | Tiers are absolute | Tiers are absolute + locks are absolute |

---

## UI for Hybrid Approach

### Card Display with Locks and Tier Info

```
┌──────────────────────────────────┐
│ Poster       │ 🔒 Show A         │  ← anime is locked
│              │ Type: TV          │
│              │ Rating: 8.76      │
├──────────────────────────────────┤
│ 🔒 Ep 1 - Intro                 │  ← locked via anime
│   \ v1 1080p HEVC [Group]       │     no deletion indicator needed
│ 🔒 Ep 2 - Adventure             │
│   \ v1 720p AVC [Group]         │
└──────────────────────────────────┘

┌───────────────────────────────────────────────────────┐
│ Poster       │ Anime B                                │
│              │ Type: TV                               │
├───────────────────────────────────────────────────────┤
│ 🔒 Ep 1 - Special                                    │
│   \ v1 1080p HEVC [Group]                            │
│ Ep 2 - Start                                         │
│   \ v1 480p XviD [OldGrp] 🔴  Watched, 30 eps       │
│        (ep 2 → cur ep 32, XviD, 600kbps exp 1200)    │
│   \ v2 1080p HEVC [NewGrp] 🟢  Protected (in buffer) │
│ Ep 3 - Continue                                      │
│   \ v1 720p [Group]       🟡  Unwatched, 5 eps       │
│        behind (ep 3 → cur ep 8, quality medium)       │
└───────────────────────────────────────────────────────┘
```

### Sidebar Deletion Queue with Lock Info

```
┌────────────────────────────────────────┐
│ ▼ Deletion Queue                       │
│   Space: 42 / 500 GB                   │
│   Threshold: 50 GB                     │
│   Locked: 3 anime, 2 eps              │
│                                        │
│   ── Next to delete ──                 │
│   1. old-ep01.avi                      │
│      Tier 0: Superseded by v2          │
│      v1 480p XviD → v2 1080p HEVC     │
│      [🔒 Lock ep] [🔒 Lock anime]      │
│   2. show-ep30.mkv                     │
│      Tier 2: Watched, 30 eps           │
│      ep 2 → cur ep 32, H.264          │
│      850kbps (exp 1200kbps)            │
│      [🔒 Lock ep] [🔒 Lock anime]      │
│   3. dub-ep05.mkv                      │
│      Tier 5: Language mismatch         │
│      audio: Italian (pref: Japanese)   │
│      alt has Japanese/English          │
│      [🔒 Lock ep] [🔒 Lock anime]      │
└────────────────────────────────────────┘
```

### Detail Dialog with Lock Controls

```
File: show-ep30.mkv
Anime: Show B | Episode: 30 | Group: SubGroup
──────────────────────────────────────────────
Classification: Tier 2 — WATCHED, FAR FROM CURRENT
Reason: Watched, 30 episodes from current position.
        ep 2 → current ep 32

Intra-tier ranking: #2 of 15 files in Tier 2
  Distance:          30 eps  (ep 2 → current ep 32)
  Codec:             H.264   (no penalty; modern codecs: HEVC, AV1)
  Bitrate:           850 kbps (expected 1200 kbps for 720p H.264, deviation 29%)
──────────────────────────────────────────────
Lock status: Not locked
  [🔒 Lock this episode]  [🔒 Lock entire anime]
──────────────────────────────────────────────
Gap protection: No (episodes 29 and 31 have files)
Queue position: #2 overall
```

---

## Implementation Classes (Hybrid)

### DeletionLock (value type)
```
struct DeletionLock {
    int id;             // deletion_locks.id
    int aid;            // -1 if episode-level lock (NULL in DB)
    int eid;            // -1 if anime-level lock (NULL in DB)
    qint64 lockedAt;    // Unix timestamp
    QString reason;     // User-provided note

    bool isAnimeLock() const { return aid > 0 && eid < 0; }
    bool isEpisodeLock() const { return eid > 0 && aid < 0; }
};
```

### DeletionLockManager (new class)
Single responsibility: CRUD operations on the `deletion_locks` table + propagation to `mylist.deletion_locked`.
```
class DeletionLockManager {
public:
    void lockAnime(int aid, const QString &reason);
    void unlockAnime(int aid);
    void lockEpisode(int eid, const QString &reason);
    void unlockEpisode(int eid);

    bool isAnimeLocked(int aid) const;
    bool isEpisodeLocked(int eid) const;
    bool isFileLocked(int lid) const;       // Checks both anime and episode locks
    DeletionLock getLock(int aid, int eid) const;

    QList<DeletionLock> allLocks() const;
    int lockedAnimeCount() const;
    int lockedEpisodeCount() const;

signals:
    void lockChanged(int aid, int eid, bool locked);

private:
    void propagateToMylist(int aid, int eid, int lockValue);
    void recalculateMylistLocksForAnime(int aid);    // After anime unlock
    void recalculateMylistLocksForEpisode(int eid);  // After episode unlock
};
```

### HybridDeletionClassifier (new class)
Single responsibility: assigns tier + intra-tier score to a file.
```
class HybridDeletionClassifier {
public:
    explicit HybridDeletionClassifier(
        const DeletionLockManager &lockManager,
        const WatchSessionManager &sessionManager);

    DeletionCandidate classify(int lid) const;

private:
    int calculateTier0Score(int lid) const;  // Superseded revision ordering
    int calculateTier1Score(int lid) const;  // Watched, no session ordering
    int calculateTier2Score(int lid) const;  // Watched, far from current ordering
    int calculateTier3Score(int lid) const;  // Low-quality duplicate ordering
    int calculateTier4Score(int lid) const;  // Unwatched, far behind ordering
    int calculateTier5Score(int lid) const;  // Preference mismatch ordering

    const DeletionLockManager &m_lockManager;
    const WatchSessionManager &m_sessionManager;
};
```

### DeletionQueue (extended from procedural design)
```
class DeletionQueue {
public:
    explicit DeletionQueue(
        HybridDeletionClassifier &classifier,
        DeletionLockManager &lockManager);

    void rebuild();
    const DeletionCandidate* next() const;
    QList<DeletionCandidate> topN(int n) const;

    // Lock actions (delegates to DeletionLockManager + rebuilds queue)
    void lockAnime(int aid, const QString &reason);
    void unlockAnime(int aid);
    void lockEpisode(int eid, const QString &reason);
    void unlockEpisode(int eid);

private:
    QList<DeletionCandidate> m_candidates;
    HybridDeletionClassifier &m_classifier;
    DeletionLockManager &m_lockManager;
};
```

### DeletionCandidate (extended)
```
struct DeletionCandidate {
    int lid;
    int aid;
    int eid;
    int tier;                  // 0-5 or PROTECTED
    int intraTierScore;        // Scoring within the tier
    QString reason;            // Full reason with actual values:
                               // "Watched, 30 eps from current — ep 2 → current ep 32, H.264, bitrate 850kbps (expected 1200kbps)"
                               // "Language mismatch — audio: Italian (preferred: Japanese), sub: none (preferred: English); alternative has Japanese/English"
    QString filePath;
    QString animeName;
    QString episodeLabel;      // "Ep 30 - Title"
    bool gapProtected;
    bool locked;               // True if anime or episode is locked
    QString lockReason;        // "Anime locked (rewatching)"
};
```

---

## Migration Path (Hybrid)

1. **Phase 1 — Lock infrastructure**: Add `deletion_locks` table + `mylist.deletion_locked` column. Implement `DeletionLockManager`. Add lock/unlock to anime card and episode context menus. No changes to deletion logic yet — locks are stored but not enforced.

2. **Phase 2 — Hybrid classifier**: Implement `HybridDeletionClassifier` alongside existing `calculateDeletionScore()`. Add setting: "Deletion strategy: Scoring / Hybrid". When hybrid is selected, `deleteNextEligibleFile()` uses `HybridDeletionClassifier` instead of `calculateDeletionScore()`. Locks are enforced as absolute protection.

3. **Phase 3 — UI integration**: Wire sidebar deletion queue and card indicators to `DeletionQueue`. Show tier + reason + lock status. Lock/unlock buttons in sidebar and detail dialog.

4. **Phase 4 — Remove pure scoring**: Once hybrid is validated, remove `calculateDeletionScore()` and all `SCORE_*` constants. `HybridDeletionClassifier` becomes the sole deletion decision maker.
