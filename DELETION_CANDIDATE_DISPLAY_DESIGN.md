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

Gap protection prevents deleting files that would create holes in episode sequences. Gap protection is **cross-anime, bidirectional**: it considers prequel/sequel chains (via `AnimeChain` relation data) in **both directions** (prequels and sequels) and **spans across the entire series** — e.g., if the user has Inuyasha → Final Act → Yashahime → Yashahime S2, gap protection checks all chain boundaries in both directions (Inuyasha↔Final Act, Final Act↔Yashahime, Yashahime↔S2). Deleting the last episode of Inuyasha when the first episode of Final Act exists would create a forward cross-series gap; deleting Final Act ep 1 when Inuyasha ep 167 exists would create a backward cross-series gap.

**Missing intermediate anime** (Q28): If the user has files for Inuyasha and Yashahime but **no local files** for the intermediate Final Act, gap protection still applies — the missing intermediate is treated as a gap. The chain is walked by relation data regardless of which anime have local files: Inuyasha→Final Act→Yashahime is the chain, and the absence of Final Act files means there is already a gap in the collection. Deleting boundary episodes of either Inuyasha or Yashahime would widen that gap, so gap protection prevents it.

**Existing code to reuse (Q23)**: The `AnimeChain` class (`animechain.h`) already handles chain building, expansion via `RelationLookupFunc`, ordering from prequel→sequel, and relation tracking (`m_relations` map of `aid → (prequel_aid, sequel_aid)`). The `wouldCreateGap()` method (`watchsessionmanager.cpp:1772-1896`) already handles intra-anime gap detection (checking if episodes exist both before and after the candidate). For cross-anime extension: `wouldCreateGap()` should be extended to find the `AnimeChain` containing the file's anime, then check boundary episodes between adjacent anime in the chain (last episode of predecessor ↔ first episode of successor). When walking the chain, intermediate anime with no local files are **not skipped** — they count as existing gaps, and boundary episodes of neighboring anime with local files are protected to avoid widening the gap. No new chain logic is needed — reuse `AnimeChain::getAnimeIds()` to walk the ordered chain and check boundary pairs.

Lower score = higher deletion priority.

---

## Option 1: Lock Icons on Anime Cards (Embedded in MyList)

### Description
Add the 🔒 lock icon to the anime card title and episode rows when a lock is active. No per-file risk indicators — the card stays clean. Lock/unlock is done via context menus (right-click on card header or episode row).

### Mockup
```
┌──────────────────────────────────┐
│ Poster       │ 🔒 Title          │  ← lock icon only when anime is locked
│              │ Type: TV          │
│              │ Rating: 8.76      │
├──────────────────────────────────┤
│ 🔒 Ep 1 - Intro                 │  ← lock icon on locked episodes
│   \ v1 1080p HEVC [Group]       │
│ Ep 2 - Adventure                │  ← no icon = not locked
│   \ v1 720p AVC [Group]         │
│ Ep 3 - Battle                   │
│   \ v1 480p XviD [Group]        │
│   \ v2 1080p HEVC [Group]       │
└──────────────────────────────────┘
```

### Pros
- **Zero navigation cost**: visible right where the user already looks at their collection
- **Low UI disruption**: only adds a 🔒 icon when lock is active; card stays clean otherwise
- **Contextual**: user sees the lock status alongside episode info, watch state, and quality metadata

### Cons
- **No deletion info on cards**: user must switch to the Current Choice tab to see what will be deleted next
- **Lock-only**: cards show lock state but not deletion priority or reason

### Implementation Complexity
**Low**. Requires:
- 🔒 icon in card title when anime is locked
- 🔒 icon on episode rows when episode is locked
- Context menu entries for lock/unlock on card header and episode rows

---

## Option 2: Dedicated "Current Choice" Tab

### Description
Add a new top-level tab (alongside MyList, Hasher, Settings, etc.) that shows a sorted table of all files ranked by deletion score. This tab is the single place for reviewing, overriding, and understanding deletion decisions.

### Mockup
```
[Hasher] [MyList] [Current Choice] [Settings] [Log]
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

Option 1 (lock icons on cards) provides at-a-glance lock visibility without navigation cost. Option 2 (dedicated tab) provides collection-wide overview, A vs B learning, deletion queue, and history. They complement each other:

- **Option 1** answers: "Is this anime/episode locked?" — visible while browsing the collection.
- **Option 2** answers: "What will be deleted next? What was deleted before? What has the system learned?" — visible when managing disk space.

### Implementation Plan

**Phase 1 — Current Choice Tab** (Option 2):
The tab is the primary workspace for understanding and controlling deletion behavior. It shows the full ranked candidate list, A vs B prompts, learned weight visualization, deletion history, and space metrics. This is where the user teaches the system.

**Phase 2 — Lock Icons on Cards** (Option 1):
Add 🔒 lock icon to anime card titles and episode rows when a lock is active. Lock/unlock via context menus (right-click on card header or episode row). No per-file risk indicators — cards stay clean.

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
- Lower-quality duplicate when a strictly better file exists for the same episode (based on resolution, codec generation, and bitrate relative to resolution)
- Language mismatch when a better-matching alternative exists locally (based on the user's configured preferred audio and subtitle languages)

These are **procedural** — no scoring, no user input needed. The criteria are deterministic rules based on technical metadata and user-configured language preferences.

### Q2: How do we decide between files that DON'T have a clearly better replacement?
**Answer: Ask the user, pairwise: "A or B?"**

When the system needs to free space and the remaining candidates are all "legitimate" files (different anime, different episodes, no duplicates), there is no objectively correct answer. The user must choose.

### Q3: How does the system learn from user choices?
**Answer: Each A vs B choice adjusts the weights of individual factors.**

When the user picks "keep A, delete B," the system looks at which factors differ between A and B and adjusts the weight of each differing factor. Over time, the weights converge on the user's actual priorities.

### Q4: What happens before the system has learned anything?
**Answer: All factor weights start at 0. Early choices are essentially random — and that's okay.**

The system presents A vs B pairs anyway. The user's early choices build the weight profile. After ~50 choices, the system starts making sensible autonomous decisions.

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

### Episode Distance: Episode Count vs. File Size (Bitrate-Weighted)

The current system calculates distance as episode count: "this file is 47 episodes away from the current position." But episode count treats all files equally regardless of size.

**Size-weighted distance** factors bitrate into the distance calculation. Consider two 100-episode series: one in 480p (~200MB/ep) and one in 4K (~4GB/ep). Deleting a single 4K episode frees 20× more space than a 480p episode. At the same time, deleting a 480p episode barely changes anything — it has lower deletion impact.

| Metric | Formula | Behavior |
|--------|---------|----------|
| **Episode distance** | `abs(fileEp - currentEp)` | Treats a 200MB episode the same as a 4GB episode. Optimizes for "content I'm unlikely to watch." |
| **Size-weighted distance** | `abs(fileEp - currentEp) * file.sizeBytes` | Prioritizes freeing space from large files that are also far away. A 4GB 4K file 10 eps away ranks higher than a 200MB 480p file 30 eps away. Deleting the 4K file has much higher impact on free space. The product (distance × size) is measured in byte-episodes — e.g., 40 GB·eps means "40 GB of content-distance impact." |
| **Pure size** | `file.sizeBytes` | Ignores content position entirely. Maximizes space reclaimed per deletion but may delete nearby files. |

**Recommendation**: Use **size-weighted distance** as the learnable factor. This means:
- **4K/high-bitrate files far from current position** are the first to go — they free the most space per deletion.
- **480p/low-bitrate files** have low deletion impact (freeing 200MB barely changes anything) and naturally rank lower even if far away.
- File size and episode distance combine naturally into a single metric — no need for separate "distance" and "size" factors.
- This captures the user's intuition: "deleting a 4K episode of a show I'm far from is a better trade-off than deleting a small 480p episode of something I'm about to watch."

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
| Size-weighted distance | 47 eps × 420 MB = 19.7 GB·eps | 12 eps × 1.8 GB = 21.6 GB·eps | A < B (B has higher deletion impact) | +δ for size-weighted distance weight (user deleted the file with higher space impact) |
| Group status | Active | Disbanded | A > B | +δ for group weight (user kept the active-group file) |

Note: Resolution (720p vs 1080p) is handled procedurally, not by the learning system. File size and episode distance are combined in `size_weighted_distance` — the 1.8 GB DBZ file at 12 eps distance has a higher deletion impact (21.6 GB·eps) than the 420 MB Naruto file at 47 eps distance (19.7 GB·eps).

Each factor weight starts at 0 and moves up or down with each choice. The adjustment δ is small (e.g., 0.1) so no single choice dominates.

### The Learning Formula

```
For each factor F that differs between the kept file (K) and the deleted file (D):

  diff = F(K) - F(D)
  if abs(diff) < MIN_FACTOR_DIFFERENCE:  // e.g., 0.01
      continue  // Factors that are nearly equal teach us nothing

  direction = sign(diff)
  // +1 if the kept file scores higher on this factor
  // -1 if the kept file scores lower on this factor

  weight[F] += LEARNING_RATE * direction

  // LEARNING_RATE = 0.1 (tunable parameter)
  // MIN_FACTOR_DIFFERENCE = 0.01 (tunable parameter)
