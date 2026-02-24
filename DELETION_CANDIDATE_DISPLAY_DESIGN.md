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

## Recommended Approach: Option 1 + Option 2 Combined

Option 1 (inline indicators on cards) provides contextual, per-file risk visibility without navigation cost. Option 2 (dedicated tab) provides collection-wide overview, bulk operations, and detailed analysis. They complement each other:

- **Option 1** answers: "Is this specific file safe?" — visible while browsing the collection.
- **Option 2** answers: "What will be deleted next across everything?" — visible when managing disk space.

### Implementation Plan

**Phase 1 — Deletion Tab** (Option 2):
The tab is the primary workspace for understanding and controlling deletion behavior. It shows the full ranked candidate list, A vs B prompts, learned weight visualization, lock controls, and space metrics. This is where the user teaches the system.

**Phase 2 — Inline Card Indicators** (Option 1):
Add per-file colored risk icons (🟢🟡🔴) to the episode tree in anime cards. Tooltip shows the tier + reason. Clicking the icon navigates to the file's entry in the Deletion tab for full details. This provides at-a-glance awareness without leaving MyList.

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
Audio (Japanese — preferred)    +30   Japanese (matches preferred)
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

# Key Decision Questions

The deletion system design involves many interacting parts. To cut through the complexity, here are the fundamental questions that need answers:

### Q1: What gets deleted without asking?
**Answer: Only things with objectively better replacements.**
- Superseded file revisions (v1 when v2 exists locally)
- Lower-quality duplicate (480p XviD when 1080p HEVC exists for same episode)
- Language mismatch when a matching alternative exists locally

These are **procedural** — no scoring, no user input needed. If a strictly better local alternative exists, the inferior file can go.

### Q2: How do we decide between files that DON'T have a clearly better replacement?
**Answer: Ask the user, pairwise: "A or B?"**

When the system needs to free space and the remaining candidates are all "legitimate" files (different anime, different episodes, no duplicates), there is no objectively correct answer. The user must choose.

### Q3: How does the system learn from user choices?
**Answer: Each A vs B choice adjusts the weights of individual factors.**

When the user picks "keep A, delete B," the system looks at which factors differ between A and B and adjusts the weight of each differing factor. Over time, the weights converge on the user's actual priorities.

### Q4: What happens before the system has learned anything?
**Answer: All factor weights start at 0. Early choices are essentially random — and that's okay.**

The system presents A vs B pairs anyway. The user's early choices build the weight profile. After ~20-30 choices, the system starts making sensible autonomous decisions.

### Q5: Can the user override the system?
**Answer: Yes — locks prevent auto-deletion; A vs B choices always take priority over autonomous decisions.**

### Q6: What about distance from current episode?
**Answer: Use size-weighted distance (episode distance × file size).**

A 2GB file 10 episodes away is a bigger space opportunity than a 200MB file 30 episodes away.

### Q7: Do factor weights apply globally or per-anime?
**Answer: Globally.** The weights represent the user's general priorities (e.g., "I care more about anime rating than codec quality"). Per-anime preferences emerge naturally because each anime has different factor values.

---

# Deep Analysis: Three Categories of Deletion Factors

## Overview

The current scoring system treats all 17 factors uniformly — each one adds or subtracts from a single integer. But these factors fall into three fundamentally different categories that should be handled differently:

| Category | Examples | Nature | Decision Method |
|----------|----------|--------|-----------------|
| **Technical** | Codec, bitrate, resolution, file version | Objective, measurable | Deterministic rules (no scoring needed) |
| **User Requirements** | Audio language, subtitle language | Fixed constraints (user speaks limited languages) | Hard filter (match/reject, no scoring needed) |
| **Preferences** | Which anime to keep, episode distance, rating | Subjective, elastic, per-title | Requires scoring + user input |

The key insight: **technical factors and user requirements can be resolved with straightforward procedural logic**. Only the preference category genuinely needs a scoring system — and even then, the score should be informed by explicit per-title user preferences rather than inferred from metadata.

---

## Category 1: Technical Factors (Deterministic)

These factors have objectively correct answers. They do not need scoring; they can be resolved with simple rules.

### File Version / Revision
- **Rule**: If a newer local revision exists for the same episode, the older one is always deletable.
- **No scoring needed**: v1 is always inferior to v2 when both are local. This is Tier 0 in the procedural model.

### Video Quality (Resolution + Codec + Bitrate)
- **Rule**: When multiple local files exist for the same episode, the one with lower quality is always the deletion candidate.
- **Quality is a composite**: resolution (480p < 720p < 1080p), codec (XviD < H.264 < HEVC < AV1), bitrate (relative to expected for resolution+codec).
- **No scoring needed**: `qualityScore(file) = resolutionScore + codecScore + bitrateNormalized` produces a single number that can be directly compared. The lower-quality file is always the candidate.
- **Edge case**: Two files with the same quality but different codecs (e.g., HEVC 720p vs AV1 720p) — compare codec efficiency to break the tie.

