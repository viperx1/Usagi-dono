# Window Class SOLID Analysis Report

**Date:** 2025-12-08  
**Analysis By:** GitHub Copilot  
**Status:** COMPREHENSIVE ANALYSIS COMPLETE

---

## Executive Summary

This document provides a detailed analysis of the Window class focusing on:
1. **SOLID principle violations**
2. **Code duplication with other classes**
3. **Concrete recommendations for improvements**

### Key Findings

| Finding | Severity | Impact |
|---------|----------|--------|
| Single Responsibility Principle Violation | 🔴 CRITICAL | Window has 8 distinct responsibilities |
| Code Duplication (Directory Walking) | 🟠 HIGH | Duplicate code in Button2Click/Button3Click |
| Code Duplication (Worker Pattern) | 🟡 MEDIUM | 3 worker classes with identical database patterns |
| Insufficient Delegation to Managers | 🟠 HIGH | Window still manages too much UI logic |

---

## Current State

### Size and Complexity

```
Window Class Metrics:
- Implementation: 6,104 lines (window.cpp)
- Header: 498 lines (window.h)
- Total: 6,602 lines
- Public/Private Methods: ~97
- Member Variables: ~90+
- Dependencies: 58 includes in header
```

### Existing Manager Classes

Window delegates **some** responsibilities to manager classes:
- ✅ `MyListCardManager` - Manages anime card lifecycle and updates
- ✅ `PlaybackManager` - Handles media player integration
- ✅ `WatchSessionManager` - Manages watch session tracking
- ✅ `DirectoryWatcher` - Monitors directories for new files
- ✅ `MyListFilterSidebar` - Filter UI and state
- ⚠️ **But Window still contains too much coordinator logic**

---

## SOLID Principle Violations

### 1. Single Responsibility Principle (SRP) 🔴 CRITICAL

**Violation:** Window class has **8 distinct responsibilities**

#### Responsibility 1: Hasher Management (~800 lines)

**Methods:**
- `Button1Click()` - Add files to hasher
- `Button2Click()` - Add directories to hasher  
- `Button3Click()` - Add last directory to hasher
- `ButtonHasherStartClick()` - Start hashing
- `ButtonHasherStopClick()` - Stop hashing
- `ButtonHasherClearClick()` - Clear hasher queue
- `getNotifyFileHashed()` - Process hashed file results
- `provideNextFileToHash()` - Provide files to hasher threads
- `setupHashingProgress()` - Initialize progress tracking
- `processPendingHashedFiles()` - Process hashed files in batches
- `hashesinsertrow()` - Add file to hasher table
- `shouldFilterFile()` - Check if file matches filter masks

**Issues:**
- Direct UI manipulation (QTableWidget)
- Direct thread pool interaction
- File system operations
- Progress tracking
- Database updates
- All mixed in Window class

**Recommendation:** Extract to `HasherCoordinator` class
- Owns hasher UI widgets
- Manages hasher thread pool
- Handles file selection and filtering
- Coordinates progress updates
- Window becomes observer of hasher events

#### Responsibility 2: MyList Card View Management (~1200 lines)

**Methods:**
- `loadMylistAsCards()` - Load entire mylist as cards
- `sortMylistCards()` - Sort cards by criteria
- `applyMylistFilters()` - Apply filter to cards
- `onCardClicked()` - Handle card click
- `onCardEpisodeClicked()` - Handle episode click on card
- `onPlayAnimeFromCard()` - Play anime from card
- `checkAndRequestChainRelations()` - Request missing relation data
- `onMylistLoadingFinished()` - Handle background loading completion
- `onMylistSortChanged()` - Handle sort change
- `saveMylistSorting()` - Save sort preferences
- `restoreMylistSorting()` - Restore sort preferences
- `onToggleFilterBarClicked()` - Toggle filter sidebar

**Issues:**
- Window still handles card view coordination
- MyListCardManager exists but Window controls it
- Filter logic split between Window and FilterSidebar
- Background loading workers in Window class
- Too much UI coordination in Window

**Recommendation:** Extract to `MyListViewController` class
- Owns scroll area and card container
- Coordinates with MyListCardManager
- Handles filter sidebar integration
- Manages background loading
- Window becomes observer of view events