```

**Why direction-only, not magnitude-weighted**: A magnitude-weighted formula (`weight[F] += rate * diff`) would let a single large difference dominate the update. The direction-only approach treats each choice as one "vote" per factor. This makes learning more stable: 10 consistent choices in the same direction move a weight by 1.0 regardless of how extreme any individual comparison was. If magnitude-weighting proves beneficial in practice, it can be added later.

**Why 0.1 learning rate**: At 0.1, it takes ~10 consistent choices in the same direction to move a factor weight by 1.0. With 5 factors and assuming each choice updates 2-3 of them, the system needs ~50 total choices before weights stabilize (chosen as a conservative threshold — even with enough data, early weight noise may produce occasional wrong decisions). This is a tunable parameter stored in settings; it can be adjusted if the system learns too slowly or too aggressively.

### Factor Definitions (Learnable)

These are the factors whose weights are learned through A vs B choices. Each factor produces a normalized value for any file:

| Factor | How it's computed | Range | Higher = |
|--------|------------------|-------|----------|
| `anime_rating` | AniDB rating × 100 (integer 0–1000, e.g., 875 = 8.75) / 1000 | 0.0 – 1.0 | Better rated |
| `size_weighted_distance` | 1.0 - normalize(abs(distance) × sizeBytes) | 0.0 – 1.0 | Closer to current AND/OR smaller file. Combines episode distance with file size: a 4K file far away gets a low value (highly deletable), a small file close by gets a high value (worth keeping). See "Episode Distance: Episode Count vs. File Size" section. |
| `group_status` | active=1.0, stalled=0.5, disbanded=0.0 | 0.0 – 1.0 | More active group |
| `watch_recency` | days since last watched, normalized | 0.0 – 1.0 | Watched more recently |
| `view_percentage` | watched episodes / total episodes for the anime's session | 0.0 – 1.0 | More of the anime has been watched |

Note: `episode_distance` and `file_size` are no longer separate factors — they are combined into `size_weighted_distance`. This captures the insight that a 4GB 4K episode frees 20× more space than a 200MB 480p episode, so bitrate/size naturally factors into the distance metric. Technical factors (codec, resolution) and language factors are NOT in this list — they are handled procedurally. Only subjective, elastic factors are learned.

**Why `view_percentage` instead of `session_active`**: A binary "has active session" flag is unreliable because sessions are started automatically when any episode is played — even briefly to check quality. A user who watched 1 of 500 episodes is very different from a user who watched 480 of 500. The view percentage (watched/total in the session) captures actual engagement: a session at 95% means the user is nearly done and those files are dispensable; a session at 2% means the user just started (or just checked) and the files should be treated with lower confidence.

**Handling anime with no session** (never played any episode):

This is ambiguous — the anime could be garbage not worth clicking, or planned for later viewing. Four possible approaches:

| Approach | view_percentage value | Effect on deletion score | Pros | Cons |
|----------|----------------------|--------------------------|------|------|
| **A: Default to 0.0** | 0.0 (fully unwatched) | Low engagement → more deletable if weight is positive | Simple; garbage anime gets cleaned up | Planned-to-watch anime also gets penalized |
| **B: Default to 0.5** | 0.5 (neutral midpoint) | No bias — doesn't push toward keep or delete | Safe default; doesn't favor either direction | Doesn't distinguish between garbage and planned |
| **C: Exclude from score** | N/A — factor skipped | Factor has no influence for this file | Most honest — system admits it doesn't know | Complicates score calculation (variable factor count); files with fewer factors are less comparable |
| **D: Use mylist state** | 0.0 for unknown, 0.3 for "plan to watch" if available | AniDB mylist state provides weak signal | Can distinguish planned-to-watch from truly unknown | Relies on user maintaining mylist state, which many don't |

**Recommendation**: Approach B (default to 0.5). The neutral midpoint ensures that no-session anime is neither favored nor penalized by the view_percentage factor. The 0.5 value is static for the duration that an anime has no session — it does NOT change through learning. However, the view_percentage **weight** still adjusts from A vs B choices involving files that DO have sessions, so the system's overall sensitivity to view_percentage improves over time. For no-session anime, the other 4 factors (rating, size-weighted distance, group, recency) still contribute meaningful scores.

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
| **First ~15 deletions** | System has no learned weights. Every A vs B pair is essentially random. User must pick manually each time. Each pick teaches the system. |
| **~15-50 deletions** | Weights are emerging but noisy. The system starts proposing "better" A vs B pairs (pairing files where the expected winner is ambiguous, to maximize learning). Autonomous deletions are still not reliable. |
| **~50+ deletions** | Weights have stabilized. The system can autonomously select deletion candidates that align with user preferences. A vs B prompts become rare — only shown when two candidates are very close in score. |
| **Ongoing** | Every autonomous deletion is implicitly confirmed (user didn't intervene). Occasionally, the system shows an A vs B to refine or verify weights, especially when new anime are added. |

### When to Request A vs B vs. Autonomous Delete

A vs B choices are never presented as popups. When user input is needed, the system:
1. Loads the A vs B pair into the "A vs B" groupbox in the Current Choice tab.
2. Shows a red exclamation mark (❗) on the tray icon's right half (if threshold hit).
3. Waits until the user opens the Current Choice tab and makes a choice.
4. If space pressure is critical and no choice has been made, the system **does not** force a deletion — it waits.

```
function handleDeletionNeeded():
    candidates = classifyAndRank(allLocalFiles)  // Returns only unlocked candidates (Q29: locked files are in queue but not in A vs B while other candidates exist)

    if candidates is empty:
        return

    // If the top candidate is from a procedural tier (superseded, duplicate, language mismatch),
    // delete it autonomously — no user interaction needed.
    if candidates[0].tier <= PROCEDURAL_TIER_MAX:
        delete(candidates[0])
        return

    // Q17: Single candidate in Tier 3 — show in A vs B groupbox.
    // If a locked file exists for the same episode, pit candidate against it for context;
    // otherwise show as single-file confirmation ("Delete this? [Yes] [Skip]").
    if candidates.length == 1:
        lockedPeer = findLockedPeerForContext(candidates[0])
        if lockedPeer:
            queueAvsBChoice(candidates[0], lockedPeer, peerIsLocked: true)
        else:
            queueSingleConfirmation(candidates[0])
        return

    // If factor weights are undertrained (< 50 choices made), always request choice.
    if totalChoicesMade < MIN_CHOICES:
        queueAvsBChoice(candidates[0], candidates[1])
        return

    // If the top two candidates are very close in learned score, request choice.
    scoreDiff = abs(candidates[0].learnedScore - candidates[1].learnedScore)
    if scoreDiff < CONFIDENCE_THRESHOLD:
        queueAvsBChoice(candidates[0], candidates[1])
        return

    // Otherwise, delete autonomously.
    delete(candidates[0])
