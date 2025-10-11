# MyList Tab UI Layout

## Before Changes
```
+----------------------------------------------------------+
|                      MyList Tab                          |
+----------------------------------------------------------+
|                                                          |
|  [Load MyList from Database]                             |
|                                                          |
|  +----------------------------------------------------+  |
|  |                                                    |  |
|  |           (Empty Tree View)                       |  |
|  |        No entries to display                      |  |
|  |                                                    |  |
|  +----------------------------------------------------+  |
|                                                          |
+----------------------------------------------------------+
```

## After Changes
```
+----------------------------------------------------------+
|                      MyList Tab                          |
+----------------------------------------------------------+
|                                                          |
|  +----------------------------------------------------+  |
|  | [Load MyList from Database] [Fetch MyList Stats]  |  |
|  |                             [Import MyList]        |  |
|  +----------------------------------------------------+  |
|                                                          |
|  +----------------------------------------------------+  |
|  | Anime Name            | Episode  | State | Viewed  |  |
|  +----------------------------------------------------+  |
|  | ▼ Cowboy Bebop                                     |  |
|  |     | Episode 1 - Asteroid Blues    | HDD  | Yes   |  |
|  |     | Episode 2 - Stray Dog Strut   | HDD  | Yes   |  |
|  |     | Episode 3 - Honky Tonk Women  | HDD  | No    |  |
|  | ▼ Ghost in the Shell: SAC                         |  |
|  |     | Episode 1 - Section 9        | HDD  | Yes   |  |
|  |     | Episode 2 - Proof of Justice | HDD  | No    |  |
|  +----------------------------------------------------+  |
|                                                          |
+----------------------------------------------------------+
```

## Button Functions

### 1. Load MyList from Database (Existing)
- Loads data from SQLite database
- Displays in hierarchical tree view
- Shows: Anime → Episodes
- Columns: Name, Episode, State, Viewed, Storage, LID

### 2. Fetch MyList Stats from API (NEW)
- Sends MYLISTSTAT command to AniDB UDP API
- Returns statistics:
  - Total entries
  - Watched count
  - Total file size
  - Watched file size
  - Percentages
- Displays in log output
- Provides instructions for full import

### 3. Import MyList from File (NEW)
- Opens file dialog
- Supports formats:
  - XML (*.xml)
  - CSV/TXT (*.csv, *.txt)
  - Auto-detect
- Parses and imports to database
- Shows success message with count
- Auto-refreshes tree view

## User Workflow

### Initial Setup (First Time Users)
```
1. Click [Fetch MyList Stats from API]
   → Log shows: "MYLISTSTAT command sent"
   → Log shows: "To get actual entries, export from anidb.net..."

2. Open browser → https://anidb.net
   → Login
   → Navigate to MyList
   → Click "Export"
   → Download mylist.xml or mylist.csv

3. Click [Import MyList from File]
   → Select downloaded file
   → Log shows: "Successfully imported 245 mylist entries"
   → Tree view auto-refreshes

4. View imported data
   → Tree shows all anime
   → Expand anime to see episodes
   → All data displayed correctly
```

### Regular Use
```
1. Click [Load MyList from Database]
   → Displays current mylist
   
2. After adding new entries (via hashing)
   → Click refresh to update display
   
3. To sync with AniDB
   → Export from AniDB
   → Import file
   → Database updated
```

## Import Dialog

```
+----------------------------------------------------------+
|                   Import MyList Export                   |
+----------------------------------------------------------+
|                                                          |
|  Look in: [Downloads                            ] [▼]   |
|                                                          |
|  +----------------------------------------------------+  |
|  | Name                           | Type      | Size  |  |
|  |----------------------------------------------------|  |
|  | mylist_export.xml             | XML File  | 250KB |  |
|  | mylist_export.csv             | CSV File  | 180KB |  |
|  | mylist_backup.txt             | Text File | 175KB |  |
|  +----------------------------------------------------+  |
|                                                          |
|  File name: [mylist_export.xml                       ]  |
|                                                          |
|  Files of type: [MyList Files (*.xml *.txt *.csv)]  [▼] |
|                                                          |
|                             [  Open  ]  [  Cancel  ]     |
+----------------------------------------------------------+
```

## Log Output Examples

### Fetch Stats
```
[2024-10-11 02:15:23] Fetching mylist statistics from API...
[2024-10-11 02:15:23] MYLISTSTAT command sent. Note: To get actual mylist entries, 
                      use 'Import MyList from File' with an export from 
                      https://anidb.net/perl-bin/animedb.pl?show=mylist&do=export
[2024-10-11 02:15:25] MYLISTSTAT: 245|180|1234567890|987654321|80|73|2150
```

### Import Success
```
[2024-10-11 02:16:45] Successfully imported 245 mylist entries
[2024-10-11 02:16:45] Loaded 245 mylist entries for 87 anime
```

### Import Error
```
[2024-10-11 02:17:12] Error: Cannot open file /path/to/nonexistent.xml
```

### Load Empty Database
```
[2024-10-11 02:18:00] Loaded 0 mylist entries for 0 anime
```

## Data Display Columns

```
+----------------+---------------------------+---------+---------+------------------+--------+
| Anime Name     | Episode                   | State   | Viewed  | Storage          | LID    |
+----------------+---------------------------+---------+---------+------------------+--------+
| Cowboy Bebop   | Episode 1 - Asteroid...  | HDD     | Yes     | /anime/cb01.mkv  | 123456 |
|                | Episode 2 - Stray Dog... | HDD     | Yes     | /anime/cb02.mkv  | 123457 |
|                | Episode 3 - Honky Ton... | HDD     | No      | /anime/cb03.mkv  | 123458 |
+----------------+---------------------------+---------+---------+------------------+--------+
```

## State Values

- **Unknown** - State 0
- **HDD** - State 1 (on hard disk)
- **CD/DVD** - State 2 (on optical media)
- **Deleted** - State 3 (removed/deleted)

## Viewed Values

- **Yes** - Has non-zero viewdate
- **No** - Empty or zero viewdate

## Tree View Features

- ✅ Hierarchical display (Anime → Episodes)
- ✅ Expandable/collapsible anime entries
- ✅ Sortable columns
- ✅ Alternating row colors
- ✅ No edit triggers (read-only display)
- ✅ Auto-expand after load
- ✅ Episode names when available
- ✅ Fallback to "Anime #[aid]" for missing names

## Keyboard Shortcuts

- **Space** - Expand/collapse selected item
- **Ctrl+A** - Select all
- **Arrow Keys** - Navigate tree
- **Home/End** - First/last item

## Context Menu (Future Enhancement)

Right-click on entry could show:
- View Details
- Mark as Watched/Unwatched
- Change State
- Edit Storage Location
- Remove from MyList
- Query AniDB for Updates

## Accessibility

- Clear button labels
- Helpful log messages
- Visual feedback on actions
- Error messages in log
- Success confirmation
- Progress indication
