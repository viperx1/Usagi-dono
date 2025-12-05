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

# Source files
set(SOURCES
    src/anidbapi.cpp
    src/mask.cpp
    src/myanidbapi.cpp
    src/main.cpp
    src/window.cpp
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
    src/mylistfiltersidebar.cpp
    src/watchchunkmanager.cpp
    src/virtualflowlayout.cpp
    src/watchsessionmanager.cpp
    src/applicationsettings.cpp
)

# Header files
set(HEADERS
    src/anidbapi.h
    src/mask.h
    src/main.h
    src/window.h
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
    src/mylistfiltersidebar.h
    src/watchchunkmanager.h
    src/animeutils.h
    src/uicolors.h
    src/virtualflowlayout.h
    src/watchsessionmanager.h
    src/applicationsettings.h
)

# Create executable
if(WIN32)
    add_executable(usagi ${SOURCES} ${HEADERS} usagi.rc)
else()
    add_executable(usagi ${SOURCES} ${HEADERS})
endif()

# Link Qt libraries - DYNAMIC LINKING ONLY
# Simplified linking for dynamic Qt builds
target_link_libraries(usagi PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Gui
    Qt6::Core
    z
)

# Link platform-specific libraries
if(WIN32)
    # Only dbghelp for crash logging
    target_link_libraries(usagi PRIVATE dbghelp)
    
    # Force console subsystem
    target_link_options(usagi PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

# Include directories
target_include_directories(usagi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Copy icon files to build output directory for runtime loading
add_custom_command(TARGET usagi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/usagi.ico
        $<TARGET_FILE_DIR:usagi>/usagi.ico
    COMMENT "Copying usagi.ico to output directory"
)

add_custom_command(TARGET usagi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../usagi.png
        $<TARGET_FILE_DIR:usagi>/usagi.png
    COMMENT "Copying usagi.png to output directory"
)