```

### The [Skip] Button

When the user clicks [Skip] on an A vs B prompt:
- Neither file is deleted.
- **No weight adjustment happens** — Skip is purely "not now", never a learning signal.
- The system remembers this pair was skipped and does not re-present it immediately.
- Deletion is deferred until space pressure increases or the user manually triggers it.

This applies equally to normal A vs B pairs and locked-peer contexts (Q17). When a single candidate is pitted against a locked file, the purpose is to **bring to the user's attention** that they might want to unlock something — not to train weights. The locked peer comparison is informational, not a learning opportunity.

### Locked File Interaction in Queue (Q26)

When the user clicks a locked file (🔒) in the queue, it is shown alongside its nearest candidate in the A vs B groupbox. The locked file is displayed with [Unlock file] and [Unlock anime] buttons:

```
┌─ A vs B ─────────────────────────────────────────┐
│                                                   │
│ 🔒 Locked file selected from queue               │
│                                                   │
│ [A] bleach-ep01.mkv                              │
│ ────────────────────                              │
│ Bleach                                            │
│ Episode 1 · 1080p HEVC                           │
│ 1.2 GB · Watched                                 │
│ 🔒 Locked (anime-level)                          │
│                                                   │
│ [B] naruto-003.mkv  (nearest candidate)          │
│ ────────────────────                              │
│ Naruto Shippuden                                  │
│ Episode 3 · 720p H.264                           │
│ 420 MB · Watched                                  │
│                                                   │
│ [Unlock file] [Unlock anime] [Back to queue]     │
└───────────────────────────────────────────────────┘
```

The locked file cannot be deleted from this view. The [Unlock file] and [Unlock anime] buttons allow the user to remove the lock if they decide the file/anime should be eligible for deletion. After unlocking, the file re-enters the normal queue ranking. This interaction surfaces locked files to the user's attention without requiring them to navigate to the anime card's context menu.

### Database Schema for Learned Weights

```sql
-- Factor weights learned from user A vs B choices
CREATE TABLE deletion_factor_weights (
    factor TEXT PRIMARY KEY,           -- 'anime_rating', 'size_weighted_distance', etc.
    weight REAL DEFAULT 0.0,           -- Current learned weight
    total_adjustments INTEGER DEFAULT 0 -- How many times this weight was adjusted
);

-- History of A vs B choices (for analysis and potential retraining)
CREATE TABLE deletion_choices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    kept_lid INTEGER,                  -- lid of the file the user chose to keep
    deleted_lid INTEGER,               -- lid of the file the user chose to delete
    kept_factors TEXT,                  -- JSON: {"anime_rating": 0.82, "size_weighted_distance": 0.12, ...}
    deleted_factors TEXT,               -- JSON: {"anime_rating": 0.78, "size_weighted_distance": 0.88, ...}
    chosen_at INTEGER                  -- Unix timestamp
);
CREATE INDEX idx_deletion_choices_time ON deletion_choices(chosen_at);
```

### UI: Current Choice Tab Layout

The Current Choice tab uses a vertical layout with groupboxes — **no sub-tabs**. The layout is:

1. **A vs B** (top-left) + **Weights** (top-right) — side by side
2. **Queue** (middle, full width)
3. **History** (bottom, full width)

Selecting an entry from Queue or History loads its details in the A vs B groupbox.

All A vs B interaction happens **inside the Current Choice tab only** — no popup dialogs, no modal windows. When disk usage hits the threshold and user action is required, the tray icon shows a red exclamation mark (❗) on its right half. The ❗ disappears when space drops below the threshold. The user opens the Current Choice tab at their convenience.

```
[Hasher] [MyList] [Current Choice] [Settings] [Log]
                           ▲ active

