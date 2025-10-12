# Anime Titles Import Feature

## Overview

This feature automatically downloads and imports anime titles from AniDB's public API into the local database. The titles are used to provide better anime name resolution and search capabilities.

## Implementation

### Database Schema

A new table `anime_titles` has been added to store the anime titles:

```sql
CREATE TABLE IF NOT EXISTS `anime_titles`(
    `aid` INTEGER,
    `type` INTEGER,
    `language` TEXT,
    `title` TEXT,
    PRIMARY KEY(`aid`, `type`, `language`, `title`)
);
```

### Title Types

The `type` field indicates the title type:
- `1` = Main title
- `2` = Official title
- `3` = Official title (alternate)
- `4` = Synonym/Short title

### Languages

Common language codes include:
- `en` = English
- `ja` = Japanese
- `x-jat` = Romaji (Romanized Japanese)
- Plus many other language codes

## Features

### Automatic Download on Startup

The application automatically checks if anime titles need to be updated when it starts. Titles are downloaded if:
- They have never been downloaded before, OR
- The last update was more than 24 hours ago

This ensures compliance with AniDB's API usage policy of not requesting the file more than once per day.

### Data Source

Anime titles are downloaded from: `http://anidb.net/api/anime-titles.dat.gz`

The file is in gzip-compressed format and contains tab-delimited data:
```
aid|type|language|title
```

Example:
```
1|1|x-jat|Seikai no Monshou
1|2|en|Crest of the Stars
1|3|ja|星界の紋章
2|1|x-jat|Kidou Senshi Gundam
2|2|en|Mobile Suit Gundam
```

### Decompression

The downloaded `.gz` file is automatically decompressed using Qt's decompression utilities. The implementation handles gzip format by:
1. Detecting gzip magic bytes (0x1f 0x8b)
2. Skipping the gzip header (10 bytes minimum)
3. Prepending a zlib header to the deflate data
4. Using `qUncompress` to decompress

### Data Storage

All anime titles are stored in the `anime_titles` table. On each update:
1. Previous titles are deleted
2. New titles are parsed and inserted
3. SQL injection is prevented by escaping single quotes

### Update Tracking

The last update timestamp is stored in the `settings` table:
```sql
INSERT OR REPLACE INTO `settings` 
VALUES (NULL, 'last_anime_titles_update', '<timestamp>');
```

## Usage

### Automatic Usage

No user action is required. The feature runs automatically on application startup.

### Manual Triggering

If needed, the download can be triggered programmatically:

```cpp
if(adbapi->shouldUpdateAnimeTitles()) {
    adbapi->downloadAnimeTitles();
}
```

### Querying Titles

Anime titles can be queried from the database:

```sql
-- Get all titles for a specific anime
SELECT * FROM anime_titles WHERE aid = 1;

-- Get English title for an anime
SELECT title FROM anime_titles WHERE aid = 1 AND language = 'en' AND type = 2;

-- Search for anime by title
SELECT DISTINCT aid FROM anime_titles WHERE title LIKE '%Gundam%';
```

## API Compliance

This implementation follows AniDB's API guidelines:

- ✅ **Rate Limiting**: File is only requested once per 24 hours maximum
- ✅ **User Agent**: Proper User-Agent header is set (`Usagi/<version>`)
- ✅ **Error Handling**: Network errors are gracefully handled and logged
- ✅ **Retry Logic**: Failed downloads are retried on next startup

## Error Handling

The implementation includes comprehensive error handling:

1. **Network Errors**: If download fails, an error is logged and the update is retried on next startup
2. **Decompression Errors**: If decompression fails, the error is logged and the update is retried
3. **Parsing Errors**: Invalid lines are skipped, allowing partial data import
4. **Database Errors**: SQL errors are caught and logged

## Testing

Unit tests are provided in `tests/test_anime_titles.cpp`:

- Database table creation
- Update timestamp storage and retrieval
- 24-hour update interval logic
- Anime titles parsing
- Special character handling
- Comment line skipping

To run tests:
```bash
cd build
ctest -R test_anime_titles -V
```

## Logging

The feature provides detailed logging through the `Debug()` function:

- "Downloading anime titles from AniDB..." - Download initiated
- "Downloaded X bytes of compressed anime titles data" - Download complete
- "Decompressed to X bytes" - Decompression successful
- "Parsed and stored X anime titles" - Parsing complete
- "Anime titles updated successfully at <timestamp>" - Update complete
- "Failed to download anime titles: <error>" - Download error
- "Failed to decompress anime titles data. Will retry on next startup." - Decompression error

## Future Enhancements

Potential improvements for future versions:

1. **Incremental Updates**: Instead of deleting all titles, use UPDATE/INSERT logic
2. **Better Decompression**: Use a dedicated gzip library for more robust decompression
3. **Caching**: Cache decompressed data to disk for faster restarts
4. **Progress Reporting**: Add progress signals for UI updates
5. **Manual Refresh Button**: Allow users to manually trigger updates (with rate limiting)
6. **Title Search Integration**: Integrate title search into the file matching UI

## Technical Notes

### Dependencies

- Qt6::Network for HTTP downloads
- Qt6::Core for data decompression and string handling
- Qt6::Sql for database operations

### Thread Safety

The download and parsing operations run on the main Qt event loop thread. The QNetworkAccessManager handles asynchronous downloads through Qt's signal/slot mechanism.

### Performance

- Download size: ~5-10 MB compressed, ~20-30 MB uncompressed
- Typical download time: 1-5 seconds (depending on connection)
- Parsing time: <1 second for ~100,000+ titles
- Database insertion: 1-2 seconds with transaction batching

### Memory Usage

The entire file is loaded into memory during processing. For the typical anime-titles.dat.gz file:
- Compressed: ~5-10 MB
- Decompressed: ~20-30 MB
- Peak memory usage during import: ~50 MB

This is acceptable for modern systems but should be considered for resource-constrained environments.

## Troubleshooting

### Download Fails

If the download consistently fails:
1. Check internet connectivity
2. Verify that `http://anidb.net` is accessible
3. Check firewall/proxy settings
4. Review application logs for specific error messages

### Decompression Fails

If decompression fails:
1. The file format may have changed (check AniDB API documentation)
2. Download may be incomplete (check file size in logs)
3. Try deleting the `last_anime_titles_update` setting to force a fresh download

### No Titles in Database

If titles aren't being imported:
1. Check application logs for errors
2. Verify the `anime_titles` table exists
3. Run the unit tests to verify parsing logic
4. Check database permissions

## References

- AniDB API: https://wiki.anidb.net/API
- Anime Titles API: http://anidb.net/api/anime-titles.dat.gz
- AniDB API Guidelines: https://wiki.anidb.net/API#Guidelines
