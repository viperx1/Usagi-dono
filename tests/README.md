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
- Qt5 development libraries
- Qt Test framework
- qmake build tool

### Build Instructions

```bash
cd tests
qmake tests.pro
make
```

### Running Tests

After building, run the test executable:

```bash
./test_hash
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
2. Add the test file to `SOURCES` in `tests.pro`
3. Rebuild and run tests

## Test Framework

These tests use the Qt Test framework. For more information, see:
https://doc.qt.io/qt-5/qtest-overview.html

