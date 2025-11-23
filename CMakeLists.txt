cmake_minimum_required(VERSION 3.16)

project(Usagi-dono VERSION 1.0.0 LANGUAGES CXX)

# ==========================================================
# 🧠 COMPILER CONFIGURATION
# ==========================================================

# Use modern C++ standard (C++17)
set(CMAKE_CXX_STANDARD 17)
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
    # This project is designed for Windows with LLVM MinGW only
    # We do not support MSVC, GCC, or MSVC-compatible clang-cl
    
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR 
            "This project requires LLVM MinGW Clang compiler on Windows.\n"
            "Current compiler: ${CMAKE_CXX_COMPILER_ID}\n"
            "Please install Qt with LLVM MinGW toolchain and configure CMake to use it.\n"
            "See BUILD.md for instructions.")
    endif()

    # Verify this is LLVM MinGW (not MSVC-compatible clang-cl)
    # LLVM MinGW Clang accepts --target=x86_64-w64-mingw32 flag
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} --target=x86_64-w64-mingw32 --version
        RESULT_VARIABLE MINGW_CHECK_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(NOT MINGW_CHECK_RESULT EQUAL 0)
        message(FATAL_ERROR
            "This project requires LLVM MinGW Clang (not clang-cl or MSVC-compatible Clang).\n"
            "The detected Clang compiler does not support MinGW target.\n"
            "Compiler: ${CMAKE_CXX_COMPILER}\n"
            "Please install Qt with LLVM MinGW toolchain.\n"
            "See BUILD.md for instructions.")
    endif()
    
    message(STATUS "Configuring for LLVM MinGW Clang on Windows")

    # Force CMake to treat Clang as GNU-compatible, not MSVC-compatible
    # This prevents CMake from using MSVC-style flags like -fuse-ld=lld-link
    set(CMAKE_CXX_COMPILER_FRONTEND_VARIANT "GNU")
    set(CMAKE_C_COMPILER_FRONTEND_VARIANT "GNU")

    # Ensure debug info isn't stripped
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")

    # Force target to MinGW ABI (required to link against Qt MinGW libraries)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=x86_64-w64-mingw32")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --target=x86_64-w64-mingw32")

    # Use the LLD linker with GNU-style interface (not lld-link)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=lld")
    
    # Override link rule templates to ensure GNU-style linking for LLVM MinGW
    set(CMAKE_CXX_LINK_EXECUTABLE
        "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
    set(CMAKE_CXX_CREATE_SHARED_LIBRARY
        "<CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <SONAME_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
    
    message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
endif()

# ==========================================================
# 🧬 DEFINES
# ==========================================================

# Disable Crypto++ debug traps (we handle crashes ourselves)
add_compile_definitions(CRYPTOPP_DEBUG=0)

# ==========================================================
# ⚙️ QT SETUP
# ==========================================================

find_package(Qt6 COMPONENTS Core Gui Widgets Network Sql Test REQUIRED)

# Check if Qt is statically linked
# Method 1: Check target type
get_target_property(qt_core_type Qt6::Core TYPE)
set(QT_IS_STATIC_BY_TYPE FALSE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    set(QT_IS_STATIC_BY_TYPE TRUE)
endif()

# Method 2: Check if library file has static extension
# .a files are static libraries (MinGW, GCC, Clang on Windows/Unix/macOS)
# Note: .lib files are NOT checked - on Windows with MSVC, .lib are import libraries
# for DLLs (dynamic), not static libraries. This project uses MinGW, not MSVC.
set(QT_IS_STATIC_BY_EXT FALSE)
# Try IMPORTED_LOCATION first, then IMPORTED_LOCATION_RELEASE, then fall back to LOCATION
get_target_property(qt_core_location Qt6::Core IMPORTED_LOCATION)
if(NOT qt_core_location)
    get_target_property(qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
endif()
if(NOT qt_core_location)
    get_target_property(qt_core_location Qt6::Core LOCATION)
endif()
if(qt_core_location)
    # Check for .a extension (static library for MinGW/GCC/Clang)
    if(qt_core_location MATCHES "\\.a$")
        set(QT_IS_STATIC_BY_EXT TRUE)
    endif()
endif()

# Qt is static if either method detects it
if(QT_IS_STATIC_BY_TYPE OR QT_IS_STATIC_BY_EXT)
    # Define QT_STATIC globally for static Qt builds
    add_compile_definitions(QT_STATIC QT_STATICPLUGIN)
    
    # Set MOC options for static Qt builds
    set(CMAKE_AUTOMOC_MOC_OPTIONS "-DQT_STATIC" "-DQT_STATICPLUGIN")
    
    # Set variable for use in subdirectories
    set(QT_IS_STATIC TRUE CACHE BOOL "Qt is statically linked" FORCE)
else()
    set(QT_IS_STATIC FALSE CACHE BOOL "Qt is statically linked" FORCE)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ==========================================================
# 🔧 POST-QT LINKER FLAGS CLEANUP (LLVM MinGW only)
# ==========================================================
# Qt's CMake configuration may add unexpected flags. Clean them up to ensure
# we maintain GNU-style linking for LLVM MinGW.
if(WIN32)
    # Clean up Qt target interface options
    # Qt6 imported targets might have MSVC-style flags in their INTERFACE_LINK_OPTIONS and INTERFACE_COMPILE_OPTIONS
    foreach(qt_target Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Test)
        if(TARGET ${qt_target})
            # Clean up compile options (removes -Xclang --dependent-lib=msvcrt, -D_DLL, -D_MT)
            get_target_property(qt_compile_options ${qt_target} INTERFACE_COMPILE_OPTIONS)
            if(qt_compile_options)
                list(FILTER qt_compile_options EXCLUDE REGEX "dependent-lib")
                list(FILTER qt_compile_options EXCLUDE REGEX "^-D_DLL$")
                list(FILTER qt_compile_options EXCLUDE REGEX "^-D_MT$")
                set_target_properties(${qt_target} PROPERTIES INTERFACE_COMPILE_OPTIONS "${qt_compile_options}")
            endif()
            
            # Clean up link options (removes -fuse-ld=lld-link and MSVC-style subsystem flags)
            get_target_property(qt_link_options ${qt_target} INTERFACE_LINK_OPTIONS)
            if(qt_link_options)
                list(FILTER qt_link_options EXCLUDE REGEX "^-fuse-ld=lld-link$")
                list(FILTER qt_link_options EXCLUDE REGEX "^/subsystem:")
                list(FILTER qt_link_options EXCLUDE REGEX "^-Xlinker$")
                list(FILTER qt_link_options EXCLUDE REGEX "^/INCREMENTAL")
                list(FILTER qt_link_options EXCLUDE REGEX "^/DEBUG")
                list(FILTER qt_link_options EXCLUDE REGEX "^-nostartfiles$")
                list(FILTER qt_link_options EXCLUDE REGEX "^-nostdlib$")
                set_target_properties(${qt_target} PROPERTIES INTERFACE_LINK_OPTIONS "${qt_link_options}")
            endif()
        endif()
    endforeach()
    
    message(STATUS "Post-Qt CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
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
