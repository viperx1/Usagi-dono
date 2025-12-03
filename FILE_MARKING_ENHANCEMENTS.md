# File Marking Enhancements

## Overview

This document describes the enhanced file marking system for determining which files should be marked for deletion or download. The system now includes multiple criteria to make intelligent decisions about file prioritization.

## New Features

### 1. Audio Language Preferences

**Location:** Settings Tab → File Marking Preferences → Preferred Audio Languages

**Description:** Users can specify a comma-separated list of preferred audio languages (e.g., "japanese,english"). Files with matching audio languages will receive a scoring bonus to prioritize keeping them over files with non-preferred languages.

**Implementation:**
- Setting stored in database as `preferredAudioLanguages`
- Default value: "japanese"
- Score bonus: +30 for matching, -40 for non-matching
- Language matching is case-insensitive and supports partial matches

### 2. Subtitle Language Preferences

**Location:** Settings Tab → File Marking Preferences → Preferred Subtitle Languages

**Description:** Users can specify a comma-separated list of preferred subtitle languages (e.g., "english,none"). Files with matching subtitle languages will receive a scoring bonus.

**Implementation:**
- Setting stored in database as `preferredSubtitleLanguages`
- Default value: "english"
- Score bonus: +20 for matching, -20 for non-matching
- Language matching is case-insensitive and supports partial matches

### 3. Version Preference

**Location:** Settings Tab → File Marking Preferences → Prefer highest version

**Description:** When enabled (default: on), the system already prioritizes keeping the highest version of each episode. This was previously implemented and is now exposed as a user setting for transparency.

**Implementation:**
- Setting stored in database as `preferHighestVersion`
- Default value: true (enabled)
- Already integrated with existing version comparison logic
- Files with older versions get significant penalties (score: -1000 per higher version file found)

### 4. Quality Preference

**Location:** Settings Tab → File Marking Preferences → Prefer highest quality

**Description:** When enabled (default: on), files with higher AniDB quality ratings are prioritized for keeping.

**Implementation:**
- Setting stored in database as `preferHighestQuality`
- Default value: true (enabled)
- Quality scoring based on AniDB `file.quality` field (higher = better):
  - "very high": 100
  - "high": 80
  - "medium": 60
  - "low": 40
  - "very low": 20
  - "corrupted" / "eyecancer": 10
  - Unknown: 50
- Score bonus: +25 for high quality (≥60), -35 for low quality (<40)

**Note:** Quality is now defined solely by the AniDB quality field, not by source (Blu-ray, DVD, etc.) or resolution. This provides a more accurate quality assessment as rated by AniDB.

### 5. Bitrate Preference

**Location:** Settings Tab → File Marking Preferences → Baseline Bitrate (Mbps)

**Description:** Users can specify a baseline bitrate in Mbps for 1080p content (default: 3.5 Mbps). This setting enables automatic bitrate calculation for all resolutions using a resolution-agnostic formula that scales with pixel count.

**Implementation:**
- Setting stored in database as `preferredBitrate`
- Default value: 3.5 Mbps (for 1080p)
- Universal bitrate formula: `bitrate = baseline × (resolution_megapixels / 2.07)`
  - Where 2.07 is the megapixel count of 1080p (1920×1080)
- Examples with 3.5 Mbps baseline:
  - 480p (0.41 MP) → 0.7 Mbps
  - 720p (0.92 MP) → 1.6 Mbps
  - 1080p (2.07 MP) → 3.5 Mbps
  - 1440p (3.69 MP) → 6.2 Mbps
  - 4K (8.29 MP) → 14.0 Mbps
- Penalty system (only applies when multiple files exist):
  - 0-10% difference from expected: no penalty
  - 10-30% difference: -10 penalty
  - 30-50% difference: -25 penalty
  - 50%+ difference: -40 penalty
- Supported resolution formats:
  - Named resolutions: "480p", "720p", "1080p", "1440p", "2K", "4K", "8K"
  - Numeric format: "WIDTHxHEIGHT" (e.g., "1920x1080")

**Rationale:** This approach is perfect for anime content which typically features flat colors, sharp edges, simple motion, and repeated frames. Scaling bitrate by pixel count keeps quality consistent across different resolutions without manual adjustment.

