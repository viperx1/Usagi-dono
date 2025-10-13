# MyList Import Guide

## Overview

This application now supports importing your complete mylist from AniDB through two methods:

1. **Fetch MyList Stats from API** - Uses the UDP API to get statistics
2. **Import MyList from File** - Imports a mylist export file downloaded from AniDB

## Method 1: Fetch MyList Stats (UDP API)

This method uses the AniDB UDP API to fetch mylist statistics:

1. Click the **"Fetch MyList Stats from API"** button in the MyList tab
2. The application will send a MYLISTSTATS command to AniDB
3. Statistics will be returned (total entries, watched count, etc.)
4. Note: This only provides statistics, not the actual mylist entries

## Method 2: Download MyList from AniDB (NEW - Easiest)

The quickest way to import your complete mylist:

### Step 1: Download from AniDB
1. Open Usagi-dono application
2. Go to the **MyList** tab
3. Click **"Download MyList from AniDB"** button
4. Select your preferred template format:
   - **csv-adborg (Recommended)** - Optimized format, works with all features
   - **xml** - Full structured data
   - **csv** - Standard format
   - **json** - JSON format
   - **anidb** - AniDB default
5. Your browser will open to AniDB's export page
6. Log in to AniDB if needed
7. Click **"Export"** to download the file

### Step 2: Import the Downloaded File
1. Return to Usagi-dono
2. Click **"Import MyList from File"**
3. Select the file you just downloaded
4. The application will automatically detect the format and import the data
5. After import, your mylist will display automatically

## Method 3: Import from Export File (Manual Download)

To import your complete mylist with all entries:

### Step 1: Export Your MyList from AniDB

1. Log in to your AniDB account at https://anidb.net
2. Navigate to your MyList: https://anidb.net/perl-bin/animedb.pl?show=mylist
3. Click on "Export" at the top of the page
4. Choose one of the following formats:
   - **XML** (recommended) - Full structured data
   - **CSV/TXT** - Comma-separated values
5. Download the export file to your computer

### Step 2: Import the File

1. Open Usagi-dono application
2. Go to the **MyList** tab
3. Click **"Import MyList from File"**
4. Select the downloaded export file
5. The application will automatically detect the format and import the data
6. After import, click **"Load MyList from Database"** to view your imported data

## Supported File Formats

### XML Format
```xml
<?xml version="1.0" encoding="UTF-8"?>
<mylistexport>
  <mylist lid="123456" fid="789012" eid="345678" aid="901234" gid="567890" 
          state="1" viewdate="1234567890" storage="/path/to/file"/>
  <!-- More entries... -->
</mylistexport>
```

### CSV/TXT Format

The application now supports multiple CSV template formats:

#### Standard MYLISTEXPORT Format
```
lid,fid,eid,aid,gid,date,state,viewdate,storage,source,other,filestate
123456,789012,345678,901234,567890,1234567890,1,1234567890,/path/to/file,...
```

#### csv-adborg Template Format
```
aid,eid,gid,lid,status,viewdate,anime_name,episode_name
901234,345678,567890,123456,1,1234567890,Test Anime,Episode 1
```

The parser automatically detects which format is being used by reading the header row and mapping columns by name. This means you can use any AniDB MYLISTEXPORT template that includes the required fields (lid, aid).

## Data Fields

- **lid** - MyList ID (unique identifier for each mylist entry)
- **fid** - File ID
- **eid** - Episode ID
- **aid** - Anime ID
- **gid** - Group ID
- **state** - Storage state (0=unknown, 1=HDD, 2=CD/DVD, 3=deleted)
- **viewdate** - When the episode was watched (used to set "viewed" status)
- **storage** - Storage location path

## Troubleshooting

### No entries imported
- Check that the file format is correct
- Ensure the file is a valid AniDB export
- Look at the log output for error messages

### Missing anime/episode names
- The import only imports mylist data (lid, fid, eid, aid, etc.)
- Anime and episode names will be fetched when you hash files
- Or they will display as "Anime #[aid]" if not yet fetched

### Duplicate entries
- The import uses `INSERT OR REPLACE`, so existing entries will be updated
- This is safe to run multiple times

## Notes

- Importing from file is the recommended method as it doesn't require API calls
- The UDP API has strict rate limiting, so bulk queries are not supported
- After importing, you can use the normal file hashing feature to update entries
- The import process respects your existing database data
