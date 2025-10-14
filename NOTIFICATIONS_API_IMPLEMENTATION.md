# Notifications API Implementation

## Overview
Implemented bare minimum notifications API to fully automate mylist export importing from AniDB.

## Implementation

### AniDB API Methods

#### NotifyEnable()
```cpp
QString AniDBApi::NotifyEnable()
{
    // Sends NOTIFYLIST command to enable push notifications
    // Called automatically after successful login
}
```

#### PushAck(int nid)
```cpp
QString AniDBApi::PushAck(int nid)
{
    // Sends PUSHACK nid={nid} to acknowledge notification
    // Called automatically when notification received
}
```

### Notification Codes

| Code | Name | Description | Action |
|------|------|-------------|--------|
| 270 | NOTIFICATION | Notification received | Parse and emit signal |
| 271 | NOTIFYACK | Acknowledge received | Log confirmation |
| 272 | NO SUCH NOTIFICATION | Invalid nid | Log error |
| 290 | NOTIFYLIST | Notification list | Log list |
| 291 | NOTIFYLIST ENTRY | List entry | Log entry |

### Notification Message Format

AniDB UDP API notification format:
```
270 {int4 nid}|{int2 type}|{int4 fromuid}|{int4 date}|{str title}|{str body}
```

Example:
```
270 12345|1|100|1234567890|MyList Export Ready|Your mylist export is ready: http://anidb.net/export/12345-user-export.tgz
```

## Automatic Import Flow

### 1. Enable Notifications
```cpp
void Window::getNotifyLoggedIn(QString tag, int code)
{
    // Called when login successful (code 200/201)
    adbapi->NotifyEnable();  // Enable notifications
}
```

### 2. Receive Notification
```cpp
// In ParseMessage() when code 270 received
QStringList parts = notification.split("|");
int nid = parts[0].toInt();
QString body = parts[5];  // Contains export URL

emit notifyMessageReceived(nid, body);
PushAck(nid);  // Auto-acknowledge
```

### 3. Extract URL
```cpp
void Window::getNotifyMessageReceived(int nid, QString message)
{
    // Extract .tgz URL from notification body
    QRegularExpression urlRegex("https?://[^\\s]+\\.tgz");
    QRegularExpressionMatch match = urlRegex.match(message);
    
    if(match.hasMatch())
    {
        QString exportUrl = match.captured(0);
        // Download and import...
    }
}
```

### 4. Download Export
```cpp
// Uses QNetworkAccessManager for HTTP download
QNetworkAccessManager *manager = new QNetworkAccessManager(this);
QNetworkRequest request(QUrl(exportUrl));
request.setHeader(QNetworkRequest::UserAgentHeader, "Usagi/1");

QNetworkReply *reply = manager->get(request);
```

### 5. Save to Temp
```cpp
QString tempPath = QDir::tempPath() + "/mylist_export_" + 
    QString::number(QDateTime::currentMSecsSinceEpoch()) + ".tgz";

QFile file(tempPath);
file.write(reply->readAll());
```

### 6. Parse and Import
```cpp
int count = parseMylistCSVAdborg(tempPath);

if(count > 0)
{
    logOutput->append(QString("Successfully imported %1 entries").arg(count));
    loadMylistFromDatabase();  // Refresh UI
}
```

### 7. Cleanup
```cpp
QFile::remove(tempPath);  // Remove temporary file
reply->deleteLater();
manager->deleteLater();
```

## Status Label Updates

The status label provides real-time feedback:

| Stage | Status Text |
|-------|-------------|
| Notification received | "MyList Status: Downloading export..." |
| Download complete | "MyList Status: Parsing export..." |
| Import successful | "MyList Status: X entries loaded" |
| Download failed | "MyList Status: Download failed" |
| Parse failed | "MyList Status: Import failed" |

## User Workflow

### From User's Perspective

1. **User logs into application**
   - Notifications automatically enabled

2. **User requests export on AniDB website**
   - Go to https://anidb.net/perl-bin/animedb.pl?show=mylist
   - Click "Export"
   - Select "csv-adborg" template
   - Click "Export"

3. **AniDB generates export**
   - Takes a few seconds to minutes depending on list size

4. **AniDB sends notification**
   - Notification contains download link

5. **Application automatically:**
   - Receives notification via UDP
   - Extracts download URL
   - Downloads tar.gz file
   - Parses csv-adborg format
   - Imports to database
   - Refreshes display
   - Cleans up temp file

6. **User sees updated mylist**
   - No manual intervention required!

## Technical Details

### Signal/Slot Architecture

