# File Scoring System Analysis

**Date:** 2025-12-04  
**Scope:** Deep analysis of the file scoring system in WatchSessionManager  
**Purpose:** Comprehensive documentation of system behavior, workflows, and correctness assessment

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
1. **Short-term:** Clarify double-penalty behavior, add edge case tests
2. **Medium-term:** Add codec awareness, implement favorites/protection system
3. **Long-term:** Optimize for large collections, add rewatch tracking

**Conclusion:** The file scoring system is well-designed, functionally correct, and accomplishes its intended purpose. It provides a sophisticated, multi-factor approach to file prioritization that balances user preferences, technical quality, and viewing behavior. While there are opportunities for refinement, the current implementation is production-ready and effective.
