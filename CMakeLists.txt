cmake_minimum_required(VERSION 3.16)

project(Usagi-dono VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug symbols for crash log stack traces
# This allows crash logs to show function names from Usagi codebase
if(MSVC)
    # MSVC: Add /Zi for debug info and /DEBUG for linker
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
else()
    # GCC/Clang: Add -g for debug symbols
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

# When using Clang on Windows with MinGW Qt, we need to ensure proper linking
# Clang on Windows may default to MSVC-style linking, but MinGW Qt uses GNU ABI
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Use MinGW-compatible target
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=x86_64-w64-mingw32")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --target=x86_64-w64-mingw32")
    
    # Override CMake's automatic linker detection to prevent use of lld-link
    # CMake automatically adds -fuse-ld=lld-link for Clang on Windows, but this
    # is incompatible with MinGW target. Remove it from all linker flag variables.
    string(REGEX REPLACE "-fuse-ld=[^ ]+" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REGEX REPLACE "-fuse-ld=[^ ]+" "" CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}")
    string(REGEX REPLACE "-fuse-ld=[^ ]+" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    string(REGEX REPLACE "-fuse-ld=[^ ]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    # Also clear cached linker flags that may contain -fuse-ld=lld-link
    set(CMAKE_EXE_LINKER_FLAGS_INIT "")
    
    set(CMAKE_LINKER "ld")
    set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
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
