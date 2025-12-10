# Usagi-dono

An AniDB client for managing your anime collection with advanced tracking, hashing, and mylist management features.

## Quick Start

### Building
See [BUILD.md](BUILD.md) for detailed build instructions for Windows, Linux, and CI/CD workflows.

**Quick build:**
```bash
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build . --target usagi
```

### Requirements
- Qt 6.8 LTS (dynamic libraries)
- CMake 3.16+
- C++17 compatible compiler (MinGW on Windows, GCC/Clang on Linux)
- zlib development libraries

## Documentation

### Current Features

These features are implemented and available in the current codebase:

#### Core Features
- **[Anime Titles Database](ANIME_TITLES_FEATURE.md)** - Automatic anime titles import from AniDB
  - [Quick Start Guide](ANIME_TITLES_QUICKSTART.md)
- **[MyList Management](MYLIST_AUTOLOAD_IMPLEMENTATION.md)** - Automatic mylist loading and synchronization
  - [Field Updates](MYLIST_FIELD_UPDATE_IMPLEMENTATION.md) - Update mylist entry metadata
  - [API Guidelines](MYLIST_API_GUIDELINES.md) - AniDB API compliance and best practices
  - [Feature Tests](MYLIST_FEATURE_TEST.md) - Manual testing procedures
- **[Export Check Interval](EXPORT_CHECK_INTERVAL_IMPLEMENTATION.md)** - Periodic export notification checking
- **[Notifications API](NOTIFICATIONS_API_IMPLEMENTATION.md)** - AniDB notification handling

#### File Management
- **[Directory Watcher](DIRECTORY_WATCHER_FEATURE.md)** - Automatic monitoring and hashing of new files
- **[Unknown Files Handling](UNKNOWN_FILES_FEATURE.md)** - Manual binding of files not in AniDB
- **[CSV Import](CSV_ADBORG_SUPPORT.md)** - Import anime data from CSV-adborg format

#### Playback & Tracking
- **[Media Playback](PLAYBACK_FEATURE.md)** - MPC-HC integration with progress tracking
- **[Watch Session Tracking](CHUNK_BASED_WATCH_TRACKING.md)** - Series chain and watch session management

#### System
- **[Logging System](LOGGING_SYSTEM.md)** - Application logging infrastructure

### Architecture & Design

- **[Architecture Diagram](ARCHITECTURE_DIAGRAM.md)** - System architecture and component overview
- **[UI Layout](UI_LAYOUT.md)** - User interface structure and organization
- **[Technical Design Notes](TECHNICAL_DESIGN_NOTES.md)** - Implementation patterns and design decisions

### Future Plans

- **[Anime Tracking Feature Plan](FEATURE_PLAN_ANIME_TRACKING.md)** - Comprehensive roadmap for anime tracking, torrent integration, and smart file organization

### Testing

- **[Test Suite Overview](tests/README.md)** - How to build and run tests
- **[API Tests](tests/API_TESTS.md)** - AniDB API command format tests

## Project Status

Usagi-dono is actively developed with the following core capabilities:
- ✅ AniDB API integration (authentication, mylist, file queries)
- ✅ File hashing (ed2k) with multi-threaded processing
- ✅ MyList card view with advanced filtering and sorting
- ✅ Unknown files handling with manual anime binding
- ✅ Media playback integration (MPC-HC)
- ✅ Directory monitoring for automatic file processing
- ✅ Watch session tracking and series chain management
- ✅ Anime titles database with auto-updates
- 🚧 Torrent integration (planned)
- 🚧 Advanced statistics and insights (planned)

## Contributing

### Code Style
- Follow SOLID principles for class design
- Use Qt 6.8 APIs exclusively (no compatibility code for other versions)
- Run clazy on modified files before committing
- Avoid `auto` keyword (strictly prohibited per project guidelines)

### Building & Testing
```bash
# Build with compile commands for clazy
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run clazy on modified files
clazy --standalone path/to/file.cpp

# Run tests
ctest -V
```

### Submitting Changes
1. Ensure code builds without warnings (`-Wall -Wextra -Wpedantic -Werror`)
2. Run clazy and fix all errors and warnings
3. Test changes locally
4. Update documentation if adding/changing features

## License

[License information needed]

## Contact

[Contact information needed]
