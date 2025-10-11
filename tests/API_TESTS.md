# AniDB API Command Format Tests

## Overview

This document describes the test suite for validating AniDB API command format integrity in the Usagi application. The tests ensure that API commands are correctly formatted according to the [AniDB UDP API Definition](https://wiki.anidb.net/UDP_API_Definition).

## Test File

**Location**: `tests/test_anidbapi.cpp`

**Framework**: Qt Test

**Dependencies**: Qt6::Core, Qt6::Test

## Purpose

These tests validate **command format integrity only**. They do NOT test:
- Network communication
- Authentication/session management
- Response parsing
- Database operations
- Rate limiting
- Error handling

The tests ensure that command strings are constructed with the correct syntax, parameters, and encoding before being sent to the AniDB API.

## Test Coverage

### 1. AUTH Command Tests

#### testAuthCommandFormat()
Validates the AUTH command format:
```
AUTH user=<str>&pass=<str>&protover=<int4>&client=<str>&clientver=<int4>&enc=<str>
```

**Verifies**:
- Command starts with "AUTH "
- All required parameters are present (user, pass, protover, client, clientver, enc)
- Parameter values are correctly substituted
- Parameters are separated by '&'

**Example valid command**:
```
AUTH user=testuser&pass=testpass&protover=3&client=usagi&clientver=1&enc=utf8
```

#### testAuthCommandParameterEncoding()
Documents behavior with special characters in credentials. In production, special characters should be URL-encoded:
- Spaces: `%20`
- Ampersands: `%26`
- Equal signs: `%3D`

### 2. LOGOUT Command Tests

#### testLogoutCommandFormat()
Validates the LOGOUT command format:
```
LOGOUT
```

**Verifies**:
- Command is exactly "LOGOUT " (with trailing space)
- No additional parameters

### 3. MYLISTADD Command Tests

#### testMylistAddBasicFormat()
Validates basic MYLISTADD command with required parameters:
```
MYLISTADD size=<int8>&ed2k=<str>&state=<int2>
```

**Verifies**:
- Command starts with "MYLISTADD "
- Required parameters: size, ed2k, state
- Large file sizes (qint64) are handled correctly
- ED2K hash format (32-character hex string)

**Example valid command**:
```
MYLISTADD size=734003200&ed2k=a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4&state=1
```

#### testMylistAddWithOptionalParameters()
Validates MYLISTADD with optional parameters:
```
MYLISTADD size=<int8>&ed2k=<str>&viewed=<int2>&storage=<str>&state=<int2>
```

**Optional parameters**:
- `viewed`: 0 or 1 (note: implementation subtracts 1 from input value)
- `storage`: Free-form text (e.g., "HDD", "External Drive")

**Verifies**:
- Optional parameters are only added when provided
- Correct parameter formatting

#### testMylistAddWithEditFlag()
Validates MYLISTADD with edit flag for updating existing entries:
```
MYLISTADD size=<int8>&ed2k=<str>&edit=1&state=<int2>
```

**Verifies**:
- Edit flag format: `&edit=1`
- Used when updating existing mylist entries (response code 310)

#### testMylistAddParameterOrder()
Verifies that parameters can be in any order (API accepts flexible ordering).

### 4. FILE Command Tests

#### testFileCommandFormat()
Validates FILE command format:
```
FILE size=<int8>&ed2k=<str>&fmask=<hexstr>&amask=<hexstr>
```

**Verifies**:
- Command starts with "FILE "
- All required parameters present
- fmask and amask are hex-encoded

**Example valid command**:
```
FILE size=734003200&ed2k=a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4&fmask=7ff8fef8&amask=f0f0f0f0
```

#### testFileCommandMasks()
Validates fmask and amask encoding:
- **Length**: Exactly 8 characters
- **Format**: Lowercase hexadecimal
- **Padding**: Leading zeros when necessary

**Mask meanings**:
- `fmask`: File information mask (which file fields to return)
- `amask`: Anime information mask (which anime fields to return)

**Example masks**:
```cpp
unsigned int fmask = 0x7FF8FEF8;  // Returns most file information
unsigned int amask = 0xF0F0F0F0;  // Returns anime metadata
```

### 5. MYLIST Command Tests

#### testMylistCommandWithLid()
Validates MYLIST query by lid (mylist entry ID):
```
MYLIST lid=<int4>
```

**Verifies**:
- Command starts with "MYLIST "
- lid parameter format
- Correct lid value substitution

**Example valid command**:
```
MYLIST lid=12345
```

**Note**: Should NOT be used for bulk queries (violates API guidelines).

#### testMylistStatCommandFormat()
Validates MYLISTSTAT command:
```
MYLISTSTAT
```

**Verifies**:
- Command is exactly "MYLISTSTAT " (with trailing space)
- No parameters required

**Returns**: Total entries, watched count, file sizes, etc.

### 6. General Format Tests

#### testParameterSeparators()
Validates parameter separator format:
- Parameters separated by `&` character
- No spaces around separators (` &` or `& ` is invalid)

#### testSpecialCharacterEncoding()
Documents behavior with special characters:
- Spaces in storage parameter
- Ampersands in values (breaks parameter parsing)

**Important**: Production code should URL-encode special characters:
```
"External HDD" → "External%20HDD"
"HDD&SSD"      → "HDD%26SSD"
```

## Running the Tests

### Build and Run

```bash
# From repository root
mkdir build && cd build
cmake ..
make test_anidbapi

# Run tests
./tests/test_anidbapi

# Run with CTest
ctest -R test_anidbapi -V
```

### Expected Output

```
********* Start testing of TestAniDBApiCommands *********
Config: Using QtTest library 6.x.x
PASS   : TestAniDBApiCommands::initTestCase()
PASS   : TestAniDBApiCommands::testAuthCommandFormat()
PASS   : TestAniDBApiCommands::testAuthCommandParameterEncoding()
PASS   : TestAniDBApiCommands::testLogoutCommandFormat()
PASS   : TestAniDBApiCommands::testMylistAddBasicFormat()
PASS   : TestAniDBApiCommands::testMylistAddWithOptionalParameters()
PASS   : TestAniDBApiCommands::testMylistAddWithEditFlag()
PASS   : TestAniDBApiCommands::testMylistAddParameterOrder()
PASS   : TestAniDBApiCommands::testFileCommandFormat()
PASS   : TestAniDBApiCommands::testFileCommandMasks()
PASS   : TestAniDBApiCommands::testMylistCommandWithLid()
PASS   : TestAniDBApiCommands::testMylistStatCommandFormat()
PASS   : TestAniDBApiCommands::testParameterSeparators()
PASS   : TestAniDBApiCommands::testSpecialCharacterEncoding()
PASS   : TestAniDBApiCommands::cleanupTestCase()
Totals: 15 passed, 0 failed, 0 skipped, 0 blacklisted, Xms
********* Finished testing of TestAniDBApiCommands *********
```

## API Commands Reference

### Implemented and Tested Commands

| Command | Purpose | Required Parameters | Optional Parameters |
|---------|---------|-------------------|-------------------|
| AUTH | Authenticate | user, pass, protover, client, clientver, enc | - |
| LOGOUT | End session | - | - |
| MYLISTADD | Add file to mylist | size, ed2k, state | viewed, storage, edit |
| FILE | Query file info | size, ed2k, fmask, amask | - |
| MYLIST | Query mylist entry | lid | - |
| MYLISTSTAT | Get mylist stats | - | - |

### Implementation Files

Command implementations are in:
- `usagi/src/anidbapi.h` - Class declarations
- `usagi/src/anidbapi.cpp` - Command implementations:
  - `AniDBApi::Auth()` (line ~288)
  - `AniDBApi::Logout()` (line ~302)
  - `AniDBApi::MylistAdd()` (line ~312)
  - `AniDBApi::File()` (line ~352)
  - `AniDBApi::Mylist()` (line ~373)

## Compliance with AniDB API Guidelines

These tests help ensure compliance with:

1. **Rate Limiting**: Commands are queued (not tested here, but in production)
2. **Parameter Format**: Correct syntax and encoding
3. **Command Structure**: Proper command names and parameter names
4. **Data Types**: Correct numeric and string formatting
5. **Hex Encoding**: Proper fmask/amask format

## Future Enhancements

Potential additions to the test suite:

1. **URL Encoding Tests**: Add proper URL encoding and validate it
2. **Parameter Validation**: Test parameter value ranges and constraints
3. **Session ID Tests**: Verify session ID (`s=<str>`) is added correctly
4. **Tag Tests**: Verify tag parameter (`tag=<str>`) is added correctly
5. **Unicode Tests**: Test UTF-8 encoding of non-ASCII characters
6. **Error Cases**: Test handling of invalid/missing parameters

## Contributing

When adding new API commands:

1. Implement the command in `anidbapi.cpp`
2. Add corresponding tests in `test_anidbapi.cpp`
3. Document the command format and parameters
4. Verify against AniDB API specification
5. Run all tests to ensure no regressions

## References

- [AniDB UDP API Definition](https://wiki.anidb.net/UDP_API_Definition)
- [Qt Test Framework](https://doc.qt.io/qt-6/qtest-overview.html)
- Repository documentation:
  - `MYLIST_API_GUIDELINES.md`
  - `TECHNICAL_DESIGN_NOTES.md`
  - `tests/README.md`
