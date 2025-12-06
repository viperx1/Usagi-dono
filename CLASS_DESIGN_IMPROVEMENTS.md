# Class Design Improvements

## Overview

This document describes the class design improvements made to the Usagi-dono codebase to better follow SOLID principles and improve maintainability, testability, and extensibility.

## Completed Improvements

### 1. ApplicationSettings Class

**Problem:** Settings were scattered throughout the `AniDBApi` class, violating the Single Responsibility Principle.

**Solution:** Created a dedicated `ApplicationSettings` class that:
- Encapsulates all application settings in one place
- Groups related settings into logical sub-structures
- Provides type-safe getters and setters
- Handles database persistence internally
- Auto-saves on each setter call

**Files:**
- `usagi/src/applicationsettings.h`
- `usagi/src/applicationsettings.cpp`

**Benefits:**
- **Single Responsibility:** Settings management is now isolated
- **Encapsulation:** Database access is hidden from users
- **Type Safety:** Proper types instead of all strings
- **Testability:** Easy to mock for unit tests
- **Maintainability:** All settings in one place

**Usage Example:**
```cpp
ApplicationSettings settings(database);
settings.load();

// Access grouped settings
settings.setUsername("user");
QString user = settings.getUsername();

// Access via struct groups
settings.watcher().enabled = true;
settings.tray().minimizeToTray = false;

settings.save();  // Or auto-saves on each setter
```

### 2. ProgressTracker Class

**Problem:** Progress tracking logic was scattered throughout the `Window` class with manual ETA calculations, timestamp management, and history tracking.

**Solution:** Created a reusable `ProgressTracker` class that:
- Encapsulates progress tracking state
- Calculates ETA using moving average
- Provides thread-safe operations
- Handles progress throttling
- Formats durations as human-readable strings

**Files:**
- `usagi/src/progresstracker.h`
- `usagi/src/progresstracker.cpp`

**Benefits:**
- **Reusability:** Can be used for any progress-based operation
- **Thread Safety:** All methods are mutex-protected
- **Simplicity:** Clean API hides complex ETA calculation
- **Accuracy:** Moving average provides stable ETA estimates
- **Testability:** Easy to unit test in isolation

**Usage Example:**
```cpp
ProgressTracker tracker(totalFiles);
tracker.start();

// In worker thread:
tracker.updateProgress(completedFiles);

// In UI update:
double percent = tracker.getProgressPercent();
QString eta = tracker.getETAString();  // "2m 15s"
```

### 3. LocalFileInfo Class

**Problem:** `UnboundFileData` struct was defined twice (once in window.h header, once nested in Window class). No validation or utility methods.

**Solution:** Created a proper `LocalFileInfo` class that:
- Single definition used across entire codebase
- Validates file paths and ED2K hashes
- Provides utility methods (isVideoFile, exists, extension, etc.)
- Type-safe with proper encapsulation
- Clear interface for file information

**Files:**
- `usagi/src/localfileinfo.h`
- `usagi/src/localfileinfo.cpp`

**Benefits:**
- **Single Source of Truth:** No duplicate definitions
- **Validation:** Hash and path validation built-in
- **Utility Methods:** Common operations readily available
- **Type Safety:** qint64 for size, proper QString usage
- **Extensibility:** Easy to add new file operations

**Usage Example:**
```cpp
LocalFileInfo fileInfo(filename, filepath, hash, size);

if (fileInfo.isValid() && fileInfo.hasHash()) {
    if (fileInfo.isVideoFile()) {
        QString ext = fileInfo.extension();
        QString dir = fileInfo.directory();
    }
}
```

## Analysis of Remaining Issues

### High Priority (Should be addressed next)

#### 1. Window Class - God Object
**Issue:** Window class has 790+ lines in header, mixes UI, business logic, and data management.

**Recommendation:** Extract the following classes:
- `MyListViewController` - Manages card view, filtering, sorting
- `HashingCoordinator` - Coordinates hasher threads and UI
- `PlaybackController` - Manages playback UI state
- `TrayIconManager` - System tray integration

#### 2. AniDB Response Data Structures
**Issue:** FileData, AnimeData, EpisodeData, GroupData are plain structs with no validation.

**Recommendation:** Convert to proper classes:
- `AniDBFileInfo` - Type-safe file data with validation
- `AniDBAnimeInfo` - Anime data with proper date/number types
- `AniDBEpisodeInfo` - Episode data with epno parsing
- `AniDBGroupInfo` - Group data

