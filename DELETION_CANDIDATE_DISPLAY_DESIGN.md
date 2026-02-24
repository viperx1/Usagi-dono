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
  Reason: Superseded files serve no purpose.

Tier 1 — WATCHED, FAR, NO ACTIVE SESSION
  Rule: File is watched AND the anime has no active watch session
        AND the file is not in an anime the user explicitly protected.
  Sort: Largest file size first (reclaim space fastest).
  Reason: Completed viewing, no ongoing interest.

Tier 2 — WATCHED, FAR FROM CURRENT POSITION
  Rule: File is watched AND distance from current episode > aheadBuffer
        AND anime HAS an active session.
  Sort: Distance from current episode descending (farthest first).
  Reason: Already viewed; unlikely to re-watch soon.

Tier 3 — UNWATCHED, LOW QUALITY DUPLICATE
  Rule: File is unwatched AND another file for the same episode exists
        with higher quality/resolution AND that other file is also local.
  Sort: Lower quality first.
  Reason: Better version is available locally.

Tier 4 — UNWATCHED, FAR BEHIND CURRENT POSITION
  Rule: File is unwatched AND episode number < current episode - aheadBuffer
        (i.e., the user has already passed this episode).
  Sort: Distance behind current episode descending.
  Reason: User likely skipped or no longer needs these.

Tier 5 — PREFERENCE MISMATCH
  Rule: File does not match preferred audio language
        AND does not match preferred subtitle language
        AND another file for the same episode exists that does match.
  Sort: Least-matching first (no audio match + no sub match before just no sub match).
  Reason: User has a better-fitting version available.

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
  \ v1 720p [Group]       🟡 Watched, 12 eps from current
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
│      Superseded (v2)    │
│      [Protect]          │
│   2. anime-12.mkv       │
│      Watched, no session│
│      2.4 GB             │
│      [Protect]          │
│   3. anime-03.mkv       │
│      Watched, 47 eps    │
│      from current       │
│      [Protect]          │
└─────────────────────────┘
```

### Detail Dialog
```
File: show-01.avi
Anime: Show A | Episode: 1 | Group: SubGroup
────────────────────────────────────────────
Status: Tier 0 — SUPERSEDED REVISION
Reason: Version 2 exists locally for this episode.
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
    QString reason;       // Human-readable: "Superseded (v2 exists)"
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
