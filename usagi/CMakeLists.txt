cmake_minimum_required(VERSION 3.16)

project(usagi LANGUAGES CXX)

# Check if Qt is statically linked once and reuse the result
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    set(QT_IS_STATIC TRUE)
else()
    set(QT_IS_STATIC FALSE)
endif()

# Note: CMAKE_AUTOMOC_MOC_OPTIONS is set in root CMakeLists.txt for static Qt builds
# to ensure MOC sees QT_STATIC before any Qt headers are processed

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
    src/watchchunkmanager.cpp
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
    src/watchchunkmanager.h
    src/animeutils.h
    src/uicolors.h
)

# Create executable
add_executable(usagi ${SOURCES} ${HEADERS})

# For static Qt builds, ensure all compile units see QT_STATIC
if(WIN32 AND QT_IS_STATIC)
    # Explicitly set QT_STATIC for this target
    target_compile_definitions(usagi PRIVATE QT_STATIC QT_STATICPLUGIN)
    
    # Import platform plugins for static Qt on Windows
    qt_import_plugins(usagi
        INCLUDE Qt::QWindowsIntegrationPlugin Qt::QWindowsVistaStylePlugin
    )
endif()

# Link Qt libraries
# For static Qt builds, link order matters to resolve circular dependencies
# Qt6::Concurrent is included even if not directly used, as it may be required
# for proper static linking with Qt6::Core
target_link_libraries(usagi PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Gui
    Qt6::Concurrent
    Qt6::Core
    z
)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(usagi PRIVATE dbghelp)
    
    # For static Qt builds on Windows, add additional system libraries
    # These are dependencies of Qt's static libraries that must be manually linked
    if(QT_IS_STATIC)
        target_link_libraries(usagi PRIVATE
            winmm        # Windows multimedia
            version      # Version information
            netapi32     # Network API
            ws2_32       # Windows Sockets
            iphlpapi     # IP Helper API
            crypt32      # Cryptography API
            ole32        # OLE32
            oleaut32     # OLE Automation
            uuid         # UUID functions
        )
    endif()
    
    # Force console subsystem for LLVM MinGW
    target_link_options(usagi PRIVATE
        "-Wl,--subsystem,console"
    )
    
    # Link Qt6::Core again at the end to resolve circular dependencies
    target_link_libraries(usagi PRIVATE Qt6::Core)
    
    # For static Qt builds, link Qt6::EntryPoint which contains WinMain
    if(QT_IS_STATIC)
        if(TARGET Qt6::EntryPoint)
            target_link_libraries(usagi PRIVATE Qt6::EntryPoint)
        endif()
    endif()
endif()

# Include directories
target_include_directories(usagi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
