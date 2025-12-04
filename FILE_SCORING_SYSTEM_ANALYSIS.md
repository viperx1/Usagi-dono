# File Scoring System Analysis

**Date:** 2025-12-04  
**Scope:** Deep analysis of the file scoring system in WatchSessionManager  
**Purpose:** Comprehensive documentation of system behavior, workflows, and correctness assessment

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [System Purpose](#system-purpose)
3. [Scoring Algorithm Architecture](#scoring-algorithm-architecture)
4. [Detailed Factor Analysis](#detailed-factor-analysis)
   - Base Score, Hidden Cards, Watch Status
   - File Version Scoring
   - Language Preferences
   - Quality, Bitrate, Rating, Group Status
   - Session-Based Scoring
   - Series Chain Support
5. [Auto-Marking System](#auto-marking-system)
6. [Workflow Analysis](#workflow-analysis)
   - New User Starting to Watch
   - Multi-Season Anime Tracking
   - Quality Comparison
   - Hidden Anime Cleanup
   - Bitrate Quality Control
7. [Identified Issues and Edge Cases](#identified-issues-and-edge-cases)
8. [Test Coverage Assessment](#test-coverage-assessment)
9. [Performance Considerations](#performance-considerations)
10. [Correctness Verdict](#correctness-verdict)
11. **[Extended Analysis: Critical Design Considerations](#extended-analysis-critical-design-considerations)** ⭐ NEW
    - Group Rating vs. Anime Rating
    - Series Continuity and Sequential Deletion
    - Duplicate File Handling
    - Minimum Anime Retention
    - Codec Awareness (Deep Dive)
12. [Final Assessment](#final-assessment)

---

## Executive Summary

The file scoring system is a sophisticated priority-based file management system implemented in `WatchSessionManager` that determines which anime video files should be kept, downloaded, or deleted. The system uses a multi-factor scoring algorithm to calculate a priority score for each file, where **higher scores indicate files that should be kept** and **lower scores indicate files eligible for deletion**.

**Key Finding:** The system is well-designed and functionally correct for its intended purpose, with comprehensive test coverage and robust error handling. However, there are some edge cases and potential improvements identified in this analysis.

---

## System Purpose

### Primary Goals

1. **Automated Space Management**: Automatically identify files that can be safely deleted when disk space is low
2. **Quality-Based Retention**: Prioritize keeping higher quality files over lower quality versions
3. **Watch Progress Awareness**: Keep files relevant to active viewing sessions while marking already-watched content as deletable
4. **User Preference Alignment**: Respect user preferences for languages, quality, bitrate, and other attributes
5. **Download Prioritization**: Identify which episodes should be downloaded next based on viewing progress

### Design Philosophy

The scoring system follows a **weighted priority model** where multiple factors contribute to a file's final score:
- Each factor adds or subtracts points based on its importance
- Critical factors (version, watch status, distance from current episode) have larger weights
- Less critical factors (group status, rating) have smaller adjustments
- The cumulative score determines file priority

---

## Scoring Algorithm Architecture

### Core Structure

The scoring system is implemented in `WatchSessionManager::calculateMarkScore(int lid)` and uses the following approach:

```
Base Score: 50 (neutral starting point)
+ Watch Status Adjustments
+ Session-Based Scoring
+ Quality/Version Penalties
+ Language Preference Bonuses
+ Bitrate Distance Penalties
+ Group/Rating Modifiers
= Final Score (higher = keep, lower = delete)
```

### Scoring Constants (from watchsessionmanager.h)

```cpp
// Base modifiers
SCORE_HIDDEN_CARD = -50           // Hidden anime cards
SCORE_ACTIVE_SESSION = 100        // File in active watching session
SCORE_IN_AHEAD_BUFFER = 75        // Within ahead buffer range
SCORE_ALREADY_WATCHED = -30       // Already watched episodes
SCORE_NOT_WATCHED = 50            // Unwatched episodes
SCORE_DISTANCE_FACTOR = -1        // Per episode distance from current

// Version/Quality modifiers
SCORE_OLDER_REVISION = -1000      // Per higher version file found (strongest penalty)
SCORE_PREFERRED_AUDIO = 30        // Matches preferred audio language
SCORE_PREFERRED_SUBTITLE = 20     // Matches preferred subtitle language
SCORE_NOT_PREFERRED_AUDIO = -40   // Doesn't match preferred audio
SCORE_NOT_PREFERRED_SUBTITLE = -20 // Doesn't match preferred subtitle
SCORE_HIGHER_QUALITY = 25         // Quality >= 60 (high quality)
SCORE_LOWER_QUALITY = -35         // Quality < 40 (low quality)

// Anime/Group modifiers
SCORE_HIGH_RATING = 15            // Anime rating >= 800 (8.0/10)
SCORE_LOW_RATING = -15            // Anime rating < 600 (6.0/10)
SCORE_ACTIVE_GROUP = 20           // Files from active groups
SCORE_STALLED_GROUP = -10         // Files from stalled groups
SCORE_DISBANDED_GROUP = -25       // Files from disbanded groups

// Bitrate modifiers (only when multiple files exist)
SCORE_BITRATE_CLOSE = -10         // 10-30% from expected
SCORE_BITRATE_MODERATE = -25      // 30-50% from expected
SCORE_BITRATE_FAR = -40           // 50%+ from expected
```

---

## Detailed Factor Analysis

### 1. Base Score (Starting Point: 50)

**Purpose:** Provides a neutral baseline for all files before adjustments are applied.

**Analysis:** The choice of 50 as the base is appropriate - it's high enough that unwatched files with minor penalties remain positive, but low enough that multiple negative factors can drive the score well below zero for deletion eligibility.

**Works Correctly:** ✅ Yes

---

### 2. Hidden Card Status (Score: -50)

**Implementation:**
```cpp
if (aid > 0 && isCardHidden(aid)) {
    score += SCORE_HIDDEN_CARD;  // -50
}
```

**Purpose:** Makes files from hidden anime more eligible for deletion, as users have explicitly marked these anime as less important.

**Workflow Example:**
- User hides anime they're no longer interested in
- All episodes from that anime get -50 penalty
- Files become prime candidates for deletion when space is needed

**Analysis:** This is a strong signal of user intent. A -50 penalty is significant enough to make hidden anime a priority for deletion without completely overriding other factors like version quality.

**Works Correctly:** ✅ Yes

**Potential Issue:** If a user hides an anime temporarily (e.g., to declutter UI), they might not want all files immediately marked for deletion. However, this is likely edge case behavior.

---

### 3. Watch Status (Score: -30 or +50)

**Implementation:**
```cpp
QSqlQuery q(db);
q.prepare("SELECT viewed, local_watched FROM mylist WHERE lid = ?");
q.addBindValue(lid);
if (q.exec() && q.next()) {
    int viewed = q.value(0).toInt();
    int localWatched = q.value(1).toInt();
    
    if (viewed > 0 || localWatched > 0) {
        score += SCORE_ALREADY_WATCHED;  // -30
    } else {
        score += SCORE_NOT_WATCHED;  // +50
    }
}
```

**Purpose:** 
- Watched files are more eligible for deletion (users can re-download if needed)
- Unwatched files are prioritized for keeping (users haven't seen them yet)

**Workflow Example:**
- User watches episode 1-5 of a 12-episode series
- Episodes 1-5 get -30 penalty (deletable)
- Episodes 6-12 get +50 bonus (keep for future viewing)
- If space is needed, system deletes episodes 1-5 first

**Analysis:** The 80-point swing (-30 vs +50) is appropriate. Unwatched content deserves stronger protection than the penalty for watched content, reflecting that users are more tolerant of re-downloading watched episodes.

**Works Correctly:** ✅ Yes

**Edge Case:** Users who rewatch content may not want watched episodes deleted. The system doesn't distinguish between "watched once" and "rewatching frequently."

---

### 4. File Version Scoring (Score: -1000 per higher version)

**Implementation:**
```cpp
int fileCount = getFileCountForEpisode(lid);
if (fileCount > 1) {
    int higherVersionCount = getHigherVersionFileCount(lid);
    if (higherVersionCount > 0) {
        score += higherVersionCount * SCORE_OLDER_REVISION;  // -1000 each
    }
}
```

**Version Detection Logic:**
```cpp
int WatchSessionManager::getFileVersion(int lid) const {
    // State field bit encoding (from AniDB UDP API):
    //   Bit 2 (4): FILE_ISV2 - file is version 2
    //   Bit 3 (8): FILE_ISV3 - file is version 3
    //   Bit 4 (16): FILE_ISV4 - file is version 4
    //   Bit 5 (32): FILE_ISV5 - file is version 5
    // No version bits set = version 1
    
    if (state & 32) return 5;      // Bit 5: v5
    else if (state & 16) return 4; // Bit 4: v4
    else if (state & 8) return 3;  // Bit 3: v3
    else if (state & 4) return 2;  // Bit 2: v2
    return 1;                       // Default: v1
}
```

**Purpose:** Strongly prioritize keeping the newest version of each episode. Older versions (v1, v2) should be deleted in favor of v2, v3, etc., which typically contain fixes for errors or quality improvements.

**Workflow Example:**
- Episode 1 has three files: v1, v2, and v3
- v1 has 2 higher versions (v2, v3): score -= 2000
- v2 has 1 higher version (v3): score -= 1000
- v3 has 0 higher versions: no penalty
- When space is needed, v1 is deleted first, then v2, keeping v3

**Analysis:** The -1000 penalty is **extremely strong** - the strongest penalty in the system. This is intentional and correct, as keeping multiple versions of the same episode is wasteful, and older versions are objectively inferior.

**Works Correctly:** ✅ Yes

**Important Behavior:** The version penalty only applies when `fileCount > 1`, meaning at least 2 local files exist for the same episode. This prevents applying penalties when there's only one file (which would be pointless).

**Test Coverage:** Excellent - `testFileVersionScoring()` verifies this behavior.

---

### 5. Language Preferences

#### 5.1 Audio Language (Score: +30 or -40)

**Implementation:**
```cpp
bool hasAudioMatch = matchesPreferredAudioLanguage(lid);
if (hasAudioMatch) {
    score += SCORE_PREFERRED_AUDIO;  // +30
} else {
    QString audioLang = getFileAudioLanguage(lid);
    if (!audioLang.isEmpty()) {
        score += SCORE_NOT_PREFERRED_AUDIO;  // -40
    }
}
```

**Purpose:** Prioritize files with user's preferred audio languages (e.g., Japanese for sub watchers, English for dub watchers).

**Workflow Example:**
- User sets preferred audio: "japanese,english"
- Episode 1 has 3 files: Japanese audio (+30), English audio (+30), German audio (-40)
- German audio file becomes prime deletion candidate
- Japanese and English audio files are protected

**Analysis:** The asymmetric penalty (-40 for mismatch vs +30 for match) slightly favors deletion of non-preferred languages, which is appropriate for space management. The system correctly avoids applying penalties when audio language info is missing (prevents penalizing files with unknown data).

**Language Matching Logic:**
```cpp
// Supports word boundary matching to avoid false positives
// e.g., 'eng' doesn't match 'bengali'
QString normalizedAudioLang = audioLang.toLower().trimmed();
if (normalizedAudioLang == trimmedLang || 
    normalizedAudioLang.startsWith(trimmedLang + " ") ||
    normalizedAudioLang.endsWith(" " + trimmedLang) ||
    normalizedAudioLang.contains(" " + trimmedLang + " ")) {
    return true;
}
```

**Works Correctly:** ✅ Yes

**Potential Issue:** The word boundary matching may not catch all variations. For example, "japanese (main)" would match "japanese", but "japanese/english" might not match correctly depending on the format.

#### 5.2 Subtitle Language (Score: +20 or -20)

**Implementation:** Similar to audio language with smaller weights.

**Purpose:** Secondary preference factor for subtitle languages.

**Analysis:** Appropriately weighted lower than audio (+20/-20 vs +30/-40) since subtitle preference is typically less critical than audio language for most users.

**Works Correctly:** ✅ Yes

---

### 6. Quality Scoring (Score: +25 or -35)

**Implementation:**
```cpp
QSqlQuery q2(db);
q2.prepare("SELECT value FROM settings WHERE name = 'preferHighestQuality'");
if (q2.exec() && q2.next() && q2.value(0).toString() == "1") {
    QString quality = getFileQuality(lid);
    int qualityScore = getQualityScore(quality);
    
    if (qualityScore >= QUALITY_HIGH_THRESHOLD) {        // >= 60
        score += SCORE_HIGHER_QUALITY;  // +25
    } else if (qualityScore < QUALITY_LOW_THRESHOLD) {   // < 40
        score += SCORE_LOWER_QUALITY;  // -35
    }
}
```

**Quality Score Mapping:**
```cpp
int WatchSessionManager::getQualityScore(const QString& quality) const {
    QString q = quality.toLower().trimmed();
    if (q == "very high") return 100;
    if (q == "high") return 80;
    if (q == "medium") return 60;
    if (q == "low") return 40;
    if (q == "very low") return 20;
    if (q == "corrupted" || q == "eyecancer") return 10;
    return 50;  // Unknown/default
}
```

**Purpose:** Prioritize keeping high-quality encodes over low-quality ones, based on AniDB's quality assessment.

**Workflow Example:**
- Episode 1 has 2 files: "very high" quality (score 100) and "low" quality (score 40)
- High quality file: base + 25
- Low quality file: base - 35
- Low quality file is deleted first when space is needed

**Analysis:** This system is **only enabled when the user has `preferHighestQuality` setting turned on**, providing user control. The quality assessment relies on AniDB's quality field, which is generally reliable but subjective.

**Works Correctly:** ✅ Yes

**Potential Issue:** The quality score is binary (high vs low vs medium), with no gradations. "Very high" (100) and "high" (80) both get the same +25 bonus, which could be refined to distinguish between them.

---

### 7. Bitrate Distance Scoring (Score: 0 to -40)

**Implementation:**
```cpp
int bitrate = getFileBitrate(lid);
QString resolution = getFileResolution(lid);
if (bitrate > 0 && !resolution.isEmpty() && fileCount > 1) {
    double bitrateScore = calculateBitrateScore(bitrate, resolution, fileCount);
    score += static_cast<int>(bitrateScore);
}
```

**Bitrate Calculation Formula:**
```cpp
// Universal formula: bitrate = baseline × (resolution_megapixels / 2.07)
// Where 2.07 = megapixels of 1080p (1920×1080)
double expectedBitrate = baselineBitrate * (megapixels / 2.07);

// Examples with 3.5 Mbps baseline:
// 480p (0.41 MP) → 0.7 Mbps
// 720p (0.92 MP) → 1.6 Mbps
// 1080p (2.07 MP) → 3.5 Mbps
// 1440p (3.69 MP) → 6.2 Mbps
// 4K (8.29 MP) → 14.0 Mbps

// Penalty based on percentage difference:
if (percentDiff > 50.0) return -40;       // 50%+ difference
else if (percentDiff > 30.0) return -25;  // 30-50% difference
else if (percentDiff > 10.0) return -10;  // 10-30% difference
else return 0;                             // 0-10% difference (acceptable)
```

**Purpose:** Penalize files that have bitrates significantly different from the expected value for their resolution. This catches both over-compressed (too low bitrate) and bloated (unnecessarily high bitrate) files.

**Workflow Example:**
- User sets baseline bitrate: 3.5 Mbps for 1080p
- Episode 1 has 3 files (1080p):
  - File A: 3.5 Mbps (0% diff) → no penalty
  - File B: 4.9 Mbps (40% diff) → -25 penalty
  - File C: 7.0 Mbps (100% diff) → -40 penalty
- Files B and C are more likely to be deleted

**Analysis:** This is a **sophisticated feature** that accounts for resolution differences using a universal scaling formula. The penalty only applies when `fileCount > 1`, preventing arbitrary penalization when there's only one file available.

**Works Correctly:** ✅ Yes, with caveats

**Potential Issues:**

1. **Anime vs Live Action:** The formula assumes anime content characteristics (flat colors, simple motion). It may not be appropriate for anime with complex motion or grain (e.g., theatrical releases with film grain).

2. **Encoding Efficiency:** Modern codecs (H.265/HEVC) can achieve the same quality at 50% lower bitrate than H.264. The system doesn't account for codec differences.

3. **Symmetric Penalty:** Files with bitrate too high OR too low get the same penalty. However, a too-high bitrate (bloated file) wastes space, while too-low bitrate (over-compressed) sacrifices quality. These should arguably be treated differently.

4. **Resolution Parsing:** The system handles named resolutions (1080p, 4K) and WxH format (1920x1080), but may fail on unusual formats.

---

### 8. Anime Rating Scoring (Score: +15 or -15)

**Implementation:**
```cpp
int rating = getFileRating(lid);
if (rating >= RATING_HIGH_THRESHOLD) {        // >= 800 (8.0/10)
    score += SCORE_HIGH_RATING;  // +15
} else if (rating > 0 && rating < RATING_LOW_THRESHOLD) {  // < 600 (6.0/10)
    score += SCORE_LOW_RATING;  // -15
}
```

**Purpose:** Slightly prioritize keeping files from highly-rated anime and deprioritize files from poorly-rated anime.

**Workflow Example:**
- Anime A (rating 8.5/10 = 850): all episodes get +15
- Anime B (rating 5.5/10 = 550): all episodes get -15
- When disk space is low, episodes from Anime B are deleted first

**Analysis:** The ±15 bonus is appropriately small - it provides a tiebreaker between otherwise equal files but doesn't override more important factors. However, this assumes **user ratings align with AniDB community ratings**, which may not always be true.

**Works Correctly:** ✅ Yes, but with philosophical questions

**Potential Issue:** A user may personally love a niche anime with a low AniDB rating. The system would mark it for deletion despite the user's preference. A better approach might be to use the user's personal rating if available.

---

### 9. Group Status Scoring (Score: +20, -10, or -25)

**Implementation:**
```cpp
int gid = getFileGroupId(lid);
if (gid > 0) {
    int groupStatus = getGroupStatus(gid);
    if (groupStatus == 1) {          // Ongoing
        score += SCORE_ACTIVE_GROUP;  // +20
    } else if (groupStatus == 2) {   // Stalled
        score += SCORE_STALLED_GROUP; // -10
    } else if (groupStatus == 3) {   // Disbanded
        score += SCORE_DISBANDED_GROUP; // -25
    }
}
```

**Purpose:** Prioritize files from active groups (better support/updates) and deprioritize files from disbanded groups (no future support).

**Workflow Example:**
- Episode 1 has 2 files: one from an active group, one from a disbanded group
- Active group file: +20
- Disbanded group file: -25
- Disbanded group file is more likely to be deleted

**Analysis:** This is a **reasonable heuristic** but makes assumptions about group quality. A disbanded group may have produced excellent encodes, while an ongoing group may produce poor ones. Group status ≠ file quality.

**Works Correctly:** ✅ Functionally, but philosophically questionable

**Potential Issue:** This could lead to deleting high-quality files from disbanded legendary groups in favor of lower-quality files from active but mediocre groups.

---

### 10. Session-Based Scoring

This is the **most complex and critical part** of the scoring system, determining file priority based on active viewing sessions.

#### 10.1 Active Session Bonus (Score: +100)

**Implementation:**
```cpp
auto sessionInfo = findActiveSessionInSeriesChain(aid);
int sessionAid = std::get<0>(sessionInfo);

if (sessionAid > 0) {
    score += SCORE_ACTIVE_SESSION;  // +100
    // ... additional session-based scoring
}
```

**Purpose:** Files from anime with active viewing sessions get a significant bonus to prevent deletion while watching.

**Works Correctly:** ✅ Yes

#### 10.2 Ahead Buffer Protection (Score: +75)

**Implementation:**
```cpp
int currentEp = getCurrentSessionEpisode(sessionAid);
int totalEpisodePosition = episodeOffset + episodeNumber;
int currentTotalPosition = sessionOffset + currentEp;
int distance = totalEpisodePosition - currentTotalPosition;

int aheadBuffer = m_aheadBuffer;  // Default: 3 episodes

if (distance >= 0 && distance <= aheadBuffer) {
    score += SCORE_IN_AHEAD_BUFFER;  // +75
}
```

**Purpose:** Protect episodes within the "ahead buffer" (next N episodes to watch) from deletion to ensure continuous viewing.

**Workflow Example:**
- User is watching episode 5 of a 12-episode series
- Ahead buffer set to 3
- Episodes 5, 6, 7, 8 are within the buffer → each gets +75
- Episodes 1-4 (already watched) don't get buffer bonus
- Episodes 9-12 (far ahead) don't get buffer bonus

**Analysis:** The ahead buffer is a **smart feature** that balances keeping future content available while allowing cleanup of distant episodes. The +75 bonus is substantial but not as strong as the +100 for active session, creating a priority gradient.

**Works Correctly:** ✅ Yes

#### 10.3 Distance-Based Scoring (Score: -1 per episode distance)

**Implementation:**
```cpp
int distance = totalEpisodePosition - currentTotalPosition;
score += distance * SCORE_DISTANCE_FACTOR;  // -1 per episode away
```

**Purpose:** Create a linear gradient where episodes farther from the current position have lower scores.

**Workflow Example:**
- Current episode: 5
- Episode 5: distance = 0 → no distance penalty
- Episode 6: distance = 1 → -1
- Episode 7: distance = 2 → -2
- Episode 10: distance = 5 → -5
- Episode 1: distance = -4 → +4 (episodes behind get a bonus!)

**Analysis:** This is the **primary scoring mechanism** for determining file priority within a session. The linear gradient is simple and effective.

**Interesting Behavior:** Episodes **behind** the current position get a **positive** distance modifier (e.g., +4 for 4 episodes back). This partially offsets the -30 "already watched" penalty, meaning recently watched episodes are kept slightly longer than episodes watched long ago.

**Works Correctly:** ✅ Yes, but behavior may be unintuitive

**Potential Issue:** The positive distance for behind episodes could be confusing. Users might expect all watched episodes to have similar (low) scores, but this gives recent watched episodes higher scores than distant watched episodes.

#### 10.4 Session Watched Episodes (Score: -30 additional)

**Implementation:**
```cpp
if (m_sessions.contains(aid) && m_sessions[aid].watchedEpisodes.contains(episodeNumber)) {
    score += SCORE_ALREADY_WATCHED;  // -30 (applied again)
}
```

**Purpose:** Episodes marked as watched within the current session get an additional penalty.

**Analysis:** This creates a **double-dipping scenario** - an episode can get the -30 penalty from both the general watch status check AND the session-specific check, totaling -60. This may be intentional to strongly prioritize deletion of watched content.

**Works Correctly:** ⚠️ Possible double penalty

**Potential Issue:** It's unclear if this double penalty is intentional or a bug. If intentional, it should be documented. If not, it should be fixed.

---

### 11. Series Chain Support

**Implementation:**
```cpp
QList<int> WatchSessionManager::getSeriesChain(int aid) const {
    // Start from original prequel
    int currentAid = getOriginalPrequel(aid);
    
    // Follow sequel chain
    while (currentAid > 0 && !visited.contains(currentAid)) {
        chain.append(currentAid);
        // Look for sequel...
    }
    return chain;
}
```

**Purpose:** Handle multi-season anime as a continuous viewing experience. Sessions span across prequels/sequels.

**Workflow Example:**
- Anime series: Season 1 (12 eps), Season 2 (12 eps), Season 3 (12 eps)
- User starts session on Season 1, episode 1
- Current position: Season 1, episode 10
- System calculates episode offsets:
  - Season 1 episodes: offset = 0, positions 1-12
  - Season 2 episodes: offset = 12, positions 13-24
  - Season 3 episodes: offset = 24, positions 25-36
- Distance calculated correctly across seasons:
  - Season 1, Ep 10: distance = 0 (current)
  - Season 2, Ep 1: distance = 3 (next season, episode 13)
  - Season 2, Ep 5: distance = 7 (episode 17)

**Analysis:** This is an **advanced feature** that significantly enhances user experience by treating multi-season anime as continuous. The offset calculation is mathematically correct.

**Works Correctly:** ✅ Yes

**Test Coverage:** Excellent - `testGetSeriesChain()` and `testGetOriginalPrequel()` verify this behavior.

---

## Auto-Marking System

The scoring system feeds into two auto-marking systems:

### 1. Auto-Mark for Deletion

**Trigger:** Disk space falls below threshold
**Process:**
1. Check available space on watched path
2. Compare against threshold (fixed GB or percentage)
3. If below threshold, calculate how much space needs to be freed
4. Query all local files with their sizes
5. Calculate score for each file
6. Sort by score (lowest first)
7. Mark files for deletion until enough space would be freed

**Implementation:**
```cpp
void WatchSessionManager::autoMarkFilesForDeletion() {
    // Calculate how many bytes to free
    qint64 spaceToFreeBytes = (threshold - availableGB) * 1024^3;
    
    // Get all local files with scores
    QList<CandidateInfo> candidates;
    // ... populate candidates with scores
    
    // Sort by score (lowest = most deletable)
    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateInfo& a, const CandidateInfo& b) {
                  return a.score < b.score;
              });
    
    // Mark files until enough space is freed
    qint64 accumulatedSpace = 0;
    for (const auto& candidate : candidates) {
        if (accumulatedSpace >= spaceToFreeBytes) break;
        
        markFileForDeletion(candidate.lid);
        accumulatedSpace += candidate.size;
    }
}
```

**Workflow Example:**
- Disk: 500 GB total, 45 GB available
- Threshold: 50 GB minimum
- Need to free: 5 GB
- System scores all files, sorts, marks lowest-scored files totaling 5+ GB

**Analysis:** The system correctly:
- Calculates the exact amount of space needed
- Accumulates file sizes to avoid over-deleting
- Verifies files exist on disk before marking (prevents marking stale database entries)
- Cleans up missing files (database inconsistencies)

**Works Correctly:** ✅ Yes

**Potential Issues:**

1. **File Existence Check:** The system checks if files exist before marking, which is good. However, this happens during the marking phase, not during the initial scan. If files are frequently deleted externally, this could cause performance issues.

2. **Greedy Algorithm:** The system uses a greedy approach (mark files until quota met). This is efficient but might not produce the optimal selection if file sizes vary greatly.

3. **Percentage Calculation:** When using percentage threshold, the system calculates `threshold = (percentage / 100.0) * totalGB`. This is correct but could be clearer in documentation.

---

### 2. Auto-Mark for Download

**Purpose:** Mark episodes that should be downloaded next based on active sessions and ahead buffer.

**Implementation:**
```cpp
void WatchSessionManager::autoMarkFilesForDownload() {
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        const SessionInfo& session = it.value();
        if (!session.isActive) continue;
        
        int aheadBuffer = m_aheadBuffer;
        
        // Find files for episodes in the ahead buffer
        for (int ep = session.currentEpisode; ep <= session.currentEpisode + aheadBuffer; ep++) {
            // Query for files without local files
            // Mark for download
        }
    }
}
```

**Workflow Example:**
- User watching episode 5 of 12-episode series
- Ahead buffer: 3
- Local files exist for episodes 1-7
- System marks episodes 8, 9, 10 for download (within buffer)

**Analysis:** This is straightforward and works correctly for the intended use case.

**Works Correctly:** ✅ Yes

**Potential Issue:** The query looks for files where `local_file IS NULL OR local_path = ''`, but the join is on `local_files` table. If a mylist entry exists without a corresponding local_files entry, it might not be caught. However, this is likely a rare edge case.

---

## Workflow Analysis

### Workflow 1: New User Starting to Watch

**Scenario:** User adds a new anime to their collection and starts watching.

**Steps:**
1. User adds anime to mylist (12 episodes)
2. System calls `onNewAnimeAdded(aid)`
3. Auto-starts session at episode 1
4. `autoMarkFilesForDownload()` marks episodes 1-4 for download (ahead buffer = 3)
5. User downloads and watches episode 1
6. `markEpisodeWatched(aid, 1)` updates session, current episode becomes 2
7. Episode 1 score changes:
   - Previously: +50 (not watched) +100 (active session)
   - Now: -30 (watched) +100 (active session) +4 (distance bonus for 4 episodes behind)
   - Net: Score dropped from ~150 to ~74
8. Episode 2 becomes current (distance = 0)
9. Episode 5 now within ahead buffer, marked for download

**Result:** System correctly manages downloads and keeps recently watched episodes while marking old episodes as deletable.

**Works Correctly:** ✅ Yes

---

### Workflow 2: Multi-Season Anime Tracking

**Scenario:** User watches a 3-season anime series continuously.

**Steps:**
1. User starts session on Season 1, Episode 1
2. System detects series chain: [Season1_AID, Season2_AID, Season3_AID]
3. Session tracks progress across all seasons:
   - Season 1, Episodes 1-12: offsets 0-11
   - Season 2, Episodes 1-12: offsets 12-23
   - Season 3, Episodes 1-12: offsets 24-35
4. User watches Season 1 completely
5. System marks Season 1 episodes for deletion (all watched, distance negative)
6. Current episode advances to Season 2, Episode 1 (offset 12)
7. Ahead buffer protects Season 2, Episodes 1-4
8. Season 1 episodes have very low scores (watched + far away)
9. Disk space low → auto-deletion triggered
10. Season 1 episodes deleted (lowest scores)
11. User continues watching Season 2 seamlessly

**Result:** System correctly handles multi-season progression, cleaning up old seasons while protecting current and upcoming content.

**Works Correctly:** ✅ Yes

**Edge Case:** If user wants to rewatch Season 1 after finishing Season 3, those episodes are likely deleted. System doesn't distinguish between "temporary deletion" and "never want to watch again."

---

### Workflow 3: Quality Comparison - Multiple Files per Episode

**Scenario:** User has multiple versions of the same episode with different qualities and must choose which to keep.

**Given:**
- Episode 1 has 4 files:
  - File A: v2, very high quality, japanese audio, 1080p, 3.5 Mbps bitrate, active group
  - File B: v1, high quality, japanese audio, 1080p, 3.5 Mbps bitrate, disbanded group
  - File C: v2, medium quality, english dub, 720p, 1.2 Mbps bitrate, active group
  - File D: v2, low quality, japanese audio, 480p, 0.5 Mbps bitrate, stalled group

**Score Calculation:**

**File A (best overall):**
- Base: 50
- Not watched: +50
- Active session: +100
- Version penalty: 0 (v2, no higher versions)
- Quality: +25 (very high)
- Audio: +30 (japanese matches preference)
- Bitrate: 0 (perfect match for 1080p)
- Group: +20 (active)
- **Total: ~275**

**File B (older version):**
- Base: 50
- Not watched: +50
- Active session: +100
- Version penalty: -1000 (v1, 1 higher version exists)
- Quality: +25 (high)
- Audio: +30 (japanese)
- Bitrate: 0 (perfect)
- Group: -25 (disbanded)
- **Total: ~-770**

**File C (wrong audio):**
- Base: 50
- Not watched: +50
- Active session: +100
- Version penalty: 0 (v2)
- Quality: 0 (medium, between thresholds)
- Audio: -40 (english, doesn't match japanese preference)
- Bitrate: -10 (20% off expected for 720p)
- Group: +20 (active)
- **Total: ~170**

**File D (low quality):**
- Base: 50
- Not watched: +50
- Active session: +100
- Version penalty: 0 (v2)
- Quality: -35 (low)
- Audio: +30 (japanese)
- Bitrate: -25 (30% off expected for 480p)
- Group: -10 (stalled)
- **Total: ~160**

**Result:** Deletion priority: File B (-770) → File D (160) → File C (170) → File A (275)

**Analysis:** 
- ✅ File B correctly has the lowest score due to older version (intended behavior)
- ✅ File A correctly has the highest score (best quality + preferred language + active group)
- ✅ Files C and D are close, with quality and bitrate differences balancing out
- ✅ System correctly prioritizes version over all other factors

**Works Correctly:** ✅ Yes - Priorities align with expected user preferences

---

### Workflow 4: Hidden Anime Cleanup

**Scenario:** User hides an anime they're no longer interested in.

**Steps:**
1. User marks anime as hidden in UI
2. Database updated: `is_hidden = 1`
3. Next auto-deletion scan runs
4. All episodes of hidden anime get -50 penalty
5. Assuming average episode scores of ~100:
   - Normal episode: 100
   - Hidden episode: 50
6. When space needed, hidden episodes deleted first

**Result:** System correctly interprets hiding as a deletion signal.

**Works Correctly:** ✅ Yes

**Edge Case:** If user hides anime temporarily (for UI organization), files might be deleted unintentionally. However, this is likely acceptable since hiding is a strong user signal.

---

### Workflow 5: Bitrate Quality Control

**Scenario:** User downloads multiple encodes to compare quality and wants to keep the best one.

**Given:**
- User preference: 3.5 Mbps baseline for 1080p
- Episode 1 has 3 files (all 1080p, v2, same audio/subs):
  - File A: 3.5 Mbps (perfect match)
  - File B: 10 Mbps (bloated, 186% of expected)
  - File C: 1.5 Mbps (over-compressed, 43% of expected)

**Score Calculation:**

**File A:**
- Base: ~200 (base + bonuses)
- Bitrate penalty: 0 (0% difference)
- **Total: ~200**

**File B:**
- Base: ~200
- Bitrate penalty: -40 (186% difference = 86% off)
- **Total: ~160**

**File C:**
- Base: ~200
- Bitrate penalty: -40 (43% of expected = 57% off)
- **Total: ~160**

**Result:** File A kept, Files B and C marked for deletion.

**Analysis:**
- ✅ System correctly identifies Files B and C as outliers
- ⚠️ Files B and C have the same penalty despite different issues (bloated vs compressed)
- ⚠️ A bloated file wastes space but maintains quality, while compressed file saves space but loses quality. Should these be treated differently?

**Works Correctly:** ✅ Mostly, with philosophical questions about symmetric penalties

---

## Identified Issues and Edge Cases

### 1. Double Watch Penalty ⚠️

**Issue:** Episodes can receive the -30 watch penalty twice:
1. From general watch status check (`viewed` or `local_watched`)
2. From session-specific watched episodes check

**Impact:** Watched episodes get -60 instead of -30.

**Severity:** Low - May be intentional to strongly prioritize deletion

**Recommendation:** Clarify intent in documentation or prevent double-counting

---

### 2. Symmetric Bitrate Penalty ⚠️

**Issue:** Over-compressed and bloated files receive the same penalty.

**Impact:** Files with too-high bitrate (waste space, maintain quality) treated the same as files with too-low bitrate (save space, lose quality).

**Severity:** Low - Rare practical impact

**Recommendation:** Consider asymmetric penalties: higher penalty for over-compressed files

---

### 3. Rating-Based Scoring Ignores Personal Preference ⚠️

**Issue:** System uses AniDB community rating, not user's personal rating.

**Impact:** User's favorite niche anime with low rating gets deletion penalty.

**Severity:** Low to Medium - Depends on user's taste alignment with community

**Recommendation:** Use user's personal rating if available, fall back to AniDB rating

---

### 4. Group Status as Quality Proxy ⚠️

**Issue:** Group status (active/disbanded) used as quality indicator.

**Impact:** High-quality files from disbanded legendary groups may be deleted over low-quality files from active mediocre groups.

**Severity:** Low - Group bonuses are small relative to other factors

**Recommendation:** Document that group status is a heuristic, not definitive quality measure

---

### 5. Language Matching Edge Cases ⚠️

**Issue:** Language matching may not handle all format variations (e.g., "japanese/english", "jpn", etc.)

**Impact:** Some multi-language or abbreviated formats might not match correctly.

**Severity:** Low - Most AniDB data uses standard formats

**Recommendation:** Expand regex to handle more variations

---

### 6. No Distinction Between Watched Once vs. Rewatch ⚠️

**Issue:** System doesn't distinguish between files watched once (deletable) and files the user rewatches frequently (keep).

**Impact:** Rewatched favorites may be deleted.

**Severity:** Medium - Affects users who rewatch content

**Recommendation:** Track rewatch count and apply bonus for frequently rewatched content

---

### 7. Quality Score Binary Thresholds ⚠️

**Issue:** Quality scoring uses only two thresholds (high ≥60, low <40), lumping "very high" and "high" together.

**Impact:** System doesn't distinguish between gradations of quality.

**Severity:** Low - The distinction is minor for deletion purposes

**Recommendation:** Consider finer-grained scoring (e.g., bonus scales with quality score)

---

### 8. Codec Awareness ⚠️

**Issue:** Bitrate formula doesn't account for codec efficiency (H.265 vs H.264).

**Impact:** High-quality H.265 files may be penalized for "too-low" bitrate despite being correctly encoded.

**Severity:** Medium - H.265 adoption is increasing

**Recommendation:** Adjust expected bitrate based on codec (H.265 = ~50% of H.264 expected)

---

### 9. Missing File Cleanup During Scan 🔧

**Issue:** Missing files are cleaned up during auto-deletion scan, which could cause performance issues if many files are missing.

**Impact:** Auto-deletion scan may be slow if database is out of sync with disk.

**Severity:** Low - Good that it's handled, but placement could be optimized

**Recommendation:** Consider separate maintenance task for database cleanup

---

### 10. No Protection for "Favorites" ⭐

**Issue:** System has no concept of "favorite" or "priority" anime that should never be deleted.

**Impact:** Even highly-rated, preferred-language content can be deleted if disk space is very low.

**Severity:** Medium - Users may want absolute protection for certain content

**Recommendation:** Add "protected" flag or favorite system that gives files infinite score

---

## Test Coverage Assessment

### Excellent Coverage ✅

1. **Session Management:**
   - `testStartSession()` - Session creation
   - `testEndSession()` - Session termination
   - `testMarkEpisodeWatched()` - Watch progress tracking
   - `testSessionPersistence()` - Database save/load

2. **File Marking:**
   - `testCalculateMarkScore()` - Basic scoring
   - `testSetFileMarkType()` - Mark type assignment
   - `testGetFilesForDeletion()` - Deletion queue
   - `testFileMarkPersistence()` - Marking persistence (verified as in-memory only)

3. **Series Chain:**
   - `testGetOriginalPrequel()` - Prequel chain traversal
   - `testGetSeriesChain()` - Multi-season handling

4. **Version Scoring:**
   - `testFileVersionScoring()` - Version comparison and penalties

5. **Bitrate Scoring:**
   - `testCalculateBitrateScore*()` - Multiple bitrate scenarios
   - `testCalculateExpectedBitrate()` - Formula verification

6. **Sequential Deletion:**
   - `testSequentialDeletion()` - API confirmation workflow

### Missing Coverage ⚠️

1. **Language Preferences:**
   - No tests for audio/subtitle language matching
   - No tests for word boundary logic

2. **Quality Scoring:**
   - No tests for quality threshold behavior
   - No tests for quality score mapping

3. **Group Status:**
   - No tests for group status scoring
   - No tests for group status lookup

4. **Rating-Based Scoring:**
   - No tests for rating thresholds
   - No tests for rating lookup

5. **Edge Cases:**
   - No tests for missing database fields
   - No tests for malformed data
   - No tests for empty/null values

### Recommendation

Add comprehensive test suite for:
- Language matching edge cases
- Quality and rating scoring
- Group status handling
- Null/empty data handling
- Combined factor interactions

---

## Performance Considerations

### Database Queries

The `calculateMarkScore()` method makes multiple database queries per file:
1. Watch status query (viewed, local_watched)
2. File version query (state bits)
3. File count query (same episode)
4. Higher version count query (version comparison)
5. Audio language query
6. Subtitle language query
7. Quality query (if enabled)
8. Rating query
9. Group ID query
10. Group status query
11. Bitrate query (if multiple files)
12. Resolution query (if multiple files)

**Impact:** With 1000 files, this could be 10,000+ queries.

**Mitigation:** Most queries use indexed lookups (lid, fid, aid), which are fast. However, for large collections, consider:
1. Caching file metadata in memory
2. Batch queries (fetch all data for all files at once)
3. Materialized views for common joins

**Current Performance:** Likely acceptable for typical collections (100-1000 files), but could be optimized for power users (10,000+ files).

---

## Correctness Verdict

### ✅ Accomplishes Core Purpose

The file scoring system successfully:
1. ✅ Prioritizes keeping high-quality, preferred files
2. ✅ Identifies deletable files based on watch status
3. ✅ Protects files in active viewing sessions
4. ✅ Handles multi-season anime correctly
5. ✅ Respects user preferences (language, quality, bitrate)
6. ✅ Manages disk space automatically
7. ✅ Provides download recommendations

### ⚠️ Minor Issues Identified

1. Potential double-penalty for watched episodes (clarification needed)
2. Symmetric bitrate penalties (philosophical question)
3. Community ratings vs. personal ratings
4. Codec-agnostic bitrate calculations
5. Language matching edge cases
6. No protection for "favorite" content
7. Binary quality thresholds

### 🔧 Optimization Opportunities

1. Database query batching for large collections
2. Metadata caching
3. Separate cleanup task for missing files
4. Finer-grained quality scoring
5. Codec-aware bitrate expectations

---

## Extended Analysis: Critical Design Considerations

This section addresses specific design issues and improvement opportunities identified through deeper analysis of the scoring system's behavior and edge cases.

---

### 1. Group Rating vs. Anime Rating for Quality Assessment

#### Current Implementation

The system uses **anime rating** (community score 0-10) as a quality indicator:
```cpp
int rating = getFileRating(lid);
if (rating >= RATING_HIGH_THRESHOLD) {        // >= 800 (8.0/10)
    score += SCORE_HIGH_RATING;  // +15
} else if (rating > 0 && rating < RATING_LOW_THRESHOLD) {  // < 600 (6.0/10)
    score += SCORE_LOW_RATING;  // -15
}
```

**Problem:** Anime rating reflects the **overall quality of the anime series** (story, characters, entertainment value), not the **technical quality of the specific file/encode**.

#### Recommendation: Switch to Group Rating

**Rationale:**
- **Group rating** (from AniDB group database) reflects the **release group's reputation and encoding quality**
- A highly-rated release group (e.g., score 9+) consistently produces better encodes
- Group rating is file-specific, while anime rating is series-specific
- Groups with high ratings typically:
  - Use better encoding settings
  - Provide accurate timing for subtitles
  - Include proper fonts and styling
  - Fix errors in subsequent releases
  - Maintain consistent quality standards

**Implementation Analysis:**

The system already has infrastructure for group data:
```cpp
int gid = getFileGroupId(lid);  // Already implemented
int groupStatus = getGroupStatus(gid);  // Already implemented
```

To add group rating support, we would need:
```cpp
// New helper method (not implemented):
int getGroupRating(int gid) const;

// Modified scoring logic:
int gid = getFileGroupId(lid);
if (gid > 0) {
    int groupRating = getGroupRating(gid);
    if (groupRating >= GROUP_RATING_HIGH_THRESHOLD) {  // e.g., >= 900 (9.0/10)
        score += SCORE_HIGH_GROUP_RATING;  // e.g., +30 (stronger than +15)
    } else if (groupRating > 0 && groupRating < GROUP_RATING_LOW_THRESHOLD) {  // e.g., < 400 (4.0/10)
        score += SCORE_LOW_GROUP_RATING;  // e.g., -30 (stronger than -15)
    }
}
```

**Database Schema Check:**

Looking at the existing schema from test files:
```sql
CREATE TABLE IF NOT EXISTS `group` (
    gid INTEGER PRIMARY KEY,
    name TEXT,
    shortname TEXT,
    status INTEGER DEFAULT 0
)
```

**Issue:** The `group` table doesn't currently store `rating` field. This would need to be:
1. Added to the schema
2. Populated via AniDB API GROUP command (if available)
3. Cached with reasonable TTL

**Hybrid Approach (Recommended):**

Use **both** group rating (primary) and anime rating (secondary):
```cpp
// Primary: Group rating (technical quality)
int gid = getFileGroupId(lid);
if (gid > 0) {
    int groupRating = getGroupRating(gid);
    if (groupRating >= 900) {
        score += 30;  // Strong bonus for excellent groups
    } else if (groupRating < 400) {
        score += -30;  // Strong penalty for poor groups
    }
}

// Secondary: Anime rating (content quality, smaller weight)
int animeRating = getFileRating(lid);
if (animeRating >= 850) {  // Raise threshold (only truly exceptional anime)
    score += 10;  // Smaller bonus than before
} else if (animeRating < 500) {  // Lower threshold (only truly terrible anime)
    score += -10;  // Smaller penalty than before
}
```

**Impact Assessment:**

| Scenario | Current Behavior | With Group Rating | Improvement |
|----------|------------------|-------------------|-------------|
| Popular anime (8.5/10), poor group (4.0/10) | +15 (keeps poor encode) | +10 - 30 = -20 | ✅ Correctly deprioritizes |
| Niche anime (5.5/10), excellent group (9.5/10) | -15 (deletes good encode) | -10 + 30 = +20 | ✅ Correctly prioritizes |
| Average anime (7.0/10), average group (6.0/10) | 0 (neutral) | 0 (neutral) | ✅ No change |
| Highly rated anime (9.0/10), highly rated group (9.5/10) | +15 | +10 + 30 = +40 | ✅ Strong protection |

**Conclusion:** Switching to group rating (or using a hybrid approach) would significantly improve the system's ability to identify high-quality encodes regardless of anime popularity. This aligns better with the goal of quality-based retention.

---

### 2. Series Continuity: Preventing Gap Creation

#### Current Implementation

The scoring system uses **distance-based scoring**:
```cpp
int distance = totalEpisodePosition - currentTotalPosition;
score += distance * SCORE_DISTANCE_FACTOR;  // -1 per episode away
```

**How it works:**
- Current episode (distance = 0): no penalty
- Episode 1 ahead (distance = 1): -1 penalty
- Episode 5 ahead (distance = 5): -5 penalty
- Episode 10 behind (distance = -10): +10 bonus (!)

**Problem Identified:** Episodes **behind** the current position get a **positive distance modifier**, which partially offsets the "already watched" penalty. This can lead to deletion patterns that create **gaps in the middle of the series**, breaking continuity.

#### Series Continuity Principle (Clarified)

**Goal:** When deleting files from a series, **never create gaps in the middle** by deleting episodes that have unwatched episodes on both sides.

**Critical Principle:** Series continuity means maintaining a **contiguous sequence** of episodes from episode 1 onwards. Deleting episode 5 when episodes 1-4 and 6-12 exist creates a gap that breaks the viewing experience.

**Deletion Priority (from highest to lowest priority for deletion):**
1. **Duplicate files per episode** - Always delete duplicates first (keep best version only)
2. **Watched episodes** - Already viewed, less critical to keep
3. **Last episodes** - Farthest from the beginning, delete from the end backwards
4. **NEVER delete middle episodes** - Episodes that create gaps should have maximum protection

**Key Insight:** Rewatching is mostly irrelevant in this scenario. The goal is to maintain a **watchable sequence** from episode 1 onwards for the current/future first-time viewing, not to preserve the ability to rewatch.

#### The Gap Problem

**Bad Deletion Pattern (Creates Gap):**
```
Start:     [1][2][3][4][5][6][7][8][9][10][11][12]
Delete 5:  [1][2][3][4][ ][6][7][8][9][10][11][12]  ❌ GAP!
```
User cannot watch continuously from episode 1. The series is now broken.

**Good Deletion Pattern (No Gaps):**
```
Start:     [1][2][3][4][5][6][7][8][9][10][11][12]
Delete 12: [1][2][3][4][5][6][7][8][9][10][11][ ]  ✅ No gap
Delete 11: [1][2][3][4][5][6][7][8][9][10][ ][ ]   ✅ No gap
Delete 10: [1][2][3][4][5][6][7][8][9][ ][ ][ ]    ✅ No gap
```
User can still watch continuously from episode 1 through 9.

#### Proposed Solution: Gap-Aware Scoring

**Core Algorithm:**
1. For each episode, determine if deleting it would create a gap
2. If it would create a gap, apply massive protection bonus
3. Prioritize deleting from the ends (watched episodes from beginning, unwatched from end)

**Gap Detection:**
```cpp
bool wouldCreateGap(int lid) const {
    int aid = getAnimeIdForFile(lid);
    int episodeNum = getEpisodeNumber(lid);
    
    // Check if there are episodes before and after this one
    bool hasEpisodesBefore = hasLocalFileForEpisode(aid, episodeNum - 1);
    bool hasEpisodesAfter = hasLocalFileForEpisode(aid, episodeNum + 1);
    
    // If episodes exist on both sides, deleting this creates a gap
    return hasEpisodesBefore && hasEpisodesAfter;
}
```

**Modified Scoring Logic:**
```cpp
int WatchSessionManager::calculateMarkScore(int lid) const {
    int score = 50;
    // ... existing scoring logic ...
    
    // GAP PROTECTION: Massive bonus for middle episodes
    if (wouldCreateGap(lid)) {
        score += SCORE_MIDDLE_EPISODE_PROTECTION;  // e.g., +1000
        // This makes middle episodes nearly impossible to delete
    }
    
    return score;
}
```

#### Revised Deletion Priority System

**Priority 1: Duplicate Files (Highest Priority for Deletion)**
- When multiple files exist for the same episode, delete all but the best
- Version penalties already handle this: `-1000` per higher version
- **No gap creation** since episode still has one file remaining

**Priority 2: Watched Episodes**
- Episodes marked as watched are eligible for deletion
- But still protected from creating gaps
- Delete from **oldest watched first** (farthest from current position)

**Priority 3: Last Episodes (Unwatched)**
- When no watched episodes remain or all would create gaps
- Delete from the **end backwards** (episode 12 → 11 → 10 → ...)
- This maintains continuity from episode 1

**Priority 4: Middle Episodes (NEVER)**
- Episodes that would create gaps receive maximum protection
- Only deleted in extreme cases (hidden anime, explicit user deletion)

#### Workflow Example: Current vs. Proposed

**Scenario:** 12-episode series, currently at episode 7, ahead buffer = 3
- Episodes 1-6: watched
- Episodes 7-10: unwatched, local files exist
- Episodes 11-12: unwatched, local files exist

**Current System Behavior:**
```
Score calculations:
- Episode 1: +26 (watched, far behind)
- Episode 4: +23 (watched, behind)
- Episode 6: +21 (watched, just behind)
- Episode 7: +50 (current)
- Episode 8: +124 (ahead buffer)
- Episode 10: +122 (ahead buffer)
- Episode 11: +46 (beyond buffer)
- Episode 12: +45 (beyond buffer)

Deletion order: 6 → 4 → 1 → 12 → 11 → 7 → ...
Result: [x][x][3][x][5][x][7][8][9][10][x][x]
```
❌ **Creates multiple gaps** (missing 1, 2, 4, 6)

**Proposed System Behavior:**
```
Gap protection applied:
- Episode 1: +26 + 1000 = 1026 (would create gap before 2-3)
- Episode 4: +23 + 1000 = 1023 (would create gap between 3 and 5)
- Episode 6: +21 + 1000 = 1021 (would create gap between 5 and 7)
- Episode 7: +50 + 1000 = 1050 (would create gap)
- Episode 8: +124 + 1000 = 1124 (would create gap)
- Episode 10: +122 + 1000 = 1122 (would create gap)
- Episode 11: +46 + 1000 = 1046 (would create gap between 10 and 12)
- Episode 12: +45 (LAST episode, no gap created)

Deletion order: 12 → 11 → 10 → 9 → 8 → 7 → 6 → 5 → 4 → 3 → 2 → 1
Result: [1][2][3][4][5][6][ ][ ][ ][ ][ ][ ]
```
✅ **No gaps created** - Contiguous sequence maintained from episode 1-6

#### Special Case: All Episodes Watched

When all episodes in a series are watched:
```
[1✓][2✓][3✓][4✓][5✓][6✓][7✓][8✓][9✓][10✓][11✓][12✓]
```

**Deletion order:**
1. Delete duplicates first (if any)
2. Delete from the end backwards: 12 → 11 → 10 → 9 → ...
3. Preserve episode 1 as long as possible (entry point to series)

**Gap protection still applies:**
- Deleting episode 6 when 1-5 and 7-12 exist: still creates a gap
- Must delete from ends only: 12, then 11, then 10, etc.

#### Edge Cases

**Case 1: Partial Series (Episodes 5-10 only)**
```
Missing 1-4: [ ][ ][ ][ ][5][6][7][8][9][10]
```
- Episode 5 is the "first" episode (no episodes before it)
- Deleting 5 creates no gap (it's an endpoint)
- Deletion order: 10 → 9 → 8 → 7 → 6 → 5

**Case 2: Already Has Gaps**
```
Existing state: [1][2][ ][4][5][ ][ ][8][9][10][ ][12]
```
- Episode 4: already has gap before it (3 missing), deleting creates no new gap
- Episode 8: already has gaps before it, deleting creates no new gap
- Protected episodes: 2 (would create gap 1-4), 5 (would create gap 4-8), 9 (would create gap 8-10), 10 (would create gap 9-12)

**Case 3: Single Episode Remaining per Anime**
```
After extensive deletion: [1][2][3][ ][ ][ ][ ][ ][ ][ ][ ][ ]
```
- Episode 3 is now the "last" episode (no episodes after it)
- Can be safely deleted without creating a gap
- Minimum retention policy (section 4) would protect these

#### Integration with Other Factors

**Duplicate Files + Gap Protection:**
```cpp
// Episode 5 has 3 files: v1, v2, v3
v3: base_score + 0 (no version penalty) + 1000 (gap protection) = 1050
v2: base_score - 1000 (version penalty) + 1000 (gap protection) = 50
v1: base_score - 2000 (version penalty) + 1000 (gap protection) = -950

Deletion order: v1 → v2 → keep v3
```
✅ Duplicates deleted first, best version protected by gap protection

**Watched Episodes + Gap Protection:**
```cpp
// Episode 3 (watched, middle episode)
base: 50
watched: -30
gap protection: +1000
Total: 1020
```
✅ Watched middle episodes still protected from deletion

#### Alternative Approach: Gap Check as Deletion Condition

**User Suggestion:** Instead of adding gap protection to the score, check for gaps as a **condition during the deletion loop**. Since scores are already calculated for all files, we can iterate through them in score order and skip files that would create gaps.

**Conceptual Difference:**

**Approach A: Gap Score (Current Proposal)**
```cpp
int calculateMarkScore(int lid) const {
    int score = 50;
    // ... existing scoring logic ...
    
    if (wouldCreateGap(lid)) {
        score += 1000;  // Massive protection bonus
    }
    return score;
}

void autoMarkFilesForDeletion() {
    // Calculate scores for all files
    QList<ScoredFile> files = getAllFilesWithScores();
    
    // Sort by score (lowest first)
    std::sort(files.begin(), files.end());
    
    // Mark files until space condition met
    for (auto& file : files) {
        if (spaceFreed >= spaceNeeded) break;
        markForDeletion(file.lid);
        spaceFreed += file.size;
    }
}
```

**Approach B: Gap Condition (User Suggestion)**
```cpp
int calculateMarkScore(int lid) const {
    int score = 50;
    // ... existing scoring logic ...
    // NO gap protection in score
    return score;
}

void autoMarkFilesForDeletion() {
    // Calculate scores for all files
    QList<ScoredFile> files = getAllFilesWithScores();
    
    // Sort by score (lowest first)
    std::sort(files.begin(), files.end());
    
    // Mark files until space condition met
    for (auto& file : files) {
        if (spaceFreed >= spaceNeeded) break;
        
        // CHECK: Would deleting this file create a gap?
        if (wouldCreateGap(file.lid)) {
            continue;  // Skip this file, try next one
        }
        
        markForDeletion(file.lid);
        spaceFreed += file.size;
    }
}
```

#### Comparative Analysis

**Advantages of Approach B (Gap as Condition):**

1. **Cleaner Separation of Concerns**
   - Scoring logic focuses on file quality/priority
   - Gap prevention is a deletion constraint, not a quality metric
   - More intuitive: "calculate quality, then enforce constraints"

2. **More Flexible**
   - Easy to add multiple constraints (gap prevention, last file protection, etc.)
   - Can combine conditions with boolean logic
   - Can disable constraint checking without recalculating scores

3. **Better Performance**
   - Gap check only runs for files being considered for deletion
   - No need to check gaps for high-scored files that won't be deleted anyway
   - Scores don't need recalculation when gap status changes

4. **Simpler Score Interpretation**
   - Score purely reflects file quality/priority
   - No artificial inflation from protection bonuses
   - Easier to debug and understand why files are kept/deleted

5. **Dynamic Gap Detection**
   - As files are deleted, gap status changes for remaining files
   - With condition approach, this is automatically handled
   - With score approach, would need to recalculate scores after each deletion

**Example: Dynamic Gap Handling**

**Scenario:** Episodes [1][2][3][4][5][6] exist, all watched, need to delete 3 files

**Approach A (Gap Score):**
```
Initial scores:
- Episode 1: 20 + 1000 = 1020 (gap protection)
- Episode 2: 22 + 1000 = 1022 (gap protection)
- Episode 3: 24 + 1000 = 1024 (gap protection)
- Episode 4: 26 + 1000 = 1026 (gap protection)
- Episode 5: 28 + 1000 = 1028 (gap protection)
- Episode 6: 30 + 0 = 30 (endpoint, no gap)

Deletion order: 6 → (need to recalculate scores)
After deleting 6, episode 5 becomes endpoint:
- Episode 5: 28 + 0 = 28 (no longer creates gap)

Deletion order: 6 → 5 → (recalculate again)
After deleting 5, episode 4 becomes endpoint:
- Episode 4: 26 + 0 = 26

Final: 6 → 5 → 4 deleted, [1][2][3] remain
```
⚠️ **Problem:** Requires score recalculation after each deletion

**Approach B (Gap Condition):**
```
Initial scores (no gap protection):
- Episode 1: 20
- Episode 2: 22
- Episode 3: 24
- Episode 4: 26
- Episode 5: 28
- Episode 6: 30

Deletion loop:
1. Try episode 1: wouldCreateGap(1)? Yes (2 exists after) → SKIP
2. Try episode 2: wouldCreateGap(2)? Yes (1 before, 3 after) → SKIP
3. Try episode 3: wouldCreateGap(3)? Yes (2 before, 4 after) → SKIP
4. Try episode 4: wouldCreateGap(4)? Yes (3 before, 5 after) → SKIP
5. Try episode 5: wouldCreateGap(5)? Yes (4 before, 6 after) → SKIP
6. Try episode 6: wouldCreateGap(6)? No (5 before, nothing after) → DELETE
   (Now: [1][2][3][4][5])
7. Try episode 1: wouldCreateGap(1)? Yes (2 exists after) → SKIP
8. Try episode 2: wouldCreateGap(2)? Yes → SKIP
9. Try episode 3: wouldCreateGap(3)? Yes → SKIP
10. Try episode 4: wouldCreateGap(4)? Yes → SKIP
11. Try episode 5: wouldCreateGap(5)? No (4 before, nothing after) → DELETE
    (Now: [1][2][3][4])
12. Try episode 1: wouldCreateGap(1)? Yes → SKIP
13. Try episode 2: wouldCreateGap(2)? Yes → SKIP
14. Try episode 3: wouldCreateGap(3)? Yes → SKIP
15. Try episode 4: wouldCreateGap(4)? No (3 before, nothing after) → DELETE

Final: 6 → 5 → 4 deleted, [1][2][3] remain
```
✅ **Better:** Gap detection is dynamic and automatic, no recalculation needed

**Advantages of Approach A (Gap in Score):**

1. **Single-Pass Sorting**
   - Sort once by score, delete in order
   - No need to skip files and try next ones
   - More predictable deletion order

2. **Explicit Priority**
   - Gap protection visible in the score
   - Can see exactly how much protection is applied
   - Easier to tune protection levels

3. **Simpler Loop Logic**
   - Just iterate and delete until quota met
   - No conditional skipping logic
   - Fewer edge cases to handle

**Disadvantages of Approach B (Gap as Condition):**

1. **Potential Starvation (Clarified)**
   - **User insight:** With proper scoring, it's impossible to miss first or last episodes
   - First episode has no episode before it → never creates gap → always deletable
   - Last episode has no episode after it → never creates gap → always deletable
   - **Real concern:** May need multiple loop passes to meet target quota
   - **Critical consideration:** Should loop restart after each deletion to avoid deleting high-scored files?

**Analysis of Starvation Scenario:**

**Scenario:** All middle episodes exist [1][2][3][4][5][6][7][8][9][10], all watched
```
Sorted by score (lowest first):
- Episode 5: score 20 (middle, creates gap)
- Episode 4: score 22 (middle, creates gap)
- Episode 6: score 24 (middle, creates gap)
- Episode 3: score 26 (middle, creates gap)
- Episode 7: score 28 (middle, creates gap)
- Episode 2: score 30 (middle, creates gap)
- Episode 8: score 32 (middle, creates gap)
- Episode 9: score 34 (middle, creates gap)
- Episode 10: score 36 (endpoint, no gap!)
- Episode 1: score 38 (endpoint, no gap!)

Deletion loop (single pass):
1. Try ep 5 (20): creates gap → SKIP
2. Try ep 4 (22): creates gap → SKIP
3. Try ep 6 (24): creates gap → SKIP
4. Try ep 3 (26): creates gap → SKIP
5. Try ep 7 (28): creates gap → SKIP
6. Try ep 2 (30): creates gap → SKIP
7. Try ep 8 (32): creates gap → SKIP
8. Try ep 9 (34): creates gap → SKIP
9. Try ep 10 (36): NO gap (endpoint) → DELETE
   (Now: [1][2][3][4][5][6][7][8][9])
10. Try ep 1 (38): NO gap (endpoint) → DELETE
    (Now: [2][3][4][5][6][7][8][9])
```

✅ **No starvation:** Loop successfully deletes episodes 10 and 1 despite all middle episodes being skipped.

**Problem: Single-Pass Limitation**

After first pass, [2][3][4][5][6][7][8][9] remain. Now:
- Episode 2 becomes new first (endpoint, no gap)
- Episode 9 becomes new last (endpoint, no gap)

But we already passed them in the sorted list! If quota not met, need another pass.

**Solution 1: Multi-Pass Loop**
```cpp
void autoMarkFilesForDeletion() {
    QList<ScoredFile> files = getAllFilesWithScores();
    std::sort(files.begin(), files.end());
    
    qint64 spaceFreed = 0;
    QSet<int> deletedEpisodes;
    bool deletedSomething = true;
    
    // Keep looping until quota met or no more deletions possible
    while (spaceFreed < spaceNeeded && deletedSomething) {
        deletedSomething = false;
        
        for (auto& file : files) {
            if (spaceFreed >= spaceNeeded) break;
            if (isAlreadyDeleted(file.lid, deletedEpisodes)) continue;
            
            if (wouldCreateGap(file.lid, deletedEpisodes)) continue;
            if (isLastFileAndCreatesGap(file.lid, deletedEpisodes)) continue;
            
            markForDeletion(file.lid);
            spaceFreed += file.size;
            deletedSomething = true;
            
            if (isLastFileForEpisode(file.lid)) {
                deletedEpisodes.insert(getEpisodeId(file.lid));
            }
        }
    }
}
```

**Solution 2: Restart Loop After Each Deletion (User Suggestion)**
```cpp
void autoMarkFilesForDeletion() {
    QList<ScoredFile> files = getAllFilesWithScores();
    std::sort(files.begin(), files.end());
    
    qint64 spaceFreed = 0;
    QSet<int> deletedEpisodes;
    
    size_t index = 0;
    while (spaceFreed < spaceNeeded && index < files.size()) {
        auto& file = files[index];
        
        if (wouldCreateGap(file.lid, deletedEpisodes)) {
            index++;  // Try next file
            continue;
        }
        
        if (isLastFileAndCreatesGap(file.lid, deletedEpisodes)) {
            index++;
            continue;
        }
        
        markForDeletion(file.lid);
        spaceFreed += file.size;
        
        if (isLastFileForEpisode(file.lid)) {
            deletedEpisodes.insert(getEpisodeId(file.lid));
        }
        
        // CRITICAL: Restart from beginning after each deletion
        // This ensures we always check newly-available endpoints first
        index = 0;
    }
}
```

**Comparison:**

**Solution 1 (Multi-Pass):**
- ✅ Simpler logic (just loop until no more deletions)
- ✅ Respects score order within each pass
- ⚠️ May delete files out of strict score order across passes
- Example: Pass 1 deletes ep 10 (score 36), Pass 2 deletes ep 2 (score 30 < 36)

**Solution 2 (Restart After Deletion):**
- ✅ Always processes lowest-scored available file first
- ✅ Strictly respects score order
- ⚠️ More complex (manual index management)
- ⚠️ **Critical insight from user:** Prevents accidentally deleting high-scored files

**Example: Why Restart Matters**

Scenario: [1][2][3][4][5][6][7][8][9][10], need to delete 5 files
```
Scores: ep1=38, ep2=30, ep3=26, ep4=22, ep5=20, ep6=24, ep7=28, ep8=32, ep9=34, ep10=36

Without restart (multi-pass):
Pass 1: Delete ep10 (36) → [1..9]
Pass 2: Delete ep9 (34) → [1..8]  (ep2 now available at score 30)
Pass 3: Delete ep8 (32) → [1..7]  (ep7 now available at score 28)
Continue deleting high-scored endpoints...

With restart:
Delete ep10 (36) → restart → [1..9]
Delete ep1 (38) → restart → [2..9]
Delete ep9 (34) → restart → [2..8]
Delete ep2 (30) → restart → [3..8]
Delete ep8 (32) → restart → [3..7]
```

❌ **Problem with both approaches:** They delete higher-scored endpoints before lower-scored middle episodes become available!

**Optimal Solution 3: Priority Queue with Dynamic Updates**
```cpp
void autoMarkFilesForDeletion() {
    // Use priority queue that automatically reorders as deletions occur
    auto cmp = [](const ScoredFile& a, const ScoredFile& b) { return a.score > b.score; };
    std::priority_queue<ScoredFile, std::vector<ScoredFile>, decltype(cmp)> pq(cmp);
    
    // Initialize with all files
    for (auto& file : getAllFilesWithScores()) {
        pq.push(file);
    }
    
    qint64 spaceFreed = 0;
    QSet<int> deletedEpisodes;
    QSet<int> deletedFiles;
    
    while (spaceFreed < spaceNeeded && !pq.empty()) {
        auto file = pq.top();
        pq.pop();
        
        if (deletedFiles.contains(file.lid)) continue;
        
        if (wouldCreateGap(file.lid, deletedEpisodes)) continue;
        if (isLastFileAndCreatesGap(file.lid, deletedEpisodes)) continue;
        
        markForDeletion(file.lid);
        spaceFreed += file.size;
        deletedFiles.insert(file.lid);
        
        if (isLastFileForEpisode(file.lid)) {
            deletedEpisodes.insert(getEpisodeId(file.lid));
        }
    }
}
```

⚠️ **Still doesn't solve the core problem:** Scores are pre-calculated and don't change when gap status changes.

**Best Solution: Accept Score Order Limitation**

**User is correct:** The real concern is that we might delete high-scored files when lower-scored files become available later.

**Reality check:** This is acceptable behavior because:
1. **Gap protection is a hard constraint** - maintaining series continuity is more important than perfect score ordering
2. **Endpoints naturally have higher scores** - they're farther from current position or already watched, so deleting them first is usually correct
3. **Middle episodes are protected** - the most important files (current and nearby) are kept regardless

**Recommended Approach: Multi-Pass with Score Awareness**
```cpp
void autoMarkFilesForDeletion() {
    QList<ScoredFile> files = getAllFilesWithScores();
    std::sort(files.begin(), files.end());
    
    qint64 spaceFreed = 0;
    QSet<int> deletedEpisodes;
    
    // Multi-pass until quota met or no deletions possible
    bool madeProgress = true;
    while (spaceFreed < spaceNeeded && madeProgress) {
        madeProgress = false;
        
        for (auto& file : files) {
            if (spaceFreed >= spaceNeeded) break;
            if (deletedEpisodes.contains(getEpisodeId(file.lid))) continue;
            
            if (wouldCreateGap(file.lid, deletedEpisodes)) continue;
            if (isLastFileAndCreatesGap(file.lid, deletedEpisodes)) continue;
            
            markForDeletion(file.lid);
            spaceFreed += file.size;
            madeProgress = true;
            
            if (isLastFileForEpisode(file.lid)) {
                deletedEpisodes.insert(getEpisodeId(file.lid));
            }
        }
    }
    
    // If quota not met, log warning (series continuity prevents further deletion)
    if (spaceFreed < spaceNeeded) {
        LOG(QString("Could not meet space quota (%1 MB freed of %2 MB needed) - "
                    "remaining files protected by series continuity")
            .arg(spaceFreed/1024/1024).arg(spaceNeeded/1024/1024));
    }
}
```

✅ **This approach:**
- Respects score order within each pass
- Handles dynamic gap status changes across passes
- Terminates when no more deletions possible
- Logs when series continuity prevents reaching quota

2. **Less Predictable**
   - Deletion order depends on gap status, which changes dynamically
   - Harder to explain to users why certain files weren't deleted
   - Debug complexity: "file had low score but wasn't deleted because..."

3. **Complexity in Edge Cases**
   - What if no non-gap files exist and space is critical?
   - How to handle partial series (episodes 5-10 only)?
   - Need additional logic for emergency deletion

**Hybrid Approach (Recommended):**

Combine both approaches for optimal results:

```cpp
void autoMarkFilesForDeletion() {
    QList<ScoredFile> files = getAllFilesWithScores();
    std::sort(files.begin(), files.end());
    
    qint64 spaceFreed = 0;
    QSet<int> deletedEpisodes;
    
    for (auto& file : files) {
        if (spaceFreed >= spaceNeeded) break;
        
        // PRIMARY CHECK: Would this create a gap given current deletions?
        if (wouldCreateGapAfterDeletions(file.lid, deletedEpisodes)) {
            continue;  // Skip gap-creating files
        }
        
        // SECONDARY CHECK: Is this the last file for the episode?
        if (isLastFileForEpisode(file.lid, deletedEpisodes)) {
            // Only delete last file if it's an endpoint
            if (wouldCreateGapIfEpisodeDeleted(file.lid, deletedEpisodes)) {
                continue;  // Protect last file of middle episode
            }
        }
        
        markForDeletion(file.lid);
        spaceFreed += file.size;
        
        // Track which episodes are fully deleted
        if (isLastFileForEpisode(file.lid, deletedEpisodes)) {
            deletedEpisodes.insert(getEpisodeNumber(file.lid));
        }
    }
    
    // FALLBACK: If space target not met and all remaining files create gaps
    if (spaceFreed < spaceNeeded && !deletedSomething) {
        // Emergency mode: allow gap creation for hidden/low-priority anime
        // Or: notify user that space target cannot be met without breaking series
    }
}
```

**Benefits of Hybrid:**
- Uses scoring for quality/priority ranking
- Uses conditions for hard constraints (gap prevention)
- Handles dynamic gap status changes
- Provides fallback for edge cases

#### Recommendation

**Use Approach B (Gap as Condition) with the hybrid implementation:**

**Reasons:**
1. ✅ Better separation of concerns (quality vs. constraints)
2. ✅ Automatic dynamic gap detection as deletions occur
3. ✅ More flexible and easier to extend with additional constraints
4. ✅ Better performance (gap checks only on deletion candidates)
5. ✅ Simpler scoring logic (no artificial protection bonuses)

**Implementation:**
```cpp
// In watchsessionmanager.cpp
void WatchSessionManager::autoMarkFilesForDeletion() {
    // ... existing space calculation ...
    
    // Get all files with scores
    QList<CandidateInfo> candidates = getAllCandidatesWithScores();
    
    // Sort by score (lowest = most deletable)
    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateInfo& a, const CandidateInfo& b) {
                  return a.score < b.score;
              });
    
    qint64 accumulatedSpace = 0;
    QSet<int> deletedEpisodes;  // Track fully deleted episodes
    
    for (const auto& candidate : candidates) {
        if (accumulatedSpace >= spaceToFreeBytes) break;
        
        // CONSTRAINT 1: Gap prevention
        if (wouldCreateGap(candidate.lid, deletedEpisodes)) {
            LOG(QString("Skipping lid=%1: would create gap").arg(candidate.lid));
            continue;
        }
        
        // CONSTRAINT 2: Last file protection
        if (isLastFileForEpisode(candidate.lid)) {
            if (wouldCreateGapIfEpisodeDeleted(candidate.lid, deletedEpisodes)) {
                LOG(QString("Skipping lid=%1: last file of middle episode").arg(candidate.lid));
                continue;
            }
        }
        
        // Mark for deletion
        markFileForDeletion(candidate.lid);
        accumulatedSpace += candidate.size;
        
        // Update tracking
        if (isLastFileForEpisode(candidate.lid)) {
            deletedEpisodes.insert(getEpisodeId(candidate.lid));
        }
    }
}
```

**Key Implementation Details:**
- `wouldCreateGap(lid, deletedEpisodes)`: Check if deleting this file (given already-deleted episodes) would create a gap
- `isLastFileForEpisode(lid)`: Check if this is the only remaining file for its episode
- `deletedEpisodes`: Track which episodes are fully deleted to update gap detection dynamically

This approach provides the best balance of simplicity, flexibility, and correctness for series continuity enforcement.

#### Implementation Status

**Currently NOT Implemented:** The system has no gap detection or prevention logic. Distance-based scoring can easily create gaps.

**Critical Implementation Requirements:**
1. Add `wouldCreateGap(lid, deletedEpisodes)` helper function
2. Add `isLastFileForEpisode(lid)` helper function
3. Modify `autoMarkFilesForDeletion()` to check conditions in deletion loop
4. Track deleted episodes dynamically during deletion loop
5. Add setting: "Maintain series continuity" (default: enabled)
6. Add fallback logic for when no deletable files exist

**Recommendation:** Gap prevention is **CRITICAL** for series continuity. The condition-based approach (Approach B with hybrid implementation) is superior to score-based approach for:
- Dynamic gap detection
- Separation of concerns
- Flexibility and extensibility
- Performance

Without gap prevention, the system can break series into unusable fragments.

---

### 3. Duplicate File Handling and Minimum Episode Retention

#### Current Implementation

When multiple files exist for the same episode, the system:
1. Calculates scores independently for each file
2. Applies version penalties (`-1000` per higher version)
3. Deletes files with lowest scores first

**Implicit Protection:** The version penalty system ensures at least one file (the highest version) gets no penalty, making it unlikely to be deleted first.

However, there is **NO EXPLICIT GUARANTEE** that at least one file per episode will be kept.

#### Critical Importance for Series Continuity

**Keeping at least one file per episode is CRITICAL for series continuity** as defined in Section 2. If all files for an episode are deleted, it creates a **permanent gap** that cannot be filled without re-downloading.

**Gap Creation Through Complete Episode Deletion:**
```
Start:     [1][2][3][4][5][6][7][8][9][10][11][12]
Delete all v1,v2,v3 of episode 5: [1][2][3][4][ ][6][7][8][9][10][11][12]
```
❌ **CRITICAL FAILURE:** Series now has a permanent gap at episode 5

**Enforcement Priority for In-Series Deletion:**

As specified by the user, the deletion priority MUST be:

1. **Duplicate files per episode** (HIGHEST PRIORITY)
   - Delete all but the best version of each episode
   - Version penalties handle this: `-1000` per higher version
   - **CRITICAL:** This must happen BEFORE any episode-level deletion

2. **Watched episodes** (SECOND PRIORITY)
   - Episodes marked as watched are eligible for deletion
   - Delete from oldest watched first
   - BUT: Still maintain gap protection (Section 2)

3. **Last episodes** (THIRD PRIORITY)
   - When no watched episodes remain
   - Delete from the **end backwards** (12 → 11 → 10 → ...)
   - This maintains continuity from episode 1

4. **Middle episodes** (NEVER DELETE)
   - Episodes that would create gaps receive maximum protection
   - See Section 2 for gap detection logic

#### Problem Scenarios

**Scenario 1: All Files Have Low Scores**
- Episode 5 has 3 files: v1, v2, v3 (all same quality)
- User watched episode 5 long ago
- Episode 5 is in the middle of the series (episodes 1-4 and 6-12 exist)
- Disk space critically low

**Current scoring:**
- v3 (best): base 50 + watched -30 + session +100 = **120**
- v2: base 50 + watched -30 + session +100 + version -1000 = **-880**
- v1: base 50 + watched -30 + session +100 + version -2000 = **-1880**

**Deletion order:** v1 → v2 → v3

**Risk:** If system deletes v3 after v1 and v2, episode 5 completely disappears, creating a gap.

**Scenario 2: Episode at End of Series**
- Episode 12 (last episode) has 3 files: v1, v2, v3
- Episodes 1-11 exist locally
- Episode 12 can be safely deleted (no gap created)

**Current scoring:** Same as Scenario 1

**Deletion order:** v1 → v2 → v3

**Acceptable:** Episode 12 can be deleted without creating a gap (it's an endpoint).

#### Proposed Solution: Hierarchical Protection System

**Principle:** Combine multiple protection mechanisms to enforce the deletion priority.

**Protection Level 1: Duplicate Elimination (Highest Priority for Deletion)**
```cpp
// Version penalty already ensures duplicates are deleted first
if (higherVersionCount > 0) {
    score += higherVersionCount * SCORE_OLDER_REVISION;  // -1000 each
}
```
✅ Already implemented correctly

**Protection Level 2: Last File Protection (CRITICAL)**
```cpp
int WatchSessionManager::calculateMarkScore(int lid) const {
    int score = 50;
    // ... existing scoring logic ...
    
    // CRITICAL: Boost score if this is the only file for the episode
    int fileCount = getFileCountForEpisode(lid);
    if (fileCount == 1) {
        // Check if deleting this would create a gap
        if (wouldCreateGap(lid)) {
            score += SCORE_LAST_FILE_WITH_GAP;  // e.g., +2000 (maximum protection)
        } else {
            score += SCORE_LAST_FILE_NO_GAP;  // e.g., +500 (strong protection)
        }
    }
    
    return score;
}
```

**Protection Level 3: Gap Prevention**
```cpp
// From Section 2: prevent middle episode deletion
if (wouldCreateGap(lid) && fileCount > 1) {
    score += SCORE_MIDDLE_EPISODE_PROTECTION;  // +1000
}
```

**Combined Protection Example:**

**Episode 5 (middle episode, last file):**
- Base: 50
- Watched: -30
- **Last file**: +500
- **Would create gap**: +2000
- **Total: 2520** ← Very high, unlikely to be deleted

**Episode 12 (last episode, last file):**
- Base: 50
- Watched: -30
- **Last file**: +500
- **No gap created**: (endpoint)
- **Total: 520** ← Protected, but deletable if needed

**Episode 5 v2 (middle episode, duplicate):**
- Base: 50
- Watched: -30
- Version penalty: -1000
- **Would create gap**: +1000 (if v3 is last file)
- **Total: 20** ← Can be deleted (duplicate, v3 remains)

#### Implementation Approach

**Recommended: Multi-Level Score Boost**
```cpp
int WatchSessionManager::calculateMarkScore(int lid) const {
    int score = 50;
    // ... existing scoring logic ...
    
    int fileCount = getFileCountForEpisode(lid);
    
    // Protection for last file of an episode
    if (fileCount == 1) {
        bool isHidden = isCardHidden(getAnimeIdForFile(lid));
        int animeRating = getFileRating(lid);
        
        // Only protect if not explicitly marked for deletion
        if (!isHidden && animeRating >= 400) {
            if (wouldCreateGap(lid)) {
                // CRITICAL: Last file of middle episode
                score += SCORE_LAST_FILE_WITH_GAP;  // +2000
            } else {
                // Last file of endpoint episode (can be deleted if needed)
                score += SCORE_LAST_FILE_NO_GAP;  // +500
            }
        }
    }
    
    // Protection for middle episodes (even with duplicates)
    if (wouldCreateGap(lid) && fileCount > 1) {
        score += SCORE_MIDDLE_EPISODE_PROTECTION;  // +1000
    }
    
    return score;
}
```

**Key Constants:**
```cpp
static const int SCORE_LAST_FILE_WITH_GAP = 2000;      // Maximum protection
static const int SCORE_LAST_FILE_NO_GAP = 500;         // Strong protection
static const int SCORE_MIDDLE_EPISODE_PROTECTION = 1000; // Prevent gaps
static const int SCORE_OLDER_REVISION = -1000;          // Delete duplicates first
```

#### Workflow Example: Enforced Priority

**Scenario:** 12-episode series, episodes 1-6 watched, episodes 5 and 10 have duplicates

**Files:**
- Episode 1: single file, watched
- Episode 5: v1, v2, v3 (watched, middle episode)
- Episode 6: single file, watched
- Episode 10: v1, v2 (unwatched, middle episode)
- Episode 12: single file, unwatched

**Scoring:**
```
Episode 5 v1: base 50 + watched -30 + version -2000 + gap protection +1000 = -980
Episode 5 v2: base 50 + watched -30 + version -1000 + gap protection +1000 = 20
Episode 5 v3: base 50 + watched -30 + last file +2000 (creates gap) = 2020

Episode 10 v1: base 50 + not watched +50 + version -1000 + gap protection +1000 = 1100
Episode 10 v2: base 50 + not watched +50 + last file +2000 (creates gap) = 2100

Episode 1: base 50 + watched -30 + last file +500 (no gap, endpoint) = 520
Episode 6: base 50 + watched -30 + last file +2000 (creates gap) = 2020
Episode 12: base 50 + not watched +50 + last file +500 (no gap, endpoint) = 600
```

**Deletion Order:**
1. **Episode 5 v1** (-980) ← Duplicate, oldest version
2. **Episode 5 v2** (20) ← Duplicate, middle version
3. **Episode 1** (520) ← Watched, endpoint, can delete without gap
4. **Episode 12** (600) ← Unwatched, endpoint, delete from end
5. **Episode 10 v1** (1100) ← Duplicate, but protected by gap detection
6. Episodes 5 v3, 6, 10 v2 have scores > 2000 ← **NEVER DELETED** (would create gaps)

✅ **Priority enforced correctly:**
1. Duplicates deleted first (5 v1, 5 v2, then 10 v1)
2. Watched episodes at endpoints (1)
3. Last episodes (12)
4. Middle episodes never deleted (5 v3, 6, 10 v2 protected)

#### Exception: Justified Deletion

There are LIMITED cases where deleting all files for an episode is acceptable:
1. **Hidden anime + explicit user deletion**: User wants entire anime gone
2. **Storage emergency**: Disk space < 5% and no other options
3. **User override**: Manual deletion command

For normal auto-deletion, **at least one file per episode MUST be maintained** to preserve series continuity.
```cpp
if (fileCount == 1) {
    bool isHidden = isCardHidden(aid);
    int animeRating = getFileRating(lid);
    
    if (!isHidden && animeRating >= 400) {  // Not hidden and not terrible
        score += SCORE_LAST_EPISODE_FILE;  // Protect
    }
}
```

#### Workflow Example

**Scenario:** Episode 1 has 3 files (v1, v2, v3), all watched, far behind current position

**Current behavior:**
- All 3 files could theoretically be marked for deletion
- Order: v1 (-1929) → v2 (-929) → v3 (71)
- If disk critically low, v3 could be deleted, leaving no files

**With Option B (score boost):**
- v3 fileCount = 3, no boost: score = 71
- Delete v1 and v2
- After deletion, v3 fileCount = 1, recalculate: score = 71 + 500 = 571
- v3 now has strong protection, unlikely to be deleted

**Result:** Episode 1 always has at least one file (v3) unless disk space is catastrophically low and other factors override.

---

### 4. Minimum Anime Retention: "Just in Case" Protection

#### Current Implementation

The system has **no concept** of minimum anime retention. Files from any anime can be deleted if:
1. Disk space falls below threshold
2. Files have low scores (watched, far from current position, etc.)

**Problem:** User might want to watch an anime "someday" but it gets completely deleted because:
- Not currently watching (no active session bonus)
- Never started watching (no session at all)
- Hidden to declutter UI (hidden penalty)
- Low community rating (rating penalty)

#### Proposed Solution: Minimum Episode Protection

**Principle:** For each anime, keep **at least N episodes** (e.g., 3-5) regardless of score, unless anime has severe justified penalties.

**Rationale:**
1. **Low commitment sampling**: Keeping 3-5 episodes lets user sample anime later
2. **Recommendation-friendly**: Can lend first few episodes to friends
3. **Rewatch starter**: Sufficient episodes to rekindle interest
4. **Storage-efficient**: 3 episodes ≈ 1-2 GB, negligible for modern drives

**Implementation Approach:**

**Option A: Protected Episode Count**
```cpp
// New setting
int m_minEpisodesPerAnime = 3;  // Default: keep at least 3 episodes

void WatchSessionManager::autoMarkFilesForDeletion() {
    // ... existing marking logic ...
    
    // Count episodes per anime
    QMap<int, QSet<int>> animeEpisodes;  // aid -> set of eids
    for (int lid : allFiles) {
        int aid = getAnimeIdForFile(lid);
        int eid = getEpisodeId(lid);
        animeEpisodes[aid].insert(eid);
    }
    
    // For each anime, protect top N episodes
    for (auto it = animeEpisodes.begin(); it != animeEpisodes.end(); ++it) {
        int aid = it.key();
        QSet<int> eids = it.value();
        
        // Check if anime has justified penalties
        bool isHidden = isCardHidden(aid);
        int animeRating = getAnimeRating(aid);
        
        if (!isHidden && animeRating >= 400) {  // Deserves protection
            // Find best episodes to protect (typically first N episodes)
            QList<QPair<int, int>> episodeScores;  // (eid, bestFileScore)
            
            for (int eid : eids) {
                int bestScore = INT_MIN;
                int bestLid = 0;
                
                // Find best file for this episode
                QList<int> episodeFiles = getFilesForEpisode(eid);
                for (int lid : episodeFiles) {
                    int score = calculateMarkScore(lid);
                    if (score > bestScore) {
                        bestScore = score;
                        bestLid = lid;
                    }
                }
                
                episodeScores.append(qMakePair(eid, bestScore));
            }
            
            // Sort by episode number (prefer keeping early episodes)
            std::sort(episodeScores.begin(), episodeScores.end(),
                      [](const QPair<int, int>& a, const QPair<int, int>& b) {
                          return getEpisodeNumber(a.first) < getEpisodeNumber(b.first);
                      });
            
            // Protect first N episodes
            for (int i = 0; i < qMin(m_minEpisodesPerAnime, episodeScores.size()); i++) {
                int eid = episodeScores[i].first;
                protectBestFileForEpisode(eid);
            }
        }
    }
}
```

**Option B: Score Boost for Early Episodes**
```cpp
int WatchSessionManager::calculateMarkScore(int lid) const {
    int score = 50;
    // ... existing scoring logic ...
    
    // Boost score for early episodes of any anime
    int episodeNumber = getEpisodeNumber(lid);
    if (episodeNumber <= 5) {  // First 5 episodes
        int aid = getAnimeIdForFile(lid);
        int episodesKept = getKeptEpisodeCount(aid);
        
        if (episodesKept < m_minEpisodesPerAnime) {
            // Boost early episodes if we haven't kept enough yet
            score += SCORE_EARLY_EPISODE_PROTECTION;  // e.g., +200
        }
    }
    
    return score;
}
```

**Recommendation:** Use **Option B** (score boost for early episodes) as it:
- Integrates naturally with the scoring system
- Prioritizes keeping episodes 1-5 (most important for sampling)
- Doesn't require complex post-processing
- Allows user to override with explicit deletion if needed

#### Configuration

Add user setting:
```cpp
// Settings
int getMinEpisodesPerAnime() const { return m_minEpisodesPerAnime; }
void setMinEpisodesPerAnime(int count) { m_minEpisodesPerAnime = count; }

// Default values:
// 0 = No minimum protection (aggressive deletion)
// 3 = Keep at least 3 episodes (balanced)
// 5 = Keep at least 5 episodes (conservative)
// -1 = Keep at least 1 episode per anime (minimal)
```

#### Exception Cases

**When to allow complete deletion:**
1. **Hidden anime**: User explicitly wants it removed
2. **Rating < 300**: Universally terrible anime (3.0/10)
3. **User marks "Delete All"**: Explicit override
4. **Storage crisis**: Disk space < 5% free (emergency mode)

#### Workflow Example

**Scenario:** User has 50 anime in collection, disk space falls below threshold

**Current behavior:**
- System marks files from all 50 anime for deletion based on scores
- Anime A (never watched, rating 6.5, hidden): all 12 episodes marked for deletion
- Anime B (watched 2 episodes, rating 7.5): episodes 1-2 deleted, 3-12 kept

**With minimum retention (N=3):**
- Anime A: episodes 1-3 protected (score boost), episodes 4-12 deleted
- Anime B: episodes 1-3 protected, episode 2 already watched (neutral), episodes 4-12 deletable

**Result:** User keeps at least 3 episodes of Anime A for potential future watching, while still freeing significant space (9 episodes deleted).

---

### 5. Codec Awareness: Deep Analysis and Implementation Strategy

#### Current Implementation: Codec-Agnostic Bitrate Calculation

The system uses a **universal bitrate formula**:
```cpp
bitrate = baseline × (resolution_megapixels / 2.07)
```

This assumes **constant encoding efficiency across all codecs**, which is incorrect.

#### Problem: Codec Efficiency Varies Drastically

**Codec Compression Efficiency (vs. H.264 baseline):**
- **H.264/AVC**: 1.0× (baseline)
- **H.265/HEVC**: ~0.5× (50% bitrate for same quality)
- **AV1**: ~0.35× (35% bitrate for same quality)
- **VP9**: ~0.6× (60% bitrate for same quality)
- **H.263/MPEG-4**: ~1.5× (50% higher bitrate needed)

**Example Comparison:**
- H.264: 3.5 Mbps for 1080p anime
- H.265: 1.75 Mbps for same quality
- AV1: 1.2 Mbps for same quality

**Current System Behavior:**
- User sets baseline: 3.5 Mbps for 1080p (assuming H.264)
- File A (1080p, H.265, 1.8 Mbps): penalty = -40 (50% below expected)
- File B (1080p, H.264, 3.5 Mbps): penalty = 0 (matches expected)

**Result:** System penalizes File A despite it being correct quality for H.265. This causes:
❌ High-quality H.265 encodes marked for deletion
❌ Bloated H.264 encodes marked for keeping
❌ Inefficient space usage

#### Solution Strategy: Multi-Level Codec Awareness

**Level 1: Codec Detection** ✅ (Partially Available)

Database schema shows:
```sql
file: codec_video TEXT, bitrate_video INTEGER
```

The system can retrieve codec information:
```cpp
QString getFileCodec(int lid) const {
    QSqlQuery q(db);
    q.prepare("SELECT f.codec_video FROM mylist m JOIN file f ON m.fid = f.fid WHERE m.lid = ?");
    q.addBindValue(lid);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return QString();
}
```

**Level 2: Codec Efficiency Mapping**

Create a codec efficiency lookup:
```cpp
double WatchSessionManager::getCodecEfficiency(const QString& codec) const {
    QString codecLower = codec.toLower().trimmed();
    
    // H.265/HEVC family
    if (codecLower.contains("hevc") || codecLower.contains("h265") || 
        codecLower.contains("h.265") || codecLower.contains("x265")) {
        return 0.5;  // 50% of H.264 bitrate
    }
    
    // AV1 family
    if (codecLower.contains("av1") || codecLower.contains("av01")) {
        return 0.35;  // 35% of H.264 bitrate
    }
    
    // VP9
    if (codecLower.contains("vp9") || codecLower.contains("vp09")) {
        return 0.6;  // 60% of H.264 bitrate
    }
    
    // H.264/AVC family (baseline)
    if (codecLower.contains("avc") || codecLower.contains("h264") || 
        codecLower.contains("h.264") || codecLower.contains("x264")) {
        return 1.0;  // 100% baseline
    }
    
    // Older/inefficient codecs
    if (codecLower.contains("xvid") || codecLower.contains("divx") || 
        codecLower.contains("mpeg4") || codecLower.contains("h263")) {
        return 1.5;  // 150% of H.264 bitrate
    }
    
    // Unknown codec: assume H.264 efficiency
    return 1.0;
}
```

**Level 3: Codec-Aware Expected Bitrate Calculation**

Modify the existing calculation:
```cpp
double WatchSessionManager::calculateExpectedBitrate(const QString& resolution, const QString& codec) const {
    // Get baseline bitrate from settings (for H.264)
    double baselineBitrate = 3.5;  // Mbps, default
    
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("SELECT value FROM settings WHERE name = 'preferredBitrate'");
        if (q.exec() && q.next()) {
            baselineBitrate = q.value(0).toDouble();
            if (baselineBitrate <= 0) {
                baselineBitrate = 3.5;
            }
        }
    }
    
    // Calculate resolution scaling
    double megapixels = parseMegapixels(resolution);
    double resolutionScaled = baselineBitrate * (megapixels / 2.07);
    
    // Apply codec efficiency
    double codecEfficiency = getCodecEfficiency(codec);
    double expectedBitrate = resolutionScaled * codecEfficiency;
    
    return expectedBitrate;
}
```

**Level 4: Enhanced Bitrate Scoring**

Update the scoring logic:
```cpp
int bitrate = getFileBitrate(lid);
QString resolution = getFileResolution(lid);
QString codec = getFileCodec(lid);

if (bitrate > 0 && !resolution.isEmpty() && fileCount > 1) {
    double bitrateScore = calculateBitrateScore(bitrate, resolution, codec, fileCount);
    score += static_cast<int>(bitrateScore);
}

// Modified scoring method:
double WatchSessionManager::calculateBitrateScore(double actualBitrate, const QString& resolution, 
                                                   const QString& codec, int fileCount) const {
    if (fileCount <= 1) {
        return 0.0;  // No penalty when only one file
    }
    
    // Convert to Mbps
    double actualBitrateMbps = actualBitrate / 1000.0;
    
    // Calculate codec-aware expected bitrate
    double expectedBitrate = calculateExpectedBitrate(resolution, codec);
    
    // Calculate percentage difference
    double percentDiff = 0.0;
    if (expectedBitrate > 0) {
        percentDiff = std::abs((actualBitrateMbps - expectedBitrate) / expectedBitrate) * 100.0;
    }
    
    // Apply penalties
    if (percentDiff > 50.0) return SCORE_BITRATE_FAR;      // -40
    if (percentDiff > 30.0) return SCORE_BITRATE_MODERATE; // -25
    if (percentDiff > 10.0) return SCORE_BITRATE_CLOSE;    // -10
    return 0.0;
}
```

#### Advanced: Codec Quality Tiers

Different codecs have different quality characteristics even at the same bitrate:

**Tier 1 (Excellent):** AV1, H.265/HEVC
- Modern, efficient, excellent quality
- Bonus: +10 (prefer modern codecs)

**Tier 2 (Good):** H.264/AVC, VP9
- Widely compatible, good quality
- Bonus: 0 (neutral)

**Tier 3 (Acceptable):** MPEG-4, XviD, DivX
- Older codecs, lower efficiency
- Penalty: -15 (prefer modern)

**Tier 4 (Poor):** MPEG-2, H.263
- Very old, inefficient
- Penalty: -30 (avoid)

Implementation:
```cpp
int codecTier = getCodecTier(codec);
if (codecTier == 1) {
    score += SCORE_MODERN_CODEC;  // +10
} else if (codecTier == 3) {
    score += SCORE_OLD_CODEC;  // -15
} else if (codecTier == 4) {
    score += SCORE_ANCIENT_CODEC;  // -30
}
```

#### Codec Comparison Example

**Scenario:** Episode 1 has 3 files, all 1080p

- **File A:** H.264, 3.5 Mbps, 1080p (perfect match for H.264)
- **File B:** H.265, 1.8 Mbps, 1080p (perfect match for H.265)
- **File C:** AV1, 1.2 Mbps, 1080p (perfect match for AV1)

**Current System:**
- File A: bitrate penalty = 0 (matches 3.5 Mbps baseline)
- File B: bitrate penalty = -40 (49% below baseline, flagged as over-compressed)
- File C: bitrate penalty = -40 (66% below baseline, flagged as over-compressed)

**With Codec Awareness:**
- File A: expected = 3.5 × 1.0 = 3.5 Mbps, actual = 3.5, diff = 0%, penalty = 0, tier bonus = 0, **total = 0**
- File B: expected = 3.5 × 0.5 = 1.75 Mbps, actual = 1.8, diff = 3%, penalty = 0, tier bonus = +10, **total = +10**
- File C: expected = 3.5 × 0.35 = 1.23 Mbps, actual = 1.2, diff = 2%, penalty = 0, tier bonus = +10, **total = +10**

**Result:** System correctly recognizes all three files as acceptable quality, with Files B and C getting small bonuses for using modern codecs.

#### Edge Cases and Considerations

**1. Codec String Variations**

AniDB may return codec strings in various formats:
- "H.265/HEVC Main@L4.1"
- "x265"
- "hevc"
- "HEVC Main 10"

Solution: Use flexible string matching (contains, case-insensitive)

**2. Multi-Codec Files**

Some files have multiple video tracks with different codecs. Solution:
- Parse primary video track codec
- Ignore secondary tracks

**3. Unknown/Missing Codec**

If codec field is empty or unknown:
- Assume H.264 efficiency (1.0×)
- No tier bonus/penalty
- Log warning for debugging

**4. Anime-Specific Characteristics**

Anime has different compression characteristics than live-action:
- **Flat colors**: Compress very efficiently
- **Line art**: Sharp edges require more bitrate
- **Static scenes**: Long GOPs possible
- **Fast motion**: Higher bitrate needed

Current formula already assumes anime characteristics. Codec awareness builds on top of this.

**5. Future-Proofing**

New codecs (e.g., H.266/VVC, AV2) can be added:
```cpp
if (codecLower.contains("vvc") || codecLower.contains("h266")) {
    return 0.25;  // Even more efficient than AV1
}
```

#### Recommended Implementation Plan

**Phase 1 (Immediate):**
1. Add `getFileCodec()` helper method
2. Add `getCodecEfficiency()` lookup table
3. Modify `calculateExpectedBitrate()` to accept codec parameter

**Phase 2 (Short-term):**
1. Update `calculateBitrateScore()` to use codec-aware calculation
2. Add codec tier bonuses/penalties
3. Add comprehensive tests for different codecs

**Phase 3 (Long-term):**
1. Add user setting: "Prefer modern codecs" (checkbox)
2. Add codec statistics to UI (show codec distribution)
3. Add codec migration suggestions (e.g., "20 files could be upgraded to H.265")

#### Impact Assessment

| Aspect | Current | With Codec Awareness | Improvement |
|--------|---------|---------------------|-------------|
| H.265 files marked for deletion | ❌ Yes (incorrect) | ✅ No (correct) | Significant |
| Space efficiency | Medium | High | Files using efficient codecs kept |
| Quality assessment | Inaccurate | Accurate | Correctly identifies quality |
| User experience | Confusing | Clear | Transparent codec handling |
| Storage savings | 0% | 30-50% | By encouraging H.265/AV1 |

**Conclusion:** Codec awareness is **critical** for accurate quality assessment and should be implemented as a high-priority enhancement. The infrastructure already exists (codec field in database), and implementation is straightforward with significant benefits.

---

## Final Assessment

**Overall Grade: A- (Excellent with minor improvements needed)**

**Strengths:**
- ✅ Well-architected with clear separation of concerns
- ✅ Comprehensive factor coverage
- ✅ Strong test coverage for core functionality
- ✅ Handles complex scenarios (multi-season, versions)
- ✅ User-configurable preferences
- ✅ Automatic space management
- ✅ Robust error handling

**Weaknesses:**
- ⚠️ Some edge cases not fully addressed
- ⚠️ Performance could be optimized for large collections
- ⚠️ Missing test coverage for some factors
- ⚠️ Documentation gaps on scoring behavior

**Recommendations:**

**Critical (Highest Impact - MUST IMPLEMENT):**
1. **Gap prevention for series continuity** - Prevent deletion of middle episodes that would create unwatchable gaps
2. **Last file per episode protection** - CRITICAL for continuity, ensures every episode has at least one file
3. **Implement codec awareness** - Prevents incorrect deletion of high-quality H.265/AV1 files

**Important (High Impact):**
4. **Minimum anime retention** - Keep at least 3-5 early episodes per anime for future sampling
5. **Switch to group rating** - Better quality assessment than anime rating
6. **Add favorites/protection system** - Allow users to explicitly protect content

**Enhancements (Medium Impact):**
7. **Clarify double-penalty behavior** - Document or fix watched episode double-counting
8. **Add edge case tests** - Improve test coverage for language matching, quality, ratings
9. **Optimize for large collections** - Batch queries and caching for 10,000+ files
10. **Add rewatch tracking** - Distinguish between "watched once" and "rewatched frequently"

**Priority Order (Revised Based on Clarifications):**
1. **Gap prevention + Last file protection** (Phase 1) - CRITICAL for series continuity
2. **Codec awareness** (Phase 1) - Prevents incorrect modern file deletion
3. **Minimum anime retention** (Phase 2)
4. **Group rating implementation** (Phase 2)
5. **Favorites/protection system** (Phase 3)

**Clarified Understanding of Series Continuity:**

Based on user feedback, series continuity is NOT about rewatching or preserving episode 1. Instead, it's about:

1. **Never creating gaps in the middle of a series** - Deleting episode 5 when episodes 1-4 and 6-12 exist breaks the viewing experience
2. **Enforced deletion priority:**
   - **First:** Duplicate files per episode (keep best version only)
   - **Second:** Watched episodes (but only if no gap created)
   - **Third:** Last episodes (delete from end backwards: 12 → 11 → 10 → ...)
   - **NEVER:** Middle episodes that would create gaps

3. **At least one file per episode is CRITICAL** - Complete episode deletion creates permanent gaps that require re-downloading

**Implementation Requirements:**

**Phase 1 (CRITICAL - Must implement together):**
```cpp
// 1. Gap detection
bool wouldCreateGap(int lid) const;

// 2. Gap-aware scoring
static const int SCORE_MIDDLE_EPISODE_PROTECTION = 1000;  // Prevent gaps
static const int SCORE_LAST_FILE_WITH_GAP = 2000;         // Maximum protection
static const int SCORE_LAST_FILE_NO_GAP = 500;            // Strong protection

// 3. Hierarchical protection in calculateMarkScore()
if (wouldCreateGap(lid)) {
    score += SCORE_MIDDLE_EPISODE_PROTECTION;
}
if (fileCount == 1) {
    score += wouldCreateGap(lid) ? SCORE_LAST_FILE_WITH_GAP : SCORE_LAST_FILE_NO_GAP;
}
```

**Phase 2 (High Priority):**
- Codec efficiency multipliers (H.265=0.5×, AV1=0.35×)
- Minimum episode retention (3-5 early episodes per anime)
- Group rating instead of anime rating

**Conclusion:** The file scoring system is well-designed and functionally correct for its core purpose. However, **series continuity is fundamentally broken** without gap prevention and last-file protection. The extended analysis reveals that these are not optional enhancements but **critical requirements** for the system to function correctly. Without them, the system can:

1. ❌ Create unwatchable gaps by deleting middle episodes
2. ❌ Delete all versions of an episode, creating permanent gaps
3. ❌ Break the viewing experience for partially-watched series

**The current implementation is NOT production-ready for series continuity.** Gap prevention and last-file protection must be implemented before the deletion system can be safely enabled. Once these are in place, the additional enhancements (codec awareness, group rating, minimum retention) would elevate the system from "functional" to "excellent."
