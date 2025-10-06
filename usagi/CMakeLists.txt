cmake_minimum_required(VERSION 3.16)

project(usagi LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 COMPONENTS Core Gui Widgets Network Sql REQUIRED)

# Enable automatic MOC
set(CMAKE_AUTOMOC ON)

# Source files
set(SOURCES
    src/anidbapi.cpp
    src/main.cpp
    src/window.cpp
    src/hasherthread.cpp
    src/hash/md4.cpp
    src/hash/ed2k.cpp
    src/anidbapi_settings.cpp
    src/Qt-AES-master/qaesencryption.cpp
)

# Header files
set(HEADERS
    src/anidbapi.h
    src/main.h
    src/window.h
    src/hasherthread.h
    src/hash/md4.h
    src/hash/ed2k.h
    src/Qt-AES-master/qaesencryption.h
)

# Create executable
add_executable(usagi ${SOURCES} ${HEADERS})

# Link Qt libraries
target_link_libraries(usagi PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

# Include directories
target_include_directories(usagi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
