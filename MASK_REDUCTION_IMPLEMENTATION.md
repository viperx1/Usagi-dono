# Anime Mask Reduction Implementation

## Problem Statement
The application was requesting the same anime data from the AniDB API on every startup, even when the data was already present in the local database. This caused unnecessary API traffic and slower startup times.

## Root Cause
The `AniDBApi::Anime()` function had a mask reduction mechanism to avoid requesting data that was already in the database, but it was incomplete:
- Only checked 8 database fields out of 34 possible fields
- Used imprecise grouping (e.g., if `eps` field existed, removed ALL episode-related mask bits)
- No visibility into the mask reduction process for debugging

## Solution
Implemented comprehensive mask reduction with detailed debug logging:

### 1. Expanded Database Field Checks
**Before:** Only checked 8 fields
```cpp
SELECT year, type, relaidlist, relaidtype, eps, startdate, enddate, picname 
FROM `anime` WHERE aid = ?
```

**After:** Checks all 34 fields that correspond to mask bits
```cpp
SELECT year, type, relaidlist, relaidtype, eps, startdate, enddate, picname,
       url, rating, vote_count, temp_rating, temp_vote_count, avg_review_rating,
       review_count, award_list, is_18_restricted, ann_id, allcinema_id, animenfo_id,
       tag_name_list, tag_id_list, tag_weight_list, date_record_updated, character_id_list,
       episodes, highest_episode, special_ep_count, specials_count, credits_count,
       other_count, trailer_count, parody_count, dateflags
FROM `anime` WHERE aid = ?
```

### 2. Individual Field Checking
**Before:** Grouped episode counts together
```cpp
if(!checkQuery.value(4).isNull() && checkQuery.value(4).toInt() > 0)
{
    amask &= ~(ANIME_EPISODES | ANIME_HIGHEST_EPISODE | ANIME_SPECIAL_EP_COUNT |
               ANIME_SPECIALS_COUNT | ANIME_CREDITS_COUNT | ANIME_OTHER_COUNT |
               ANIME_TRAILER_COUNT | ANIME_PARODY_COUNT);
}
```

**After:** Checks each field individually
```cpp
// Check episodes (index 25)
if(!checkQuery.value(25).isNull() && checkQuery.value(25).toInt() >= 0)
{
    Logger::log(QString("[AniDB Mask] Removing EPISODES from mask (value: %1)")
                .arg(checkQuery.value(25).toInt()), __FILE__, __LINE__);
    amask &= ~ANIME_EPISODES;
}

// Check highest_episode (index 26)
if(!checkQuery.value(26).isNull() && !checkQuery.value(26).toString().isEmpty())
{
    Logger::log(QString("[AniDB Mask] Removing HIGHEST_EPISODE from mask (value: %1)")
                .arg(checkQuery.value(26).toString()), __FILE__, __LINE__);
    amask &= ~ANIME_HIGHEST_EPISODE;
}
// ... and so on for each field
```

### 3. Comprehensive Debug Logging
Added logging at key points in the mask reduction process:

1. **Initial mask:** Shows the full mask before any reduction
   ```
   [AniDB Mask] Initial mask for AID 69: 0x8C00EEFF7F80F8
   ```

2. **Field checks:** Shows which fields are found in database
   ```
   [AniDB Mask] Removing RATING from mask (value: 832)
   [AniDB Mask] Removing VOTE_COUNT from mask (value: 9589)
   [AniDB Mask] Removing ANN_ID from mask (value: 17827)
   ```

3. **Final mask:** Shows the mask after all reductions
   ```
   [AniDB Mask] Final mask after reduction for AID 69: 0x0C00000000
   ```

4. **Request mask:** Shows the actual mask being sent in the API request
   ```
   [AniDB Mask] Sending ANIME request for AID 69 with mask: 0x0C00000000
   ```

## Fields Checked and Their Corresponding Mask Bits

