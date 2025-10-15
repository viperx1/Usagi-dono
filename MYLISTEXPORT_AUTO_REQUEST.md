# Automatic MYLISTEXPORT Request Implementation

## Problem Statement

The AniDB UDP API NOTIFYLIST command only returns **unread** notifications. If a user has already seen (and thus marked as read) the mylist export notification, or if there simply is no export notification yet, the application would not find any export link in the unread notifications.

Previously, the application would just log "No mylist export link found in notification" multiple times and give up, leaving the user without their mylist data.

## Solution

Implemented automatic MYLISTEXPORT request when no export link is found in unread notifications.

### Implementation Details

#### 1. MYLISTEXPORT UDP API Command

Added new API command to request mylist export programmatically:

**Command Format:**
```
MYLISTEXPORT [template={str template_name}|cancel=1]
```

**Parameters:**
- `template`: Template name (e.g., "csv-adborg", "xml", "csv", "json", "anidb")
- `cancel=1`: Cancel any pending export request

**anidbapi.h:**
```cpp
QString MylistExport(QString template_name = "csv-adborg");
QString buildMylistExportCommand(QString template_name);
```

**anidbapi.cpp:**
```cpp
QString AniDBApi::MylistExport(QString template_name)
{
    if(SID.length() == 0 || LoginStatus() == 0) { Auth(); }
    Debug("Requesting MYLISTEXPORT with template: " + template_name);
    QString msg = buildMylistExportCommand(template_name);
    // Queue command...
    return GetTag(msg);
}

QString AniDBApi::buildMylistExportCommand(QString template_name)
{
    return QString("MYLISTEXPORT template=%1").arg(template_name);
}
```

#### 2. Response Code Handlers

Implemented handlers for all MYLISTEXPORT response codes:

**Success Responses:**
- **217 EXPORT QUEUED**: Export request accepted, will be generated. Emits `notifyExportQueued` signal.
- **218 EXPORT CANCELLED**: Reply to `cancel=1` parameter - export was cancelled successfully.

**Error Responses:**
- **317 EXPORT NO SUCH TEMPLATE**: Invalid template name. Emits `notifyExportNoSuchTemplate` signal.
- **318 EXPORT ALREADY IN QUEUE**: Cannot queue another export. Emits `notifyExportAlreadyInQueue` signal.
- **319 EXPORT NO EXPORT QUEUED OR IS PROCESSING**: Reply to `cancel=1` when there's no export to cancel.

```cpp
else if(ReplyID == "217"){ // 217 EXPORT QUEUED
    Debug("217 EXPORT QUEUED");
    emit notifyExportQueued(Tag);
}
else if(ReplyID == "218"){ // 218 EXPORT CANCELLED
    Debug("218 EXPORT CANCELLED");
    // Reply to cancel=1 parameter
}
else if(ReplyID == "317"){ // 317 EXPORT NO SUCH TEMPLATE
    Debug("317 EXPORT NO SUCH TEMPLATE");
    emit notifyExportNoSuchTemplate(Tag);
}
else if(ReplyID == "318"){ // 318 EXPORT ALREADY IN QUEUE
    Debug("318 EXPORT ALREADY IN QUEUE");
    emit notifyExportAlreadyInQueue(Tag);
}
else if(ReplyID == "319"){ // 319 EXPORT NO EXPORT QUEUED OR IS PROCESSING
    Debug("319 EXPORT NO EXPORT QUEUED OR IS PROCESSING");
    // Reply to cancel=1 when nothing to cancel
}
```

#### 3. User Feedback System

Added signals and slots to provide user feedback for export status:

**Signals (anidbapi.h):**
```cpp
signals:
    void notifyExportQueued(QString tag);
    void notifyExportAlreadyInQueue(QString tag);
    void notifyExportNoSuchTemplate(QString tag);
```

**Slots (window.h):**
```cpp
void getNotifyExportQueued(QString tag);
void getNotifyExportAlreadyInQueue(QString tag);
void getNotifyExportNoSuchTemplate(QString tag);
```

