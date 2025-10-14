# Windows Build Fix Summary (Qt 6.9.2 LLVM MinGW)

## Issue
Build failure on Windows with Qt 6.9.2 LLVM MinGW static libraries:
```
ld.lld: error: undefined symbol: __declspec(dllimport) QTimer::singleShotImpl(...)
>>> referenced by D:/a/Usagi-dono/Qt/6.9.2/llvm-mingw_64/include\QtCore\qtimer.h:202
>>>               usagi/CMakeFiles/usagi.dir/src/window.cpp.obj:(Window::Window())
```

## Root Cause
The `__declspec(dllimport)` in the error indicates that Qt headers were being processed without the `QT_STATIC` preprocessor definition. When `QT_STATIC` is not defined, Qt headers declare functions with `dllimport` linkage, expecting to import from DLLs. However, we're linking against static Qt libraries (`.a` files), causing a linkage mismatch.

The function `QTimer::singleShotImpl` is an internal implementation detail of Qt 6's template-based `QTimer::singleShot()` API and requires proper static linkage declarations.

## Solution
Applied a comprehensive fix to ensure `QT_STATIC` is defined at all compilation stages:

### 1. Early Static Qt Detection (CMakeLists.txt)
- Moved `QT_STATIC` definition to immediately after `find_package(Qt6)`
- Added `QT_STATIC` to Qt targets' INTERFACE_COMPILE_DEFINITIONS to ensure propagation
- This ensures all code that includes Qt headers sees the static linkage declarations

### 2. MOC Configuration (usagi/CMakeLists.txt, tests/CMakeLists.txt)
- Added `CMAKE_AUTOMOC_MOC_OPTIONS` with `-DQT_STATIC` and `-DQT_STATICPLUGIN`
- Ensures MOC-generated files are compiled with static Qt definitions

### 3. Explicit Target Definitions
- Added explicit `target_compile_definitions(usagi PRIVATE QT_STATIC QT_STATICPLUGIN)`
- Applied same to all test targets (test_hash, test_crashlog, test_anidbapi, test_anime_titles)
- Provides defense-in-depth to guarantee static linkage

### 4. Static Plugin Imports (usagi/src/main.cpp)
- Added conditional plugin imports for static builds:
  ```cpp
  #ifdef QT_STATIC
  #include <QtPlugin>
  Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
  Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
  #endif
  ```
- Used `qt_import_plugins()` in CMakeLists.txt

### 5. Additional System Libraries (usagi/CMakeLists.txt)
- Added required Windows system libraries for static Qt builds:
  - winmm, version, netapi32, ws2_32, iphlpapi, crypt32, ole32, oleaut32, uuid
- Linked `Qt6::EntryPoint` for static builds (provides WinMain)
- Re-linked `Qt6::Core` at the end to resolve circular dependencies

### 6. Library Linking Order
- Reordered Qt libraries to put higher-level libraries first:
  - Qt6::Widgets → Qt6::Network → Qt6::Sql → Qt6::Gui → Qt6::Core
- This ensures proper symbol resolution for static linking

## Files Modified
1. `CMakeLists.txt` - Early static Qt detection and interface definitions
2. `usagi/CMakeLists.txt` - Target definitions, plugin imports, system libraries
3. `usagi/src/main.cpp` - Static plugin imports
4. `tests/CMakeLists.txt` - Static Qt definitions for test targets

## Expected Result
The build should now succeed because:
1. All compilation units see `QT_STATIC` and use correct linkage declarations
2. Required platform plugins are properly imported for static builds
3. All necessary system libraries are linked
4. Qt libraries are linked in the correct order to resolve dependencies

## Testing
This fix addresses the specific linker error. The build should complete successfully in the CI environment with Qt 6.9.2 LLVM MinGW static libraries.
