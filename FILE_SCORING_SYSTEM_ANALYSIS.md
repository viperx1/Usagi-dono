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

### 2. Series Continuity and Sequential Deletion

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

**Problem Identified:** Episodes **behind** the current position get a **positive distance modifier**, which partially offsets the "already watched" penalty. This means:
- Episode just watched (1 behind): score boost of +10 - 30 = -20
- Episode watched long ago (10 behind): score boost of +1 - 30 = -29

This creates a **gradient that favors keeping recently watched episodes**, which is counterproductive for maintaining series continuity.

#### Series Continuity Principle

**Goal:** When deleting files from an active session, delete from the **end (last episodes)** first, not from the beginning.

**Rationale:**
1. **Forward viewing is common**: Users typically watch episodes in order (1→2→3→...)
2. **Rewatching is sequential**: If user rewatches, they start from episode 1
3. **Completion continuity**: Keeping episodes 1-8 and deleting 9-12 maintains more value than keeping episodes 5-12 and deleting 1-4
4. **New viewers**: If user recommends anime to a friend, episodes 1-N are essential

#### Proposed Fix: Invert Distance Calculation for Behind Episodes

**Current behavior:**
```cpp
// Episode behind current position gets POSITIVE adjustment
int distance = -5;  // 5 episodes behind
score += distance * -1;  // score += +5 (helps keep it)
```

**Proposed behavior:**
```cpp
// Episodes behind should have LARGER negative scores (more deletable)
// Episodes ahead should have SMALLER negative scores (less deletable)
if (distance < 0) {
    // Behind current position: use absolute distance as penalty
    score += abs(distance) * SCORE_DISTANCE_FACTOR;  // -1, -2, -3...
} else {
    // Ahead of current position: smaller penalty or even bonus for far ahead
    // This prioritizes keeping unwatched content
    score += distance * SCORE_DISTANCE_FACTOR;  // -1, -2, -3...
}
```

**Even Better: Asymmetric Penalties**

```cpp
if (distance < 0) {
    // Behind current: stronger penalty for farther episodes
    // Episode 1 behind: -2, Episode 5 behind: -10, Episode 10 behind: -20
    score += distance * 2;  // Multiply by 2 for stronger effect
} else if (distance <= aheadBuffer) {
    // Within ahead buffer: strong protection
    score += SCORE_IN_AHEAD_BUFFER;  // Already implemented: +75
} else {
    // Beyond ahead buffer: gentle penalty
    score += (distance - aheadBuffer) * -0.5;  // Half penalty
}
```

#### Workflow Example: Current vs. Proposed

**Scenario:** 12-episode series, currently at episode 7, ahead buffer = 3

**Current Scoring:**
- Episode 1 (6 behind): distance = -6, adjustment = +6, watched = -30, **total ≈ +26**
- Episode 4 (3 behind): distance = -3, adjustment = +3, watched = -30, **total ≈ +23**
- Episode 6 (1 behind): distance = -1, adjustment = +1, watched = -30, **total ≈ +21**
- Episode 7 (current): distance = 0, adjustment = 0, **total ≈ +50** (not watched yet)
- Episode 8 (1 ahead): distance = +1, adjustment = -1, buffer = +75, **total ≈ +124**
- Episode 10 (3 ahead): distance = +3, adjustment = -3, buffer = +75, **total ≈ +122**
- Episode 11 (4 ahead): distance = +4, adjustment = -4, **total ≈ +46**
- Episode 12 (5 ahead): distance = +5, adjustment = -5, **total ≈ +45**

**Deletion order:** Episode 6 → Episode 4 → Episode 1 → Episode 12 → Episode 11 → ...

❌ **Problem:** System deletes from the beginning (episodes 1-6) before deleting from the end (episodes 11-12). This breaks continuity if user wants to rewatch.

**Proposed Scoring (Asymmetric):**
- Episode 1 (6 behind): distance = -6, adjustment = -12, watched = -30, **total ≈ +8**
- Episode 4 (3 behind): distance = -3, adjustment = -6, watched = -30, **total ≈ +14**
- Episode 6 (1 behind): distance = -1, adjustment = -2, watched = -30, **total ≈ +18**
- Episode 7 (current): distance = 0, adjustment = 0, **total ≈ +50**
- Episode 8 (1 ahead): distance = +1, buffer = +75, **total ≈ +124**
- Episode 10 (3 ahead): distance = +3, buffer = +75, **total ≈ +122**
- Episode 11 (4 ahead): distance = +4, adjustment = -0.5, **total ≈ +49.5**
- Episode 12 (5 ahead): distance = +5, adjustment = -1.0, **total ≈ +49.0**

**Deletion order:** Episode 1 → Episode 4 → Episode 6 → Episode 12 → Episode 11 → ...

✅ **Better:** System still prioritizes deleting watched episodes, but **maintains continuity** by deleting the oldest watched episodes first, and far-ahead episodes before near-ahead episodes.

#### Special Case: Completed Series

For series where **all episodes are watched**, the system should:
1. Keep episode 1 (essential for rewatching)
2. Delete from the end backwards (12 → 11 → 10 → ... → 2)
3. Never delete the last remaining episode (episode 1)

This is automatically handled by the asymmetric penalty: episode 1 would have the smallest penalty among watched episodes.

#### Implementation Status

**Currently NOT Implemented:** The system uses symmetric distance penalties which break series continuity.

**Recommendation:** Implement asymmetric distance penalties with stronger penalties for episodes behind the current position.

---

### 3. Duplicate File Handling and Minimum Episode Retention

#### Current Implementation

When multiple files exist for the same episode, the system:
1. Calculates scores independently for each file
2. Applies version penalties (`-1000` per higher version)
3. Deletes files with lowest scores first

