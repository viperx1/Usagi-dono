# Anime Titles Import - Quick Start Guide

## What is this feature?

The application automatically downloads anime titles from AniDB when it starts. This provides better anime name resolution and makes it easier to identify anime by various titles (English, Japanese, Romaji, etc.).

## How does it work?

### Automatic Download
- On first startup: Downloads anime titles from AniDB
- On subsequent startups: Checks if 24 hours have passed since last update
- If yes: Downloads fresh titles
- If no: Skips download (respects AniDB's rate limiting)

### Data Storage
Titles are stored in a SQLite database in the `anime_titles` table with:
- Anime ID (aid)
- Title type (main, official, synonym)
- Language (en, ja, x-jat, etc.)
- Title text

## User Experience

### First Startup
- Application starts
- Downloads anime titles (~5-10 MB)
- Decompresses and imports data (~5-10 seconds)
- Application ready to use

### Subsequent Startups
- Application starts
- Checks last update time (<1ms)
- If within 24 hours: No download, instant startup
- If after 24 hours: Downloads fresh data

### No Internet Connection
- Application starts normally
- Download fails (logged)
- Application continues to function
- Will retry on next startup when internet is available

## Checking Status

### In Application Logs
Look for these messages:
- `"Downloading anime titles from AniDB..."` - Download started
- `"Downloaded X bytes of compressed anime titles data"` - Download complete
- `"Decompressed to X bytes"` - Decompression successful
- `"Parsed and stored X anime titles"` - Import complete
- `"Anime titles updated successfully at <date>"` - Update finished

### In Database
Check the database directly:
```sql
-- Count total titles
SELECT COUNT(*) FROM anime_titles;

-- Check last update time
SELECT value FROM settings WHERE name = 'last_anime_titles_update';

-- Search for an anime
SELECT * FROM anime_titles WHERE title LIKE '%Gundam%';
```

## Troubleshooting

### Download Never Happens
**Symptom**: No download messages in logs

**Possible Causes**:
1. Already downloaded within last 24 hours
2. Database shows recent timestamp in `settings` table

**Solution**: 
- Check database: `SELECT * FROM settings WHERE name = 'last_anime_titles_update'`
- To force update: Delete the setting and restart app

### Download Fails
**Symptom**: Error message in logs

**Possible Causes**:
1. No internet connection
2. Firewall blocking access
3. AniDB server temporarily unavailable

**Solution**:
- Check internet connectivity
- Check firewall settings
- Wait and try again (app will retry on next startup)

### Decompression Fails
**Symptom**: "Failed to decompress anime titles data" in logs

**Possible Causes**:
1. Incomplete download
2. File format changed
3. Corrupted data

**Solution**:
- Delete last update timestamp to force fresh download
- Check AniDB API documentation for format changes
- Report issue if problem persists

### Empty Database
**Symptom**: `anime_titles` table is empty

**Possible Causes**:
1. Download or decompression failed (check logs)
2. Parsing error
3. Database permissions issue

**Solution**:
- Review application logs for errors
- Check database file permissions
- Run unit tests: `ctest -R test_anime_titles`

## Advanced Usage

### Force Update
To force a fresh download:
```sql
DELETE FROM settings WHERE name = 'last_anime_titles_update';
```
Then restart the application.

### Disable Auto-Update
To prevent automatic updates:
```sql
-- Set a far-future timestamp
INSERT OR REPLACE INTO settings 
VALUES (NULL, 'last_anime_titles_update', '9999999999');
```

### Query Titles
Useful SQL queries:

```sql
-- Get all English titles for anime ID 1
SELECT title FROM anime_titles 
WHERE aid = 1 AND language = 'en';

-- Get main title for anime ID 1
SELECT title FROM anime_titles 
WHERE aid = 1 AND type = 1;

-- Search across all titles
SELECT DISTINCT aid, title FROM anime_titles 
WHERE title LIKE '%keyword%';

-- Count titles by language
SELECT language, COUNT(*) FROM anime_titles 
GROUP BY language ORDER BY COUNT(*) DESC;
```

## Performance Impact

### First Startup
- Additional 5-10 seconds for download and import
- Acceptable for most users

### Subsequent Startups
- Negligible impact (<1ms timestamp check)
- No performance difference

### Disk Space
- Database grows by ~30-50 MB
- Compressed download is deleted after import

### Memory Usage
- Peak usage during import: ~50 MB
- Normal operation: minimal

## FAQ

**Q: Can I disable this feature?**
A: Yes, set a far-future timestamp in the settings (see Advanced Usage)

**Q: How often does it update?**
A: Maximum once per 24 hours (respects AniDB policy)

**Q: What if I don't have internet?**
A: App works normally, will try to update on next startup

**Q: Does this slow down the application?**
A: Only on first startup or after 24 hours (~5-10 seconds)

**Q: Is this safe?**
A: Yes, uses official AniDB API, proper error handling, and SQL injection prevention

**Q: Can I manually trigger an update?**
A: Not through UI, but you can delete the timestamp and restart (see Advanced Usage)

**Q: What data is downloaded?**
A: Only anime titles (IDs and names), no personal data

**Q: Is this required?**
A: Yes, improves anime identification and search functionality

## Support

If you experience issues:
1. Check application logs
2. Review this guide
3. Check database with SQL queries
4. Run unit tests if available
5. Report persistent issues with logs attached

## Technical Details

For developers and advanced users, see:
- `ANIME_TITLES_FEATURE.md` - Complete technical documentation
- `ANIME_TITLES_IMPLEMENTATION_SUMMARY.md` - Implementation details
- `tests/test_anime_titles.cpp` - Unit tests