```cpp
// AniDB API
signals:
    void notifyMessageReceived(int nid, QString message);

// Window
connect(adbapi, SIGNAL(notifyMessageReceived(int,QString)), 
        this, SLOT(getNotifyMessageReceived(int,QString)));
```

### URL Regex Pattern
```cpp
QRegularExpression urlRegex("https?://[^\\s]+\\.tgz");
```

Matches:
- `http://anidb.net/export/file.tgz`
- `https://anidb.net/mylist-export/12345.tgz`
- Any URL ending in `.tgz`

### Error Handling

**Download Errors**:
```cpp
if(reply->error() == QNetworkReply::NoError)
{
    // Success
}
else
{
    logOutput->append(QString("Error: %1").arg(reply->errorString()));
    mylistStatusLabel->setText("MyList Status: Download failed");
}
```

**Parse Errors**:
```cpp
int count = parseMylistCSVAdborg(tempPath);
if(count > 0)
{
    // Success
}
else
{
    logOutput->append("No entries imported");
    mylistStatusLabel->setText("MyList Status: Import failed");
}
```

**File Save Errors**:
```cpp
if(file.open(QIODevice::WriteOnly))
{
    // Success
}
else
{
    logOutput->append("Error: Cannot save export file");
}
```

## Benefits

### For Users
- ✅ **Zero manual steps** after export request
- ✅ **Real-time feedback** via status label
- ✅ **No file management** - automatic cleanup
- ✅ **Immediate results** - UI updates automatically

### For Developers
- ✅ **Minimal code** - ~100 lines total
- ✅ **Reuses existing parser** - no duplication
- ✅ **Clean architecture** - signal/slot pattern
- ✅ **Error resilient** - handles failures gracefully

## Testing

### Manual Test Steps

1. **Enable Notifications**
   ```
   - Login to application
   - Check logs: "Notifications enabled"
   ```

2. **Request Export**
   ```
   - Go to AniDB mylist page
   - Click Export
   - Select csv-adborg
   - Click Export button
   ```

3. **Wait for Notification**
   ```
   - AniDB sends notification (usually < 1 minute)
   - Check logs: "Notification {nid} received"
   ```

4. **Verify Automatic Import**
   ```
   - Check logs: "MyList export link found: {url}"
   - Check logs: "Export downloaded to: {path}"
   - Check logs: "Successfully imported {count} entries"
   - Check status label: "MyList Status: {count} entries loaded"
   - Check mylist tab: entries displayed
   ```

5. **Verify Cleanup**
   ```
   - Temp file removed automatically
   - No .tgz files in temp directory
   ```

### Debug Logging

All steps logged to log output:
```
Notification {nid} received
MyList export link found: {url}
Export downloaded to: {path}
Successfully imported {count} mylist entries
```

## Limitations

1. **URL Pattern**: Assumes .tgz extension in notification
2. **Single Download**: Only one concurrent download supported
3. **No Retry**: Failed downloads not automatically retried
4. **No Progress**: Download progress not shown (could add QProgressDialog)
5. **HTTP Only**: Uses HTTP, not authenticated download (assumes public link)

## Future Enhancements

### Possible Improvements

1. **Download Progress**
   ```cpp
   connect(reply, &QNetworkReply::downloadProgress, 
           this, &Window::updateDownloadProgress);
   ```

2. **Retry Logic**
   ```cpp
   if(downloadFailed && retryCount < 3) {
       retryDownload();
   }
   ```

3. **Queue Multiple Notifications**
   ```cpp
   QQueue<Notification> notificationQueue;
   ```

4. **Authenticated Download**
   ```cpp
   request.setRawHeader("Cookie", sessionCookie);
   ```

5. **Background Processing**
   ```cpp
   QThread *workerThread = new QThread();
   // Move download/parse to thread
   ```

## Code Files Changed

### anidbapi.h
- Added `NotifyEnable()` declaration
- Added `PushAck(int nid)` declaration
- Added `notifyMessageReceived(int, QString)` signal

### anidbapi.cpp
- Implemented `NotifyEnable()` method
- Implemented `PushAck()` method
- Added notification code handling (270-272, 290-291)
- Added `emit notifyMessageReceived()` in ParseMessage()

### window.h
- Added QNetworkAccessManager includes
- Added `getNotifyMessageReceived(int, QString)` slot

### window.cpp
- Connected notification signal to slot
- Implemented `getNotifyMessageReceived()` handler
- Added notification enable on login
- Added automatic download/import logic

## Commit
- Hash: 7bc9d1e
- Files changed: 4 (anidbapi.h, anidbapi.cpp, window.h, window.cpp)
- Lines added: ~160

## Status
✅ Implementation complete
⏳ Manual testing required (Qt environment)
🎯 Ready for automated mylist import via notifications