### 6. Resolution Preference

**Location:** Settings Tab → File Marking Preferences → Preferred Resolution

**Description:** Users can specify their preferred target resolution (default: "1080p"). This preference is used in conjunction with the bitrate calculator to determine the expected bitrate for file quality comparison.

**Implementation:**
- Setting stored in database as `preferredResolution`
- Default value: "1080p"
- Available presets: 480p, 720p, 1080p, 1440p, 4K, 8K
- Editable combo box allows custom resolutions
- Used to calculate expected bitrate via the universal formula
- Files are scored based on how close their bitrate is to the expected value for their resolution

### 7. Group Status Tracking

**Description:** Files from active release groups receive a bonus, while files from stalled or disbanded groups receive penalties.

**Implementation:**
- Automatically fetched via GROUPSTATUS command when available
- Group status stored in database with gid
- Status values: 0=unknown, 1=ongoing, 2=stalled, 3=disbanded
- Score adjustments:
  - Active/ongoing groups: +20 bonus
  - Stalled groups: -10 penalty
  - Disbanded groups: -25 penalty
- Method `requestGroupStatus(int gid)` available to fetch status for specific groups

### 8. Rating-Based Scoring

**Description:** Files from highly-rated anime (rating ≥ 8.0/10) receive a bonus to prioritize keeping them, while files from poorly-rated anime (rating < 6.0/10) receive a penalty.

**Implementation:**
- Automatically applied based on anime rating from AniDB
- Score bonus: +15 for highly rated (≥800), -15 for poorly rated (<600)
- Rating scale: 0-1000 (e.g., 8.23 = 823)

## Score Calculation System

The file marking system uses a comprehensive scoring algorithm to determine file priority:

### Base Scoring Components

1. **Base score**: 50 (neutral starting point)
2. **Hidden card penalty**: -50 (hidden anime are more eligible for deletion)
3. **Watch status**:
   - Already watched: -30 (more eligible for deletion)
   - Not watched: +50 (keep for future viewing)
4. **Version comparison**: -1000 per higher version file found (strongly prefer newest versions)

### New Scoring Components

5. **Language preferences**:
   - Preferred audio: +30
   - Non-preferred audio: -40
   - Preferred subtitles: +20
   - Non-preferred subtitles: -20
6. **Quality scoring** (when enabled, based on AniDB quality field):
   - High quality (≥60): +25
   - Low quality (<40): -35
7. **Group status**:
   - Active/ongoing groups: +20
   - Stalled groups: -10
   - Disbanded groups: -25
8. **Rating-based**:
   - Highly rated anime (≥8.0): +15
   - Poorly rated anime (<6.0): -15

### Session-Based Scoring

8. **Active session**: +100 (actively watching this series)
9. **Ahead buffer**: +75 (within the next N episodes to watch)
10. **Distance factor**: -1 per episode away from current position

### Final Priority

Files with **higher scores** are prioritized for **keeping**, while files with **lower scores** are prioritized for **deletion**.

## Database Changes

### New Settings

The following settings are stored in the `settings` table:

- `preferredAudioLanguages` (TEXT): Comma-separated list of preferred audio languages
- `preferredSubtitleLanguages` (TEXT): Comma-separated list of preferred subtitle languages
- `preferHighestVersion` (INTEGER): 1 for enabled, 0 for disabled
- `preferHighestQuality` (INTEGER): 1 for enabled, 0 for disabled

### Existing Database Columns Used

The system uses the following existing database columns:

From `file` table:
- `lang_dub`: Audio language information
- `lang_sub`: Subtitle language information
- `quality`: Quality source (Blu-ray, DVD, HDTV, etc.)
- `resolution`: Video resolution (1920x1080, 1280x720, etc.)
- `state`: File state including version flags (bits 2-5)

From `anime` table:
- `rating`: Anime rating (0-10 scale stored as text)

From `mylist` table:
- `viewed`: Server-side watch status
- `local_watched`: Local watch status

## UI Changes

### Settings Tab - New Section

A new "File Marking Preferences" section has been added with the following controls:

1. **Preferred Audio Languages** (Text field)
   - Placeholder: "japanese,english"
   - Tooltip: Explains comma-separated format and prioritization behavior

