cmake_minimum_required(VERSION 3.16)

project(Usagi-dono VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug symbols for crash log stack traces
# This allows crash logs to show function names from Usagi codebase
# GCC/Clang: Add -g for debug symbols and -gdwarf-4 for compatibility with Windows dbghelp
# -gdwarf-4: Use DWARF version 4 (more compatible with Windows debugging tools)
# -g: Include debug information
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -gdwarf-4")

# On Windows with MinGW, also ensure debug info is not stripped
if(WIN32)
    # Don't strip symbols from the executable
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
endif()

# Disable CRYPTOPP_DEBUG to prevent Crypto++ library from installing signal handlers
# that interfere with our crash log handler. CRYPTOPP_DEBUG causes CRYPTOPP_ASSERT
# to raise SIGTRAP (Unix) or call DebugBreak() (Windows), which conflicts with
# our SIGSEGV handler and prevents proper crash log stack trace collection.
# By defining CRYPTOPP_DEBUG=0, we prevent it from being auto-enabled by DEBUG/_DEBUG.
add_compile_definitions(CRYPTOPP_DEBUG=0)

# When using Clang on Windows with MinGW Qt, we need to ensure proper linking
# Clang on Windows may default to using lld-link (MSVC-style), but MinGW Qt uses GNU ABI
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Use MinGW-compatible target
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=x86_64-w64-mingw32")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --target=x86_64-w64-mingw32")
    
    # Use LLD linker with GNU-style interface (not lld-link which is MSVC-style)
    # This must be set globally to prevent CMake from auto-detecting and using lld-link
    # The -fuse-ld=lld flag tells Clang to use LLD with GNU-compatible command line
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=lld")
endif()

# Find Qt6
find_package(Qt6 COMPONENTS Core Gui Widgets Network Sql REQUIRED)

# Enable automatic MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Enable testing globally
enable_testing()

# Add subdirectories
add_subdirectory(usagi)
add_subdirectory(tests)
