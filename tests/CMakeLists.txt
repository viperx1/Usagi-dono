cmake_minimum_required(VERSION 3.16)

project(test_hash LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 COMPONENTS Core Test REQUIRED)

# Enable automatic MOC
set(CMAKE_AUTOMOC ON)

# Enable testing
enable_testing()

# Source files
set(SOURCES
    test_hash.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/hash/ed2k.cpp
)

# Header files
set(HEADERS
    ../usagi/src/hash/md4.h
    ../usagi/src/hash/ed2k.h
)

# Create test executable
add_executable(test_hash ${SOURCES} ${HEADERS})

# Link Qt libraries
target_link_libraries(test_hash
    Qt6::Core
    Qt6::Test
)

# Include directories
target_include_directories(test_hash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Add test
add_test(NAME test_hash COMMAND test_hash)