┌──────────────────────────────────────────────────────────────────┐
│ Deletion Management              [PREVIEW] Space: 42 / 500 GB  │
│ Threshold: 50 GB │ Mode: Auto │ [▶ Run Now] [⏸ Pause]          │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│ ┌─ A vs B ────────────────────────────┐┌─ Learned Weights ────────┐│
│ │                                   ││                          ││
│ │ ⚡ Choice needed                   ││ Choices: 50 (trained)    ││
│ │                                   ││                          ││
│ │ [A] naruto-003.mkv               ││ Factor        Weight     ││
│ │ ────────────────────              ││ ────────────────────     ││
│ │ Naruto Shippuden                  ││ Anime rating  +0.42 ████││
│ │ Episode 3 · 720p H.264           ││ Size×dist     +0.31 ███ ││
│ │ 420 MB · Watched                  ││ Group status  +0.08 █   ││
│ │ 47 eps from current               ││ Watch recency -0.05 ░   ││
│ │ Rating: 8.2 · Group: Active      ││ View %        +0.55 ████││
│ │                                   ││                          ││
│ │ [B] dbz-045.mkv                  ││ [Reset weights ⚠]       ││
│ │ ────────────────────              │└──────────────────────────┘│
│ │ Dragon Ball Z                     │                             │
│ │ Episode 45 · 1080p HEVC          │                             │
│ │ 1.8 GB · Watched                  │                             │
│ │ 12 eps from current               │                             │
│ │ Rating: 7.8 · Group: Disbanded   │                             │
│ │                                   │                             │
│ │  [ Delete A ]  [ Delete B ]  [ Skip ]                          │
│ └───────────────────────────────────┘                             │
│                                                                  │
│ ┌─ Deletion Queue ──────────────────────────────────────────────┐│
│ │ #  File              Anime           Tier    Reason           ││
│ │ ─────────────────────────────────────────────────────────────  ││
│ │ 1  show-01-v1.avi    Show A          T0     Superseded (v2)  ││
│ │ 2  dub-ep05.mkv      Anime C         T2     Lang mismatch    ││
│ │ 3  naruto-003.mkv    Naruto          T3     Score: 0.23  ←   ││
│ │ 🔒 bleach-ep01.mkv   Bleach          —      Locked (anime)   ││
│ │ 4  dbz-045.mkv       Dragon Ball Z   T3     Score: 0.31  ←   ││
│ │ 🔒 op-ep50.mkv       One Piece       —      Locked (episode) ││
│ │                                                               ││
│ │ Full list of all files. Items 1-2: auto-delete (procedural). ││
│ │ 3-4: loaded above. 🔒: locked files in natural sort position ││
│ │ (Q29) but not shown in A vs B while other candidates exist.  ││
│ │ Click 🔒 entry → shows in A vs B with [Unlock] buttons.      ││
│ └───────────────────────────────────────────────────────────────┘│
│                                                                  │
│ ┌─ Deletion History ────────────────────── Total freed: 234 GB ─┐│
│ │ Filter: [All ▾] [All types ▾]                                 ││
│ │                                                                ││
│ │ Date         File              Anime       Type       Size    ││
│ │ ──────────────────────────────────────────────────────────────  ││
│ │ 2026-02-24   dbz-045.mkv       Dragon Ball  user_avsb  1.8GB ││
│ │ 2026-02-24   naruto-003.mkv    Naruto       learned    420MB ││
│ │ 2026-02-23   show-01-v1.avi    Show A       procedural 2.4GB ││
│ │ 2026-02-23   dub-ep05.mkv      Anime C      procedural 620MB ││
│ │ ...                                                            ││
│ │                                                                ││
│ │ Click entry to load details in A vs B.                        ││
│ └────────────────────────────────────────────────────────────────┘│
└──────────────────────────────────────────────────────────────────┘
```

**Tray icon behavior**: When disk usage hits the deletion threshold and user action is required (A vs B choice pending), the tray icon shows a red exclamation mark (❗) on its right half. The ❗ disappears when space drops below the threshold (deletions freed enough space). No popups, no notifications, no toasts — the user opens the Current Choice tab when convenient. If space pressure is critical and no choice has been made, the system **waits indefinitely** — it never auto-deletes Tier 3 files without user input, regardless of how long the choice has been pending (Q25).

**Preview mode** (Q27): When space is below the threshold (no space pressure), the queue still shows all ranked candidates (Q18/Q24). A **[PREVIEW]** label is shown inside the Current Choice tab header area (next to the space indicator) to indicate that no urgent deletions are needed. The tray icon also shows the ❗ when in active deletion mode (threshold exceeded). When in preview mode, the ❗ is absent and the [PREVIEW] label is visible. No other visual distinction is needed. If the user makes an A vs B choice in preview mode, the **file is actually deleted** — the user explicitly chose to delete it, and the system honors that choice. Weight adjustments are recorded as usual (Q22).

### Integration with Procedural Tiers and Locks

The A vs B learning system works alongside — not instead of — the procedural tier system:

| Mechanism | Scope | Behavior |
|-----------|-------|----------|
| **Lock (🔒)** | Protect highest-rated file | Locked episode/anime keeps 1 best file; duplicates still eligible for deletion and A vs B |
| **Procedural tiers (T0-T2)** | Deterministic deletions | Files with objectively better replacements are deleted autonomously |
| **A vs B choices** | Preference learning | User makes choices in the Current Choice tab when convenient; tray icon signals pending choice |
| **Learned weights** | Autonomous preference deletions | Once weights are trained (50+ choices), most non-procedural deletions happen automatically |

The highest-rated file of a locked episode never appears in A vs B and is never auto-deleted. Lower-rated duplicates of locked episodes ARE eligible for deletion — the lock ensures at least 1 file remains, not that all files are preserved. A procedurally-deletable file (superseded, duplicate, language mismatch) is deleted without asking. The A vs B prompt only appears in the Current Choice tab for genuinely ambiguous cases — and over time, those cases become rarer as the weights converge.

---

# Hybrid Approach: Procedural Tiers + A vs B Learning + Manual Locks

## Motivation

The pure scoring system is hard to reason about (17 interacting weights). The pure procedural system is easy to reason about but loses the fine-grained ordering that scoring provides within a tier — when two watched files are both far from the current position, codec quality and bitrate matter for picking which one to delete first, and a single sort key cannot capture that.

The hybrid approach uses **procedural rules for tier assignment** (the "why") and **scoring for intra-tier ordering** (the "which one first"). Manual locks give the user a hard override that no algorithm can bypass.

---

## Manual Lock Mechanism

### What Can Be Locked

| Lock Target | Effect | Granularity |
|-------------|--------|-------------|
| **Anime lock** | The highest-rated file for each episode of this anime is protected. Duplicates (lower-rated files for the same episode) are still eligible for deletion. | Entire anime (all episodes) |
| **Episode lock** | The highest-rated file for this episode is protected. Duplicates are still eligible for deletion. | Single episode |

Locks are simple on/off — no reason or notes. A locked item stays locked until the user unlocks it.

**Key behavior**: A lock guarantees that **at least 1 file** (the highest-rated) remains for the locked episode/anime. It does NOT prevent deletion of duplicate files for the same episode. This means a locked episode with 3 files (v1, v2, v3) can still have v1 and v2 deleted — only v3 (highest-rated) is protected. See Q12 for how "highest-rated" is determined.

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
    if isLocked(file):  // checks lock + highest-rated-for-episode
        return { tier: PROTECTED, reason: lockReason(file) }

    if wouldCreateGap(file):
        if isIntraAnimeGap(file):
            return { tier: PROTECTED, reason: "Gap protection (within anime)" }
        else:
            return { tier: PROTECTED, reason: "Gap protection (cross-anime chain: "
                   + getChainDescription(file) + ")" }

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

    // Q17: Single Tier 3 candidate — show in A vs B, optionally pit against locked peer.
    // Note: procedural candidates (tier 0-2) are auto-deleted above, so by this point
    // only Tier 3 candidates remain. If only one Tier 3 candidate exists, pit it against
    // a locked peer for context, or show as single-file confirmation.
    tier3Candidates = candidates.filter(c => c.tier == 3)
    if tier3Candidates.length == 1:
        lockedPeer = findLockedPeerForContext(candidates[0])
        if lockedPeer:
            queueAvsBChoice(candidates[0], lockedPeer, peerIsLocked: true)
        else:
            queueSingleConfirmation(candidates[0])
        return

    // Learned tier (3): may need A vs B
    totalChoices = countTotalChoices()  // from deletion_choices table
    if totalChoices < MIN_CHOICES_BEFORE_AUTONOMOUS:  // 50
        // Not enough training data — always request choice
        queueAvsBChoice(candidates[0], candidates[1])
        return

    // Enough training — check confidence
    if candidates.length >= 2:
        scoreDiff = abs(candidates[0].score - candidates[1].score)
        if scoreDiff < CONFIDENCE_THRESHOLD:  // e.g., 0.1
            queueAvsBChoice(candidates[0], candidates[1])
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
        if abs(diff) > MIN_FACTOR_DIFFERENCE:  // Only learn from factors that meaningfully differ
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
  └── For EACH episode of anime X, protects the HIGHEST-RATED file
       └── Lower-rated duplicates for the same episode are still eligible for deletion
       └── New episodes/files added after the lock are covered automatically

Episode lock (deletion_locks.aid = NULL, eid = Y)
  └── Protects the HIGHEST-RATED file for episode Y
       └── Lower-rated duplicates for the same episode are still eligible for deletion
       └── New files added after the lock inherit lock status; the "highest rated"
           is re-evaluated when files change
```