#### Responsibility 3: Unknown Files Management (~400 lines)

**Methods:**
- `unknownFilesInsertRow()` - Add unknown file to table
- `loadUnboundFiles()` - Load unbound files from database
- `onUnknownFileAnimeSearchChanged()` - Handle anime search
- `onUnknownFileEpisodeSelected()` - Handle episode selection
- `onUnknownFileBindClicked()` - Bind file to episode
- `onUnknownFileNotAnimeClicked()` - Mark as not anime
- `onUnknownFileRecheckClicked()` - Recheck file in AniDB
- `onUnknownFileDeleteClicked()` - Delete unknown file
- `onUnboundFilesLoadingFinished()` - Handle loading completion

**Issues:**
- Direct QTableWidget manipulation
- Database queries in Window
- Complex binding logic in Window
- AniDB API calls from Window

**Recommendation:** Extract to `UnknownFilesManager` class
- Owns unknown files table widget
- Encapsulates all unknown file operations
- Coordinates with AniDB API
- Window becomes observer of binding events

#### Responsibility 4: Playback Management (~300 lines)

**Methods:**
- `onPlayButtonClicked()` - Handle play button click
- `onPlaybackPositionUpdated()` - Update playback position
- `onPlaybackCompleted()` - Handle playback completion
- `onPlaybackStopped()` - Handle playback stop
- `onPlaybackStateChanged()` - Update UI on state change
- `onFileMarkedAsLocallyWatched()` - Mark file as watched
- `onAnimationTimerTimeout()` - Animate play buttons
- `onResetWatchSession()` - Reset watch session

**Issues:**
- PlaybackManager exists but Window still has UI coordination
- Animation logic in Window
- Watch status updates in Window
- Database updates triggered from Window

**Recommendation:** Extract to `PlaybackController` class
- Owns play button animation
- Coordinates PlaybackManager with UI
- Handles watch status updates
- Window becomes observer of playback events

#### Responsibility 5: System Tray Management (~200 lines)

**Methods:**
- `createTrayIcon()` - Create system tray icon
- `onTrayIconActivated()` - Handle tray icon activation
- `onTrayShowHideAction()` - Show/hide window from tray
- `onTrayExitAction()` - Exit from tray menu
- `updateTrayIconVisibility()` - Update tray icon visibility
- `loadTraySettings()` - Load tray preferences
- `saveTraySettings()` - Save tray preferences
- `loadUsagiIcon()` - Load application icon

**Issues:**
- Tray logic mixed with Window lifecycle
- Settings access from Window
- Icon loading utility in Window

**Recommendation:** Extract to `TrayIconManager` class
- Owns QSystemTrayIcon
- Manages tray settings
- Provides show/hide/exit actions
- Window observes tray events

#### Responsibility 6: Settings Management (~200 lines)

**Methods:**
- `saveSettings()` - Save all application settings
- `onWatcherEnabledChanged()` - Handle watcher setting change
- `onWatcherBrowseClicked()` - Browse for watched directory
- `onMediaPlayerBrowseClicked()` - Browse for media player
- `setAutoStartEnabled()` - Enable/disable autostart
- `registerAutoStart()` - Register with OS autostart
- `unregisterAutoStart()` - Unregister from OS autostart

**Issues:**
- ApplicationSettings class exists but Window creates UI
- Settings page UI hardcoded in Window
- Platform-specific autostart logic in Window
- File dialog operations in Window

**Recommendation:** Extract to `SettingsController` class
- Owns settings page widgets
- Manages settings UI
- Handles platform-specific features
- Window becomes container for settings page

#### Responsibility 7: API Integration (~500 lines)

**Methods:**
- `ButtonLoginClick()` - Handle login button
- `getNotifyLoggedIn()` - Handle login notification
- `getNotifyLoggedOut()` - Handle logout notification
- `getNotifyMylistAdd()` - Handle mylist add notification
- `getNotifyMessageReceived()` - Handle AniDB messages
- `getNotifyCheckStarting()` - Handle notification check start
- `getNotifyExportQueued()` - Handle export queued
- `getNotifyEpisodeUpdated()` - Handle episode update
- `getNotifyAnimeUpdated()` - Handle anime update
- `requestMylistExportManually()` - Request mylist export
- `parseMylistExport()` - Parse exported mylist XML

