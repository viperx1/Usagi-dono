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

        # Ensure debug info is emitted in a way Windows GDB/LLDB can read
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gcodeview") # optional: generates CodeView info too
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
