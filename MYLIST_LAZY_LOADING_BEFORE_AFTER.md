# MyList Lazy-Loading - Before & After Comparison

## Before Implementation

### User Experience
```
MyList Tab (Log shows many warnings)
├── Anime: Example Anime #1
│   ├── Episode: 1, Title: Episode 1
│   ├── Episode: 2, Title: Episode 2
│   └── Episode: 34, Title: (empty)
├── Anime: Example Anime #2
│   ├── Episode: 37, Title: (empty)
│   └── Episode: 38, Title: (empty)
└── ...

Log Output:
20:21:26: AniDBApi: UDP socket created
Warning: No episode number found for EID 1 (AID 1)
Warning: No episode name found for EID 1 (AID 1)
Warning: No episode number found for EID 2 (AID 1)
Warning: No episode name found for EID 2 (AID 1)
Warning: No episode number found for EID 34 (AID 12)
Warning: No episode name found for EID 34 (AID 12)
...
```

### Problems
- ❌ Cluttered log with hundreds of warnings
- ❌ Empty episode titles confusing
- ❌ No way to fetch missing data
- ❌ Poor user experience

## After Implementation

### User Experience
```
MyList Tab (Clean, no warnings)
├── [+] Anime: Example Anime #1  (collapsed)
├── [+] Anime: Example Anime #2  (collapsed)
└── ...

User expands anime:
├── [-] Anime: Example Anime #1  (expanded)
│   ├── Episode: Loading..., Title: Loading...
│   ├── Episode: Loading..., Title: Loading...
│   └── Episode: Loading..., Title: Loading...

Log Output:
20:21:26: AniDBApi: UDP socket created
Loaded 42 mylist entries for 12 anime
User expands anime - requesting data...
Requesting episode data for EID 1 (AID 1)
Requesting episode data for EID 2 (AID 1)
...
Episode data received for EID 1 (AID 1), refreshing display...

After data arrives:
├── [-] Anime: Example Anime #1  (expanded)
│   ├── Episode: 1, Title: The Beginning
│   ├── Episode: 2, Title: First Steps  
│   └── Episode: Special 1, Title: Behind the Scenes
```

### Benefits
- ✅ Clean log, no warnings
- ✅ User-friendly "Loading..." placeholders
- ✅ Automatic data fetching on demand
- ✅ Great user experience

## Technical Flow

### Data Flow Diagram
```
┌─────────────────────────────────────────────────────────────┐
│ 1. MyList Loads from Database                               │
│    - Check if episode data exists                           │
│    - If missing: Show "Loading..." and track EID            │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. User Expands Anime in Tree                               │
│    - onMylistItemExpanded() triggered                       │
│    - Check tracked EIDs for this anime                      │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. Queue EPISODE API Requests                               │
│    - adbapi->Episode(eid) for each missing episode          │
│    - Commands inserted into packets table                   │
│    - Remove from tracking set (prevent duplicates)          │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. Packet Sender (2100ms timer)                             │
│    - Sends one command at a time                            │
│    - Respects AniDB 2-second rate limit                     │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. AniDB Responds (240 EPISODE)                             │
│    - ParseMessage() handles response                        │
│    - Data stored in episode table                           │
│    - Emit notifyEpisodeUpdated(eid, aid)                    │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│ 6. UI Refreshes Automatically                               │
│    - getNotifyEpisodeUpdated() slot triggered               │
│    - loadMylistFromDatabase() reloads tree                  │
│    - Episode now shows proper number and name               │
└─────────────────────────────────────────────────────────────┘
```

## Code Changes Summary

### AniDB API Layer (43 lines added)
```cpp
// New command
QString Episode(int eid);
QString buildEpisodeCommand(int eid);

// New response handler
else if(ReplyID == "240") {
    // Parse: eid|aid|length|rating|votes|epno|eng|romaji|kanji|...
    // Store in database
    emit notifyEpisodeUpdated(eid, aid);
}

// New signal
void notifyEpisodeUpdated(int eid, int aid);
```

### UI Layer (43 lines added, 4 removed)
```cpp
// Track missing episodes
QSet<int> episodesNeedingData;

// On load: show placeholders
episodeNumber = "Loading...";
episodesNeedingData.insert(eid);

// On expand: request data
void onMylistItemExpanded(QTreeWidgetItem *item) {
    for each episode with missing data:
        adbapi->Episode(eid);
        episodesNeedingData.remove(eid);
}

// On data received: refresh
void getNotifyEpisodeUpdated(int eid, int aid) {
    loadMylistFromDatabase();
}
```

### Test Layer (30 lines added)
```cpp
void testEpisodeCommandFormat() {
    api->Episode(12345);
    QString cmd = getLastPacketCommand();
    QVERIFY(cmd.startsWith("EPISODE"));
    QVERIFY(cmd.contains("eid=12345"));
}
```

## Statistics

- **Total changes**: 142 lines added, 4 removed
- **Files modified**: 5
- **New API commands**: 1 (EPISODE)
- **New response handlers**: 1 (240 EPISODE)
- **New signals**: 1 (notifyEpisodeUpdated)
- **New slots**: 2 (onMylistItemExpanded, getNotifyEpisodeUpdated)
- **Documentation files**: 2

## Backwards Compatibility

✅ **Fully compatible** - no breaking changes:
- Existing database schema unchanged
- Existing API commands still work
- Existing UI elements unchanged
- Existing settings and configuration preserved
- Only behavior change: replacing warnings with loading placeholders