**Issues:**
- Direct AniDB API interaction
- Complex XML parsing in Window
- Database updates from notifications
- UI updates from API events

**Recommendation:** Keep in Window (main coordinator)
- Window is the main application coordinator
- Should delegate work but can coordinate API events
- Simplify by extracting parsing logic to separate utility
- Move database updates to appropriate managers

#### Responsibility 8: UI Layout and Initialization (~400 lines)

**Methods:**
- `Window()` - Constructor creates entire UI
- `startupInitialization()` - Startup logic
- `closeEvent()` - Handle window close
- `changeEvent()` - Handle window state changes
- `startBackgroundLoading()` - Start background workers
- `onAnimeTitlesLoadingFinished()` - Handle titles loading
- `safeClose()` - Safe shutdown
- `shot()` - Screenshot functionality

**Issues:**
- Massive constructor (creates all UI)
- Complex initialization sequence
- Background loading logic in Window
- Event handlers in Window

**Recommendation:** Keep in Window (it IS the window)
- This is Window's core responsibility
- But can extract background loading to separate class
- Constructor can delegate UI creation to helper methods

---

### 2. Open/Closed Principle (OCP) 🟠 HIGH

**Violation:** Adding new features requires modifying Window class

**Examples:**
- Adding new hasher feature → modify Window
- Adding new filter type → modify Window
- Adding new playback feature → modify Window
- Adding new API notification → modify Window

**Impact:**
- High risk of breaking existing functionality
- Difficult to test new features in isolation
- Merge conflicts when multiple features developed

**Recommendation:**
- Extract responsibilities to focused classes
- Use observer pattern for event communication
- Plugin architecture for extensibility

---

### 3. Liskov Substitution Principle (LSP) ✅ OK

**Status:** No violations detected

Window inherits from QWidget correctly and doesn't violate LSP.

---

### 4. Interface Segregation Principle (ISP) 🟡 MEDIUM

**Violation:** Window has many unrelated public slots

**Issue:**
Window has 40+ public slots covering different responsibilities:
- Hasher slots
- MyList slots  
- Playback slots
- Settings slots
- API slots
- UI slots

**Impact:**
- Unclear interface
- Difficult to understand what Window does
- Hard to mock for testing

**Recommendation:**
- Extract slots into focused interfaces
- Each manager class provides its own interface
- Window becomes thin coordinator

---

### 5. Dependency Inversion Principle (DIP) 🟡 MEDIUM

**Violation:** Window depends on concrete implementations

**Examples:**
```cpp
// Window.h - Direct dependencies on concrete classes
MyListCardManager *cardManager;
PlaybackManager *playbackManager;
WatchSessionManager *watchSessionManager;
DirectoryWatcher *directoryWatcher;
```

**Issue:**
- Hard to test Window in isolation
- Can't substitute implementations
- Tight coupling to concrete classes

**Impact:**
- Difficult unit testing
- Hard to change implementations
- Inflexible design

**Recommendation:**
- Define interfaces for major dependencies
- Inject dependencies via constructor
- Use dependency injection container

---

## Code Duplication Analysis

### 1. Directory Walking Pattern 🟠 HIGH PRIORITY

**Location 1:** `Button2Click()` lines 1445-1456
**Location 2:** `Button3Click()` lines 1472-1483

**Duplicate Code:**
```cpp
QDirIterator directory_walker(files.first(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
files.pop_front();
while(directory_walker.hasNext())
{
    QFileInfo file = QFileInfo(directory_walker.next());
    
    if (!shouldFilterFile(file.absoluteFilePath())) {
        hashesinsertrow(file, renameto->checkState());
    }
}
```

**Analysis:**
- Exact same logic repeated
- 12 lines duplicated
- Both methods iterate directories and add files

**Recommendation:** Extract method
```cpp
private:
    void addFilesFromDirectory(const QString &dirPath);
```

