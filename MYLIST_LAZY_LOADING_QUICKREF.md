# MyList Episode Data Loading - Quick Reference

## What It Does
Automatically loads missing episode information when you expand anime entries in your MyList.

## Usage

### 1. View MyList
- MyList loads automatically on startup
- Entries with missing episode data show "Loading..." instead of warnings

### 2. Expand an Anime
- Click the arrow/plus to expand an anime entry
- Application automatically requests missing episode data from AniDB
- You'll see log messages: "Requesting episode data for EID..."

### 3. Wait for Data
- Data arrives within a few seconds (respects 2-second rate limit)
- Log shows: "Episode data received for EID..."
- MyList automatically refreshes with updated information

### 4. View Complete Data
- Episode numbers appear (1, 2, 3... or Special 1, etc.)
- Episode titles are displayed
- No more "Loading..." placeholders

## What Gets Loaded

Missing episode information:
- **Episode Number** (epno): Regular episode numbers, specials, credits, etc.
- **Episode Name**: Episode titles in English or romaji

## Rate Limiting

- Respects AniDB's mandatory 2-second delay
- Multiple episodes are queued automatically
- No need to wait between expansions

## Tips

- **Expand once**: Data is requested immediately on first expansion
- **Be patient**: Large anime series may take a few minutes to fully load
- **Check logs**: The Log tab shows all API activity
- **Persistent**: Data is saved to database for future sessions

## Troubleshooting

### "Loading..." doesn't change
- Check if you're logged in to AniDB
- Check the Log tab for errors
- Ensure you have internet connection
- Verify episode exists in AniDB database

### Too slow
- This is by design - AniDB enforces 2-second delays
- Consider using MyList Export feature for bulk updates

### Data still missing after loading
- Some very old anime may not have complete episode data in AniDB
- Try viewing the anime on AniDB website to confirm data exists

## Technical Notes

- Uses AniDB UDP API EPISODE command
- Data is stored in local SQLite database
- Survives application restart
- No duplicate requests for same episode

## Related Features

- **MyList Export**: Download complete mylist data from AniDB web interface
- **File Hashing**: Episode data is collected when hashing/adding files
- **Database View**: All data is stored locally for offline viewing
