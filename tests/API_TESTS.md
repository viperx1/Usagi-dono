# AniDB API Command Format Tests

## Overview

This document describes the test suite for validating AniDB API command format integrity in the Usagi application. The tests ensure that API commands are correctly formatted according to the [AniDB UDP API Definition](https://wiki.anidb.net/UDP_API_Definition).

## Test File

**Location**: `tests/test_anidbapi.cpp`

**Framework**: Qt Test

**Dependencies**: Qt6::Core, Qt6::Test, Qt6::Network, Qt6::Sql, Qt6::Widgets

## Purpose

These tests validate that the actual API functions in `anidbapi.cpp` generate correctly formatted commands. The tests:
- Instantiate `AniDBApi` with test credentials
- Call actual API functions (Auth(), MylistAdd(), File(), Mylist())
- Query the database `packets` table to retrieve generated commands
- Validate command format matches AniDB API specification

The tests ensure that command strings are constructed with the correct syntax, parameters, and encoding before being sent to the AniDB API.

## Test Coverage

### 1. AUTH Command Tests

#### testAuthCommandFormat()
Calls `AniDBApi::Auth()` and validates the generated command format:
```
AUTH user=<str>&pass=<str>&protover=<int4>&client=<str>&clientver=<int4>&enc=<str>
```

**Test Flow**:
1. Creates AniDBApi instance with test username/password
2. Calls `api->Auth()`
3. Queries database for inserted command
4. Verifies format and parameter values

**Verifies**:
- Command starts with "AUTH "
- All required parameters are present (user, pass, protover, client, clientver, enc)
- Parameter values are correctly substituted
- Parameters are separated by '&'

**Example valid command**:
```
AUTH user=testuser&pass=testpass&protover=3&client=usagitest&clientver=1&enc=utf8
```

### 2. LOGOUT Command Tests

#### testLogoutCommandFormat()
Documents the LOGOUT command format (Logout() calls Send() directly, requiring socket):
```
LOGOUT
```

**Verifies**:
- Command is exactly "LOGOUT " (with trailing space)
- No additional parameters

### 3. MYLISTADD Command Tests

#### testMylistAddBasicFormat()
Calls `AniDBApi::MylistAdd()` with required parameters and validates format:
```
MYLISTADD size=<int8>&ed2k=<str>&state=<int2>
```

**Test Flow**:
1. Calls `api->MylistAdd(734003200, "a1b2c3...", 0, 1, "", false)`
2. Retrieves command from database
3. Validates format

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
Calls `AniDBApi::MylistAdd()` with optional parameters:
```
MYLISTADD size=<int8>&ed2k=<str>&viewed=<int2>&storage=<str>&state=<int2>
```

**Optional parameters tested**:
- `viewed`: 0 or 1 (note: function implementation subtracts 1 from input)
- `storage`: Free-form text (e.g., "HDD", "External Drive")

**Verifies**:
- Optional parameters are only added when provided
- Correct parameter formatting
- Viewed parameter is decremented correctly

#### testMylistAddWithEditFlag()
Calls `AniDBApi::MylistAdd()` with edit=true flag:
```
MYLISTADD size=<int8>&ed2k=<str>&edit=1&state=<int2>
```

**Verifies**:
- Edit flag format: `&edit=1`
- Used when updating existing mylist entries (response code 310)

### 4. FILE Command Tests

#### testFileCommandFormat()
Calls `AniDBApi::File()` and validates generated command:
```
FILE size=<int8>&ed2k=<str>&fmask=<hexstr>&amask=<hexstr>
```

**Test Flow**:
1. Calls `api->File(734003200, "a1b2c3...")`
2. Retrieves command from database
3. Validates all required parameters present

**Verifies**:
- Command starts with "FILE "
- All required parameters present
- fmask and amask are hex-encoded

**Example valid command**:
```
FILE size=734003200&ed2k=a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4&fmask=7ff8fef8&amask=f0f0f0f0
```

#### testFileCommandMasks()
Validates fmask and amask encoding from actual `File()` function:
- **Length**: Exactly 8 characters
- **Format**: Lowercase hexadecimal
- **Padding**: Leading zeros when necessary

**Mask meanings**:
- `fmask`: File information mask (which file fields to return)
- `amask`: Anime information mask (which anime fields to return)

**Verification**: Uses regex to extract and validate hex format

### 5. MYLIST Command Tests

#### testMylistCommandWithLid()
Calls `AniDBApi::Mylist()` with a lid and validates:
```
MYLIST lid=<int4>
```

**Test Flow**:
1. Calls `api->Mylist(12345)`
2. Retrieves command from database
3. Validates lid parameter

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
Calls `AniDBApi::Mylist()` with lid=-1 to trigger MYLISTSTATS:
```
MYLISTSTATS
```

**Verifies**:
- Command is exactly "MYLISTSTATS " (with trailing space)
- No parameters required

**Returns**: Total entries, watched count, file sizes, etc.

### 6. Command Name Validation Test

#### testCommandNamesAreValid()
Validates that all command names used in `anidbapi.cpp` match the official AniDB UDP API Definition to prevent typos like the MYLISTSTAT bug.

**Test Flow**:
1. Maintains a list of all valid API commands from https://wiki.anidb.net/UDP_API_Definition
2. Calls each implemented API function (Auth, MylistAdd, File, Mylist, etc.)
3. Extracts the command name from each generated command string
4. Verifies each command name exists in the valid API command list
5. Reports any invalid command names with descriptive error messages

**Valid Commands List** (from API definition):
- Session Management: AUTH, LOGOUT, ENCRYPT, ENCODING, PING, VERSION, UPTIME
- Data Commands: FILE, ANIME, ANIMEDESC, EPISODE, GROUP, GROUPSTATUS, PRODUCER, CHARACTER, CREATOR, CALENDAR, REVIEW, MYLIST, MYLISTSTATS, MYLISTADD, MYLISTDEL, MYLISTMOD, MYLISTEXPORT, MYLISTIMPORT, VOTE, RANDOMRECOMMENDATION, NOTIFICATION, NOTIFYLIST, NOTIFYADD, NOTIFYMOD, NOTIFYDEL, NOTIFYGET, NOTIFYACK, SENDMSG, USER

**Commands Validated**:
- AUTH
- MYLISTADD  
- FILE
- MYLIST
- MYLISTSTATS (previously had typo: MYLISTSTAT)

**Failure Example**:
If code contains `QString("MYLISTSTAT ")` instead of `QString("MYLISTSTATS ")`, test will fail with:
```
FAIL! Command 'MYLISTSTAT' is not in valid API command list - this was the typo bug!
```

**Purpose**: Catches command name typos at test time rather than runtime (598 UNKNOWN COMMAND errors).

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
PASS   : TestAniDBApiCommands::testLogoutCommandFormat()
PASS   : TestAniDBApiCommands::testMylistAddBasicFormat()
PASS   : TestAniDBApiCommands::testMylistAddWithOptionalParameters()
PASS   : TestAniDBApiCommands::testMylistAddWithEditFlag()
PASS   : TestAniDBApiCommands::testFileCommandFormat()
PASS   : TestAniDBApiCommands::testFileCommandMasks()
PASS   : TestAniDBApiCommands::testMylistCommandWithLid()
PASS   : TestAniDBApiCommands::testMylistStatCommandFormat()
PASS   : TestAniDBApiCommands::testCommandNamesAreValid()
PASS   : TestAniDBApiCommands::cleanupTestCase()
Totals: 12 passed, 0 failed, 0 skipped, 0 blacklisted, Xms
********* Finished testing of TestAniDBApiCommands *********
```

## API Commands Reference

### Implemented and Tested Commands

| Command | Purpose | Required Parameters | Optional Parameters | Tested |
|---------|---------|-------------------|-------------------|--------|
| AUTH | Authenticate | user, pass, protover, client, clientver, enc | - | ✅ |
| LOGOUT | End session | - | - | ✅ |
| MYLISTADD | Add file to mylist | size, ed2k, state | viewed, storage, edit | ✅ |
| FILE | Query file info | size, ed2k, fmask, amask | - | ✅ |
| MYLIST | Query mylist entry | lid | - | ✅ |
| MYLISTSTATS | Get mylist stats | - | - | ✅ |

**Note**: All command names are validated against the official AniDB API definition in `testCommandNamesAreValid()` to prevent typos.

### Implementation Files

Command implementations are in:
- `usagi/src/anidbapi.h` - Class declarations
- `usagi/src/anidbapi.cpp` - Command implementations:
  - `AniDBApi::Auth()` (line ~288)
  - `AniDBApi::Logout()` (line ~302)
  - `AniDBApi::MylistAdd()` (line ~312)
  - `AniDBApi::File()` (line ~352)
  - `AniDBApi::Mylist()` (line ~373)

## Test Architecture

### How Tests Work

1. **Setup** (`initTestCase()`):
   - Creates `AniDBApi` instance with test client name/version
   - Sets test username and password
   - Initializes database connection

2. **Test Execution**:
   - Calls actual API function (e.g., `api->MylistAdd(...)`)
   - Function inserts command into `packets` database table
   - Test queries database to retrieve command: `SELECT str FROM packets WHERE processed = 0`
   - Validates command format, parameters, and values

3. **Cleanup** (`cleanup()`):
   - Clears `packets` table between tests
   - Ensures test isolation

4. **Teardown** (`cleanupTestCase()`):
   - Deletes `AniDBApi` instance

### Database Schema

Tests rely on the `packets` table schema:
```sql
CREATE TABLE packets (
    tag INTEGER PRIMARY KEY,
    str TEXT,
    processed BOOL DEFAULT 0,
    sendtime INTEGER,
    got_reply BOOL DEFAULT 0,
    reply TEXT
);
```

Commands are stored in the `str` column before being sent to the API.

## Compliance with AniDB API Guidelines

These tests help ensure compliance with:

1. **Rate Limiting**: Commands are queued (not tested here, but in production)
2. **Parameter Format**: Correct syntax and encoding
3. **Command Structure**: Proper command names and parameter names
4. **Data Types**: Correct numeric and string formatting
5. **Hex Encoding**: Proper fmask/amask format

## What Tests Validate

✅ **Actual Function Output**: Tests call real `AniDBApi` methods  
✅ **Database Integration**: Verifies commands are inserted correctly  
✅ **Command Format**: Validates syntax matches API specification  
✅ **Required Parameters**: Ensures all mandatory fields present  
✅ **Optional Parameters**: Tests conditional parameter inclusion  
✅ **Data Types**: Verifies correct formatting (hex, integers, strings)  
✅ **Parameter Encoding**: Checks separators and structure  

## What Tests Do NOT Validate

❌ **Network Communication**: No actual UDP packets sent  
❌ **Authentication**: No real login/session management  
❌ **Response Parsing**: Commands are tested, not responses  
❌ **Rate Limiting**: Timer/queue behavior not tested  
❌ **Error Handling**: Focus is on successful command generation  

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
