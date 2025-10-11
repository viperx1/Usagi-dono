# Usagi-dono Tests

This directory contains unit tests for the Usagi-dono application.

## Test Coverage

- **test_hash.cpp**: Tests for hash implementations
  - MD4 empty file hashing
  - MD4 file hashing with content
  - ED2K initialization test
  - ED2K basic hashing functionality
  - ED2K file hashing

- **test_crashlog.cpp**: Tests for crash log encoding
  - Verify crash log is written in ASCII/UTF-8 encoding
  - Verify crash log is NOT written in UTF-16LE encoding
  - Detect and demonstrate incorrect UTF-16LE encoding
  - Verify QTextStream with UTF-8 encoding without QIODevice::Text flag (new test)
  - Ensure crash log content is readable

- **test_anidbapi.cpp**: Tests for AniDB API command format integrity
  - AUTH command format validation (user, pass, protover, client, clientver, enc)
  - LOGOUT command format validation
  - MYLISTADD command format with required and optional parameters
  - FILE command format with fmask/amask hex encoding
  - MYLIST command format with lid parameter
  - MYLISTSTAT command format
  - Parameter separator validation
  - Special character encoding behavior documentation
  
## Building and Running Tests

### Prerequisites
- Qt6 development libraries (6.9.2)
- Qt Test framework
- CMake 3.16 or higher

### Build Instructions

```bash
# From the repository root
mkdir build
cd build
cmake ..
cmake --build .

# Or from the tests directory (standalone)
cd tests
mkdir build
cd build
cmake ..
cmake --build .
```

### Running Tests

After building, run the test executable:

```bash
# From build directory
./tests/test_hash
./tests/test_crashlog
./tests/test_anidbapi

# Or use CTest to run all tests
ctest --output-on-failure

# Run a specific test
ctest -R test_anidbapi --output-on-failure
```

You should see output like:
```
********* Start testing of TestHashFunctions *********
...
Totals: X passed, 0 failed, 0 skipped, 0 blacklisted
********* Finished testing of TestHashFunctions *********
```

## Adding New Tests

To add new tests:

1. Edit `test_hash.cpp` and add new test methods to the `TestHashFunctions` class
2. Add the test method name to the `private slots:` section
3. Rebuild and run tests

Alternatively, create a new test file:
1. Create a new test file (e.g., `test_newfeature.cpp`)
2. Add the test file to `SOURCES` in `tests/CMakeLists.txt`
3. Rebuild and run tests

## Test Framework

These tests use the Qt Test framework. For more information, see:
https://doc.qt.io/qt-6/qtest-overview.html

