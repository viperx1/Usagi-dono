# Tagless Response Detection Flow Diagram

## Normal Response Flow (No Detection Triggered)

```
Input: "40 200 LOGIN ACCEPTED"
       │
       ├─ Split by space
       │
       ├─ Tag = "40"
       │  ReplyID = "200"
       │
       ├─ Check: Is Tag numeric? → YES (40)
       │  Check: Is ReplyID numeric? → YES (200)
       │
       ├─ Detection: Both numeric → NORMAL RESPONSE
       │
       └─ Output: Tag="40", ReplyID="200" ✅
          Routes to: 200 LOGIN ACCEPTED handler
```

## Tagless Error Response Flow (Detection Triggered)

```
Input: "598 UNKNOWN COMMAND"
       │
       ├─ Split by space
       │
       ├─ Tag = "598"
       │  ReplyID = "UNKNOWN"
       │
       ├─ Check: Is Tag numeric? → YES (598)
       │  Check: Is ReplyID numeric? → NO (UNKNOWN)
       │
       ├─ Detection: Tag numeric + ReplyID non-numeric → TAGLESS RESPONSE
       │
       ├─ Correction:
       │  ReplyID = Tag     (ReplyID = "598")
       │  Tag = "0"         (default)
       │
       ├─ Debug: "[AniDB Response] Tagless response detected"
       │
       └─ Output: Tag="0", ReplyID="598" ✅
          Routes to: 598 UNKNOWN COMMAND handler
```

## Decision Tree

```
                    Parse Message
                         |
                         v
                Token = Split(" ")
                         |
                         v
          Tag = token[0], ReplyID = token[1]
                         |
                         v
            Is Tag numeric AND ReplyID exists?
                    /        \
                  NO          YES
                  |            |
                  v            v
            Use as-is    Is ReplyID numeric?
                              /      \
                            YES      NO
                             |        |
                             v        v
                        Use as-is  SWAP VALUES
                                   Tag = "0"
                                   ReplyID = original Tag
                                   
```

## Before vs After Comparison

### Before Fix

```
AniDB Server         Parser              Handler
    |                  |                   |
    |  "598 UNKNOWN"   |                   |
    |----------------->|                   |
    |                  |                   |
    |             Tag="598"                |
    |             ReplyID="UNKNOWN"        |
    |                  |                   |
    |                  |  ReplyID="UNKNOWN"|
    |                  |------------------>| ❌ UNSUPPORTED
    |                  |                   | Falls through
```

### After Fix

```
AniDB Server         Parser              Handler
    |                  |                   |
    |  "598 UNKNOWN"   |                   |
    |----------------->|                   |
    |                  |                   |
    |             Tag="598"                |
    |             ReplyID="UNKNOWN"        |
    |                  |                   |
    |          🔍 DETECTION!               |
    |           Tag numeric?  YES          |
    |           ReplyID text? YES          |
    |                  |                   |
    |          ⚙️ CORRECTION!              |
    |           Tag="0"                    |
    |           ReplyID="598"              |
    |                  |                   |
    |                  |  ReplyID="598"    |
    |                  |------------------>| ✅ 598 Handler
    |                  |                   | Proper error msg
```

## Implementation Details

### Location
- File: `usagi/src/anidbapi.cpp`
- Function: `ParseMessage(QString Message, QString ReplyTo, QString ReplyToMsg)`
- Lines: 152-180

### Key Logic
```cpp
bool tagIsNumeric = false;
Tag.toInt(&tagIsNumeric);

if(tagIsNumeric && token.size() > 0 && !ReplyID.isEmpty())
{
    bool replyIsNumeric = false;
    ReplyID.toInt(&replyIsNumeric);
    
    if(!replyIsNumeric)
    {
        // Tagless response detected - swap values
        ReplyID = Tag;
        Tag = "0";
    }
}
```

### Safety Checks
1. ✅ Tag must be numeric (error code)
2. ✅ Token list must not be empty
3. ✅ ReplyID must not be empty
4. ✅ ReplyID must be non-numeric (error text)

All conditions must be true for correction to occur.
