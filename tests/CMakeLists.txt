cmake_minimum_required(VERSION 3.16)

project(usagi_tests LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 COMPONENTS Core Test REQUIRED)

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
)

add_executable(test_crashlog ${CRASHLOG_TEST_SOURCES})

target_link_libraries(test_crashlog PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_crashlog PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

add_test(NAME test_crashlog COMMAND test_crashlog)
