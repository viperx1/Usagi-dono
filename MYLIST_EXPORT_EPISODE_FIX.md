# MyList Export Episode Data Fix

## Issue
The mylist export XML contains episode information (titles and numbers), but this data was not being extracted during import. This caused unnecessary API calls to fetch episode data that was already available in the export.

## Root Cause
The `parseMylistExport()` function in `window.cpp` was only extracting:
- Anime ID (`aid`)
- Episode ID (`eid`)  
- Mylist ID (`lid`)
- File ID (`fid`)

But it was **not** extracting episode details from the `<Ep>` XML tag:
```xml
<Ep Id="12814" EpNo="1" Name="OVA" NameRomaji="..." NameKanji="...">
  <File Id="54357" LId="16588092" ... />
</Ep>
```

## Impact
When `loadMylistFromDatabase()` ran, it would query the database and find:
- Empty `episode.name` → Display "Loading..." → Mark for API fetch
- Empty `episode.epno` → Display "Loading..." → Mark for API fetch

This triggered unnecessary EPISODE API calls when users expanded anime items in the mylist tree.

## Solution
Modified `parseMylistExport()` to:

1. **Extract episode data** from the `<Ep>` XML element:
   - `EpNo` → Episode number (e.g., "1", "2", "S1", "C1")
   - `Name` → Episode title (e.g., "OVA", "First Episode")
   - `NameRomaji` → Romaji episode title
   - `NameKanji` → Kanji episode title

2. **Store in database** using `INSERT OR REPLACE INTO episode`:
   ```sql
   INSERT OR REPLACE INTO `episode` 
   (`eid`, `epno`, `name`, `nameromaji`, `namekanji`) 
   VALUES (...)
   ```

3. **Proper SQL escaping** for single quotes to prevent SQL injection

## Result
After parsing the export:
- Episode data is already in the database
- `loadMylistFromDatabase()` finds populated `episode.name` and `episode.epno`
- No "Loading..." displayed
- No API calls needed for episode information
- Users see episode titles and numbers immediately

## Code Changes
### Main Change: window.cpp
```cpp
else if(xml.name() == QString("Ep"))
{
    // Get episode ID, number, and name from Ep element
    QXmlStreamAttributes attributes = xml.attributes();
    currentEid = attributes.value("Id").toString();
    currentEpNo = attributes.value("EpNo").toString();
    currentEpName = attributes.value("Name").toString();
    QString currentEpNameRomaji = attributes.value("NameRomaji").toString();
    QString currentEpNameKanji = attributes.value("NameKanji").toString();
    
    // Store episode data in episode table if we have valid data
    if(!currentEid.isEmpty() && (!currentEpNo.isEmpty() || !currentEpName.isEmpty()))
    {
        QString epName_escaped = QString(currentEpName).replace("'", "''");
        QString epNo_escaped = QString(currentEpNo).replace("'", "''");
        QString epNameRomaji_escaped = QString(currentEpNameRomaji).replace("'", "''");
        QString epNameKanji_escaped = QString(currentEpNameKanji).replace("'", "''");
        
        QString episodeQuery = QString("INSERT OR REPLACE INTO `episode` "
            "(`eid`, `epno`, `name`, `nameromaji`, `namekanji`) "
            "VALUES (%1, '%2', '%3', '%4', '%5')")
            .arg(currentEid)
            .arg(epNo_escaped)
            .arg(epName_escaped)
            .arg(epNameRomaji_escaped)
            .arg(epNameKanji_escaped);
        
        QSqlQuery episodeQueryExec(db);
        if(!episodeQueryExec.exec(episodeQuery))
        {
            logOutput->append(QString("Warning: Failed to insert episode data (eid=%1): %2")
                .arg(currentEid).arg(episodeQueryExec.lastError().text()));
        }
    }
}
```

### Test Updates: test_mylist_xml_parser.cpp
- Added episode table creation in `initTestCase()`
- Updated sample XML to include `Name` attribute in `<Ep>` tags
- Added test verification for episode data extraction
- Verifies all three test episodes are stored correctly

## Testing
The test verifies:
1. ✅ Episode table is created with correct schema
2. ✅ Episode data is extracted from `EpNo` and `Name` attributes
3. ✅ Episode data is stored in database with correct values
4. ✅ All three test episodes (2614, 2615, 12814) are present

## Files Changed
- `usagi/src/window.cpp` - Modified `parseMylistExport()` function
- `tests/test_mylist_xml_parser.cpp` - Updated test to verify episode extraction

## Benefits
- ✅ Eliminates unnecessary API calls for episode data
- ✅ Faster mylist display (no waiting for API responses)
- ✅ Reduces load on AniDB API servers
- ✅ Works offline (episode data already in export)
- ✅ Minimal code changes (surgical fix)