**Implementations (window.cpp):**
```cpp
void Window::getNotifyExportQueued(QString tag)
{
    logOutput->append(QString("MyList export queued successfully (Tag: %1)").arg(tag));
    mylistStatusLabel->setText("MyList Status: Export queued - waiting for notification...");
}

void Window::getNotifyExportAlreadyInQueue(QString tag)
{
    logOutput->append(QString("Export already in queue (Tag: %1) - waiting for current export").arg(tag));
    mylistStatusLabel->setText("MyList Status: Export already queued - waiting...");
}

void Window::getNotifyExportNoSuchTemplate(QString tag)
{
    logOutput->append(QString("ERROR: Export template not found (Tag: %1)").arg(tag));
    mylistStatusLabel->setText("MyList Status: Export failed - invalid template");
}
```

#### 4. Notification Tracking System

Added tracking to count how many notifications are checked:

**window.h:**
```cpp
// Notification tracking for mylist export
int expectedNotificationsToCheck;
int notificationsCheckedWithoutExport;
bool isCheckingNotifications;
```

**Initialization:**
```cpp
expectedNotificationsToCheck = 0;
notificationsCheckedWithoutExport = 0;
isCheckingNotifications = false;
```

#### 5. Signal/Slot Communication

Added signal to inform window when notification checking starts:

**anidbapi.h:**
```cpp
signals:
    void notifyCheckStarting(int count);
```

**anidbapi.cpp (emitted in NOTIFYLIST handlers):**
```cpp
if(notificationsToFetch > 0)
{
    emit notifyCheckStarting(notificationsToFetch);
    for(int i = 0; i < notificationsToFetch; i++)
    {
        NotifyGet(nid.toInt());
    }
}
```

**window.cpp:**
```cpp
void Window::getNotifyCheckStarting(int count)
{
    isCheckingNotifications = true;
    expectedNotificationsToCheck = count;
    notificationsCheckedWithoutExport = 0;
    logOutput->append(QString("Starting to check %1 notifications").arg(count));
}
```

#### 6. Automatic Export Request Logic

Modified `getNotifyMessageReceived()` to track and request:

```cpp
void Window::getNotifyMessageReceived(int nid, QString message)
{
    // ... check for export link ...
    
    if(exportLinkFound)
    {
        // Reset tracking, start download
        isCheckingNotifications = false;
        expectedNotificationsToCheck = 0;
        notificationsCheckedWithoutExport = 0;
        // ... download logic ...
    }
    else
    {
        logOutput->append("No mylist export link found in notification");
        
        if(isCheckingNotifications)
        {
            notificationsCheckedWithoutExport++;
            
            // If we've checked all notifications with no export found
            if(notificationsCheckedWithoutExport >= expectedNotificationsToCheck)
            {
                logOutput->append(QString("Checked %1 notifications - requesting new export")
                    .arg(expectedNotificationsToCheck));
                
                // Request MYLISTEXPORT with csv-adborg template
                adbapi->MylistExport("csv-adborg");
                
                // Reset state
                isCheckingNotifications = false;
                expectedNotificationsToCheck = 0;
                notificationsCheckedWithoutExport = 0;
            }
        }
    }
}
```

## Workflow

### Scenario 1: Export Link Found in Notifications
```
1. User logs in
2. NOTIFYLIST returns unread notifications
3. Check last 10 message notifications
4. Export link found in notification #3
5. Download and import mylist
```

### Scenario 2: No Export Link Found - Auto Request
```
1. User logs in
2. NOTIFYLIST returns unread notifications  
3. Check last 10 message notifications
4. No export link found in any of them
5. Automatically request MYLISTEXPORT template=csv-adborg
6. AniDB queues export generation (response 217)
7. Status: "Export queued - waiting for notification..."
8. When export ready, AniDB sends new notification
9. New notification received via code 270 (PUSH) or next NOTIFYLIST
10. Export link found, download and import mylist
```

### Scenario 3: Export Already Queued
```
1. User requests export
2. MYLISTEXPORT sent
3. AniDB responds with 318 EXPORT ALREADY IN QUEUE
4. Status: "Export already queued - waiting..."
5. Wait for existing export notification
```

### Scenario 4: Invalid Template (Should Not Happen)
```
1. User requests export with invalid template
2. MYLISTEXPORT sent
3. AniDB responds with 317 EXPORT NO SUCH TEMPLATE
4. Status: "Export failed - invalid template"
5. Log error for debugging
```

## Response Code Reference