### File Size (for same quality)
- **Rule**: When two files have the same quality, prefer to delete the larger one (it wastes more space for the same content).
- **No scoring needed**: direct comparison of `file.sizeBytes`.

These factors are resolved **before** any scoring happens. They are used to:
1. Identify unconditionally deletable files (superseded revisions, low-quality duplicates).
2. Break ties when two files cover the same episode.

---

## Category 2: User Requirements (Hard Constraints)

These factors have a fixed, limited set of acceptable values. Users speak a small number of languages; this does not vary by anime or change dynamically.

### Audio Language
- **Constraint**: User has a ranked list of preferred audio languages (e.g., [Japanese, English]).
- **Rule**: If a file's audio does not match any preferred language AND a local alternative with matching audio exists for the same episode → the non-matching file is deletable.
- **No scoring needed**: This is a match/reject filter, not a relative weight. A file in Italian when the user only speaks Japanese and English is always the candidate if a Japanese version exists.
- **Note**: If no matching alternative exists locally, the non-matching file is kept (it is the only copy).

### Subtitle Language
- **Constraint**: Same as audio — ranked list of preferred subtitle languages.
- **Rule**: Same logic. Non-matching subtitle when a matching alternative exists locally → deletable.
- **Combined with audio**: Audio mismatch is worse than subtitle-only mismatch (user cannot understand the content at all vs. can understand but without subtitles).

### Why These Are Not "Scoring" Factors

The current system assigns +30 for matching audio and -40 for non-matching. This implies that matching audio is somehow "worth" 30 points — a meaningless number. In reality, either the user can understand the file or they cannot. A file in a language the user does not speak should only be kept if no alternative exists. This is binary, not weighted.

---

## Category 3: Preferences (Requires Scoring + User Input)

These are the factors that **cannot be resolved by deterministic rules**. They involve subjective user preferences about which anime and episodes to prioritize. This is where a scoring system is genuinely needed — but the score should incorporate explicit user input, not just inferred metadata.

### The Problem with Current Preference Factors

The current system uses anime rating, release group status, and hidden card state as proxies for user preference. But:
- **Anime rating** (AniDB community rating) does not reflect the individual user's attachment to the series.
- **Release group status** (active/stalled/disbanded) is a proxy for future updates, not user preference.
- **Hidden card** is a UI display choice, not a deletion preference.

A user might have a poorly-rated anime they love (guilty pleasure) or a highly-rated anime they have no interest in finishing. The scoring system cannot know this without explicit user input.

### Episode Distance: Episode Count vs. File Size

The current system calculates distance as episode count: "this file is 47 episodes away from the current position." But episode count treats all files equally regardless of size.

**Size-weighted distance** would calculate: "this file's distance, weighted by its size contribution, means deleting it frees more space per unit of 'content distance'."

| Metric | Formula | Behavior |
|--------|---------|----------|
| **Episode distance** | `abs(fileEp - currentEp)` | Treats a 200MB episode the same as a 2GB episode. Optimizes for "content I'm unlikely to watch." |
| **Size-weighted distance** | `abs(fileEp - currentEp) * file.sizeBytes` | Prioritizes freeing space from large files that are also far away. A 2GB file 10 eps away ranks higher than a 200MB file 30 eps away. |
| **Pure size** | `file.sizeBytes` | Ignores content position entirely. Maximizes space reclaimed per deletion but may delete nearby files. |

**Recommendation**: Use **size-weighted distance** as one of the learnable factors. This means:
- Within distance-based scenarios: delete the largest-and-farthest first.
- File size and episode distance combine naturally into a single metric.

This is better than pure episode count because it maximizes space reclaimed from files the user is unlikely to need, and better than pure size because it still respects content proximity.

---

## Pairwise Comparison Learning System (A vs B)

### The Core Idea

Technical factors and language requirements are handled by procedural rules. The remaining question is: **when the system needs to delete and no objective winner exists, which file should go?**

Instead of asking the user to assign abstract scores or like/dislike tags, the system presents a **concrete pairwise choice** before each non-trivial deletion:

```
┌──────────────────────────────────────────────────────────────┐
│ Space needed: 2.1 GB to reach threshold                      │
│                                                              │
│ Which file should be deleted?                                │
│                                                              │
│   [A] naruto-ep03.mkv              [B] dbz-ep45.mkv         │
│   ────────────────────              ────────────────────     │
│   Naruto Shippuden                  Dragon Ball Z            │
│   Episode 3                         Episode 45               │
│   720p H.264, 420 MB                1080p HEVC, 1.8 GB      │
│   Watched, 47 eps from current      Watched, 12 eps from cur│
│   Rating: 8.2                       Rating: 7.8              │
│   Group: Active                     Group: Disbanded         │
│                                                              │
│           [ Delete A ]    [ Delete B ]    [ Skip ]           │
└──────────────────────────────────────────────────────────────┘
```

