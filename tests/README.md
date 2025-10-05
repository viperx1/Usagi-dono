# Usagi-dono Tests

This directory contains unit tests for the Usagi-dono application.

## Test Coverage

- **test_hash.cpp**: Tests for hash implementations
  - MD4 empty file hashing
  - MD4 file hashing with content
  - ED2K initialization test
  - ED2K basic hashing functionality
  - ED2K file hashing
  
## Building and Running Tests

### Prerequisites
- Qt5 or Qt6 development libraries
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

# Or use CTest
ctest --output-on-failure
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
https://doc.qt.io/qt-5/qtest-overview.html

