cmake_minimum_required(VERSION 3.16)

project(Usagi-dono VERSION 1.0.0 LANGUAGES CXX)

# ==========================================================
# 🧠 COMPILER CONFIGURATION
# ==========================================================

# Use modern enough C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================================
# 🧩 DEBUG SYMBOLS & CRASH TRACE SUPPORT
# ==========================================================

# Ensure we're not building a "Release" that strips everything
# You can override this via: cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Generate full DWARF debug symbols with macros
# -g3         : full debug info (includes macros)
# -gdwarf-4   : DWARF v4 for Windows compatibility
# -O0         : disable optimizations for accurate stack traces
# -fno-omit-frame-pointer : keep frame pointers for reliable call stacks
set(DEBUG_FLAGS "-g3 -gdwarf-4 -O0 -fno-omit-frame-pointer")

# Apply globally
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEBUG_FLAGS}")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${DEBUG_FLAGS}")

# ==========================================================
# 🪄 WINDOWS / MINGW / LLVM SPECIAL CASES
# ==========================================================

if(WIN32)
    # Ensure debug info isn't stripped
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Configuring for Clang with MinGW target")

        # Force target to MinGW ABI (so it links against Qt MinGW libs)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=x86_64-w64-mingw32")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --target=x86_64-w64-mingw32")

        # Use the LLD linker with GNU-style interface (not lld-link)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=lld")
        
        # Remove any MSVC-style linker flags that might have been added by CMake's Clang detection
        # This is necessary when using system LLVM on Windows with MinGW target
        string(REPLACE "-fuse-ld=lld-link" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        string(REPLACE "-fuse-ld=lld-link" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
        string(REPLACE "-fuse-ld=lld-link" "" CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}")
        
        # Remove problematic MSVC-style flags that CMake might add
        string(REGEX REPLACE "-nostartfiles" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        string(REGEX REPLACE "-nostdlib" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        string(REGEX REPLACE "-nostartfiles" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
        string(REGEX REPLACE "-nostdlib" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
        
        # Override the link rule template to use GNU-style linking
        # CMake's Clang detection on Windows sets MSVC-style link rules, but we need GNU-style
        # This ensures -nostartfiles/-nostdlib and other MSVC flags don't get added
        set(CMAKE_CXX_LINK_EXECUTABLE
            "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
        set(CMAKE_CXX_CREATE_SHARED_LIBRARY
            "<CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <SONAME_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
        
        message(STATUS "Initial CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
    endif()
endif()

# ==========================================================
# 🧬 DEFINES
# ==========================================================

# Disable Crypto++ debug traps (we handle crashes ourselves)
add_compile_definitions(CRYPTOPP_DEBUG=0)

# ==========================================================
# ⚙️ QT SETUP
# ==========================================================

find_package(Qt6 COMPONENTS Core Gui Widgets Network Sql REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ==========================================================
# 🔧 POST-QT LINKER FLAGS CLEANUP (Windows + Clang only)
# ==========================================================
# Qt's CMake configuration or CMake's platform detection may add MSVC-style
# linker flags when it detects Clang on Windows. We need to remove these
# since we're using MinGW target with GNU-style linking.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Remove MSVC-style -fuse-ld=lld-link flag if present
    string(REPLACE "-fuse-ld=lld-link" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-fuse-ld=lld-link" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    string(REPLACE "-fuse-ld=lld-link" "" CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}")
    
    # Remove problematic MSVC-style flags that might be added by Qt or CMake
    string(REGEX REPLACE "-nostartfiles" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REGEX REPLACE "-nostdlib" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REGEX REPLACE "-nostartfiles" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    string(REGEX REPLACE "-nostdlib" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    
    message(STATUS "Cleaned up linker flags after Qt find_package")
    message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
endif()

# ==========================================================
# 🧪 TESTING
# ==========================================================
enable_testing()

# ==========================================================
# 📁 PROJECT STRUCTURE
# ==========================================================
add_subdirectory(usagi)
add_subdirectory(tests)

# ==========================================================
# 💾 POST-BUILD: Optional symbol separation
# ==========================================================
# You can uncomment this block if you want to generate `.debug` symbol files
# after building. Works on LLVM-MinGW.
#
# add_custom_command(TARGET usagi POST_BUILD
#     COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:usagi> $<TARGET_FILE:usagi>.debug
#     COMMAND ${CMAKE_STRIP} --strip-debug $<TARGET_FILE:usagi>
#     COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=$<TARGET_FILE:usagi>.debug $<TARGET_FILE:usagi>
#     COMMENT "Split debug symbols to separate file for crash symbolization"
# )
