# MyList File Hierarchy Implementation

## Overview
This document describes the implementation of the three-level hierarchy feature for the MyList view: **Anime → Episode → Files**.

## Issue
The original mylist structure was two-level (anime → episode), but episodes can have multiple file entries (e.g., v1/v2, external subtitles, different releases). The new structure supports grouping files by type with visual indicators.

## Changes Made

### 1. New FileTreeWidgetItem Class (window.h)
Added a custom tree widget item for file-level entries with:
- **FileType enum**: Video, Subtitle, Audio, Other
- **Metadata storage**: resolution, quality, group name
- **Visual indicators**: Background colors based on file type
  - Video: Light blue (#E6F0FF)
  - Subtitle: Light yellow (#FFFAE6)
  - Audio: Light pink (#FFE6F0)
  - Other: Light gray (#F0F0F0)

### 1.1 File Type Detection (window.cpp)
Added `determineFileType()` helper method with:
- Extensible pattern matching using QStringList
- Support for common file extensions: mkv, mp4, avi, srt, ass, mp3, flac, etc.
- Case-insensitive matching
- Default to Video for empty filetype

### 2. Updated loadMylistFromDatabase() (window.cpp)
Modified to create a three-level hierarchy:
- **Level 1 (Anime)**: Aggregates episode statistics
- **Level 2 (Episodes)**: Shows episode number and title (empty state/viewed/storage columns)
- **Level 3 (Files)**: Shows individual file information with:
  - Column 1: File info (resolution, quality, group)
  - Column 2: File type
  - Column 3: State (HDD/CD/DVD/Deleted)
  - Column 4: Viewed status
  - Column 5: Storage location
  - Column 6: MyList ID (lid)

Key improvements:
- Queries file metadata from the `file` table (resolution, quality, filetype)
- Queries group name from the `group` table
- Groups files under their corresponding episodes
- Applies color coding based on file type
- Maintains episode-level statistics (counts each episode once, not per file)
- **Performance**: Uses two-pass approach for viewed count calculation (O(n) instead of O(n²))
  - First pass: Creates tree structure and counts episodes
  - Second pass: Calculates viewed counts per episode efficiently

### 3. Preferred File Selection (Stub)
Added `selectPreferredFile()` method for future playback feature:
- **FilePreference struct**: Stores user preferences for resolution, group, and quality
- **Selection logic**: Currently returns first video file, with placeholders for:
  - Resolution matching
  - Group preference
  - Quality preference (higher/lower)

## Database Schema
The implementation uses these tables:
- **mylist**: Each entry represents a file (lid is unique per file)
- **file**: Contains file metadata (fid, resolution, quality, filetype)
- **episode**: Contains episode information (eid, epno, name)
- **anime**: Contains anime information (aid, eps, eptotal)
- **group**: Contains release group names (gid, name)

## Future Enhancements
1. Implement full preference-based file selection logic
2. Add user settings UI for file preferences
3. Add playback integration
4. Support for file version indicators (v1, v2, etc.)
5. Add right-click context menu for file actions

## Testing Notes
The changes maintain backward compatibility:
- Old mylist entries still display correctly
- Episodes without file metadata show basic information
- Sorting and statistics calculations remain accurate

**Note**: The `updateOrAddMylistEntry()` function still uses the old two-level hierarchy for now. It will need to be updated to support dynamic addition of files to the new three-level structure. Currently, it will work correctly but files added dynamically may not appear in the new structure until the mylist is reloaded.

## UI Behavior
- Anime items are collapsed by default
- Episodes are collapsed by default (showing aggregated info)
- Expanding an episode reveals all files for that episode
- File type is visually indicated by background color
- File information is shown inline (resolution, quality, group)