| Code | Name | Description | Signal Emitted | User Message |
|------|------|-------------|----------------|--------------|
| 217 | EXPORT QUEUED | Export request accepted | `notifyExportQueued` | "Export queued - waiting for notification..." |
| 218 | EXPORT CANCELLED | Reply to cancel=1 | None | (Only for cancel operations) |
| 317 | EXPORT NO SUCH TEMPLATE | Invalid template name | `notifyExportNoSuchTemplate` | "Export failed - invalid template" |
| 318 | EXPORT ALREADY IN QUEUE | Cannot queue another | `notifyExportAlreadyInQueue` | "Export already queued - waiting..." |
| 319 | EXPORT NO EXPORT QUEUED | Reply to cancel=1 when nothing to cancel | None | (Only for cancel operations) |

## Benefits

✅ **Automatic Recovery**: If no export exists, one is automatically requested  
✅ **No Manual Intervention**: User doesn't need to manually request export  
✅ **Handles All Cases**: Works whether export doesn't exist or was already marked read  
✅ **Complete Error Handling**: All response codes properly handled with user feedback  
✅ **Uses Correct API**: Uses UDP API MYLISTEXPORT command, not web interface  
✅ **Template Selection**: Requests "csv-adborg" template by default  
✅ **Efficient**: Only requests export if truly needed (no export link found)  
✅ **User Feedback**: Clear status messages for all scenarios  

## API Compatibility

The implementation follows the AniDB UDP API specification:

- **Command**: `MYLISTEXPORT [template={str template_name}|cancel=1]`
- **Required Parameter**: `template` (e.g., "csv-adborg", "xml", "csv", "json", "anidb")
- **Optional Parameter**: `cancel=1` (to cancel pending export)
- **Response Codes**:
  - 217 EXPORT QUEUED
  - 218 EXPORT CANCELLED (reply to cancel=1)
  - 317 EXPORT NO SUCH TEMPLATE
  - 318 EXPORT ALREADY IN QUEUE
  - 319 EXPORT NO EXPORT QUEUED OR IS PROCESSING (reply to cancel=1)

## Testing Scenarios

The implementation can be tested by:

1. **Scenario: No previous export**
   - Fresh AniDB account or cleared notifications
   - Login to application
   - Observe automatic MYLISTEXPORT request
   - Verify "Export queued" message
   - Wait for notification with export link