An anime lock implies all its episodes are locked. If an anime is locked and the user also locks a specific episode, the episode lock is redundant but harmless. Unlocking the anime does NOT remove explicit episode locks — they are independent.

### Lock Resolution for a File

A lock protects a file only if it is the **highest-rated file** for that episode. Duplicates (lower-rated files for the same locked episode) are not protected.

```
// Cache locked IDs at queue rebuild time to avoid per-file queries:
//   m_lockedAnimeIds = SELECT aid FROM deletion_locks WHERE aid IS NOT NULL
//   m_lockedEpisodeIds = SELECT eid FROM deletion_locks WHERE eid IS NOT NULL

function isLocked(file):
    // Check if the file's anime or episode has a lock
    hasLock = (file.aid in m_lockedAnimeIds) OR (file.eid in m_lockedEpisodeIds)
    if NOT hasLock:
        return false

    // Lock only protects the highest-rated file for this episode.
    // If this file is NOT the highest-rated for its episode, it's not protected.
    return isHighestRatedFileForEpisode(file.lid, file.eid)

function isHighestRatedFileForEpisode(lid, eid):
    // Find all local files for this episode, rank by rating criteria (see Q12 for definition)
    files = getLocalFilesForEpisode(eid)
    if files.isEmpty():
        return false
    bestFile = files.sortByRatingCriteria(DESC).first()  // Q12+Q16: language match first (sub > audio), then full composite score
    return bestFile.lid == lid

function lockReason(file):
    if EXISTS in deletion_locks WHERE aid = file.aid AND eid IS NULL:
        return "Anime locked (highest rated kept)"

    if EXISTS in deletion_locks WHERE eid = file.eid:
        return "Episode locked (highest rated kept)"

    return ""
```

### Edge Cases

| Scenario | Behavior |
|----------|----------|
| Locked episode has 3 files (ep05-480p.avi, ep05-720p.mkv, ep05-1080p.mkv) | Only ep05-1080p.mkv (highest-rated per Q12) is protected. The other two are eligible for deletion as duplicates. |
| New file arrives for locked episode that is higher-rated | New file becomes the "highest-rated" and is now protected. The previously protected file becomes eligible. |
| User locks anime, then unlocks a single episode within it | Episode remains locked via anime lock. To unlock one episode, user must unlock the anime and lock remaining episodes individually — or we add an "exclude episode" feature later. |
| File is the only copy, episode is not locked, anime is not locked | Protected by gap check OR by "only copy of unwatched episode" rule. Lock is not needed. |
| User locks anime that has no files yet | Lock is stored in `deletion_locks`. When files are added, the highest-rated file per episode is protected. |
| User deletes a file manually (context menu) while it is locked | Manual deletion bypasses auto-deletion locks. Locks only protect against the automatic deletion system. The UI should show a confirmation: "This file is locked against auto-deletion. Delete anyway?" |
| Disk space is critically low and all remaining files are locked (each is the sole/highest file) | System logs a warning and does not delete. The user must manually unlock files or free space by other means. |

---

## Comparison: Why Hybrid Was Chosen (vs. alternatives considered)

