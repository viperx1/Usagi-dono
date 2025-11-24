cmake_minimum_required(VERSION 3.16)

project(usagi LANGUAGES CXX)

# FORCE DYNAMIC LINKING - Disable all static Qt configurations
message(STATUS "Configuring usagi for DYNAMIC Qt linking")
set(QT_IS_STATIC FALSE)

# Remove any QT_STATIC definitions
remove_definitions(-DQT_STATIC)
remove_definitions(-DQT_STATICPLUGIN)

# Explicitly define QT_SHARED to force dynamic linking
add_compile_definitions(QT_SHARED)

# Enable automatic MOC
set(CMAKE_AUTOMOC ON)

# Core library source files (excluding main.cpp and window.cpp which are app-specific)
set(CORE_SOURCES
    src/anidbapi.cpp
    src/mask.cpp
    src/myanidbapi.cpp
    src/hasherthread.cpp
    src/hasherthreadpool.cpp
    src/hash/md4.cpp
    src/hash/ed2k.cpp
    src/anidbapi_settings.cpp
    src/Qt-AES-master/qaesencryption.cpp
    src/crashlog.cpp
    src/epno.cpp
    src/aired.cpp
    src/directorywatcher.cpp
    src/logger.cpp
    src/playbackmanager.cpp
    src/playbuttondelegate.cpp
    src/animecard.cpp
    src/flowlayout.cpp
    src/mylistcardmanager.cpp
    src/watchchunkmanager.cpp
)

# Core library header files
set(CORE_HEADERS
    src/anidbapi.h
    src/mask.h
    src/hasherthread.h
    src/hasherthreadpool.h
    src/hash/md4.h
    src/hash/ed2k.h
    src/Qt-AES-master/qaesencryption.h
    src/crashlog.h
    src/epno.h
    src/aired.h
    src/directorywatcher.h
    src/logger.h
    src/playbackmanager.h
    src/playbuttondelegate.h
    src/animecard.h
    src/flowlayout.h
    src/mylistcardmanager.h
    src/watchchunkmanager.h
    src/animeutils.h
    src/uicolors.h
)

# Create a static library with core functionality
# This library will be used by both the main executable and tests
add_library(usagi_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

# Link Qt libraries to the core library
target_link_libraries(usagi_core PUBLIC
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Gui
    Qt6::Core
    z
)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(usagi_core PUBLIC dbghelp)
endif()

# Set include directories for the core library
target_include_directories(usagi_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Application-specific source files
set(APP_SOURCES
    src/main.cpp
    src/window.cpp
)

set(APP_HEADERS
    src/window.h
)

# Create executable
add_executable(usagi ${APP_SOURCES} ${APP_HEADERS})

# Link the core library to the main executable
target_link_libraries(usagi PRIVATE usagi_core)

# Force console subsystem for Windows
if(WIN32)
    target_link_options(usagi PRIVATE
        "-Wl,--subsystem,console"
    )
endif()
