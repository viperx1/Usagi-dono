# Debug Output Example - App Freeze Diagnosis

This file shows example output from the debug logging added to diagnose the app freeze issue.

## Scenario 1: Normal Startup (No Freeze)

```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 6 [AniDB Init] Constructor started
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 11 [AniDB Init] Starting DNS lookup for api.anidb.net (this may block)
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 13 [AniDB Init] DNS lookup completed
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 17 [AniDB Init] DNS resolved successfully to 89.163.218.20
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 29 [AniDB Init] Setting up database connection
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 44 [AniDB Init] Opening database
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 47 [AniDB Init] Database opened, starting transaction
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 61 [AniDB Init] Committing database transaction
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 63 [AniDB Init] Database transaction committed
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 66 [AniDB Init] Reading settings from database
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 88 [AniDB Init] Initializing network manager
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 92 [AniDB Init] Setting up packet sender timer
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 100 [AniDB Init] Checking if anime titles need update
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 916 [AniDB Anime Titles] Checking if anime titles need update
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 933 [AniDB Anime Titles] Last update was 3600 seconds ago, needs update: no
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 108 [AniDB Init] Anime titles are up to date, skipping download
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 111 [AniDB Init] Constructor completed successfully
```

**Result**: App starts normally, no freeze detected.

---

## Scenario 2: Freeze During DNS Lookup

```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 6 [AniDB Init] Constructor started
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 11 [AniDB Init] Starting DNS lookup for api.anidb.net (this may block)
```
*[App appears frozen here, no further logs]*

**Diagnosis**: DNS lookup is hanging. 
- **Root Cause**: `QHostInfo::fromName()` is blocking
- **Solution**: Use `QHostInfo::lookupHost()` with async callback instead
- **Workaround**: Check network/DNS connectivity

---

## Scenario 3: First Run - Downloading Anime Titles

```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 6 [AniDB Init] Constructor started
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 11 [AniDB Init] Starting DNS lookup for api.anidb.net (this may block)
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 13 [AniDB Init] DNS lookup completed
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 17 [AniDB Init] DNS resolved successfully to 89.163.218.20
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 29 [AniDB Init] Setting up database connection
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 44 [AniDB Init] Opening database
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 47 [AniDB Init] Database opened, starting transaction
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 61 [AniDB Init] Committing database transaction
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 63 [AniDB Init] Database transaction committed
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 66 [AniDB Init] Reading settings from database
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 88 [AniDB Init] Initializing network manager
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 92 [AniDB Init] Setting up packet sender timer
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 100 [AniDB Init] Checking if anime titles need update
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 916 [AniDB Anime Titles] Checking if anime titles need update
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 926 [AniDB Anime Titles] No previous download found, download needed
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 103 [AniDB Init] Starting anime titles download
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 111 [AniDB Init] Constructor completed successfully
Downloading anime titles from AniDB...
```
*[Short delay while downloading...]*
```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 958 [AniDB Anime Titles] Download callback triggered
Downloaded 1847293 bytes of compressed anime titles data
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 976 [AniDB Anime Titles] Detected gzip format, using zlib decompression
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1006 [AniDB Anime Titles] Starting inflate operation (this may take a moment)
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1026 [AniDB Anime Titles] Decompression completed successfully
Decompressed to 12847392 bytes
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1043 [AniDB Anime Titles] Starting to parse and store titles
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1065 [AniDB Anime Titles] Starting to parse anime titles data (12847392 bytes)
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1069 [AniDB Anime Titles] Starting database transaction for 187432 lines
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1074 [AniDB Anime Titles] Clearing old anime titles from database
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1108 [AniDB Anime Titles] Processing progress: 1000 titles inserted
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1108 [AniDB Anime Titles] Processing progress: 2000 titles inserted
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1108 [AniDB Anime Titles] Processing progress: 3000 titles inserted
...
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1108 [AniDB Anime Titles] Processing progress: 50000 titles inserted
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1113 [AniDB Anime Titles] Committing database transaction with 52847 titles
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1114 [AniDB Anime Titles] Parsed and stored 52847 anime titles successfully
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1045 [AniDB Anime Titles] Finished parsing and storing titles
Anime titles updated successfully at 2025-10-15 15:04:32
```

**Result**: App shows progress, no freeze. User can see it's working.

---

## Scenario 4: Freeze During Anime Titles Parsing

```
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 6 [AniDB Init] Constructor started
...
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 111 [AniDB Init] Constructor completed successfully
Downloading anime titles from AniDB...
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 958 [AniDB Anime Titles] Download callback triggered
Downloaded 1847293 bytes of compressed anime titles data
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 976 [AniDB Anime Titles] Detected gzip format, using zlib decompression
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1006 [AniDB Anime Titles] Starting inflate operation (this may take a moment)
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1026 [AniDB Anime Titles] Decompression completed successfully
Decompressed to 12847392 bytes
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1043 [AniDB Anime Titles] Starting to parse and store titles
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1065 [AniDB Anime Titles] Starting to parse anime titles data (12847392 bytes)
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1069 [AniDB Anime Titles] Starting database transaction for 187432 lines
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1074 [AniDB Anime Titles] Clearing old anime titles from database
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1108 [AniDB Anime Titles] Processing progress: 1000 titles inserted
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1108 [AniDB Anime Titles] Processing progress: 2000 titles inserted
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1108 [AniDB Anime Titles] Processing progress: 3000 titles inserted
```
*[App appears frozen here, but progress is still being logged every 1000 titles]*

**Diagnosis**: Large database transaction is blocking the UI thread.
- **Root Cause**: Processing 50,000+ titles in one transaction blocks UI
- **Solution**: 
  1. Move parsing to background thread/worker
  2. Process in smaller batches with `QApplication::processEvents()` between batches
  3. Use `QFuture`/`QtConcurrent` for async processing
- **Note**: Progress logs prove app is NOT frozen, just UI is not responsive during DB work

---

## Scenario 5: Freeze During Database Transaction Commit

```
...
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1108 [AniDB Anime Titles] Processing progress: 52000 titles inserted
D:/a/Usagi-dono/Usagi-dono/usagi/src/anidbapi.cpp 1113 [AniDB Anime Titles] Committing database transaction with 52847 titles
```
*[App appears frozen here, no further logs for several seconds]*

**Diagnosis**: Database commit is taking too long.
- **Root Cause**: SQLite transaction commit is slow with 50,000+ inserts
- **Solution**: 
  1. Use `PRAGMA journal_mode=WAL` for better concurrent write performance
  2. Commit in smaller batches (e.g., every 5000 titles)
  3. Use prepared statements for faster inserts

---

## Key Insights from Debug Logs

1. **Exact Freeze Location**: Logs will show the last successful operation before freeze
2. **Progress Tracking**: Every 1000 titles shows app is working, not frozen
3. **Timing Information**: File/line numbers help locate problematic code
4. **Category Tags**: `[AniDB Init]` vs `[AniDB Anime Titles]` helps identify phase
5. **Data Validation**: Shows actual data sizes and counts being processed

## Next Steps After Diagnosis

Based on which scenario occurs:
- **DNS Freeze**: Convert to async DNS lookup
- **Database Freeze**: Move to background thread or process in smaller batches
- **Download Freeze**: Add timeout and retry logic
- **Parsing Freeze**: Optimize SQL queries or use prepared statements