#### 3. Truncated Response Handling
**Issue:** Complex state machine logic mixed with AniDBApi.

**Recommendation:** Create `ResponseContinuation` class to manage multi-part response assembly.

### Medium Priority

#### 4. Notification Tracker
**Issue:** Export notification state scattered across AniDBApi.

**Recommendation:** Create `NotificationTracker` class with clear state machine.

#### 5. Anime Data Cache
**Issue:** MyListCardManager has complex CardCreationData struct and caching logic.

**Recommendation:** Create separate `AnimeDataCache` class to manage caching independently.

### Low Priority (Future work)

#### 6. Database Access Abstraction
**Issue:** Direct SQL queries throughout codebase (tight coupling).

**Recommendation:** Create repository pattern with interfaces:
- `ISettingsRepository`
- `IAnimeRepository`
- `IMylistRepository`

#### 7. Global Pointers
**Issue:** Global `adbapi` pointer violates Dependency Inversion Principle.

**Recommendation:** Use dependency injection or service locator pattern.

## Class Design Principles Applied

### Single Responsibility Principle (SRP)
✅ `ApplicationSettings` - Only manages settings
✅ `ProgressTracker` - Only tracks progress
✅ `LocalFileInfo` - Only represents local file data

### Open/Closed Principle (OCP)
✅ New setting types can be added without modifying core logic
✅ ProgressTracker can be extended for new calculation methods

### Liskov Substitution Principle (LSP)
✅ Minimal inheritance usage (mostly composition)

### Interface Segregation Principle (ISP)
✅ Small, focused interfaces
✅ Classes don't force clients to depend on unused methods

### Dependency Inversion Principle (DIP)
⚠️  Still needs work - database dependencies are concrete
📝 Future: Add repository interfaces

## Testing Strategy

Each new class is designed with testability in mind:

1. **ApplicationSettings** - Can be tested with in-memory database
2. **ProgressTracker** - Pure logic, easy to unit test
3. **LocalFileInfo** - Can be tested with temporary files

Example test structure:
```cpp
TEST(ApplicationSettingsTest, LoadSaveCycle) {
    QSqlDatabase db = createTestDatabase();
    ApplicationSettings settings(db);
    
    settings.setUsername("testuser");
    settings.save();
    
    ApplicationSettings loaded(db);
    loaded.load();
    
    EXPECT_EQ(loaded.getUsername(), "testuser");
}
```

## Migration Guide

To use the new classes in existing code:

### 1. ApplicationSettings Migration

**Before:**
```cpp
adbapi->setUsername("user");
QString user = adbapi->getUsername();
```

**After:**
```cpp
ApplicationSettings settings(db);
settings.setUsername("user");
QString user = settings.getUsername();
```

### 2. ProgressTracker Migration

**Before:**
```cpp
// Manual progress tracking in Window
hashingTimer.start();
totalHashParts = calculateTotal();
completedHashParts = 0;
// ... complex ETA calculation
```

**After:**
```cpp
ProgressTracker tracker(totalHashParts);
tracker.start();
tracker.updateProgress(completedHashParts);
QString eta = tracker.getETAString();
```

### 3. LocalFileInfo Migration

**Before:**
```cpp
struct UnboundFileData {
    QString filename;
    QString filepath;
    QString hash;
    qint64 size;
};
```

**After:**
```cpp
LocalFileInfo fileInfo(filename, filepath, hash, size);
if (fileInfo.isVideoFile()) {
    // process
}
```

## Build Integration

All new classes have been added to `usagi/CMakeLists.txt`:
- Source files added to `SOURCES`
- Header files added to `HEADERS`
- No additional dependencies required (uses existing Qt modules)

## Code Quality Checks

Before committing changes:
1. ✅ Build with cmake
2. ✅ Run existing tests
3. 🔄 Run clazy (when available)
4. 🔄 Verify no regressions

## Future Work

See "Analysis of Remaining Issues" section above for prioritized list of additional refactoring opportunities.

Key areas:
1. Extract controllers from Window class
2. Convert AniDB data structs to classes
3. Add repository pattern for database access
4. Eliminate global pointers

## References

- [SOLID Principles](https://en.wikipedia.org/wiki/SOLID)
- [Clean Code by Robert C. Martin](https://www.oreilly.com/library/view/clean-code-a/9780136083238/)
- [Effective C++ by Scott Meyers](https://www.aristeia.com/books.html)

## Authors

- Initial Analysis: Copilot AI
- Implementation: Copilot AI
- Review: viperx1

---

*Last Updated: 2025-12-05*