2. **Preferred Subtitle Languages** (Text field)
   - Placeholder: "english,none"
   - Tooltip: Explains comma-separated format and prioritization behavior

3. **Prefer highest version** (Checkbox)
   - Default: Checked
   - Tooltip: Explains version preference behavior

4. **Prefer highest quality** (Checkbox)
   - Default: Checked
   - Tooltip: Explains quality/resolution preference

All settings are saved when the "Save Settings" button is clicked.

## Implementation Details

### Code Files Modified

1. **anidbapi.h** - Added new setting fields
2. **anidbapi.cpp** - Added setting initialization and loading
3. **anidbapi_settings.cpp** - Implemented getters and setters
4. **watchsessionmanager.h** - Added new helper methods and score constants
5. **watchsessionmanager.cpp** - Implemented enhanced scoring logic and helper methods
6. **window.cpp** - Added UI controls and save logic

### Helper Methods Added

In `WatchSessionManager`:

- `matchesPreferredAudioLanguage(int lid)` - Check if file matches audio preferences
- `matchesPreferredSubtitleLanguage(int lid)` - Check if file matches subtitle preferences
- `getQualityScore(const QString& quality)` - Convert AniDB quality string to numeric score
- `getFileQuality(int lid)` - Retrieve file quality from database
- `getFileAudioLanguage(int lid)` - Retrieve audio language from database
- `getFileSubtitleLanguage(int lid)` - Retrieve subtitle language from database
- `getFileRating(int lid)` - Retrieve anime rating for file
- `getFileGroupId(int lid)` - Retrieve group ID for file
- `getGroupStatus(int gid)` - Retrieve group status from database

In `AniDBApi`:

- `requestGroupStatus(int gid)` - Send GROUPSTATUS command to AniDB API

## Completed Features

All initially planned features have been implemented:

1. ✅ **Audio language preferences** - User configurable with scoring
2. ✅ **Subtitle language preferences** - User configurable with scoring
3. ✅ **Version preference** - Highest version prioritized (exposed as setting)
4. ✅ **Quality preference** - Uses AniDB quality field for accurate scoring
5. ✅ **Group status tracking** - GROUPSTATUS command integration with scoring
6. ✅ **Rating-based scoring** - Anime rating influence on file priority

## Future Enhancements

The following additional features could be implemented:

### 1. Additional Database/API Options

**Possible Features:**
- **Source preference**: Prioritize specific sources (e.g., prefer Blu-ray > DVD > TV) in addition to quality
- **Resolution preference**: Add resolution-based scoring alongside quality
- **Codec preference**: Prioritize modern codecs (H.265 > H.264)
- **Audio codec preference**: Prioritize lossless audio (FLAC > AAC > MP3)
- **Filesize efficiency**: Consider quality-to-size ratio
- **Release date**: Prefer newer releases of the same episode
- **Group reputation**: Use historical quality data from preferred groups
- **Episode type filtering**: Separate handling for specials, OVAs, movies
- **Batch release detection**: Prioritize batch releases over individual episodes

## Backward Compatibility

All new settings have sensible defaults and are optional. Existing installations will continue to work with the default values:
- Audio preference: "japanese"
- Subtitle preference: "english"
- Version preference: enabled
- Quality preference: enabled

No database migrations are required as all settings use the existing `settings` table structure.

## Testing

To test the enhanced file marking system:

1. Navigate to Settings tab
2. Configure preferred languages and preferences
3. Click "Save Settings"
4. Navigate to MyList tab
5. Observe file marking colors (blue for download, light red for deletion)
6. Verify that files matching preferences have higher priority (less likely to be marked for deletion)

## Performance Considerations

The enhanced scoring system makes additional database queries for each file being scored. However:
- Queries are simple lookups using indexed columns
- Results could be cached if performance becomes an issue
- Scoring is only recalculated when marking changes or sessions are updated
- Most operations use JOIN queries to fetch data efficiently

## Notes

- Language matching is flexible and supports partial matches (e.g., "japanese" matches "japanese (main)")
- The "none" keyword can be used for subtitle preferences to indicate no subtitles preferred
- All scoring bonuses and penalties are tuned to work together - extreme values ensure critical factors (like version) override less important ones (like rating)
- The system maintains backward compatibility with existing file marking logic