| Field Name | Mask Bit | Check Logic |
|------------|----------|-------------|
| year | ANIME_YEAR | NOT NULL and not empty string |
| type | ANIME_TYPE | NOT NULL and not empty string |
| relaidlist | ANIME_RELATED_AID_LIST | NOT NULL and not empty string |
| relaidtype | ANIME_RELATED_AID_TYPE | NOT NULL and not empty string |
| startdate | ANIME_AIR_DATE | NOT NULL and not empty string |
| enddate | ANIME_END_DATE | NOT NULL and not empty string |
| picname | ANIME_PICNAME | NOT NULL and not empty string |
| url | ANIME_URL | NOT NULL and not empty string |
| rating | ANIME_RATING | NOT NULL and not empty string |
| vote_count | ANIME_VOTE_COUNT | NOT NULL and > 0 |
| temp_rating | ANIME_TEMP_RATING | NOT NULL and not empty string |
| temp_vote_count | ANIME_TEMP_VOTE_COUNT | NOT NULL and > 0 |
| avg_review_rating | ANIME_AVG_REVIEW_RATING | NOT NULL and not empty string |
| review_count | ANIME_REVIEW_COUNT | NOT NULL and > 0 |
| award_list | ANIME_AWARD_LIST | NOT NULL and not empty string |
| is_18_restricted | ANIME_IS_18_RESTRICTED | NOT NULL |
| ann_id | ANIME_ANN_ID | NOT NULL and > 0 |
| allcinema_id | ANIME_ALLCINEMA_ID | NOT NULL and > 0 |
| animenfo_id | ANIME_ANIMENFO_ID | NOT NULL and not empty string |
| tag_name_list | ANIME_TAG_NAME_LIST | NOT NULL and not empty string |
| tag_id_list | ANIME_TAG_ID_LIST | NOT NULL and not empty string |
| tag_weight_list | ANIME_TAG_WEIGHT_LIST | NOT NULL and not empty string |
| date_record_updated | ANIME_DATE_RECORD_UPDATED | NOT NULL and > 0 |
| character_id_list | ANIME_CHARACTER_ID_LIST | NOT NULL and not empty string |
| episodes | ANIME_EPISODES | NOT NULL and >= 0 |
| highest_episode | ANIME_HIGHEST_EPISODE | NOT NULL and not empty string |
| special_ep_count | ANIME_SPECIAL_EP_COUNT | NOT NULL and >= 0 |
| specials_count | ANIME_SPECIALS_COUNT | NOT NULL and >= 0 |
| credits_count | ANIME_CREDITS_COUNT | NOT NULL and >= 0 |
| other_count | ANIME_OTHER_COUNT | NOT NULL and >= 0 |
| trailer_count | ANIME_TRAILER_COUNT | NOT NULL and >= 0 |
| parody_count | ANIME_PARODY_COUNT | NOT NULL and >= 0 |
| dateflags | ANIME_DATEFLAGS | NOT NULL and not empty string |

## Expected Behavior

### Scenario 1: New Anime (not in database)
- Initial mask: Full mask (all bits set)
- Database query: Returns no results
- Final mask: Full mask (no reduction)
- Result: Full anime data requested from API

### Scenario 2: Partial Anime Data
- Initial mask: Full mask (all bits set)
- Database query: Returns anime with only `year` and `type` fields
- Mask reduction: Remove ANIME_YEAR and ANIME_TYPE bits
- Final mask: All bits except YEAR and TYPE
- Result: Request remaining fields from API

### Scenario 3: Complete Anime Data
- Initial mask: Full mask (all bits set)
- Database query: Returns anime with all fields populated
- Mask reduction: Remove all mask bits
- Final mask: 0 (empty)
- Result: Skip API request entirely (return empty tag)

## Testing

The existing test suite in `tests/test_api_optimization.cpp` validates:
1. `testAnimeCommandReducesMaskForPartialData()` - Verifies mask reduction with partial data
2. `testAnimeCommandSkipsRequestWhenAnimeExists()` - Verifies request is made with reduced mask

With the new implementation, these tests should show:
- More precise mask reduction (only removes bits for fields that actually exist)
- Comprehensive logging output showing the reduction process
- Better API efficiency by avoiding redundant data requests

## Impact

### Performance
- Reduces unnecessary API traffic
- Faster startup times when anime data is already cached
- More efficient use of API rate limits

### Debugging
- Clear visibility into which fields are cached vs. requested
- Easy to diagnose mask reduction issues
- Helps identify incomplete data in database

### Maintenance
- Individual field checking makes it easier to add/remove fields
- More maintainable than grouped logic
- Better alignment with database schema

## Notes

1. **Name fields excluded:** The mask intentionally excludes anime name fields (ANIME_ROMAJI_NAME, ANIME_KANJI_NAME, etc.) as these come from a separate anime titles dump file.

2. **Zero values:** For integer count fields (episodes, specials_count, etc.), we check `>= 0` because 0 is a valid value meaning "no episodes of this type" vs. NULL meaning "unknown".

3. **Legacy `eps` field:** The old `eps` field is still in the database for backward compatibility but is not used for mask reduction since the individual episode count fields are more accurate.

## Future Improvements

1. Consider caching the mask reduction result to avoid repeated database queries
2. Add periodic refresh of anime data that hasn't been updated recently
3. Implement similar comprehensive mask reduction for FILE and EPISODE commands
