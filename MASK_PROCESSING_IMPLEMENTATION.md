# Mask Data Processing Implementation

## Overview
This document describes the implementation of bit-by-bit mask processing for AniDB API responses as requested in the issue "mask data processing".

## Requirements (from issue)
1. Process mask in a loop bit by bit
2. Match every bit to the mask definition (switch case?)
3. Write data to complete data structure ("struct anime", etc)
4. Further processing

## Implementation

### 1. Data Structures

Created complete data structures for all entity types:

```cpp
struct FileData {
    QString fid, aid, eid, gid, lid;
    QString othereps, isdepr, state, size, ed2k, md5, sha1, crc;
    QString quality, source, codec_audio, bitrate_audio;
    QString codec_video, bitrate_video, resolution, filetype;
    QString lang_dub, lang_sub, length, description, airdate, filename;
};

struct AnimeData {
    QString aid, eptotal, eplast, year, type;
    QString relaidlist, relaidtype, category;
    QString nameromaji, namekanji, nameenglish, nameother, nameshort, synonyms;
};

struct EpisodeData {
    QString eid, epno, epname, epnameromaji, epnamekanji, eprating, epvotecount;
};

struct GroupData {
    QString gid, groupname, groupshortname;
};
```

### 2. Mask Processing Functions

Implemented dedicated parsing functions for each mask type:

#### parseFileMask()
Processes `file_fmask_codes` bit by bit from MSB (bit 30) to LSB (bit 0):
```cpp
if (fmask & fAID)          { data.aid = tokens.value(index++); }
if (fmask & fEID)          { data.eid = tokens.value(index++); }
if (fmask & fGID)          { data.gid = tokens.value(index++); }
// ... continues for all 24 bits
```

#### parseFileAmaskAnimeData()
Processes anime-related bits from `file_amask_codes`:
```cpp
if (amask & aEPISODE_TOTAL)      { data.eptotal = tokens.value(index++); }
if (amask & aEPISODE_LAST)       { data.eplast = tokens.value(index++); }
if (amask & aANIME_YEAR)         { data.year = tokens.value(index++); }
// ... continues for anime fields (bits 31-18)
```

#### parseFileAmaskEpisodeData()
Processes episode-related bits from `file_amask_codes`:
```cpp
if (amask & aEPISODE_NUMBER)      { data.epno = tokens.value(index++); }
if (amask & aEPISODE_NAME)        { data.epname = tokens.value(index++); }
// ... continues for episode fields (bits 15-10)
```

#### parseFileAmaskGroupData()
Processes group-related bits from `file_amask_codes`:
```cpp
if (amask & aGROUP_NAME)       { data.groupname = tokens.value(index++); }
if (amask & aGROUP_NAME_SHORT) { data.groupshortname = tokens.value(index++); }
// ... continues for group fields (bits 7-1)
```

#### parseAnimeMask()
Processes `anime_amask_codes` bit by bit for ANIME command responses:
```cpp
if (amask & ANIME_TOTAL_EPISODES)  { data.eptotal = tokens.value(index++); }
if (amask & ANIME_HIGHEST_EPISODE) { data.eplast = tokens.value(index++); }
if (amask & ANIME_YEAR)            { data.year = tokens.value(index++); }
// ... continues for all anime fields (bits 31-2)
```

### 3. Helper Functions

#### extractMasksFromCommand()
Extracts hex mask values from command strings using regex:
```cpp
// For FILE commands: "FILE size=X&ed2k=Y&fmask=ZZZZZZZZ&amask=WWWWWWWW"
// For ANIME commands: "ANIME aid=X&amask=YYYYYYYY"
QRegularExpression fmaskRegex("fmask=([0-9a-fA-F]+)");
QRegularExpression amaskRegex("amask=([0-9a-fA-F]+)");
```

### 4. Storage Functions

Implemented dedicated storage functions for each data type:
- `storeFileData()` → file table
- `storeAnimeData()` → anime table
- `storeEpisodeData()` → episode table
- `storeGroupData()` → group table

### 5. Response Handler Updates

#### FILE Response (220)
```cpp
// Extract masks from original command
extractMasksFromCommand(fileCmd, fmask, amask);

// Parse data using mask-aware functions
int index = 1;  // Start after FID
FileData fileData = parseFileMask(tokens, fmask, index);
AnimeData animeData = parseFileAmaskAnimeData(tokens, amask, index);
EpisodeData episodeData = parseFileAmaskEpisodeData(tokens, amask, index);
GroupData groupData = parseFileAmaskGroupData(tokens, amask, index);

// Store parsed data
storeFileData(fileData);
storeAnimeData(animeData);
storeEpisodeData(episodeData);
storeGroupData(groupData);
```

#### ANIME Response (230)
```cpp
// Extract amask from original command
extractMasksFromCommand(animeCmd, fmask, amask);

// Parse data using mask-aware function
int index = 1;  // Start after AID
AnimeData animeData = parseAnimeMask(tokens, amask, index);

// Update anime type in database
// (ANIME API only reliably returns type field)
```

## Benefits

### 1. Flexibility
Can now easily request different field combinations by modifying mask values without changing parsing code.

### 2. Correctness
Handles any combination of mask bits properly. Fields are extracted in the exact order specified by the mask bits.

### 3. Maintainability
Clear, explicit mapping between mask bits and fields using if-statements (similar to switch-case):
```cpp
if (fmask & fAID)  { data.aid = tokens.value(index++); }   // Bit 30
if (fmask & fEID)  { data.eid = tokens.value(index++); }   // Bit 29
if (fmask & fGID)  { data.gid = tokens.value(index++); }   // Bit 28
```

### 4. Robustness
- No hardcoded array indices
- Adapts to actual API response based on masks
- Graceful fallback to default masks if command lookup fails

### 5. Documentation
Each parse function documents which mask bits and enum values it processes.

## Backward Compatibility

The implementation maintains full backward compatibility:
- Database schema unchanged
- API command format unchanged
- Data storage logic unchanged
- Falls back to default masks (all fields) if mask extraction fails
- Same business logic for episode completion checks, etc.

## Testing

Requires Qt6 build environment for compilation and testing:
1. Build with Qt6
2. Run existing test suite
3. Verify FILE and ANIME command responses
4. Test with live AniDB API

## Code Statistics

- **New data structures**: 4 (FileData, AnimeData, EpisodeData, GroupData)
- **New parse functions**: 5 (~250 lines)
- **New store functions**: 4 (~150 lines)
- **New helper functions**: 1 (extractMasksFromCommand, ~45 lines)
- **Refactored handlers**: 2 (FILE 220, ANIME 230)
- **Total new/modified code**: ~550 lines

## Files Modified

1. `usagi/src/anidbapi.h`:
   - Added data structure definitions
   - Added function declarations

2. `usagi/src/anidbapi.cpp`:
   - Added QRegularExpression include
   - Implemented all parse, store, and helper functions
   - Refactored FILE (220) and ANIME (230) response handlers

## Mask Bit Processing Order

All mask processing follows MSB to LSB order, matching the AniDB API specification:

- **fmask**: Bit 30 → Bit 0 (file fields)
- **file_amask**: Bit 31 → Bit 0 (anime/episode/group fields in FILE response)
- **anime_amask**: Bit 31 → Bit 2 (anime fields in ANIME response)

This ensures fields appear in the response in the same order as the bit positions in the mask.
