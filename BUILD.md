# Build Documentation

This document describes the build process for Usagi-dono, including local development builds and CI/CD workflows.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Detailed Build Instructions](#detailed-build-instructions)
  - [Windows (Qt Creator)](#windows-qt-creator)
  - [Windows (Command Line)](#windows-command-line)
  - [Linux](#linux)
- [Build Configuration](#build-configuration)
- [CI/CD Workflows](#cicd-workflows)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Software

- **CMake** 3.16 or later
- **Qt 6.8 LTS** or later with the following components:
  - Qt6::Core
  - Qt6::Gui
  - Qt6::Widgets
  - Qt6::Network
  - Qt6::Sql
  - Qt6::Concurrent (for tests)
  - Qt6::Test (for tests)
- **Compiler**:
  - Windows: MinGW 64-bit (GCC) or LLVM MinGW (Clang)
  - Linux: GCC or Clang with C++17 support
- **Ninja** (optional but recommended for faster builds)
- **zlib** development libraries

### Qt Installation

**IMPORTANT:** This project requires **dynamic Qt libraries** (DLLs on Windows). Static Qt builds are no longer supported as they can cause signal/slot connection issues with the meta-object system.

#### Windows

Download and install Qt 6.8 LTS with MinGW from:
https://www.qt.io/download-qt-installer

Select the following components:
- Qt 6.8.x for MinGW 64-bit (dynamic libraries)
- MinGW 64-bit compiler toolchain

#### Linux

```bash
# Ubuntu/Debian
sudo apt-get install qt6-base-dev qt6-base-dev-tools

# Fedora
sudo dnf install qt6-qtbase-devel

# Arch Linux
sudo pacman -S qt6-base
```

## Quick Start

### Using Qt Creator (Windows/Linux)

1. Open `CMakeLists.txt` in Qt Creator
2. Configure the project with Qt 6.8 and MinGW (or GCC on Linux)
3. Select Release or Debug build configuration
4. Build → Build Project
5. Run → Run (to launch the application)

### Using Command Line

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Build the main executable
cmake --build . --target usagi

# Build tests (optional)
cmake --build . --target all
```

## Detailed Build Instructions

### Windows (Qt Creator)

1. **Open Project**
   - Launch Qt Creator
   - File → Open File or Project
   - Select `CMakeLists.txt` from the repository root

2. **Configure Kit**
   - Qt Creator will prompt you to configure a kit
   - Select "Desktop Qt 6.8.x MinGW 64-bit"
   - Ensure the kit uses dynamic Qt libraries (not static)

3. **Build Settings**
   - Click on "Projects" in the left sidebar
   - Under "Build Settings", verify:
     - Build directory: `<source>/build`
     - Build configuration: Debug or Release
     - CMake generator: Ninja (recommended) or MinGW Makefiles

4. **Build**
   - Click the hammer icon or press Ctrl+B
   - The executable will be in `build/usagi/usagi.exe`

5. **Run**
   - Click the green play button or press Ctrl+R
   - Qt Creator will automatically copy required Qt DLLs

### Windows (Command Line)

```bash
# Set up environment (adjust path to your Qt installation)
set Qt6_DIR=C:\Qt\6.8.3\mingw_64\lib\cmake
set PATH=%PATH%;C:\Qt\6.8.3\mingw_64\bin

# Create and enter build directory
mkdir build
cd build

# Configure
cmake .. -G "Ninja" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\mingw_64\lib\cmake

# Build main executable
cmake --build . --target usagi

# The executable is in: build\usagi\usagi.exe
```

**Note:** You'll need to copy Qt DLLs to the same directory as the executable for it to run outside Qt Creator. Required DLLs include:
- Qt6Core.dll
- Qt6Gui.dll
- Qt6Widgets.dll
- Qt6Network.dll
- Qt6Sql.dll
- And platform plugins in `platforms/qwindows.dll`

You can use `windeployqt.exe` to automatically copy required DLLs:
```bash
windeployqt build\usagi\usagi.exe
```

### Linux

```bash
# Install dependencies (Ubuntu/Debian example)
sudo apt-get install -y qt6-base-dev cmake ninja-build zlib1g-dev

# Create and enter build directory
mkdir build
cd build

# Configure
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Build main executable
cmake --build . --target usagi

# Run
./usagi/usagi
```

## Build Configuration

### CMake Options

The build system uses the following key configurations:

#### Dynamic Qt Linking (Forced)

The project is configured to **force dynamic Qt linking**. This is set in `CMakeLists.txt`:

```cmake
# Force dynamic linking
add_compile_definitions(QT_SHARED)
remove_definitions(-DQT_STATIC)
remove_definitions(-DQT_STATICPLUGIN)
set(QT_IS_STATIC_BUILD FALSE)
```

This ensures consistent meta-object system behavior across platforms.

#### Build Types

- **Debug**: Full debug symbols, no optimization (`-g3 -O0`)
- **Release**: Optimized build for distribution
- **RelWithDebInfo**: Optimized with debug symbols (default)

Set build type with:
```bash
cmake -DCMAKE_BUILD_TYPE=<Debug|Release|RelWithDebInfo> ..
```

#### Compiler Flags

The build automatically sets:
- C++ Standard: C++17
- Debug symbols: DWARF-4 format (Windows compatible)
- Frame pointers: Enabled for crash tracing
- Static libstdc++/libgcc: On Windows to avoid runtime DLL dependencies

### Build Targets

- `usagi` - Main application executable
- `all` - Build everything (including tests)
- `test` or run `ctest` - Run all tests

## CI/CD Workflows

The project uses GitHub Actions for continuous integration with two workflows:

### Windows Build Main (`windows-build-main.yml`)

**Triggers:**
- Push to `master` or `dev` branches
- Push of version tags (`v*.*.*` or `v*.*.*-beta*`)
- Manual workflow dispatch

**What it does:**
1. Installs Qt 6.8 MinGW (dynamic libraries)
2. Configures build with CMake
3. Builds the main `usagi` executable
4. For branch pushes: Uploads alpha build artifacts
5. For tag pushes: Creates GitHub release with executable
6. On failure: Creates issue with build logs

**Artifact naming:**
- Alpha builds: `Usagi-dono-alpha-mingw`
- Release builds: Attached to GitHub release

### Windows Build Tests (`windows-build-tests.yml`)

**Triggers:**
- Push to `main` branch
- Manual workflow dispatch

**What it does:**
1. Installs Qt 6.8 LLVM MinGW (dynamic libraries)
2. Sets up LLVM MinGW toolchain
3. Configures build with CMake and Clang
4. Builds all targets (including tests)
5. Runs test suite with CTest
6. Uploads test artifacts on success
7. On failure: Creates issue with test logs

**Compiler:**
- Uses LLVM MinGW Clang for better standards compliance
- Configured with GNU frontend variant for Qt compatibility

## Troubleshooting

### "signal not found" Errors

If you see Qt errors like:
```
qt.core.qobject.connect: QObject::connect: signal not found in <ClassName>
```

**Solution:**
1. Ensure you're using **dynamic Qt libraries**, not static
2. Delete the build directory and rebuild from scratch
3. Check that moc files are being regenerated
4. Verify Qt version is 6.8 or later

### Missing Qt DLLs on Windows

**Error:** Application fails to start with missing DLL errors.

**Solution:**
```bash
cd build/usagi
windeployqt usagi.exe
```

This copies all required Qt DLLs to the executable directory.

### Build Fails with "Qt6 not found"

**Solution:**
Set `CMAKE_PREFIX_PATH` to your Qt installation:
```bash
cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64/lib/cmake ..
```

### MOC Errors on Windows

**Error:** MOC-related errors during build.

**Solution:**
1. Ensure Qt's bin directory is in PATH
2. Delete the build directory
3. Reconfigure and rebuild

### Link Errors with zlib

**Error:** Undefined reference to `inflateEnd` or similar.

**Solution:**
- Windows: zlib should be included with Qt
- Linux: Install zlib development package:
  ```bash
  sudo apt-get install zlib1g-dev
  ```

### LLVM MinGW Not Found (Tests)

**Error:** Tests workflow can't find LLVM MinGW.

**Solution:**
The workflow automatically downloads LLVM MinGW. If it fails:
1. Check internet connectivity
2. Verify the download URL in `.github/workflows/windows-build-tests.yml`
3. Clear workflow cache and retry

## Build System Architecture

### CMake Structure

```
CMakeLists.txt          # Root CMake configuration
├── usagi/
│   └── CMakeLists.txt  # Main executable configuration
└── tests/
    └── CMakeLists.txt  # Test suite configuration
```

### Key Build Features

1. **Automatic MOC/UIC/RCC**: Qt's meta-object compiler runs automatically
2. **Platform Detection**: Automatically adapts to Windows, Linux, or macOS
3. **Compiler Detection**: Supports MinGW (GCC), LLVM MinGW (Clang), and GCC on Linux
4. **Dynamic Linking Enforcement**: Forces use of Qt DLLs for proper signal/slot behavior
5. **Debug Symbol Generation**: Full DWARF-4 symbols for crash analysis
6. **Static Runtime Linking**: libstdc++ and libgcc are statically linked to avoid runtime dependencies

## Contributing

When submitting changes that affect the build system:

1. Test on both Windows and Linux if possible
2. Ensure the CI workflows pass
3. Update this documentation if you change build requirements
4. Test both Qt Creator and command-line builds

## Additional Resources

- [Qt 6.8 Documentation](https://doc.qt.io/qt-6/)
- [CMake Documentation](https://cmake.org/documentation/)
- [Qt Creator Manual](https://doc.qt.io/qtcreator/)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
