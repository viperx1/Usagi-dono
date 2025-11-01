cmake_minimum_required(VERSION 3.16)

project(usagi LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Enable automatic MOC
set(CMAKE_AUTOMOC ON)

# Ensure MOC sees QT_STATIC for static Qt builds (inherited from parent CMakeLists.txt)
# This is necessary so that Qt headers are processed with correct linkage declarations
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    # Make sure MOC-generated files also see QT_STATIC
    set(CMAKE_AUTOMOC_MOC_OPTIONS "-DQT_STATIC" "-DQT_STATICPLUGIN")
endif()

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
    src/crashlog.cpp
    src/epno.cpp
    src/aired.cpp
    src/directorywatcher.cpp
    src/logger.cpp
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
    src/crashlog.h
    src/epno.h
    src/aired.h
    src/directorywatcher.h
    src/logger.h
)

# Create executable
add_executable(usagi ${SOURCES} ${HEADERS})

# For static Qt builds, ensure all compile units see QT_STATIC
if(WIN32)
    get_target_property(qt_core_type Qt6::Core TYPE)
    if(qt_core_type STREQUAL "STATIC_LIBRARY")
        # Explicitly set QT_STATIC for this target
        target_compile_definitions(usagi PRIVATE QT_STATIC QT_STATICPLUGIN)
        
        # Import platform plugins for static Qt on Windows
        qt_import_plugins(usagi
            INCLUDE Qt::QWindowsIntegrationPlugin Qt::QWindowsVistaStylePlugin
        )
    endif()
endif()

# Link Qt libraries
# For static Qt builds, order matters: Qt6::Concurrent depends on Qt6::Core,
# so Qt6::Concurrent must come before Qt6::Core in the link order.
# Qt6::Core is listed again at the end (line 111) to resolve circular dependencies.
target_link_libraries(usagi PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Gui
    Qt6::Concurrent
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
        # Link Qt6::Core again at the end to resolve any circular dependencies
        target_link_libraries(usagi PRIVATE Qt6::Core)
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
endif()

# Include directories
target_include_directories(usagi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