**Implicit Protection:** The version penalty system ensures at least one file (the highest version) gets no penalty, making it unlikely to be deleted first.

However, there is **NO EXPLICIT GUARANTEE** that at least one file per episode will be kept.

#### Problem Scenarios

**Scenario 1: All Files Have Low Scores**
- Episode 1 has 3 files: v1, v2, v3 (all japanese audio, good quality)
- User watched episode 1 long ago
- Episode 1 is far behind current watching position (episode 50)
- Disk space critically low

**Current scoring:**
- v3 (best): base 50 + watched -30 + distance -49 + session +100 = **71**
- v2: base 50 + watched -30 + distance -49 + session +100 + version -1000 = **-929**
- v1: base 50 + watched -30 + distance -49 + session +100 + version -2000 = **-1929**

**Deletion order:** v1 → v2 → v3

**Risk:** If disk space is extremely low and deletion threshold is high, the system could delete v3, leaving **no files for episode 1**.

**Scenario 2: Duplicate Files, All Similar Quality**
- Episode 5 has 2 files: FileA (720p, v1) and FileB (1080p, v1)
- Both unwatched, similar scores except bitrate penalty
- FileA: score ≈ 200
- FileB: score ≈ 180 (bitrate penalty for being bloated)

**Current behavior:** System could delete FileB, but there's no guarantee it keeps at least one.

#### Proposed Solution: Minimum Episode Retention

**Principle:** For each episode in an anime, **at least one file should be kept** unless:
1. The entire anime is marked for deletion (e.g., hidden card)
2. The episode is explicitly targeted for removal
3. The episode is the last in a completed series with severe penalties

**Implementation Approach:**

**Option A: Post-Processing Filter**
```cpp
void WatchSessionManager::autoMarkFilesForDeletion() {
    // ... existing marking logic ...
    
    // Post-processing: ensure at least one file per episode
    QMap<int, QList<int>> episodeFiles;  // eid -> list of lids
    
    // Group files by episode
    for (int lid : markedForDeletion) {
        int eid = getEpisodeId(lid);
        episodeFiles[eid].append(lid);
    }
    
    // For each episode, keep the highest-scored file
    for (auto it = episodeFiles.begin(); it != episodeFiles.end(); ++it) {
        int eid = it.key();
        QList<int> lids = it.value();
        
        // Count total files for this episode (including unmarked)
        int totalFiles = getFileCountForEpisode(lids.first());
        
        if (totalFiles <= lids.size()) {
            // Would delete ALL files for this episode
            // Keep the one with highest score
            int bestLid = lids.first();
            int bestScore = calculateMarkScore(bestLid);
            
            for (int lid : lids) {
                int score = calculateMarkScore(lid);
                if (score > bestScore) {
                    bestScore = score;
                    bestLid = lid;
                }
            }
            
            // Unmark the best file
            unmarkFileForDeletion(bestLid);
            LOG(QString("Protected lid=%1 (score=%2) as last file for eid=%3")
                .arg(bestLid).arg(bestScore).arg(eid));
        }
    }
}
```

**Option B: Score Boost for Last Remaining File**
```cpp
int WatchSessionManager::calculateMarkScore(int lid) const {
    int score = 50;
    // ... existing scoring logic ...
    
    // Boost score if this is the only file for the episode
    int fileCount = getFileCountForEpisode(lid);
    if (fileCount == 1) {
        score += SCORE_LAST_EPISODE_FILE;  // e.g., +500 (strong protection)
    }
    
    return score;
}
```

**Recommendation:** Use **Option B** (score boost) as it's simpler, more transparent, and integrates naturally with the scoring system.

#### Exception: Justified Deletion

There are cases where deleting all files for an episode is acceptable:
1. **Hidden anime**: User explicitly wants it gone
2. **Very low-rated anime + poor group**: Multiple severe penalties
3. **Duplicate episodes across versions**: If anime has multiple versions (TV vs BD), keeping one version's episode is sufficient

For these cases, the protection threshold can be conditional:
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

**Critical (High Impact):**
1. **Implement codec awareness** - Prevents incorrect deletion of high-quality H.265/AV1 files
2. **Fix series continuity** - Asymmetric distance penalties to delete from end backwards
3. **Add minimum episode retention** - Protect early episodes for future sampling

**Important (Medium Impact):**
4. **Switch to group rating** - Better quality assessment than anime rating
5. **Guarantee last file per episode** - Prevent complete episode deletion
6. **Add favorites/protection system** - Allow users to explicitly protect content

**Enhancements (Lower Impact):**
7. **Clarify double-penalty behavior** - Document or fix watched episode double-counting
8. **Add edge case tests** - Improve test coverage for language matching, quality, ratings
9. **Optimize for large collections** - Batch queries and caching for 10,000+ files
10. **Add rewatch tracking** - Distinguish between "watched once" and "rewatched frequently"

**Priority Order:**
1. Codec awareness (Phase 1)
2. Series continuity fix
3. Minimum episode retention
4. Group rating implementation
5. Last file protection

**Conclusion:** The file scoring system is well-designed, functionally correct, and accomplishes its intended purpose. It provides a sophisticated, multi-factor approach to file prioritization that balances user preferences, technical quality, and viewing behavior. However, the extended analysis reveals **five critical design considerations** that, if addressed, would significantly improve the system's correctness and user experience:

1. **Codec awareness** prevents deletion of correctly-encoded modern files
2. **Series continuity** maintains rewatchability by deleting from the end
3. **Minimum retention** preserves sampling capability for all anime
4. **Group rating** provides better quality assessment than anime ratings
5. **Duplicate protection** ensures at least one file remains per episode

While the current implementation is production-ready, implementing these enhancements would elevate it from "good" to "excellent" and better align with user expectations for an intelligent file management system.