The user picks one. The system deletes it AND learns from the choice.

### What the System Learns from Each Choice

When the user picks "Delete B" (keep A), the system examines which factors differ between A and B and adjusts the weight of each factor accordingly.

**Example:** User picks "Delete B" (keep Naruto ep3, delete DBZ ep45)

| Factor | File A (kept) | File B (deleted) | Diff direction | Weight adjustment |
|--------|--------------|-----------------|----------------|-------------------|
| Anime rating | 8.2 | 7.8 | A > B | +δ for rating weight (user kept the higher-rated file) |
| Episode distance | 47 eps | 12 eps | A > B (farther) | −δ for distance weight (user kept the farther file → distance matters less) |
| File size | 420 MB | 1.8 GB | A < B | +δ for size weight (user deleted the larger file → size matters) |
| Resolution | 720p | 1080p | A < B | −δ for quality weight (user kept the lower-quality file → quality matters less here) |
| Group status | Active | Disbanded | A > B | +δ for group weight (user kept the active-group file) |

Each factor weight starts at 0 and moves up or down with each choice. The adjustment δ is small (e.g., 0.1) so no single choice dominates.

### The Learning Formula

```
For each factor F that differs between the kept file (K) and the deleted file (D):

  direction = sign(F(K) - F(D))
  // +1 if the kept file scores higher on this factor
  // -1 if the kept file scores lower on this factor

  weight[F] += learningRate * direction

  // learningRate = 0.1 (small, so learning is gradual)
```

**Intuition**: If the user consistently keeps files with higher anime ratings, the weight for "anime rating" gradually becomes positive and large. If the user consistently ignores codec quality (keeping old codecs over new ones), the weight for "codec quality" stays near zero or goes negative.

### Factor Definitions (Learnable)

These are the factors whose weights are learned through A vs B choices. Each factor produces a normalized value for any file:

| Factor | How it's computed | Range | Higher = |
|--------|------------------|-------|----------|
| `anime_rating` | AniDB rating / 1000 | 0.0 – 1.0 | Better rated |
| `episode_distance` | 1.0 - (abs(distance) / maxDistance) | 0.0 – 1.0 | Closer to current |
| `file_size` | 1.0 - (sizeBytes / maxSizeBytes) | 0.0 – 1.0 | Smaller file |
| `group_status` | active=1.0, stalled=0.5, disbanded=0.0 | 0.0 – 1.0 | More active group |
| `watch_recency` | days since last watched, normalized | 0.0 – 1.0 | Watched more recently |
| `session_active` | 1.0 if anime has active session, else 0.0 | 0 or 1 | Has active session |

Note: Technical factors (codec, bitrate, resolution) and language factors are NOT in this list — they are handled procedurally. Only subjective, elastic factors are learned.

### How Factor Weights Produce a Score

Once the system has non-zero weights, it can compute an autonomous deletion score:

```
function deletionScore(file):
    score = 0
    for each factor F:
        score += weight[F] * normalizedValue(F, file)
    return score

// Lower score = more likely to be deleted
// Higher score = more likely to be kept
```

With all weights at 0, every file gets score 0 — effectively random ordering. As weights emerge from user choices, files start ranking meaningfully.

### Cold Start: All Weights at 0

