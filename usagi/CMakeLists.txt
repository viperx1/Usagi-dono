cmake_minimum_required(VERSION 3.16)

project(usagi LANGUAGES CXX)

# Note: Qt6 is already found in the root CMakeLists.txt, no need to find it again

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
