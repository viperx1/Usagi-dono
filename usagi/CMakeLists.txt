cmake_minimum_required(VERSION 3.16)

project(usagi LANGUAGES CXX)

# Note: C++ standard is inherited from root CMakeLists.txt (C++17)

# Enable debug symbols for crash log stack traces
# This allows crash logs to show function names from Usagi codebase
# Using DWARF debug format for all platforms
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# Disable CRYPTOPP_DEBUG to prevent Crypto++ library from installing signal handlers
# that interfere with our crash log handler. CRYPTOPP_DEBUG causes CRYPTOPP_ASSERT
# to raise SIGTRAP (Unix) or call DebugBreak() (Windows), which conflicts with
# our SIGSEGV handler and prevents proper crash log stack trace collection.
# By defining CRYPTOPP_DEBUG=0, we prevent it from being auto-enabled by DEBUG/_DEBUG.
add_compile_definitions(CRYPTOPP_DEBUG=0)

# Note: Qt6 is already found in the root CMakeLists.txt, no need to find it again
# Note: CMAKE_AUTOMOC and CMAKE_AUTOMOC_MOC_OPTIONS are set in root CMakeLists.txt

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

# For static Qt builds, import platform plugins
if(WIN32)
    get_target_property(qt_core_type Qt6::Core TYPE)
    if(qt_core_type STREQUAL "STATIC_LIBRARY")
        # Import platform plugins for static Qt on Windows
        # Note: QT_STATIC and QT_STATICPLUGIN are already defined globally in root CMakeLists.txt
        qt_import_plugins(usagi
            INCLUDE Qt::QWindowsIntegrationPlugin Qt::QWindowsVistaStylePlugin
        )
    endif()
endif()

# Link Qt libraries
target_link_libraries(usagi PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Gui
    Qt6::Core
    z
)

# For static Qt builds on Windows, we need additional system libraries
if(WIN32)
    get_target_property(qt_core_type Qt6::Core TYPE)
    if(qt_core_type STREQUAL "STATIC_LIBRARY")
        # Static Qt6 on Windows requires these additional libraries
        target_link_libraries(usagi PRIVATE
            winmm        # Windows multimedia
            version      # Version information
            netapi32     # Network API
            ws2_32       # Windows Sockets (already may be linked by Qt but explicit doesn't hurt)
            iphlpapi     # IP Helper API
            crypt32      # Cryptography API
            ole32        # OLE32
            oleaut32     # OLE Automation
            uuid         # UUID functions
        )
        # Note: Qt6::Core is linked again at the end of the Windows section (line 157)
        # This additional link is required for both static and shared builds to resolve 
        # QMetaType circular dependencies. The implementation at line 157 is outside
        # this static-build-only block so it applies to all Windows builds.
    endif()
endif()

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(usagi PRIVATE dbghelp)
    
    # For static Qt builds, link Qt6::EntryPoint which contains WinMain
    get_target_property(qt_core_type Qt6::Core TYPE)
    if(qt_core_type STREQUAL "STATIC_LIBRARY")
        # Qt6::EntryPoint provides WinMain/main entry point for static builds
        if(TARGET Qt6::EntryPoint)
            target_link_libraries(usagi PRIVATE Qt6::EntryPoint)
        endif()
    endif()
    
    # Force console subsystem for LLVM MinGW
    # Note: -fuse-ld=lld is already set globally in root CMakeLists.txt
    target_link_options(usagi PRIVATE
        "-Wl,--subsystem,console"  # GNU-style subsystem flag instead of MSVC /subsystem:console
    )
    
    # Link Qt6::Core again at the end to resolve circular dependencies
    # This is necessary for both static and shared Qt builds on Windows
    # to resolve QMetaType symbol dependencies (e.g., registerConverterFunction, registerMutableViewFunction)
    # Note: Project requires LLVM MinGW on Windows (enforced in root CMakeLists.txt)
    target_link_libraries(usagi PRIVATE Qt6::Core)
endif()

# Include directories
target_include_directories(usagi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
