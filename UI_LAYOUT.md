# MyList Tab UI Layout

## Current Layout (Card View)

```
+------------------------------------------------------------------------------+
|                              MyList Tab                                      |
+------------------------------------------------------------------------------+
|  [Load MyList]  [Fetch Stats]  [Import]  [Sort ▼]  [Search: ______]        |
+------------------------------------------------------------------------------+
| +-----------+  +-----------------------------------------------------------+ |
| | FILTER    |  |                    Card View Area                         | |
| | SIDEBAR   |  |                                                           | |
| |           |  |  +----------------------------------------------------+   | |
| | Status:   |  |  | Cowboy Bebop                          [▼] [Play ▶] |   | |
| | [✓] All   |  |  | Type: TV Series | Episodes: 26 | Score: 8.76       |   | |
| | [ ] Watch |  |  | Progress: 20/26 episodes                           |   | |
| | [ ] Compl |  |  | +------------------------------------------------+ |   | |
| |           |  |  | | Ep 21: Boogie Woogie Feng Shui   [▶] [✓]     | |   | |
| | Type:     |  |  | | Ep 22: Cowboy Funk               [▶] [ ]     | |   | |
| | [✓] All   |  |  | +------------------------------------------------+ |   | |
| | [ ] TV    |  |  +----------------------------------------------------+   | |
| | [ ] OVA   |  |                                                           | |
| | [ ] Movie |  |  +----------------------------------------------------+   | |
| |           |  |  | Ghost in the Shell: SAC           [▼] [Play ▶] |   | |
| | Sort:     |  |  | Type: TV Series | Episodes: 26 | Score: 8.50       |   | |
| | • Title   |  |  | Progress: 26/26 episodes (Complete)                |   | |
| | ○ Rating  |  |  | Collapsed view - click ▼ to expand episodes        |   | |
| | ○ Year    |  |  +----------------------------------------------------+   | |
| |           |  |                                                           | |
| +-----------+  +-----------------------------------------------------------+ |
+------------------------------------------------------------------------------+
```

## Main Components

### Top Toolbar
- **Load MyList**: Loads data from SQLite database into card view
- **Fetch Stats**: Gets statistics from AniDB API (total entries, watched count, etc.)
- **Import**: Opens file dialog to import MyList from XML/CSV export
- **Sort**: Dropdown to sort cards (by title, rating, year, etc.)
- **Search**: Filter cards by anime title

### Left Sidebar (Filter Panel)
Collapsible sidebar with filtering options:
- **Status Filters**: All, Currently Watching, Completed, On Hold, Dropped, Plan to Watch
- **Type Filters**: All, TV Series, OVA, Movie, Special, ONA, Music Video
- **Sort Options**: Title (A-Z), Rating (High-Low), Year (Recent-Old), Episodes (Most-Least)
- **Additional Filters**: 
  - Has Files checkbox
  - Has Unwatched Episodes checkbox
  - Series chains checkbox

### Card View Area (Main Content)
Scrollable area displaying anime cards in a vertical list:

#### Anime Card (Collapsed)
- **Title**: Anime name with preferred language
- **Metadata**: Type, episode count, AniDB rating
- **Progress**: Episodes watched / total episodes
- **Expand Button**: Click to show episode list
- **Play Button**: Plays next unwatched episode (or first episode if all watched)

#### Anime Card (Expanded)
Shows all episodes in the anime:
- **Episode List**: Each episode shows:
  - Episode number and title
  - Play button (▶) for unwatched, checkmark (✓) for watched
  - File state indicator (HDD, CD/DVD, Deleted)
- **Episode Tree**: Groups episodes by type (Normal, Special, OP/ED, etc.)
- **Actions**: Mark watched, delete file, download (if available)

## User Workflow

### Initial Setup
1. Click **Load MyList** to load from database (if previously imported)
2. If empty, click **Fetch Stats** to get overview from AniDB
3. Export MyList from https://anidb.net (MyList → Export)
4. Click **Import** and select the downloaded XML/CSV file
5. Cards appear in the main view showing all anime

### Regular Use
- **Browse**: Scroll through anime cards
- **Filter**: Use sidebar to narrow down selection
- **Search**: Type in search box to find specific anime
- **Expand**: Click ▼ on a card to see all episodes
- **Play**: Click play button to watch episodes
- **Track Progress**: Watch status updates automatically after playback

## Card View Features

### Visual Design
- **Modern Card Layout**: Clean, card-based interface instead of tree view
- **Virtual Scrolling**: Efficient rendering of large anime collections
- **Responsive Layout**: Cards adapt to window size
- **Color Coding**: Visual indicators for watch status
- **Expandable Details**: Click to expand/collapse episode lists

### Episode Display
Each episode in an expanded card shows:
- **Episode Number**: Type-specific numbering (S1, T1, etc.)
- **Title**: Episode title from AniDB (in selected language)
- **Status**: Play icon (▶) or watched checkmark (✓)
- **File Info**: State (HDD/CD/Deleted) and storage path
- **Actions**: Quick access to play, mark watched, or delete

### Filtering and Sorting
- **Real-time Filtering**: Cards update instantly when filters change
- **Multiple Sort Options**: By title, rating, year, episode count
- **Search**: Text-based search across anime titles
- **Status-based Filters**: Show only watching, completed, etc.
- **Type Filters**: TV, OVA, Movie, Special

### Watch Session Integration
- **Series Chains**: Tracks sequel relationships between anime
- **Continue Watching**: Automatically selects next episode to watch
- **Progress Tracking**: Shows X/Y episodes watched
- **Session Reset**: Reset watch session to start from beginning

## Technical Implementation

### Classes
- **MyListCardManager**: Manages card creation, filtering, and data
- **AnimeCard**: Individual anime card widget with episode list
- **MyListFilterSidebar**: Filter controls and settings
- **VirtualCardLayout**: Efficient layout for large card collections
- **WatchSessionManager**: Tracks watch sessions and series chains

### Database
Cards display data from:
- **mylist** table: Core mylist entries
- **anime** table: Anime metadata (title, type, episodes)
- **episode** table: Episode information
- **file** table: File details and paths
- **watch_sessions** table: Watch progress tracking
