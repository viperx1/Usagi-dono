# Mylist Database Bug Fix

## Issue Description
The mylist table in the local database had duplicate values in the `lid` (mylist ID) and `fid` (file ID) columns, which should contain different values. This was causing incorrect data storage and potential database integrity issues.

## Root Cause Analysis

### The Bug
The 221 MYLIST response handler in `anidbapi.cpp` was incorrectly parsing the AniDB API response. The code assumed the response format was:
```
lid|fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
```

However, according to the AniDB UDP API specification, when you query MYLIST by lid, the response does **not** include the lid itself (since you already know it from the query parameter). The actual response format is:
```
fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
```

### The Impact
The original code was mapping response fields as:
- `lid` = `token2.at(0)` (actually contains fid value!)
- `fid` = `token2.at(1)` (actually contains eid value!)
- And so on, with all subsequent fields shifted by one position

This caused:
1. The `lid` field in the database to contain the file ID instead of the mylist ID
2. The `fid` field to contain the episode ID
3. All other fields to be shifted incorrectly
4. When checking the database, both `lid` and `fid` appeared to have the same value (because lid was storing the fid value)

## The Fix

### Changes Made

#### 1. Modified 221 MYLIST Response Handler (lines 462-520)
The handler now:
1. Retrieves the original MYLIST command from the packets table using the Tag
2. Extracts the `lid` parameter from the command string (e.g., from "MYLIST lid=12345")
3. Uses this extracted lid as the first field in the INSERT statement
4. Correctly maps the response fields starting with `fid` at index 0

```cpp
else if(ReplyID == "221"){ // 221 MYLIST
    // Get the original MYLIST command to extract the lid parameter
    QString q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(Tag);
    QSqlQuery query(db);
    QString lid;
    
    if(query.exec(q) && query.next())
    {
        QString mylistCmd = query.value(0).toString();
        // Extract lid from command like "MYLIST lid=12345"
        int lidStart = mylistCmd.indexOf("lid=");
        if(lidStart != -1)
        {
            lidStart += 4; // Move past "lid="
            int lidEnd = mylistCmd.indexOf("&", lidStart);
            if(lidEnd == -1)
                lidEnd = mylistCmd.indexOf(" ", lidStart);
            if(lidEnd == -1)
                lidEnd = mylistCmd.length();
            lid = mylistCmd.mid(lidStart, lidEnd - lidStart);
        }
    }
    
    QStringList token2 = Message.split("\n");
    token2.pop_front();
    token2 = token2.first().split("|");
    // Parse mylist entry: fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
    // Note: lid is NOT included in the response - it's extracted from the query command
    if(token2.size() >= 11 && !lid.isEmpty())
    {
        q = QString("INSERT OR REPLACE INTO `mylist` (...) VALUES ('%1', '%2', ...)")
            .arg(lid)  // From extracted command parameter
            .arg(QString(token2.at(0)).replace("'", "''"))  // fid from response
            // ... rest of the fields correctly mapped
    }
}
```

#### 2. Fixed FILE Response Handler (line 383)
Also corrected a separate bug where `token2.at(26)` was used twice, causing the filename field to duplicate the airdate value. Now correctly uses `token2.at(27)` for the filename field.

#### 3. Updated Comments
Updated the comment on line 488 to reflect the correct API response format.

### Similar Patterns in Codebase
The fix follows the same pattern used by the 210 MYLIST ENTRY ADDED and 311 MYLIST ENTRY EDITED handlers, which already correctly extract information from the original command stored in the packets table.

## Verification

### Test Case
Created `tests/test_mylist_221_fix.cpp` which demonstrates:
1. The correct parsing of a 221 MYLIST response
2. Verification that lid and fid have different values (not the same)
3. Correct extraction of lid from the MYLIST command

### Example Data
**Command:**
```
MYLIST lid=123456
```

**Response:**
```
221 MYLIST
789012|297776|18795|16325|1609459200|1|1640995200|HDD|BluRay||1
```

**Before Fix:**
- lid = 789012 (incorrect - this is actually fid!)
- fid = 297776 (incorrect - this is actually eid!)

**After Fix:**
- lid = 123456 (correct - extracted from command)
- fid = 789012 (correct - from response index 0)
- eid = 297776 (correct - from response index 1)

## Database Impact

### For Existing Databases
Users with existing databases will have incorrect data in the mylist table from previous 221 MYLIST responses. The fix will only affect new MYLIST queries going forward. Consider:
1. Clearing the mylist table and re-importing data
2. Or running a MYLIST query for each entry to refresh with correct values

### Database Schema
No schema changes are required. The mylist table structure remains:
```sql
CREATE TABLE `mylist`(
    `lid` INTEGER PRIMARY KEY,
    `fid` INTEGER,
    `eid` INTEGER,
    `aid` INTEGER,
    `gid` INTEGER,
    `date` INTEGER,
    `state` INTEGER,
    `viewed` INTEGER,
    `viewdate` INTEGER,
    `storage` TEXT,
    `source` TEXT,
    `other` TEXT,
    `filestate` INTEGER
)
```

## Related Code
- MYLISTADD handler (210): Already correctly handles lid/fid
- MYLIST ENTRY EDITED handler (311): Already correctly handles lid/fid
- FILE handler (220): Fixed duplicate filename bug

## Testing
Run the test suite with:
```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

## References
- AniDB UDP API Documentation: https://wiki.anidb.net/UDP_API_Definition
- MYLIST command: https://wiki.anidb.net/UDP_API_Definition#MYLIST:_Retrieve_Mylist_Data