**Benefit:**
- DRY principle
- Single place to maintain
- Easier to test

---

### 2. Worker Thread Pattern 🟡 MEDIUM PRIORITY

**Location 1:** `MylistLoaderWorker::doWork()` lines 38-80  
**Location 2:** `AnimeTitlesLoaderWorker::doWork()` lines 82-120  
**Location 3:** `UnboundFilesLoaderWorker::doWork()` (similar pattern)

**Common Pattern:**
```cpp
void Worker::doWork()
{
    // 1. Create thread-specific database connection
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "ThreadName");
    db.setDatabaseName(m_dbName);
    
    // 2. Check if database opened
    if (!db.open()) {
        QSqlDatabase::removeDatabase("ThreadName");
        emit finished(DefaultValue);
        return;
    }
    
    // 3. Execute query
    QSqlQuery q(db);
    // ... query execution ...
    
    // 4. Close and cleanup
    db.close();
    QSqlDatabase::removeDatabase("ThreadName");
    
    // 5. Emit results
    emit finished(results);
}
```

**Analysis:**
- Pattern repeated 3 times
- ~40 lines of boilerplate each
- Error handling duplicated
- Connection management duplicated

**Recommendation:** Create base class with template method pattern

```cpp
template<typename ResultType>
class BackgroundDatabaseWorker : public QObject
{
    Q_OBJECT
protected:
    QString m_dbName;
    QString m_connectionName;
    
    // Template method
    void doWork() final;
    
    // Subclasses implement this
    virtual ResultType executeQuery(QSqlDatabase &db) = 0;
    
signals:
    void finished(const ResultType &result);
};
```

**Benefit:**
- DRY principle
- Consistent error handling
- Single place for connection management
- Type-safe results

---

### 3. UI Update Guard Pattern 🟢 LOW PRIORITY

**Locations:** Multiple methods
- `Button2Click()` - lines 1419, 1459
- `Button3Click()` - lines 1464, 1485
- `ButtonHasherClearClick()` - similar pattern

**Pattern:**
```cpp
void SomeMethod()
{
    hashes->setUpdatesEnabled(0);
    // ... do work ...
    hashes->setUpdatesEnabled(1);
}
```

**Issue:**
- If exception thrown, updates remain disabled
- Easy to forget to re-enable
- Manual management error-prone

**Recommendation:** RAII guard class

```cpp
class WidgetUpdateGuard
{
    QWidget *m_widget;
public:
    explicit WidgetUpdateGuard(QWidget *widget) : m_widget(widget) {
        if (m_widget) m_widget->setUpdatesEnabled(false);
    }
    ~WidgetUpdateGuard() {
        if (m_widget) m_widget->setUpdatesEnabled(true);
    }
};

// Usage
void Button2Click()
{
    WidgetUpdateGuard guard(hashes);
    // ... do work ...
    // Updates automatically re-enabled on scope exit
}
```

**Benefit:**
- Exception-safe
- Automatic cleanup
- Cannot forget to re-enable

---

## Code Duplication with Other Classes

### Window vs MyListCardManager

**Overlap:** Poster download and card management

**Status:** ✅ **FIXED** (2025-12-06)
- Poster download duplication removed from Window
- 171 lines eliminated
- Window now properly delegates to MyListCardManager

**Remaining Issue:**
Window still has too much card view coordination logic that should be in a view controller.

---

### Window vs HasherThreadPool

**Overlap:** Progress tracking and file distribution

**Status:** ⚠️ **PARTIAL SEPARATION**

**Window has:**
- Progress bar updates
- File request coordination
- Hashing completion handling

**HasherThreadPool has:**
- Thread management
- Hash computation
- File processing

**Issue:**
- Coordination logic mixed in Window
- Progress tracking split between Window and ProgressTracker
- File distribution logic in Window

**Recommendation:**
- Create HasherCoordinator to own coordination
- Use ProgressTracker consistently
- Window becomes observer only

---

## Detailed Recommendations

### Priority 1: HIGH - Extract Hasher Responsibility

**Create:** `HasherCoordinator` class

**Responsibilities:**
- Own hasher UI widgets (table, buttons, progress bars)
- Coordinate with HasherThreadPool
- Manage file selection and filtering
- Track progress
- Handle hashing results

