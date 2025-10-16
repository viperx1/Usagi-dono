# Mylist Bug Fix - Visual Summary

## The Problem

When the AniDB server sends a 221 MYLIST response, the data format is:

```
221 MYLIST
fid|eid|aid|gid|date|state|viewdate|storage|source|other|filestate
```

### Before Fix (INCORRECT) ❌

```
Query Command: MYLIST lid=123456

Response: 221 MYLIST
          789012|297776|18795|16325|1609459200|1|1640995200|HDD|BluRay||1
          
Code Mapping (WRONG):
    lid = token2[0] = 789012  ← This is actually the fid!
    fid = token2[1] = 297776  ← This is actually the eid!
    eid = token2[2] = 18795   ← This is actually the aid!
    aid = token2[3] = 16325   ← This is actually the gid!
    ... (all fields shifted by 1)

Database Result:
    ╔═════════╦═════════╦═════════╗
    ║   lid   ║   fid   ║   eid   ║
    ╠═════════╬═════════╬═════════╣
    ║ 789012  ║ 297776  ║ 18795   ║  ← ALL WRONG!
    ╚═════════╩═════════╩═════════╝
    
Problem: lid field contains fid value, causing duplicate-like behavior
```

### After Fix (CORRECT) ✅

```
Query Command: MYLIST lid=123456
              (stored in packets table)

Response: 221 MYLIST
          789012|297776|18795|16325|1609459200|1|1640995200|HDD|BluRay||1
          
Code Mapping (CORRECT):
    1. Extract lid from packets table: lid = 123456
    2. Parse response starting from index 0:
       fid = token2[0] = 789012  ✓
       eid = token2[1] = 297776  ✓
       aid = token2[2] = 18795   ✓
       gid = token2[3] = 16325   ✓
       ... (all fields correct)

Database Result:
    ╔═════════╦═════════╦═════════╗
    ║   lid   ║   fid   ║   eid   ║
    ╠═════════╬═════════╬═════════╣
    ║ 123456  ║ 789012  ║ 297776  ║  ← ALL CORRECT!
    ╚═════════╩═════════╩═════════╝
    
Result: All fields contain correct values!
```

## Code Changes

### Key Change in anidbapi.cpp (Line 462-520)

#### Before:
```cpp
else if(ReplyID == "221"){ // 221 MYLIST
    QStringList token2 = Message.split("\n");
    token2.pop_front();
    token2 = token2.first().split("|");
    
    if(token2.size() >= 12)
    {
        QString q = QString("INSERT OR REPLACE INTO `mylist` (...) VALUES (...)");
            .arg(QString(token2.at(0))...)  // ❌ WRONG: This is fid, not lid!
            .arg(QString(token2.at(1))...)  // ❌ WRONG: This is eid, not fid!
            ...
```

#### After:
```cpp
else if(ReplyID == "221"){ // 221 MYLIST
    // 1. Get lid from original command
    QString q = QString("SELECT `str` FROM `packets` WHERE `tag` = %1").arg(Tag);
    QSqlQuery query(db);
    QString lid;
    
    if(query.exec(q) && query.next())
    {
        QString mylistCmd = query.value(0).toString();
        // Extract lid from "MYLIST lid=12345"
        int lidStart = mylistCmd.indexOf("lid=");
        if(lidStart != -1)
        {
            lidStart += 4;
            int lidEnd = mylistCmd.indexOf("&", lidStart);
            if(lidEnd == -1)
                lidEnd = mylistCmd.indexOf(" ", lidStart);
            if(lidEnd == -1)
                lidEnd = mylistCmd.length();
            lid = mylistCmd.mid(lidStart, lidEnd - lidStart);
        }
    }
    
    // 2. Parse response (now correctly)
    QStringList token2 = Message.split("\n");
    token2.pop_front();
    token2 = token2.first().split("|");
    
    if(token2.size() >= 11 && !lid.isEmpty())
    {
        q = QString("INSERT OR REPLACE INTO `mylist` (...) VALUES (...)");
            .arg(lid)                       // ✓ CORRECT: Extracted from command
            .arg(QString(token2.at(0))...)  // ✓ CORRECT: This is fid
            .arg(QString(token2.at(1))...)  // ✓ CORRECT: This is eid
            ...
```

## Why This Pattern?

This fix follows the same pattern used by:
- **210 MYLIST ENTRY ADDED** handler (line 234-308)
- **311 MYLIST ENTRY EDITED** handler (line 558-630)

Both of these already extract information from the original command stored in the packets table, because the response doesn't contain all the original parameters.

## Additional Fix: FILE Response (Line 383)

Also fixed a bug where the filename field was duplicating the airdate value:

```cpp
// Before:
.arg(QString(token2.at(26))...)  // airdate
.arg(QString(token2.at(26))...)  // ❌ WRONG: Should be 27, not 26!

// After:
.arg(QString(token2.at(26))...)         // airdate
.arg(token2.size() > 27 ? QString(token2.at(27))... : "")  // ✓ filename
```

## Testing

Run the new test to verify the fix:
```bash
./build/tests/test_mylist_221_fix
```

The test demonstrates:
1. Correct lid extraction from MYLIST command
2. Correct parsing of response fields
3. Verification that lid ≠ fid (they must be different!)

## Impact on Users

### For New Queries
All new MYLIST (221) responses will be parsed correctly.

### For Existing Data
Data from previous MYLIST queries may be incorrect. Recommended actions:
1. Clear the mylist table: `DELETE FROM mylist;`
2. Re-import mylist data using MYLISTEXPORT
3. Or query each entry again with MYLIST command

## See Also
- [MYLIST_LID_FID_BUG_FIX.md](MYLIST_LID_FID_BUG_FIX.md) - Detailed technical documentation
- [tests/test_mylist_221_fix.cpp](tests/test_mylist_221_fix.cpp) - Test demonstrating the fix
