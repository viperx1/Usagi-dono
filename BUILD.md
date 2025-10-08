# Building Usagi-dono

## Requirements

- CMake 3.16 or later
- Qt 6.x
- C++11 compatible compiler
- Ninja (recommended) or other build system

## Compiler Requirements for Windows

**Important**: For full crash log functionality on Windows (showing function names in crash logs), you must use **Clang/LLVM** compiler, not GCC/MinGW.

### Why Clang/LLVM?

The crash log functionality on Windows relies on the Windows DbgHelp API to resolve function names from stack traces. DbgHelp requires debug symbols in either:
- PDB (Program Database) format, or
- CodeView format

**Clang/LLVM** with the LLD linker can generate PDB files with CodeView debug information, which DbgHelp can read.

**GCC/MinGW** generates DWARF debug format, which Windows DbgHelp API cannot reliably read. This means crash logs will show only memory addresses for application functions.

### Recommended Setup for Windows

1. Install Qt with LLVM MinGW:
   - Use Qt's online installer
   - Select "Qt 6.x for Windows" with "MinGW (LLVM)" variant
   - This includes both Qt and the Clang/LLVM compiler

2. Configure CMake to use Clang:
   ```bash
   # Set environment variables
   export CC=/path/to/Qt/Tools/llvm-mingw/bin/clang.exe
   export CXX=/path/to/Qt/Tools/llvm-mingw/bin/clang++.exe
   
   # Or pass directly to CMake
   cmake -G "Ninja" -S . -B build \
     -DCMAKE_C_COMPILER=/path/to/Qt/Tools/llvm-mingw/bin/clang.exe \
     -DCMAKE_CXX_COMPILER=/path/to/Qt/Tools/llvm-mingw/bin/clang++.exe \
     -DCMAKE_BUILD_TYPE=Release
   ```

3. Build:
   ```bash
   cmake --build build --config Release
   ```

4. Verify PDB files were generated:
   ```bash
   ls build/usagi/*.pdb
   ls build/tests/*.pdb
   ```

### Using GCC/MinGW on Windows

If you build with GCC/MinGW on Windows, CMake will display a warning:

```
CMake Warning:
  Building with GCC/MinGW on Windows. Crash logs may not show function names
  from the Usagi codebase because GCC generates DWARF debug format which
  Windows DbgHelp API cannot reliably read. For full crash log functionality,
  use Clang/LLVM compiler which can generate PDB files with CodeView debug format.
```

The application will still build and run correctly, but crash logs will have limited information.

## Building on Linux/macOS

On Linux and macOS, any modern compiler (GCC, Clang, LLVM) will work correctly:

```bash
cmake -G "Ninja" -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --output-on-failure --test-dir build
```

## Build Types

- **Release**: Optimized build with debug symbols (recommended for production)
- **Debug**: Unoptimized build with full debug information
- **RelWithDebInfo**: Optimized build with debug information
- **MinSizeRel**: Size-optimized build

Debug symbols are included in all build types to ensure crash logs show function names.

## Testing

Run tests after building:

```bash
ctest --output-on-failure --test-dir build
```

Or run individual tests:

```bash
./build/tests/test_hash
./build/tests/test_crashlog
```

## CI/CD

The GitHub Actions workflow automatically:
1. Installs Qt with LLVM MinGW
2. Configures CMake to use Clang from Qt
3. Builds the project
4. Runs tests
5. Uploads artifacts including PDB files

See `.github/workflows/windows-build.yml` for details.

## Troubleshooting

### CMake picks wrong compiler on Windows

If CMake automatically detects GCC instead of Clang:
- Explicitly set `CMAKE_C_COMPILER` and `CMAKE_CXX_COMPILER` as shown above
- Or set `CC` and `CXX` environment variables before running CMake

### PDB files not generated

Check that:
1. You're using Clang/LLVM compiler (not GCC)
2. You're building on Windows
3. CMake shows "The CXX compiler identification is Clang" (not GNU)
4. Build completed successfully

### Crash logs don't show function names

See [CRASHLOG.md](CRASHLOG.md) for detailed troubleshooting, including:
- How to verify debug symbols are loaded
- Checking `usagi.log` for diagnostics
- Platform-specific requirements
