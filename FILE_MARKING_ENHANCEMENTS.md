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

**Description:** When enabled (default: on), files with higher quality sources and resolutions are prioritized for keeping.

**Implementation:**
- Setting stored in database as `preferHighestQuality`
- Default value: true (enabled)
- Quality scoring (higher = better):
  - Blu-ray/BluRay: 100
  - DVD: 70
  - HDTV: 60
  - WEB: 50
  - TV: 40
  - VHS: 20
  - Unknown: 30
- Resolution scoring (higher = better):
  - 4K (3840x2160): 100
  - 1440p (2560x1440): 85
  - 1080p (1920x1080): 80
  - 720p (1280x720): 60
  - 480p: 40
  - 360p: 30
  - 240p: 20
  - Unknown: 50
- Score bonus: +25 for high quality/resolution, -35 for low quality/resolution

### 5. Rating-Based Scoring

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
6. **Quality scoring** (when enabled):
   - High quality/resolution: +25
   - Low quality/resolution: -35
7. **Rating-based**:
   - Highly rated anime: +15
   - Poorly rated anime: -15

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
- `getQualityScore(const QString& quality)` - Convert quality string to numeric score
- `getResolutionScore(const QString& resolution)` - Convert resolution to numeric score
- `getFileQuality(int lid)` - Retrieve file quality from database
- `getFileResolution(int lid)` - Retrieve file resolution from database
- `getFileAudioLanguage(int lid)` - Retrieve audio language from database
- `getFileSubtitleLanguage(int lid)` - Retrieve subtitle language from database
- `getFileRating(int lid)` - Retrieve anime rating for file

## Future Enhancements

The following features from the original issue remain to be implemented:

### 1. Group Status (GROUPSTATUS Command)

**Description:** Track the release status of fansub groups and prioritize files from active groups over inactive ones.

**Proposed Implementation:**
- Add GROUPSTATUS command to AniDB API integration
- Store group status in database (active, inactive, disbanded)
- Add scoring bonus for files from active groups
- Add scoring penalty for files from inactive/disbanded groups

### 2. Additional Database/API Options

**Possible Features:**
- **Source preference**: Prioritize specific sources (e.g., prefer Blu-ray > DVD > TV)
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
