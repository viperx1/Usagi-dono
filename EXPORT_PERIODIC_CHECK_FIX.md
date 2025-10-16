# Export Periodic Check Fix

## Issue
The periodic notification check was refetching all messages from the AniDB API every 30 seconds instead of only checking for new ones. This resulted in unnecessary API calls and potential rate limiting.

## Root Cause
When the periodic check timer triggers every 30 seconds:
1. `checkForNotifications()` sends a `NOTIFYLIST` command to AniDB
2. AniDB responds with all available notifications (e.g., 50 notifications)
3. The code would fetch the last 10 message notifications **every time**
4. These same 10 notifications were already in the database from previous checks

**Result**: The same notifications were refetched repeatedly, wasting API quota.

## Solution
Modified the NOTIFYLIST response handlers (codes 290 and 291) to:
1. Query the local database for each notification ID
2. Filter out notifications that already exist in the `notifications` table
3. Only fetch new notifications that haven't been stored yet

## Technical Details

### Files Changed
- `usagi/src/anidbapi.cpp` - Modified NOTIFYLIST response handlers

### Code Changes

#### Before (Lines 452-468 for code 290)
```cpp
// Fetch the last several message notifications to search for export link
const int maxNotificationsToFetch = 10;
int notificationsToFetch = qMin(messageNids.size(), maxNotificationsToFetch);

if(notificationsToFetch > 0)
{
    for(int i = 0; i < notificationsToFetch; i++)
    {
        QString nid = messageNids[messageNids.size() - 1 - i];
        NotifyGet(nid.toInt());
    }
}
```

#### After (Lines 452-488 for code 290)
```cpp
// Check database to filter out already-fetched notifications
QStringList newNids;
QSqlQuery query(db);
for(int i = 0; i < messageNids.size(); i++)
{
    QString nid = messageNids[i];
    query.exec(QString("SELECT nid FROM notifications WHERE nid = %1").arg(nid));
    if(!query.next())
    {
        // Not in database, this is a new notification
        newNids.append(nid);
    }
}

Debug(...) // Log total vs new message counts

// Fetch only new message notifications
const int maxNotificationsToFetch = 10;
int notificationsToFetch = qMin(newNids.size(), maxNotificationsToFetch);

if(notificationsToFetch > 0)
{
    for(int i = 0; i < notificationsToFetch; i++)
    {
        QString nid = newNids[newNids.size() - 1 - i];
        NotifyGet(nid.toInt());
    }
}
else if(messageNids.size() > 0)
{
    Debug(...) // Log that no new notifications found
}
```

### Flow Diagram

#### Before Fix
```
Periodic Check Timer (every 30s)
    ↓
NOTIFYLIST Command → AniDB
    ↓
Response: All notifications (e.g., N1, N2, N3, ..., N50)
    ↓
Extract last 10: N41-N50
    ↓
Fetch N41-N50 from API ← REFETCH SAME ONES EVERY TIME
    ↓
Store in database
```

#### After Fix
```
Periodic Check Timer (every 30s)
    ↓
NOTIFYLIST Command → AniDB
    ↓
Response: All notifications (e.g., N1, N2, N3, ..., N50)
    ↓
Check database: N1-N45 already exist
    ↓
Filter to new only: N46-N50 ← ONLY NEW ONES
    ↓
Fetch N46-N50 from API
    ↓
Store in database
    ↓
Next check: N46-N50 already in DB → Skip fetching
```

## Benefits
1. **Reduced API calls**: Only new notifications are fetched
2. **Better API quota management**: Avoids unnecessary NOTIFYGET commands
3. **Improved performance**: Less network traffic and database writes
4. **Maintains functionality**: Export notifications are still detected properly

## Testing
The fix maintains all existing functionality:
- Export notifications are still detected when they arrive
- Periodic checking continues to work
- Database storage of notifications remains unchanged
- No changes to API surface or function signatures

## Database Table Used
The fix uses the existing `notifications` table:
```sql
CREATE TABLE IF NOT EXISTS `notifications`(
    `nid` INTEGER PRIMARY KEY,
    `type` TEXT,
    `from_user_id` INTEGER,
    `from_user_name` TEXT,
    `date` INTEGER,
    `message_type` INTEGER,
    `title` TEXT,
    `body` TEXT,
    `received_at` INTEGER,
    `acknowledged` BOOL DEFAULT 0
);
```

Notifications are stored when NOTIFYGET (code 292) response is received, so the database check ensures we don't fetch already-stored notifications.
