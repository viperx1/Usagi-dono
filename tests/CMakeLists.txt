cmake_minimum_required(VERSION 3.16)

project(usagi_tests LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug symbols for crash log stack traces
# This allows crash logs to show function names from Usagi codebase
# GCC/Clang/LLVM: Add -g for debug symbols
# LLVM/Clang on Windows: Also add -gcodeview to generate CodeView format for DbgHelp API
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # LLVM/Clang on Windows with MinGW target generates DWARF by default,
    # but Windows DbgHelp API requires CodeView format to resolve symbols
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -gcodeview")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

# Find Qt6
find_package(Qt6 COMPONENTS Core Test Gui REQUIRED)

# Enable automatic MOC
set(CMAKE_AUTOMOC ON)

# Enable testing
enable_testing()

# Test 1: Hash functions test
set(HASH_TEST_SOURCES
    test_hash.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/hash/ed2k.cpp
)

set(HASH_TEST_HEADERS
    ../usagi/src/hash/md4.h
    ../usagi/src/hash/ed2k.h
)

add_executable(test_hash ${HASH_TEST_SOURCES} ${HASH_TEST_HEADERS})

# On Windows with Clang, add linker options to generate PDB file
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Explicitly specify PDB filename: <output_dir>/<executable_name>.pdb
    target_link_options(test_hash PRIVATE -fuse-ld=lld "-Wl,--pdb=$<TARGET_FILE_DIR:test_hash>/$<TARGET_FILE_BASE_NAME:test_hash>.pdb")
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC/MinGW generates DWARF which Windows DbgHelp cannot reliably read
    message(WARNING "Building test_hash with GCC/MinGW on Windows. Crash logs may not show function names. "
                    "Use Clang/LLVM for full crash log functionality with PDB generation.")
endif()

target_link_libraries(test_hash PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_hash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

add_test(NAME test_hash COMMAND test_hash)

# Test 2: Crash log encoding test
set(CRASHLOG_TEST_SOURCES
    test_crashlog.cpp
    ../usagi/src/crashlog.cpp
)

set(CRASHLOG_TEST_HEADERS
    ../usagi/src/crashlog.h
)

add_executable(test_crashlog ${CRASHLOG_TEST_SOURCES} ${CRASHLOG_TEST_HEADERS})

# On Windows with Clang, add linker options to generate PDB file
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Explicitly specify PDB filename: <output_dir>/<executable_name>.pdb
    target_link_options(test_crashlog PRIVATE -fuse-ld=lld "-Wl,--pdb=$<TARGET_FILE_DIR:test_crashlog>/$<TARGET_FILE_BASE_NAME:test_crashlog>.pdb")
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC/MinGW generates DWARF which Windows DbgHelp cannot reliably read
    message(WARNING "Building test_crashlog with GCC/MinGW on Windows. Crash logs may not show function names. "
                    "Use Clang/LLVM for full crash log functionality with PDB generation.")
endif()

target_link_libraries(test_crashlog PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(test_crashlog PRIVATE dbghelp)
endif()

target_include_directories(test_crashlog PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

add_test(NAME test_crashlog COMMAND test_crashlog)
