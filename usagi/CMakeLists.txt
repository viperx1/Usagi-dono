cmake_minimum_required(VERSION 3.16)

project(usagi LANGUAGES CXX)

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

# Disable CRYPTOPP_DEBUG to prevent Crypto++ library from installing signal handlers
# that interfere with our crash log handler. CRYPTOPP_DEBUG causes CRYPTOPP_ASSERT
# to raise SIGTRAP (Unix) or call DebugBreak() (Windows), which conflicts with
# our SIGSEGV handler and prevents proper crash log stack trace collection.
# By defining CRYPTOPP_DEBUG=0, we prevent it from being auto-enabled by DEBUG/_DEBUG.
add_compile_definitions(CRYPTOPP_DEBUG=0)

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
    src/crashlog.cpp
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
)

# Create executable
add_executable(usagi ${SOURCES} ${HEADERS})

# On Windows with Clang, add linker options to generate PDB file
# The PDB file is required for DbgHelp to resolve symbols at runtime
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # LLD linker is already set globally in root CMakeLists.txt via -fuse-ld=lld
    # Just specify PDB filename: <output_dir>/<executable_name>.pdb
    target_link_options(usagi PRIVATE "-Wl,--pdb=$<TARGET_FILE_DIR:usagi>/$<TARGET_FILE_BASE_NAME:usagi>.pdb")
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC/MinGW on Windows generates DWARF debug format in the executable
    # Windows DbgHelp API has limited support for DWARF and cannot reliably resolve symbols
    # For proper crash log function names on Windows, use Clang with -gcodeview and LLD linker
    message(WARNING "Building with GCC/MinGW on Windows. Crash logs may not show function names "
                    "from the Usagi codebase because GCC generates DWARF debug format which "
                    "Windows DbgHelp API cannot reliably read. For full crash log functionality, "
                    "use Clang/LLVM compiler which can generate PDB files with CodeView debug format.")
endif()

# Link Qt libraries
target_link_libraries(usagi PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(usagi PRIVATE dbghelp)
endif()

# Include directories
target_include_directories(usagi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