| Aspect | Pure Scoring (current) | Pure Procedural (considered) | **Hybrid + A vs B Learning (chosen)** |
|--------|-------------|----------------|--------------------------|
| **Tier assignment** | N/A (single score) | Procedural rules | Procedural rules (T0-T2) |
| **Non-procedural ordering** | N/A (global score) | Single sort key | Learned factor weights from user A vs B choices |
| **User override** | None | Per-file protect | Anime/episode lock — keeps 1 highest-rated file per episode (context menu) |
| **User input** | None (developer-set weights) | None | A vs B pairwise choices that train the system |
| **Cold start** | Hardcoded weights (may be wrong) | Hardcoded tier order | All weights at 0; learns from user |
| **"Why this file?"** | Score breakdown (17 rows) | Tier name + reason | Tier name + reason OR learned score with top contributing factors |
| **Lock persistence** | N/A | Runtime only | Database-persisted; survives restart |
| **History** | None | None | Full deletion history (all types, searchable, auditable) |
| **UI complexity** | Score numbers + breakdown | Tier labels + reasons | Tier labels + A vs B prompt + weight visualization + history |
| **Code complexity** | 1 method, 17 score lines | 1 method, 6 tier blocks | 1 classifier + 1 learner + A vs B dialog + history manager |
| **Edge case safety** | Weights may cancel out unpredictably | Tiers are absolute | Tiers are absolute + locks are absolute + learning is gradual |

---

## UI for Hybrid Approach

### Card Display with Lock Icons

Cards only show the 🔒 icon when a lock is active. No per-file risk indicators.

```
┌──────────────────────────────────┐
│ Poster       │ 🔒 Show A         │  ← anime is locked
│              │ Type: TV          │
│              │ Rating: 8.76      │
├──────────────────────────────────┤
│ 🔒 Ep 1 - Intro                 │  ← locked via anime
│   \ v1 1080p HEVC [Group]       │
│ 🔒 Ep 2 - Adventure             │
│   \ v1 720p AVC [Group]         │
└──────────────────────────────────┘

┌──────────────────────────────────┐
│ Poster       │ Anime B           │  ← no lock icon = not locked
│              │ Type: TV          │
├──────────────────────────────────┤
│ 🔒 Ep 1 - Special               │  ← episode-level lock
│   \ v1 1080p HEVC [Group]       │
│ Ep 2 - Start                    │  ← not locked, no icon
│   \ v1 480p XviD [OldGrp]      │
│   \ v2 1080p HEVC [NewGrp]     │
│ Ep 3 - Continue                 │
│   \ v1 720p [Group]            │
└──────────────────────────────────┘
```

