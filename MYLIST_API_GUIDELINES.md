# MyList API Implementation Guidelines

## AniDB API Best Practices

Based on AniDB UDP API documentation (https://wiki.anidb.net/UDP_API_Definition):

### Rate Limiting (CRITICAL)
- **Mandatory 2-second delay** between API requests
- This is already implemented via `packetsender` QTimer with 2100ms interval
- Violating this can lead to temporary or permanent bans

### MyList Query Strategy

The AniDB API provides several commands for mylist interaction:

1. **MYLISTADD** - Add a file to mylist
   - Current implementation: ✅ Implemented in `MylistAdd()`
   - Used when hashing files to add them to mylist

2. **MYLIST** - Query mylist by lid (mylist id)
   - Current implementation: ✅ Implemented in `Mylist()`
   - Used to query individual mylist entries
   - **Important**: Should NOT be used for bulk queries

3. **MYLISTSTAT** - Get mylist statistics
   - Returns: total entries, watched count, file sizes, etc.
   - Current implementation: ✅ Added response handler (223)
   - Useful for displaying summary information

4. **FILE** - Query file information (RECOMMENDED for mylist updates)
   - Returns both file and mylist data if file is in mylist
   - Current implementation: ✅ Implemented in `File()`
   - **This is the PREFERRED way** to update mylist data
   - More efficient than separate MYLIST queries

### Best Practices for MyList Feature

1. **Display mylist from local database**
   - ✅ Implemented in `loadMylistFromDatabase()`
   - Shows data that was already collected via FILE/MYLISTADD commands
   - No API calls needed for display

2. **Update mylist data when hashing files**
   - ✅ Already implemented
   - FILE command returns mylist data which is stored in database
   - Natural way to keep mylist up-to-date

3. **Avoid bulk mylist queries**
   - ❌ Don't iterate through all lids calling MYLIST
   - This violates API guidelines and will get you banned
   - Instead, rely on FILE command data collection

4. **Use MYLISTSTAT for summary info**
   - Can be called occasionally to show overview
   - Much more efficient than querying individual entries

### Current Implementation Status

The implementation follows AniDB API guidelines:
- ✅ Proper rate limiting (2-second delay)
- ✅ MyList display from local database (no API calls)
- ✅ MyList data collection via FILE command
- ✅ MYLISTADD for adding files
- ✅ MYLIST for individual queries when needed
- ✅ MYLISTSTAT handler for statistics

### Future Enhancements (Optional)

If needed, could add:
- Button to call MYLISTSTAT and show summary
- Context menu to query individual anime/episode via MYLIST
- But these are not necessary for the core feature