**Interface:**
```cpp
class HasherCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit HasherCoordinator(QWidget *parent = nullptr);
    
    // UI getters (for Window to add to layout)
    QWidget* getHasherWidget() const;
    
    // Actions
    void addFiles();
    void addDirectories();
    void addLastDirectory();
    void startHashing();
    void stopHashing();
    void clearQueue();
    
signals:
    void fileHashed(const QString &filePath, const QString &hash);
    void progressUpdated(int current, int total);
    void hashingFinished();
    
private:
    // Own all hasher UI widgets
    hashes_ *m_hashTable;
    QPushButton *m_btnAddFiles;
    // ... etc
};
```

**Benefits:**
- ~800 lines removed from Window
- Hasher becomes testable unit
- Clear single responsibility
- Easy to extend with new features

**Effort:** ~20-30 hours

---

### Priority 2: HIGH - Extract MyList View Responsibility

**Create:** `MyListViewController` class

**Responsibilities:**
- Own mylist view widgets (scroll area, filter sidebar)
- Coordinate MyListCardManager
- Manage background loading
- Handle view events

**Interface:**
```cpp
class MyListViewController : public QObject
{
    Q_OBJECT
public:
    explicit MyListViewController(QWidget *parent = nullptr);
    
    // UI getters
    QWidget* getViewWidget() const;
    
    // View operations
    void loadMylist();
    void applyFilters();
    void sortCards(int criteria, bool ascending);
    void toggleFilterBar();
    
signals:
    void cardClicked(int aid);
    void episodeClicked(int lid);
    void playAnimeRequested(int aid);
    
private:
    MyListCardManager *m_cardManager;
    MyListFilterSidebar *m_filterSidebar;
    // ... etc
};
```

**Benefits:**
- ~1000 lines removed from Window
- View becomes reusable component
- Clear separation of concerns
- Easier to add new view types

**Effort:** ~30-40 hours

---

### Priority 3: MEDIUM - Extract Unknown Files Responsibility

**Create:** `UnknownFilesManager` class

**Responsibilities:**
- Own unknown files table
- Manage anime search and binding
- Coordinate with AniDB API
- Handle file operations

**Benefits:**
- ~400 lines removed from Window
- Clear binding logic
- Testable in isolation

**Effort:** ~15-20 hours

---

### Priority 4: MEDIUM - Extract Playback UI Responsibility

**Create:** `PlaybackController` class

**Responsibilities:**
- Coordinate PlaybackManager with UI
- Manage play button animations
- Handle watch status updates

**Benefits:**
- ~300 lines removed from Window
- Cleaner playback logic
- UI/logic separation

**Effort:** ~10-15 hours

---

### Priority 5: LOW - Extract System Tray

**Create:** `TrayIconManager` class

**Benefits:**
- ~200 lines removed from Window
- Reusable tray functionality

**Effort:** ~8-10 hours

---

### Priority 6: LOW - Extract Settings Controller

**Create:** `SettingsController` class

**Benefits:**
- ~200 lines removed from Window
- Settings page as component

**Effort:** ~8-10 hours

---

## Impact Analysis

### Before Refactoring

```
Window Class:
- 6,104 lines
- 8 responsibilities
- 97 methods
- Difficult to test
- High coupling
- Merge conflicts frequent
- Bug fixes risky
```

### After Refactoring (Estimated)

```
Window Class:
- ~2,000 lines (main coordinator)
- 2 responsibilities (UI layout, API coordination)
- ~30 methods
- Easy to test
- Low coupling
- Merge conflicts rare
- Bug fixes isolated

New Classes:
- HasherCoordinator: ~800 lines
- MyListViewController: ~1,000 lines
- UnknownFilesManager: ~400 lines
- PlaybackController: ~300 lines
- TrayIconManager: ~200 lines
- SettingsController: ~200 lines
- BackgroundDatabaseWorker: ~100 lines (base class)
```

**Total Code:**
- Before: 6,104 lines in one class
- After: ~5,000 lines across 7 classes (reduction due to refactoring)

---

## Implementation Strategy