| Phase | Behavior |
|-------|----------|
| **First ~10 deletions** | System has no learned weights. Every A vs B pair is essentially random. User must pick manually each time. Each pick teaches the system. |
| **~10-30 deletions** | Weights are emerging but noisy. The system starts proposing "better" A vs B pairs (pairing files where the expected winner is ambiguous, to maximize learning). Autonomous deletions are still not reliable. |
| **~30+ deletions** | Weights have stabilized. The system can autonomously select deletion candidates that align with user preferences. A vs B prompts become rare — only shown when two candidates are very close in score. |
| **Ongoing** | Every autonomous deletion is implicitly confirmed (user didn't intervene). Occasionally, the system shows an A vs B to refine or verify weights, especially when new anime are added. |

### When to Show A vs B vs. Autonomous Delete

```
function handleDeletionNeeded():
    candidates = classifyAndRank(allLocalFiles)
    top2 = candidates[0], candidates[1]

    // If the top candidate is from a procedural tier (superseded, duplicate, language mismatch),
    // delete it autonomously — no A vs B needed.
    if top2[0].tier <= PROCEDURAL_TIER_MAX:
        delete(top2[0])
        return

    // If factor weights are undertrained (< MIN_CHOICES made), always ask.
    if totalChoicesMade < MIN_CHOICES:
        showAvsB(top2[0], top2[1])
        return

    // If the top two candidates are very close in learned score, ask.
    scoreDiff = abs(top2[0].learnedScore - top2[1].learnedScore)
    if scoreDiff < CONFIDENCE_THRESHOLD:
        showAvsB(top2[0], top2[1])
        return

    // Otherwise, delete autonomously.
    delete(top2[0])
```

### The [Skip] Button

When the user clicks [Skip] on an A vs B prompt:
- Neither file is deleted.
- No weight adjustment happens.
- The system remembers this pair was skipped and does not re-present it immediately.
- Deletion is deferred until space pressure increases or the user manually triggers it.

### Database Schema for Learned Weights

```sql
-- Factor weights learned from user A vs B choices
CREATE TABLE deletion_factor_weights (
    factor TEXT PRIMARY KEY,           -- 'anime_rating', 'episode_distance', etc.
    weight REAL DEFAULT 0.0,           -- Current learned weight
    total_adjustments INTEGER DEFAULT 0 -- How many times this weight was adjusted
);

-- History of A vs B choices (for analysis and potential retraining)
CREATE TABLE deletion_choices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    kept_lid INTEGER,                  -- lid of the file the user chose to keep
    deleted_lid INTEGER,               -- lid of the file the user chose to delete
    kept_factors TEXT,                  -- JSON: {"anime_rating": 0.82, "episode_distance": 0.12, ...}
    deleted_factors TEXT,               -- JSON: {"anime_rating": 0.78, "episode_distance": 0.88, ...}
    chosen_at INTEGER                  -- Unix timestamp
);
CREATE INDEX idx_deletion_choices_time ON deletion_choices(chosen_at);
```

### UI: A vs B in the Deletion Tab

The Deletion tab (Option 2) hosts the A vs B prompt when the system needs input:

```
[Hasher] [MyList] [Deletion] [Settings] [Log]
                      ▲ active

┌──────────────────────────────────────────────────────────────────┐
│ Deletion Management                         Space: 42 / 500 GB  │
│ Threshold: 50 GB │ Mode: Auto │ [▶ Run Now] [⏸ Pause]          │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│ ⚡ Space pressure: need to free 8 GB                             │
│                                                                  │
│ ── Your choice needed ────────────────────────────────────────── │
│                                                                  │
│   [A] naruto-ep03.mkv              [B] dbz-ep45.mkv             │
│   ────────────────────              ────────────────────         │
│   Naruto Shippuden                  Dragon Ball Z                │
│   Episode 3 · 720p H.264           Episode 45 · 1080p HEVC      │
│   420 MB · Watched                  1.8 GB · Watched             │
│   47 eps from current               12 eps from current          │
│   Rating: 8.2 · Group: Active      Rating: 7.8 · Group: Disbanded│
│                                                                  │
│         [ Delete A ]    [ Delete B ]    [ Skip ]                 │
│                                                                  │
│ ── Learned Weights (30 choices made) ─────────────────────────── │
│                                                                  │
│   Factor              Weight    Confidence                       │
│   ─────────────────────────────────────────                      │
│   Anime rating        +0.42     ████████░░  (strong)             │
│   Episode distance    +0.15     ████░░░░░░  (moderate)           │
│   File size           +0.31     ██████░░░░  (moderate)           │
│   Group status        +0.08     ██░░░░░░░░  (weak)              │
│   Watch recency       -0.05     █░░░░░░░░░  (negligible)        │
│   Active session      +0.55     █████████░  (strong)             │
│                                                                  │
│ ── Deletion Queue (autonomous) ───────────────────────────────── │
│                                                                  │
│   #  File              Anime           Tier    Reason            │
│   ─────────────────────────────────────────────────────────────  │
│   1  show-01-v1.avi    Show A          T0     Superseded (v2)    │
│   2  dub-ep05.mkv      Anime C         T5     Lang mismatch      │
│   3  naruto-003.mkv    Naruto           —     Score: 0.23        │
│   4  dbz-045.mkv       Dragon Ball Z    —     Score: 0.31        │
│                                                                  │
│   Items 1-2 will be deleted autonomously (procedural).           │
│   Items 3-4 require your choice (learned scores too close).      │
└──────────────────────────────────────────────────────────────────┘
```

### Integration with Procedural Tiers and Locks

The A vs B learning system works alongside — not instead of — the procedural tier system:

| Mechanism | Scope | Behavior |
|-----------|-------|----------|
| **Lock (🔒)** | Absolute protection | File is never auto-deleted or presented in A vs B |
| **Procedural tiers (T0-T5)** | Deterministic deletions | Files with objectively better replacements are deleted autonomously |
| **A vs B choices** | Preference learning | Only invoked for files where no procedural rule applies and learned scores are close |
| **Learned weights** | Autonomous preference deletions | Once weights are trained, most non-procedural deletions happen automatically |

A locked file never appears in A vs B. A procedurally-deletable file (superseded, duplicate, language mismatch) is deleted without asking. The A vs B prompt only appears for genuinely ambiguous cases — and over time, those cases become rarer as the weights converge.

---

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
    SupersededRevision = 0,    // Tier 0: procedural
    LowQualityDuplicate = 1,   // Tier 1: procedural
    LanguageMismatch = 2,      // Tier 2: procedural
    LearnedPreference = 3,     // Tier 3: A vs B learned scoring
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

The tiers use procedural rules for classification. Locks are the highest-priority protection. Within each deletable tier, files are ordered by the **learned factor weights** from A vs B choices. Procedural tiers handle deterministic cases; the learned scoring handles everything else.

```
── ABSOLUTE PROTECTIONS (checked first, in order) ──

Lock Check:
  Rule: File's anime or episode has an active lock in deletion_locks.
  Result: PROTECTED. No further evaluation.

Gap Check:
  Rule: Deleting this file would create a gap in the episode sequence
        (episodes exist both before and after, no other file covers this episode).
  Result: PROTECTED. No further evaluation.

── PROCEDURAL DELETION TIERS (deterministic, no scoring needed) ──

Tier 0 — SUPERSEDED REVISION
  Rule: A newer local revision exists for the same episode.
  Sort: Oldest revision first, then largest file first.
  Reason: "Superseded by v{N} — this file: v{M} {resolution} {codec}, newer: v{N} {resolution} {codec}"
  Example: "Superseded by v2 — this file: v1 480p XviD, newer: v2 1080p HEVC"

Tier 1 — UNWATCHED LOW-QUALITY DUPLICATE
  Rule: File is unwatched AND another local file for the same episode
        has strictly higher quality/resolution.
  Sort: Largest quality gap first.
  Reason: "Lower quality duplicate — this file: {resolution} {codec} {bitrate}kbps, better: {resolution} {codec} {bitrate}kbps"
  Example: "Lower quality duplicate — this file: 480p XviD 600kbps, better: 1080p HEVC 1500kbps"

Tier 2 — LANGUAGE MISMATCH WITH ALTERNATIVE
  Rule: File does not match audio AND/OR subtitle preferences
        AND a better-matching local alternative exists for the same episode.
  Sort: Mismatch severity (no audio + no sub > no sub only).
  Reason: "Language mismatch — audio: {fileAudio} (preferred: {prefAudio}), sub: {fileSub} (preferred: {prefSub}); alternative has {altAudio}/{altSub}"
  Example: "Language mismatch — audio: Italian (preferred: Japanese), sub: none (preferred: English); alternative has Japanese/English"

── LEARNED DELETION TIER (A vs B scoring) ──

Tier 3 — LEARNED PREFERENCE
  Rule: File does not fall into any procedural tier AND is not protected.
        Includes: watched files far from current, watched files with no session,
        unwatched files far behind current, etc.
  Sort: Learned deletion score (from factor weights trained by A vs B choices).
        Lower score = deleted first.
  A vs B: When the top two candidates in this tier have close scores
          (below confidence threshold), present pairwise choice to user.
  Reason: "Score: {learnedScore} — rating {rating}, distance {distance}, size {size}"
  Example: "Score: 0.23 — rating 8.2, 47 eps from current, 420 MB"

── PROTECTION BOUNDARY ──

Everything else — PROTECTED
  - Unwatched episodes at or ahead of current position
  - Episodes within the ahead buffer
  - Only local copy of an episode with no better alternative
  - Manually locked anime or episodes
```

### Why A vs B Learning Works Better Than Fixed Weights

In the pure procedural model, intra-tier ordering uses fixed sort keys (distance, size). In the original scoring model, 17 hardcoded weights interact unpredictably.

With A vs B learning, the weights for subjective factors are **derived from actual user behavior**. The user never needs to assign abstract numbers — they just answer "which of these two files should go?" and the system figures out what matters to them. Early choices are random; later choices become increasingly aligned with user priorities.

The procedural tiers (0-2) remain deterministic: superseded revisions, low-quality duplicates, and language mismatches need no learning. The A vs B system only activates for Tier 3 — files where no objective rule can decide.

### Tier Assignment + Learned Score Pseudocode

```
function classifyFile(file):

    // ── Absolute protections ──
    if isLocked(file.aid, file.eid):
        return { tier: PROTECTED, reason: lockReason(file) }

    if wouldCreateGap(file):
        return { tier: PROTECTED, reason: "Gap protection" }

    // ── Tier 0: superseded revision ──
    if hasNewerLocalRevision(file.eid, file.version):
        newer = getNewestLocalRevision(file.eid)
        score = file.version * 1000 - file.sizeBytes / (1024*1024)
        return { tier: 0, score: score,
                 reason: "Superseded by v" + newer.version
                       + " — this file: v" + file.version + " " + file.resolution + " " + file.codec
                       + ", newer: v" + newer.version + " " + newer.resolution + " " + newer.codec }

    // ── Tier 1: unwatched low-quality duplicate ──
    if NOT (file.viewed > 0 OR file.localWatched > 0):
        betterDupe = findBetterLocalDuplicate(file)
        if betterDupe:
            qualityGap = betterDupe.qualityScore - file.qualityScore
            score = -qualityGap * 100
            return { tier: 1, score: score,
                     reason: "Lower quality duplicate"
                           + " — this file: " + file.resolution + " " + file.codec + " " + file.bitrate + "kbps"
                           + ", better: " + betterDupe.resolution + " " + betterDupe.codec + " " + betterDupe.bitrate + "kbps" }

    // ── Tier 2: language mismatch with better alternative ──
    audioMatch = matchesPreferredAudio(file)
    subMatch = matchesPreferredSub(file)
    if (NOT audioMatch OR NOT subMatch) AND hasBetterLanguageAlternative(file):
        alt = getBetterLanguageAlternative(file)
        mismatchSeverity = 0
        if NOT audioMatch: mismatchSeverity += 2
        if NOT subMatch: mismatchSeverity += 1
        score = -mismatchSeverity * 1000
        reasonParts = []
        if NOT audioMatch:
            reasonParts.append("audio: " + file.audioLang + " (preferred: " + preferredAudioLang + ")")
        if NOT subMatch:
            reasonParts.append("sub: " + file.subLang + " (preferred: " + preferredSubLang + ")")
        return { tier: 2, score: score,
                 reason: "Language mismatch — " + reasonParts.join(", ")
                       + "; alternative has " + alt.audioLang + "/" + alt.subLang }

    // ── Tier 3: learned preference (A vs B scoring) ──
    // This tier covers all non-procedural candidates:
    //   - Watched files far from current
    //   - Watched files with no active session
    //   - Unwatched files far behind current
    //   - Any other non-protected file
    if isEligibleForDeletion(file):
        score = computeLearnedScore(file)
        return { tier: 3, score: score,
                 reason: formatLearnedReason(file, score) }

    // ── Protected ──
    return { tier: PROTECTED, reason: protectionReason(file) }

function isEligibleForDeletion(file):
    // File must be in a position where deletion makes sense
    isWatched = file.viewed > 0 OR file.localWatched > 0
    session = findActiveWatchSession(file.aid)
    distance = NO_SESSION
    if session:
        distance = file.totalEpisodePosition - session.currentTotalPosition

    // Watched, no active session → eligible
    if isWatched AND NOT session:
        return true

    // Watched, far from current → eligible
    if isWatched AND distance != NO_SESSION AND abs(distance) > aheadBuffer:
        return true

    // Unwatched, far behind current → eligible
    if NOT isWatched AND distance != NO_SESSION AND distance < -aheadBuffer:
        return true

    return false

function computeLearnedScore(file):
    // Compute score from learned factor weights
    score = 0
    weights = getLearnedWeights()  // from deletion_factor_weights table
    factors = normalizeFactors(file)

    for each factor F in factors:
        score += weights[F] * factors[F]

    return score

function formatLearnedReason(file, score):
    weights = getLearnedWeights()
    factors = normalizeFactors(file)

    // Show top 3 contributing factors
    contributions = []
    for each factor F:
        contributions.append({ factor: F, contribution: weights[F] * factors[F] })
    contributions.sort(by: abs(contribution) DESC)
    top3 = contributions[0..2]

    reason = "Score: " + format(score, 2)
    for each c in top3:
        reason += ", " + c.factor + ": " + format(c.contribution, 2)
    return reason
```

### A vs B Selection and Learning

```
function handleDeletionNeeded():
    if NOT isDeletionNeeded():
        return

    candidates = []
    for each file in getAllLocalFiles():
        result = classifyFile(file)
        if result.tier != PROTECTED:
            candidates.append(result)

    candidates.sort(by: tier ASC, score ASC)

    if candidates is empty:
        return

    top = candidates[0]

    // Procedural tiers (0-2): delete autonomously
    if top.tier <= 2:
        delete(top)
        return

    // Learned tier (3): may need A vs B
    totalChoices = countTotalChoices()  // from deletion_choices table
    if totalChoices < MIN_CHOICES_BEFORE_AUTONOMOUS:  // e.g., 20
        // Not enough training data — always ask
        showAvsBPrompt(candidates[0], candidates[1])
        return

    // Enough training — check confidence
    if candidates.length >= 2:
        scoreDiff = abs(candidates[0].score - candidates[1].score)
        if scoreDiff < CONFIDENCE_THRESHOLD:  // e.g., 0.1
            showAvsBPrompt(candidates[0], candidates[1])
            return

    // High confidence — delete autonomously
    delete(top)

function onUserChoice(kept, deleted):
    // Delete the chosen file
    deleteFile(deleted)

    // Learn from the choice
    keptFactors = normalizeFactors(kept)
    deletedFactors = normalizeFactors(deleted)

    for each factor F:
        diff = keptFactors[F] - deletedFactors[F]
        if abs(diff) > 0.01:  // Only learn from factors that differ
            direction = sign(diff)
            adjustWeight(F, LEARNING_RATE * direction)

    // Store choice for history
    storeChoice(kept.lid, deleted.lid, keptFactors, deletedFactors)
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

| Aspect | Pure Scoring | Pure Procedural | Hybrid + A vs B Learning |
|--------|-------------|----------------|--------------------------|
| **Tier assignment** | N/A (single score) | Procedural rules | Procedural rules (T0-T2) |
| **Non-procedural ordering** | N/A (global score) | Single sort key | Learned factor weights from user A vs B choices |
| **User override** | None | Per-file protect | Anime lock + episode lock |
| **User input** | None (developer-set weights) | None | A vs B pairwise choices that train the system |
| **Cold start** | Hardcoded weights (may be wrong) | Hardcoded tier order | All weights at 0; learns from user |
| **"Why this file?"** | Score breakdown (17 rows) | Tier name + reason | Tier name + reason OR learned score with top contributing factors |
| **Lock persistence** | N/A | Runtime only | Database-persisted; survives restart |
| **New file handling** | Scored on arrival | Classified on arrival | Classified on arrival; inherits locks |
| **UI complexity** | Score numbers + breakdown | Tier labels + reasons | Tier labels + A vs B prompt + weight visualization |
| **Code complexity** | 1 method, 17 score lines | 1 method, 6 tier blocks | 1 classifier + 1 learner + A vs B dialog |
| **Edge case safety** | Weights may cancel out unpredictably | Tiers are absolute | Tiers are absolute + locks are absolute + learning is gradual |

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
│   \ v1 480p XviD [OldGrp] 🔴  T0: Superseded by v2  │
│   \ v2 1080p HEVC [NewGrp] 🟢  Protected (in buffer) │
│ Ep 3 - Continue                                      │
│   \ v1 720p [Group]       🟡  Score: 0.23            │
│        (rating +0.18, distance -0.12, size +0.17)     │
└───────────────────────────────────────────────────────┘
```

### Sidebar Deletion Queue

```
┌────────────────────────────────────────┐
│ ▼ Deletion Queue                       │
│   Space: 42 / 500 GB                   │
│   Threshold: 50 GB                     │
│   Locked: 3 anime, 2 eps              │
│   Choices made: 34 (trained)           │
│                                        │
│   ── Auto-delete (procedural) ──       │
│   1. old-ep01.avi                      │
│      T0: Superseded by v2              │
│      [🔒 Lock ep] [🔒 Lock anime]      │
│   2. dub-ep05.mkv                      │
│      T2: Language mismatch             │
│      audio: Italian (pref: Japanese)   │
│      [🔒 Lock ep] [🔒 Lock anime]      │
│                                        │
│   ── Learned (may need A vs B) ──      │
│   3. show-ep30.mkv                     │
│      Score: 0.23                       │
│      [🔒 Lock ep] [🔒 Lock anime]      │
│   4. naruto-003.mkv                    │
│      Score: 0.31                       │
│      [🔒 Lock ep] [🔒 Lock anime]      │
└────────────────────────────────────────┘
```

### Detail Dialog with Lock Controls

```
File: show-ep30.mkv
Anime: Show B | Episode: 30 | Group: SubGroup
──────────────────────────────────────────────
Classification: Tier 3 — LEARNED PREFERENCE
Learned score: 0.23 (lower = deleted sooner)

Factor contributions:
  anime_rating:       +0.18  (8.76 → normalized 0.88, weight +0.42)
  episode_distance:   -0.12  (30 eps away → normalized 0.40, weight +0.15)
  file_size:          +0.17  (420 MB → normalized 0.79, weight +0.31)
  group_status:       +0.04  (active → 1.0, weight +0.08)
  session_active:     +0.00  (yes → 1.0, weight -0.05)
──────────────────────────────────────────────
Lock status: Not locked
  [🔒 Lock this episode]  [🔒 Lock entire anime]
──────────────────────────────────────────────
Gap protection: No (episodes 29 and 31 have files)
Queue position: #3 overall
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
Single responsibility: assigns tier + score to a file. Uses procedural rules for tiers 0-2, learned weights for tier 3.
```
class HybridDeletionClassifier {
public:
    explicit HybridDeletionClassifier(
        const DeletionLockManager &lockManager,
        const FactorWeightLearner &learner,
        const WatchSessionManager &sessionManager);

    DeletionCandidate classify(int lid) const;

private:
    int calculateTier0Score(int lid) const;    // Superseded revision ordering
    int calculateTier1Score(int lid) const;    // Low-quality duplicate ordering
    int calculateTier2Score(int lid) const;    // Language mismatch ordering
    double calculateLearnedScore(int lid) const; // Learned factor weights

    const DeletionLockManager &m_lockManager;
    const FactorWeightLearner &m_learner;
    const WatchSessionManager &m_sessionManager;
};
```

### FactorWeightLearner (new class)
Single responsibility: manages learned factor weights and processes A vs B choices.
```
class FactorWeightLearner {
public:
    // Factor weight access
    double getWeight(const QString &factor) const;
    QMap<QString, double> allWeights() const;
    int totalChoicesMade() const;
    bool isTrained() const;           // totalChoicesMade >= MIN_CHOICES

    // Compute learned score for a file
    double computeScore(int lid) const;
    QMap<QString, double> normalizeFactors(int lid) const;

    // Process a user A vs B choice
    void recordChoice(int keptLid, int deletedLid);

    // Confidence: how far apart are the top two candidates' scores
    double scoreDifference(int lid1, int lid2) const;

signals:
    void weightsUpdated();

private:
    void adjustWeight(const QString &factor, double delta);
    QMap<QString, double> m_weights;   // Loaded from deletion_factor_weights
    double m_learningRate = 0.1;
};
```

### DeletionQueue (updated)
```
class DeletionQueue {
public:
    explicit DeletionQueue(
        HybridDeletionClassifier &classifier,
        DeletionLockManager &lockManager,
        FactorWeightLearner &learner);

    void rebuild();
    const DeletionCandidate* next() const;
    QList<DeletionCandidate> topN(int n) const;

    // Returns true if A vs B prompt is needed for the top candidate
    bool needsUserChoice() const;
    QPair<DeletionCandidate, DeletionCandidate> getAvsBPair() const;

    // Lock actions (delegates to DeletionLockManager + rebuilds queue)
    void lockAnime(int aid, const QString &reason);
    void unlockAnime(int aid);
    void lockEpisode(int eid, const QString &reason);
    void unlockEpisode(int eid);

    // A vs B choice (delegates to FactorWeightLearner + rebuilds queue)
    void recordChoice(int keptLid, int deletedLid);

private:
    QList<DeletionCandidate> m_candidates;
    HybridDeletionClassifier &m_classifier;
    DeletionLockManager &m_lockManager;
    FactorWeightLearner &m_learner;
};
```

### DeletionCandidate (extended)
```
struct DeletionCandidate {
    int lid;
    int aid;
    int eid;
    int tier;                  // 0-3 or PROTECTED
    double learnedScore;       // From factor weights (tier 3 only; 0.0 for procedural tiers)
    QMap<QString, double> factorValues;  // Normalized factor values for this file
    QString reason;            // Full reason with actual values:
                               // "Superseded by v2 — this file: v1 480p XviD, newer: v2 1080p HEVC"
                               // "Score: 0.23 — rating: +0.18, distance: -0.12, size: +0.17"
    QString filePath;
    QString animeName;
    QString episodeLabel;      // "Ep 30 - Title"
    bool gapProtected;
    bool locked;               // True if anime or episode is locked
    QString lockReason;        // "Anime locked (rewatching)"
};
```

---

## Migration Path

1. **Phase 1 — Lock infrastructure**: Add `deletion_locks` table + `mylist.deletion_locked` column. Implement `DeletionLockManager`. Add lock/unlock to anime card and episode context menus. No changes to deletion logic yet — locks are stored but not enforced.

2. **Phase 2 — A vs B learning infrastructure**: Add `deletion_factor_weights` and `deletion_choices` tables. Implement `FactorWeightLearner`. All weights start at 0.

3. **Phase 3 — Hybrid classifier**: Implement `HybridDeletionClassifier` alongside existing `calculateDeletionScore()`. Procedural tiers (0-2) handle deterministic cases. Tier 3 uses learned weights. When too few choices have been made, always present A vs B. `deleteNextEligibleFile()` delegates to `HybridDeletionClassifier`.

4. **Phase 4 — UI integration**: Wire Deletion tab (Option 2) with A vs B prompt, learned weights display, and deletion queue. Wire inline card indicators (Option 1) with tier + reason. Lock controls accessible from both tab and cards.

5. **Phase 5 — Remove pure scoring**: Once the learning system has been validated, remove `calculateDeletionScore()` and all `SCORE_*` constants. `HybridDeletionClassifier` becomes the sole deletion decision maker.