Lock/unlock is done via right-click context menus on the card header (anime-level) or episode rows (episode-level). See [UI for Locking](#ui-for-locking) above.

### Sidebar Deletion Queue

**Removed**: The sidebar queue was replaced by the Queue groupbox in the Current Choice tab (see [Current Choice Tab Layout](#ui-current-choice-tab-layout) above). All deletion information is consolidated in the Current Choice tab.

### Detail View (in A vs B groupbox)

When a queue or history entry is selected, its details appear in the A vs B groupbox at the top of the Current Choice tab:

```
┌─ A vs B ──────────────────────────────────────────────────────┐
│                                                                │
│ File: show-ep30.mkv                                            │
│ Anime: Show B | Episode: 30 | Group: SubGroup                 │
│ Classification: Tier 3 — LEARNED PREFERENCE                   │
│ Learned score: 0.23 (lower = deleted sooner)                   │
│                                                                │
│ Factor contributions:                                          │
│   anime_rating:           +0.18  (8.76 → 0.88, w: +0.42)     │
│   size_weighted_distance: +0.15  (30 eps × 420 MB = 12.3     │
│                            GB·eps → 0.47 normalized, w: +0.31)│
│   group_status:           +0.04  (active → 1.0, w: +0.08)    │
│   view_percentage:        +0.00  (96% → 0.96, w: -0.05)      │
│                                                                │
│ Lock status: Not locked                                        │
│ Gap protection: No (episodes 29 and 31 have files)             │
│ Queue position: #3 overall                                     │
└────────────────────────────────────────────────────────────────┘
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

    bool isAnimeLock() const { return aid > 0 && eid < 0; }
    bool isEpisodeLock() const { return eid > 0 && aid < 0; }
};
```

### DeletionLockManager (new class)
Single responsibility: CRUD operations on the `deletion_locks` table + propagation to `mylist.deletion_locked`.
```
class DeletionLockManager {
public:
    void lockAnime(int aid);
    void unlockAnime(int aid);
    void lockEpisode(int eid);
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
    bool isTrained() const;           // totalChoicesMade >= 50

    // Compute learned score for a file
    double computeScore(int lid) const;
    QMap<QString, double> normalizeFactors(int lid) const;

    // Process a user A vs B choice
    void recordChoice(int keptLid, int deletedLid);

    // Reset all weights to 0 and clear choice history (full clean slate).
    // UI must show double confirmation before calling this.
    void resetAllWeights();

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

    void rebuild();                     // Classifies ALL local files: populates m_candidates (T0-T3) and m_lockedFiles
    const DeletionCandidate* next() const;
    QList<DeletionCandidate> allCandidates() const;  // Returns m_candidates + m_lockedFiles combined; locked entries have locked=true in DeletionCandidate

    // Returns true if A vs B prompt is needed for the top candidate
    bool needsUserChoice() const;
    QPair<DeletionCandidate, DeletionCandidate> getAvsBPair() const;

    // Lock actions (delegates to DeletionLockManager + rebuilds queue)
    void lockAnime(int aid);
    void unlockAnime(int aid);
    void lockEpisode(int eid);
    void unlockEpisode(int eid);

    // A vs B choice — always deletes the chosen file (even in preview mode)
    // and records weight adjustment (Q22)
    void recordChoice(int keptLid, int deletedLid);

private:
    QList<DeletionCandidate> m_candidates;      // Deletable candidates (T0-T3), sorted by tier+score
    QList<DeletionCandidate> m_lockedFiles;      // Locked files shown for visibility (Q21)
    // Invariant: m_candidates and m_lockedFiles are disjoint (no file in both).
    // rebuild() is the sole method that populates both lists from scratch.
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
                               // "Score: 0.23 — rating: +0.18, size×dist: +0.05, group: +0.04"
    QString filePath;
    QString animeName;
    QString episodeLabel;      // "Ep 30 - Title"
    bool gapProtected;
    bool locked;               // True if anime or episode is locked
};
```

---

## Migration Path

1. **Phase 1 — Lock infrastructure**: Add `deletion_locks` table + `mylist.deletion_locked` column. Implement `DeletionLockManager`. Add lock/unlock to anime card and episode context menus. No changes to deletion logic yet — locks are stored but not enforced.

2. **Phase 2 — A vs B learning infrastructure**: Add `deletion_factor_weights` and `deletion_choices` tables. Implement `FactorWeightLearner`. All weights start at 0.

3. **Phase 3 — Hybrid classifier**: Implement `HybridDeletionClassifier` alongside existing `calculateDeletionScore()`. Procedural tiers (0-2) handle deterministic cases. Tier 3 uses learned weights. When too few choices have been made, always present A vs B. `deleteNextEligibleFile()` delegates to `HybridDeletionClassifier`.

4. **Phase 4 — UI integration**: Wire Current Choice tab with A vs B prompt, learned weights display, deletion queue, and deletion history. Add 🔒 lock icons to anime cards (title + episode rows) when locks are active. Lock/unlock accessible via context menus on card headers and episode rows.

5. **Phase 5 — Remove pure scoring**: Once the learning system has been validated, remove `calculateDeletionScore()` and all `SCORE_*` constants. `HybridDeletionClassifier` becomes the sole deletion decision maker.

---

## Deletion History

### Purpose

The Current Choice tab includes a full history of all deletions — both autonomous (procedural and learned) and user-directed (A vs B choices). This lets the user:
1. **Review past decisions**: see exactly which files were deleted, when, and why.
2. **Spot bad patterns**: if the system keeps deleting files from an anime the user cares about, the pattern is visible.
3. **Reverse learning mistakes**: if an early A vs B choice was wrong, the user can see it and make correcting choices going forward.
4. **Audit space reclamation**: see how much space was freed over time.

### Database Schema

The `deletion_choices` table already stores A vs B user choices (see above). A separate table stores ALL deletions:

```sql
-- Full deletion history (every file ever auto-deleted or user-deleted)
CREATE TABLE deletion_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    lid INTEGER,                      -- lid of the deleted file
    aid INTEGER,                      -- anime ID
    eid INTEGER,                      -- episode ID
    file_path TEXT,                   -- path at time of deletion (file is gone, path is archived)
    anime_name TEXT,                  -- anime name at time of deletion
    episode_label TEXT,               -- "Ep 30 - Title" at time of deletion
    file_size INTEGER,                -- file size in bytes at time of deletion
    tier INTEGER,                     -- tier at time of deletion (0-3)
    reason TEXT,                      -- full reason string at time of deletion
    learned_score REAL,               -- learned score (tier 3 only; NULL for procedural)
    deletion_type TEXT,               -- 'procedural', 'learned_auto', 'user_avsb', 'manual'
    space_before INTEGER,             -- total used space before deletion (bytes)
    space_after INTEGER,              -- total used space after deletion (bytes)
    deleted_at INTEGER                -- Unix timestamp
);
CREATE INDEX idx_deletion_history_time ON deletion_history(deleted_at);
CREATE INDEX idx_deletion_history_aid ON deletion_history(aid);
CREATE INDEX idx_deletion_history_type ON deletion_history(deletion_type);
```

**`deletion_type` values**:
- `procedural`: auto-deleted via tier 0-2 (superseded, duplicate, language mismatch)
- `learned_auto`: auto-deleted via tier 3 (system was confident enough to act autonomously)
- `user_avsb`: deleted via user A vs B choice
- `manual`: deleted via manual user action (context menu delete)

**Retention limit**: Maximum 5000 entries. When a new entry is added and the count exceeds 5000, the oldest entries beyond the limit are deleted in a batch. This prevents unbounded database growth while keeping a substantial audit trail.

### UI: Deletion History Groupbox

The History groupbox is at the bottom of the Current Choice tab. Clicking a history entry loads its details in the A vs B groupbox at the top. See the [Current Choice Tab Layout](#ui-current-choice-tab-layout) mockup above.

### History Features

| Feature | Description |
|---------|-------------|
| **Filter by anime** | Show only deletions for a specific anime |
| **Filter by type** | Show only procedural / learned / user / manual deletions |
| **Sort** | By date (default), size, anime, tier |
| **Click to view** | Click a row to load full details in the A vs B groupbox |
| **Space summary** | Total space freed shown in groupbox header |
| **Retention** | Oldest entries pruned beyond 5000 |

### DeletionHistoryManager (new class)

```
class DeletionHistoryManager {
public:
    static constexpr int MAX_ENTRIES = 5000;

    // Record a deletion (prunes oldest entry if count > MAX_ENTRIES)
    void recordDeletion(int lid, int tier, const QString &reason,
                        double learnedScore, const QString &deletionType,
                        qint64 spaceBefore, qint64 spaceAfter);

    // Query history
    QList<DeletionHistoryEntry> allEntries(int limit = 100, int offset = 0) const;
    QList<DeletionHistoryEntry> entriesForAnime(int aid) const;
    QList<DeletionHistoryEntry> entriesByType(const QString &type) const;

    // Statistics
    qint64 totalSpaceFreed() const;
    int totalDeletions() const;

signals:
    void entryAdded(int historyId);

private:
    void pruneOldest();  // DELETE FROM deletion_history WHERE id IN (SELECT id FROM deletion_history ORDER BY deleted_at ASC LIMIT MAX(0, (SELECT COUNT(*) FROM deletion_history) - 5000))
};
```

### DeletionHistoryEntry (value type)

```
struct DeletionHistoryEntry {
    int id;
    int lid;
    int aid;
    int eid;
    QString filePath;
    QString animeName;
    QString episodeLabel;
    qint64 fileSize;
    int tier;
    QString reason;
    double learnedScore;       // -1.0 if not applicable (procedural tiers)
    QString deletionType;      // "procedural", "learned_auto", "user_avsb", "manual"
    qint64 spaceBefore;
    qint64 spaceAfter;
    qint64 deletedAt;          // Unix timestamp
};
```

---

## Resolved Design Decisions

| # | Question | Decision |
|---|----------|----------|
| Q1 | Gap protection scope | **Cross-anime, full chain**: uses `AnimeChain` prequel/sequel relations to prevent gaps across the entire series chain |
| Q2 | Lock reason / notes | **Simple on/off**: no reason field, no notes |
| Q3 | Min A vs B choices before autonomy | **50** (conservative; even 1000 could produce wrong decisions, so this is an acknowledged arbitrary value) |
| Q4 | Current Choice tab layout | **Vertical groupboxes**: A vs B + Weights (top), Queue (middle), History (bottom). No sub-tabs. |
| Q5 | Anime with no session | **Default to 0.5** (neutral midpoint): no-session anime is neither favored nor penalized by view_percentage. Other factors still contribute. |
| Q6 | A vs B during playback | **No popups ever**: all deletion interaction stays in Current Choice tab. Tray icon shows red ❗ when threshold is hit and user action needed. User opens tab at their convenience. |
| Q7 | Weight reset | **Yes, with double warning**: two confirmation dialogs before resetting all weights to 0 AND clearing the A vs B choice history (full clean slate) |
| Q8 | History retention | **5000 entries max**: oldest entries pruned when limit exceeded |
| Q9 | Procedural deletion notifications | **No notifications**: history is the record. No popups, no toasts, no system notifications (user has notifications silenced). |
| Q10 | Lock + new files | **Lock keeps 1 highest-rated file per episode**: new files for locked episodes are covered by the lock, but only the highest-rated file is protected. Duplicates of locked episodes are still eligible for deletion. |
| Q11 | No-session anime default | **Approach B (default to 0.5)**: neutral midpoint, so no-session anime is neither favored nor penalized by view_percentage |
| Q12 | "Highest-rated file" criteria for locks | **Option C (full composite score) with language as top priority**: subtitles first, then audio, then full composite score. Language always trumps quality. See Q16 for detailed sort order. |
| Q13 | Cross-anime gap protection depth | **Full chain, both directions**: gap protection spans the entire prequel→sequel chain, checking both prequels and sequels. All chain boundaries are checked bidirectionally. |
| Q14 | Weight reset scope | **Full clean slate**: reset all weights to 0 AND clear the A vs B choice history (`deletion_choices` table). Double warning required. |
| Q16 | "Highest-rated" sort criteria — language priority | **Subtitles first, then audio, language always trumps quality**: `sortByRatingCriteria()` sorts by (1) subtitle match (yes/no), (2) audio match (yes/no), (3) composite quality score. A file matching both sub+audio ranks above sub-only, sub-only ranks above audio-only, audio-only ranks above no match. Within each language tier, composite quality breaks ties. |
| Q17 | A vs B with only one Tier 3 candidate | **Show in A vs B groupbox**: pit candidate against a locked peer file for context (if one exists); otherwise show as single-file confirmation ("Delete this? [Yes] [Skip]"). The locked peer is shown read-only (cannot be deleted via this prompt). |
| Q18 | Queue behavior when space is sufficient | **Always show ranked list**: the queue always shows what WOULD be deleted, even when space is below threshold. Users can still make A vs B choices to train weights proactively. |
| Q19 | Cross-anime gap — chain direction | **Both directions**: check prequels AND sequels. Checking only one direction WILL create gaps. |
| Q20 | Red ❗ clear condition | **Threshold-based**: ❗ appears when threshold is hit and user action is required; disappears when space is freed below threshold (deletions cleared enough space). |
| Q21 | Single-candidate A vs B — locked peer learning | **Skip never affects weights** (same as all other Skips per [Skip] button rules). Locked peer is shown for **user awareness** — to bring attention that they might need to unlock something. The locked-peer comparison is informational, not a learning opportunity. |
| Q22 | Preview mode A vs B — delete or just learn? | **Delete the file**: the user explicitly chose to delete it; honor the choice. Weight adjustment is recorded as usual. There is no "training-only" mode — every A vs B choice is real. |
| Q23 | Gap protection — chain boundary definition | **Reuse existing code**: `AnimeChain` class handles chain building/expansion/relation tracking. `wouldCreateGap()` handles intra-anime gap detection. For cross-anime: extend `wouldCreateGap()` to use `AnimeChain::getAnimeIds()` to walk the ordered chain and check boundary episodes (last ep of predecessor ↔ first ep of successor) between adjacent anime. No new chain logic needed. |
| Q24 | Queue size limit | **Full list, no limit**: existing score calculation already iterates all local files. The new `classifyFile()` will do the same. Queue shows every file (deletable + locked), scrollable. UI can use virtual scrolling for large collections if needed. |
| Q25 | A vs B choice timeout | **Wait indefinitely**: never auto-delete Tier 3 without user input, no matter how long the choice has been pending. Tray icon ❗ stays visible. User opens Current Choice tab when ready. |
| Q26 | Locked files in queue — display and interaction | **Show alongside candidate with unlock buttons**: clicking a locked file in the queue loads it in the A vs B groupbox alongside its nearest candidate. Buttons: [Unlock file], [Unlock anime], [Back to queue]. This lets the user unlock directly from the Current Choice tab without navigating to the anime card's context menu. |
| Q27 | Preview mode — visual distinction | **[PREVIEW] label inside the Current Choice tab** + tray ❗ absent. Nothing more — no badges, no color changes, just the label and the absence of the tray exclamation mark. |
| Q28 | Cross-anime gap — missing intermediate anime | **Missing intermediate = gap exists**: if Inuyasha and Yashahime have local files but Final Act does not, the missing intermediate is treated as a gap. Boundary episodes of neighboring anime with local files are protected to avoid widening the gap. Chain is walked by relation data regardless of which anime have local files. |
| Q29 | Queue sorting — locked files position | **Natural position**: locked files are sorted by what their tier/score would be if they weren't locked. They are NOT shown in A vs B as long as other unlocked candidates exist. |
| Q30 | Weight learning — factor value normalization | **Bitrate factored into distance**: `size_weighted_distance = abs(distance) × sizeBytes`. Replaces separate `episode_distance` and `file_size` factors. 4K episodes free 20× more space than 480p episodes, so they have higher deletion impact. Normalization method deferred to Q31. |

---

## Follow-Up Questions

All previous questions (Q1-Q30) have been resolved. See the Resolved Design Decisions table above for the full list.

### Q31: Size-weighted distance normalization — global vs per-collection
The `size_weighted_distance` factor is `abs(distance) × sizeBytes`. To normalize to 0-1 range, we need a maximum value. Should this be:
- **A**: Global maximum across all files: `max(abs(distance) × sizeBytes)` in the current collection. Changes as collection grows.
- **B**: Fixed assumed maximum (e.g., 500 eps × 10 GB = 5 TB·eps). Stable but arbitrary.
Which approach? If A, should the normalization recalculate every time the collection changes?

### Q32: Locked file unlock from A vs B — immediate queue rebuild?
When the user unlocks a file/anime from the A vs B groupbox (Q26), should the queue immediately rebuild to reflect the newly unlocked files entering the candidate list? Or should the rebuild be deferred to the next deletion cycle?

### Q33: Preview mode label — dynamic or static text?
The [PREVIEW] label appears when space is below threshold. Should it show any additional info (e.g., "PREVIEW — 8 GB free" or "PREVIEW — no action needed") or just the bare "[PREVIEW]" text?

### Q34: A vs B locked peer selection — same anime or any anime?
When a single Tier 3 candidate is pitted against a locked peer (Q17), should the locked peer be from the **same anime** (if one exists), or can it be from **any anime** in the locked list? Same-anime is more contextual but may not always exist.

### Q35: Size-weighted distance — cross-anime distance calculation
For cross-anime distance, how should the episode position be calculated? For example, if the user is at episode 80 of Naruto (167 eps) and has Dragon Ball Z episode 45 (291 eps), should the distance be:
- **A**: Just the DBZ episode's distance from its own current position (if the user has a DBZ session)
- **B**: A large value (e.g., max distance) since it's a different anime entirely
- **C**: Something else?