### Phase 1: Quick Wins (8 hours)

1. ✅ Extract `addFilesFromDirectory()` method (1 hour)
2. ✅ Create `WidgetUpdateGuard` class (1 hour)
3. ✅ Create `BackgroundDatabaseWorker` base class (3 hours)
4. ✅ Refactor worker classes to use base (3 hours)

**Impact:** Remove code duplication, improve maintainability

---

### Phase 2: Hasher Extraction (20-30 hours)

1. Create HasherCoordinator class skeleton (4 hours)
2. Move hasher UI widgets to coordinator (4 hours)
3. Move hasher methods to coordinator (6 hours)
4. Update Window to use coordinator (4 hours)
5. Test and fix issues (4 hours)
6. Update documentation (2 hours)

**Impact:** 800 lines removed from Window

---

### Phase 3: MyList View Extraction (30-40 hours)

1. Create MyListViewController class skeleton (4 hours)
2. Move view widgets to controller (4 hours)
3. Move background loading to controller (6 hours)
4. Move filter coordination to controller (6 hours)
5. Move card event handlers to controller (6 hours)
6. Update Window to use controller (4 hours)
7. Test and fix issues (6 hours)
8. Update documentation (4 hours)

**Impact:** 1000 lines removed from Window

---

### Phase 4: Remaining Extractions (40-60 hours)

1. Extract UnknownFilesManager (15-20 hours)
2. Extract PlaybackController (10-15 hours)
3. Extract TrayIconManager (8-10 hours)
4. Extract SettingsController (8-10 hours)

**Impact:** 1100 lines removed from Window

---

## Testing Strategy

### Unit Tests Required

For each extracted class:
1. **Initialization tests** - Verify proper setup
2. **Action tests** - Verify each public method
3. **Signal tests** - Verify signals emitted correctly
4. **Edge case tests** - Verify error handling

### Integration Tests Required

1. **Window + Coordinators** - Verify proper integration
2. **End-to-end workflows** - Verify user scenarios work
3. **Performance tests** - Verify no regression

---

## Success Criteria

### Measurable Goals

1. ✅ Window class < 2,500 lines
2. ✅ Window class < 5 responsibilities
3. ✅ Window class < 40 methods
4. ✅ Zero code duplication (detected by tools)
5. ✅ 80%+ test coverage for extracted classes
6. ✅ Zero regression bugs
7. ✅ Build time unchanged or improved
8. ✅ All clazy warnings addressed

---

## Conclusion

The Window class currently violates the Single Responsibility Principle severely with 8 distinct responsibilities mixed into one 6,000+ line file. This analysis has identified:

### Critical Issues

1. **SRP Violation** - 8 responsibilities in one class
2. **Code Duplication** - Multiple patterns duplicated
3. **Insufficient Delegation** - Manager classes exist but underutilized
4. **Poor Testability** - Difficult to test in isolation

### Recommendations

**Immediate (Phase 1):**
- ✅ Extract duplicate directory walking code
- ✅ Create RAII update guard
- ✅ Create BackgroundDatabaseWorker base class

**High Priority (Phases 2-3):**
- ⚠️ Extract HasherCoordinator (~800 lines)
- ⚠️ Extract MyListViewController (~1000 lines)

**Medium Priority (Phase 4):**
- ⚠️ Extract UnknownFilesManager (~400 lines)
- ⚠️ Extract PlaybackController (~300 lines)

**Low Priority (Phase 4):**
- ⚠️ Extract TrayIconManager (~200 lines)
- ⚠️ Extract SettingsController (~200 lines)

### Expected Outcome

After completing all phases:
- Window: ~2,000 lines (67% reduction)
- 7 new focused classes
- Clear separation of concerns
- High testability
- Easy maintainability
- SOLID compliance

### Next Steps

This analysis document should be reviewed with the development team to:
1. Prioritize which extractions to tackle first
2. Allocate resources and timeline
3. Begin Phase 1 (quick wins) immediately
4. Plan Phases 2-4 based on priority and resources

---

**Analysis Status:** ✅ COMPLETE  
**Document Version:** 1.0  
**Last Updated:** 2025-12-08