2. **Scenario: Old export (marked read)**
   - Export notification already marked as read
   - Login to application
   - NOTIFYLIST returns no export (it's read)
   - Observe automatic MYLISTEXPORT request
   - Verify status updates

3. **Scenario: Recent export exists**
   - Unread export notification exists
   - Login to application
   - Export link found immediately
   - No MYLISTEXPORT request needed

4. **Scenario: Export already in queue**
   - Request export while one is pending
   - Receive 318 response
   - Verify "Export already queued" message
   - Wait for existing export

## Files Changed

- `usagi/src/anidbapi.h` - Added MylistExport(), signals, and command builder
- `usagi/src/anidbapi.cpp` - Implemented MYLISTEXPORT command and all response handlers
- `usagi/src/window.h` - Added tracking variables and slots
- `usagi/src/window.cpp` - Implemented tracking logic, auto-request, and user feedback

## Commits

- 3e2239b: Reverted logging changes as requested
- f8c534e: Implemented MYLISTEXPORT command and automatic request logic
- 9d5eb0f: Added comprehensive documentation
- a3ab9bf: Added remaining response codes (317, 318, 319) and user feedback

## Status

✅ Implementation complete  
✅ All response codes implemented (217, 218, 317, 318, 319)  
✅ Follows AniDB UDP API specification  
✅ Addresses the issue of NOTIFYLIST only returning unread messages  
✅ Automatically requests export when needed  
✅ Complete error handling with user feedback  


#### 3. Notification Tracking System

Added tracking to count how many notifications are checked:

**window.h:**
```cpp
// Notification tracking for mylist export
int expectedNotificationsToCheck;
int notificationsCheckedWithoutExport;
bool isCheckingNotifications;
```

**Initialization:**
```cpp
expectedNotificationsToCheck = 0;
notificationsCheckedWithoutExport = 0;
isCheckingNotifications = false;
```

#### 4. Signal/Slot Communication

Added signal to inform window when notification checking starts:

**anidbapi.h:**
```cpp
signals:
    void notifyCheckStarting(int count);
```

**anidbapi.cpp (emitted in NOTIFYLIST handlers):**
```cpp
if(notificationsToFetch > 0)
{
    emit notifyCheckStarting(notificationsToFetch);
    for(int i = 0; i < notificationsToFetch; i++)
    {
        NotifyGet(nid.toInt());
    }
}
```

**window.cpp:**
```cpp
void Window::getNotifyCheckStarting(int count)
{
    isCheckingNotifications = true;
    expectedNotificationsToCheck = count;
    notificationsCheckedWithoutExport = 0;
    logOutput->append(QString("Starting to check %1 notifications").arg(count));
}
```

#### 5. Automatic Export Request Logic

Modified `getNotifyMessageReceived()` to track and request:

```cpp
void Window::getNotifyMessageReceived(int nid, QString message)
{
    // ... check for export link ...
    
    if(exportLinkFound)
    {
        // Reset tracking, start download
        isCheckingNotifications = false;
        expectedNotificationsToCheck = 0;
        notificationsCheckedWithoutExport = 0;
        // ... download logic ...
    }
    else
    {
        logOutput->append("No mylist export link found in notification");
        
        if(isCheckingNotifications)
        {
            notificationsCheckedWithoutExport++;
            
            // If we've checked all notifications with no export found
            if(notificationsCheckedWithoutExport >= expectedNotificationsToCheck)
            {
                logOutput->append(QString("Checked %1 notifications - requesting new export")
                    .arg(expectedNotificationsToCheck));
                
                // Request MYLISTEXPORT with csv-adborg template
                adbapi->MylistExport("csv-adborg");
                
                // Reset state
                isCheckingNotifications = false;
                expectedNotificationsToCheck = 0;
                notificationsCheckedWithoutExport = 0;
            }
        }
    }
}
```

## Workflow

### Scenario 1: Export Link Found in Notifications
```
1. User logs in
2. NOTIFYLIST returns unread notifications
3. Check last 10 message notifications
4. Export link found in notification #3
5. Download and import mylist
```

### Scenario 2: No Export Link Found (NEW!)
```
1. User logs in
2. NOTIFYLIST returns unread notifications  
3. Check last 10 message notifications
4. No export link found in any of them
5. Automatically request MYLISTEXPORT template=csv-adborg
6. AniDB queues export generation (response 217)
7. When export ready, AniDB sends new notification
8. New notification received via code 270 (PUSH) or next NOTIFYLIST
9. Export link found, download and import mylist
```

## Benefits

✅ **Automatic Recovery**: If no export exists, one is automatically requested  
✅ **No Manual Intervention**: User doesn't need to manually request export  
✅ **Handles All Cases**: Works whether export doesn't exist or was already marked read  
✅ **Uses Correct API**: Uses UDP API MYLISTEXPORT command, not web interface  
✅ **Template Selection**: Requests "csv-adborg" template by default  
✅ **Efficient**: Only requests export if truly needed (no export link found)  

## API Compatibility

The implementation follows the AniDB UDP API specification:

- **Command**: `MYLISTEXPORT template={str template}&adddate={int4 adddate}`
- **Required Parameter**: `template` (e.g., "csv-adborg", "xml", "csv")
- **Optional Parameter**: `adddate` (Unix timestamp to filter entries)
- **Response Codes**:
  - 217 MYLIST EXPORT QUEUED
  - 218 MYLIST EXPORT CANCELLED

## Testing

The implementation can be tested by:

1. **Scenario: No previous export**
   - Fresh AniDB account or cleared notifications
   - Login to application
   - Observe automatic MYLISTEXPORT request
   - Wait for notification with export link

2. **Scenario: Old export (marked read)**
   - Export notification already marked as read
   - Login to application
   - NOTIFYLIST returns no export (it's read)
   - Observe automatic MYLISTEXPORT request

3. **Scenario: Recent export exists**
   - Unread export notification exists
   - Login to application
   - Export link found immediately
   - No MYLISTEXPORT request needed

## Files Changed

- `usagi/src/anidbapi.h` - Added MylistExport() and signal
- `usagi/src/anidbapi.cpp` - Implemented MYLISTEXPORT command and handlers
- `usagi/src/window.h` - Added tracking variables and slot
- `usagi/src/window.cpp` - Implemented tracking logic and auto-request

## Commit

- Hash: f8c534e
- Message: "Implement MYLISTEXPORT request when no export link found"
- Files changed: 5
- Lines: +84, -130 (net -46 lines due to removed doc file)

## Status

✅ Implementation complete  
✅ Follows AniDB UDP API specification  
✅ Addresses the issue of NOTIFYLIST only returning unread messages  
✅ Automatically requests export when needed  
