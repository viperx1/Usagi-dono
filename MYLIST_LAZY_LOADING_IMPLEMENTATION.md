# MyList Lazy-Loading Implementation

## Overview
Implemented lazy-loading of missing episode data in the MyList view. Instead of showing warnings for missing episode numbers and names, the application now displays "Loading..." placeholders and automatically fetches the data when the user expands an anime in the tree.

## Problem Solved
Previously, when mylist entries were loaded from the database with missing episode data (epno or episode name), the application would:
- Display warnings in the log for each missing field
- Show the raw EID as the episode number
- Show empty episode names
- Provide no way to fetch the missing data

This created a poor user experience with cluttered logs and incomplete information.

## Solution
The new implementation:
1. Shows "Loading..." placeholders for missing episode data
2. Tracks which episodes need data using a `QSet<int>`
3. Queues EPISODE API requests when the user expands an anime
4. Automatically refreshes the display when data arrives
5. Respects the existing 2-second rate limit via the packet queue

## Changes Made

### API Layer (`usagi/src/anidbapi.h` and `usagi/src/anidbapi.cpp`)

#### Added EPISODE Command Support
```cpp
QString Episode(int eid);
QString buildEpisodeCommand(int eid);
```
- Implements the AniDB UDP API EPISODE command
- Format: `EPISODE eid={eid}`
- Queues the request through the existing packet system

#### Added Response Handler
```cpp
else if(ReplyID == "240"){ // 240 EPISODE
    // Parse response: eid|aid|length|rating|votes|epno|eng|romaji|kanji|aired|type
    // Store in episode table
    // Emit notifyEpisodeUpdated signal
}
```
- Handles 240 EPISODE responses from AniDB
- Parses episode data and stores it in the database
- Emits signal to notify UI of updates

#### Added Signal
```cpp
void notifyEpisodeUpdated(int eid, int aid);
```
- Emitted when episode data is successfully stored
- Allows UI to refresh automatically

### UI Layer (`usagi/src/window.h` and `usagi/src/window.cpp`)

#### Added Episode Tracking
```cpp
QSet<int> episodesNeedingData;  // Track EIDs that need EPISODE API call
```
- Maintains set of episodes with missing data
- Prevents duplicate API requests

#### Modified Display Logic
In `loadMylistFromDatabase()`:
```cpp
// Before:
episodeNumber = QString::number(eid);
logOutput->append(QString("Warning: No episode number found..."));

// After:
episodeNumber = "Loading...";
episodesNeedingData.insert(eid);
```
- Replaced warnings with user-friendly placeholders
- Tracks episodes for lazy loading

#### Added Expansion Handler
```cpp
void onMylistItemExpanded(QTreeWidgetItem *item);
```
- Connected to `itemExpanded` signal on mylistTreeWidget
- Iterates through child episodes when anime is expanded
- Queues EPISODE API requests for tracked episodes
- Removes episodes from tracking set after requesting

#### Added Update Handler
```cpp
void getNotifyEpisodeUpdated(int eid, int aid);
```
- Connected to `notifyEpisodeUpdated` signal from API
- Reloads mylist display when episode data arrives
- Logs informative message about the update

### Testing (`tests/test_anidbapi.cpp`)

#### Added EPISODE Command Test
```cpp
void testEpisodeCommandFormat();
```
- Validates EPISODE command format
- Ensures command starts with "EPISODE"
- Verifies eid parameter is correctly formatted
- Checks for proper spacing and structure

## Usage Flow

1. **Startup/Load**: User opens MyList or it loads automatically
   - Entries with missing episode data show "Loading..."
   - Episodes are tracked in `episodesNeedingData` set

2. **User Expands Anime**: User clicks to expand an anime in the tree
   - `onMylistItemExpanded()` is triggered
   - Function checks which child episodes need data
   - EPISODE commands are queued for missing data
   - Episodes are removed from tracking set

3. **API Response**: AniDB sends 240 EPISODE response
   - Response is parsed in `ParseMessage()`
   - Episode data is stored in database
   - `notifyEpisodeUpdated` signal is emitted

4. **UI Update**: Signal triggers display refresh
   - `getNotifyEpisodeUpdated()` reloads mylist
   - Episode now shows proper number and name
   - No longer shows "Loading..."

## API Rate Limiting

The implementation respects AniDB's rate limiting through the existing packet queue:
- All API commands go through `INSERT INTO packets`
- The `packetsender` QTimer sends packets every 2100ms
- This ensures the mandatory 2-second delay between requests

## Benefits

1. **Better UX**: Clean, informative placeholders instead of warnings
2. **Automatic**: Data fetches happen automatically on interaction
3. **Efficient**: Only requests data when user views it
4. **Rate-Limited**: Respects AniDB API guidelines
5. **No Duplicates**: Tracking set prevents redundant requests

## Future Enhancements

Possible improvements for future versions:
- Cache which episodes have been requested to survive app restart
- Add progress indicator showing how many episodes are being loaded
- Batch EPISODE requests when multiple are needed
- Add context menu option to manually request episode data
- Store timestamp of last request to avoid re-requesting too soon

## Testing

The implementation includes:
- Unit test for EPISODE command format (`testEpisodeCommandFormat`)
- Integration with existing test infrastructure
- Validation against AniDB UDP API Definition

Manual testing required:
- Verify "Loading..." appears for missing data
- Verify expansion triggers API requests
- Verify display updates when data arrives
- Verify no duplicate requests are made
- Verify rate limiting is respected

## Commit Information

- Commit: 0ab0826
- Files changed: 5 (anidbapi.h, anidbapi.cpp, window.h, window.cpp, test_anidbapi.cpp)
- Lines added: 142
- Lines removed: 4

## References

- [AniDB UDP API Definition](https://wiki.anidb.net/UDP_API_Definition)
- Issue: "mylist" - Display "Loading" for missing episode data
