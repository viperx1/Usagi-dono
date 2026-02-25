cmake_minimum_required(VERSION 3.16)

project(usagi_tests LANGUAGES CXX)

enable_testing()

# ==========================================================
# DEBUG INFO - Print environment details for CI debugging
# ==========================================================
message(STATUS "========================================")
message(STATUS "Test Build Configuration Debug Info")
message(STATUS "========================================")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
if(WIN32)
    message(STATUS "Platform: Windows")
    message(STATUS "MINGW: ${MINGW}")
endif()
message(STATUS "========================================")

# ==========================================================
# STATIC LINKING FIX FOR WINDOWS MINGW
# ==========================================================
# Fix for Windows CI crashes: Multiple conflicting libstdc++-6.dll
# versions in PATH cause segfaults and heap corruption.
# Static linking embeds C++ runtime into executables.
if(WIN32 AND MINGW)
    message(STATUS "Enabling static C++ runtime linking for MinGW to avoid DLL conflicts")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
endif()

# ==========================================================
# FORCE DYNAMIC Qt LINKING - Ensure consistency with main build
# ==========================================================
message(STATUS "Configuring tests for DYNAMIC Qt linking")

# Explicitly disable static Qt
set(QT_IS_STATIC FALSE)
set(QT_IS_STATIC_BUILD FALSE)

# Remove any QT_STATIC definitions that may have been inherited
remove_definitions(-DQT_STATIC)
remove_definitions(-DQT_STATICPLUGIN)

# Force QT_SHARED definition to ensure dynamic linking
add_compile_definitions(QT_SHARED)

# Enable automatic MOC for test files that include .moc files
set(CMAKE_AUTOMOC ON)

# Helper function to mark usagi .cpp source files with SKIP_AUTOMOC
# This prevents duplicate MOC processing for .cpp files while allowing
# AUTOMOC to process .h files that contain Q_OBJECT macros
function(skip_automoc_for_usagi_sources target_name)
    # First, ensure AUTOMOC is enabled for this target
    set_target_properties(${target_name} PROPERTIES AUTOMOC ON)
    
    get_target_property(sources ${target_name} SOURCES)
    foreach(source ${sources})
        # Get the absolute path to properly check the source
        get_filename_component(abs_source "${source}" ABSOLUTE)
        get_filename_component(source_dir "${abs_source}" DIRECTORY)
        get_filename_component(source_name "${abs_source}" NAME)
        get_filename_component(source_ext "${abs_source}" LAST_EXT)
        
        # Skip AUTOMOC only for .cpp files from usagi/src/ directory, NOT for:
        # - test files (start with "test_" prefix)
        # - header files (.h) which need AUTOMOC for Q_OBJECT classes
        if(source_dir MATCHES ".*/usagi/src.*" AND NOT source_name MATCHES "^test_.*" AND source_ext STREQUAL ".cpp")
            message(STATUS "Skipping AUTOMOC for usagi source: ${source}")
            set_source_files_properties(${source} PROPERTIES SKIP_AUTOMOC ON)
        endif()
    endforeach()
endfunction()

# Test 1: Hash functions test
set(HASH_TEST_SOURCES
    test_hash.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/logger.cpp
)

set(HASH_TEST_HEADERS
    ../usagi/src/hash/md4.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/logger.h
)

add_executable(test_hash ${HASH_TEST_SOURCES} ${HASH_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_hash)

target_link_libraries(test_hash PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_hash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem

if(WIN32)
    target_link_options(test_hash PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hash COMMAND test_hash -v2)

# Test 2: Mask byte order test
set(MASK_TEST_SOURCES
    test_mask.cpp
    ../usagi/src/mask.cpp
)

set(MASK_TEST_HEADERS
    ../usagi/src/mask.h
)

add_executable(test_mask ${MASK_TEST_SOURCES} ${MASK_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_mask)

target_link_libraries(test_mask PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_mask PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_mask PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mask COMMAND test_mask -v2)

# Test 3: ANIME mask parsing test
set(ANIME_MASK_PARSING_TEST_SOURCES
    test_anime_mask_parsing.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(ANIME_MASK_PARSING_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/mask.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_anime_mask_parsing ${ANIME_MASK_PARSING_TEST_SOURCES} ${ANIME_MASK_PARSING_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_anime_mask_parsing)

target_compile_definitions(test_anime_mask_parsing PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_anime_mask_parsing PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_anime_mask_parsing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_anime_mask_parsing PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_anime_mask_parsing COMMAND test_anime_mask_parsing -v2)

# Test 4: Crash log encoding test
set(CRASHLOG_TEST_SOURCES
    test_crashlog.cpp
    ../usagi/src/crashlog.cpp
)

set(CRASHLOG_TEST_HEADERS
    ../usagi/src/crashlog.h
)

add_executable(test_crashlog ${CRASHLOG_TEST_SOURCES} ${CRASHLOG_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_crashlog)

target_link_libraries(test_crashlog PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(test_crashlog PRIVATE dbghelp)
    
    # Windows console subsystem
    
    target_link_options(test_crashlog PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

target_include_directories(test_crashlog PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

add_test(NAME test_crashlog COMMAND test_crashlog -v2)

# Test 3: AniDB API command format tests
set(ANIDBAPI_TEST_SOURCES
    test_anidbapi.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(ANIDBAPI_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_anidbapi ${ANIDBAPI_TEST_SOURCES} ${ANIDBAPI_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_anidbapi)

target_compile_definitions(test_anidbapi PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_anidbapi PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_anidbapi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_anidbapi PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_anidbapi COMMAND test_anidbapi -v2)

# Test 4: Compression support tests
set(COMPRESSION_TEST_SOURCES
    test_compression.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(COMPRESSION_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_compression ${COMPRESSION_TEST_SOURCES} ${COMPRESSION_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_compression)

target_compile_definitions(test_compression PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_compression PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_compression PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_compression PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_compression COMMAND test_compression -v2)


# Test: AniDB Timeout and Retry tests
set(TIMEOUT_RETRY_TEST_SOURCES
    test_anidb_timeout_retry.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(TIMEOUT_RETRY_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_anidb_timeout_retry ${TIMEOUT_RETRY_TEST_SOURCES} ${TIMEOUT_RETRY_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_anidb_timeout_retry)

target_compile_definitions(test_anidb_timeout_retry PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_anidb_timeout_retry PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_anidb_timeout_retry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_anidb_timeout_retry PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_anidb_timeout_retry COMMAND test_anidb_timeout_retry -v2)

# Test 4: Anime titles import tests
set(ANIME_TITLES_TEST_SOURCES
    test_anime_titles.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(ANIME_TITLES_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_anime_titles ${ANIME_TITLES_TEST_SOURCES} ${ANIME_TITLES_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_anime_titles)

target_compile_definitions(test_anime_titles PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_anime_titles PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_anime_titles PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_anime_titles PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_anime_titles COMMAND test_anime_titles -v2)

# Test 5: URL extraction from notifications
add_executable(test_url_extraction test_url_extraction.cpp)
skip_automoc_for_usagi_sources(test_url_extraction)

target_link_libraries(test_url_extraction PRIVATE
    Qt6::Core
    Qt6::Test
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_url_extraction PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_url_extraction COMMAND test_url_extraction -v2)

# Test 6: MyList XML Parser
add_executable(test_mylist_xml_parser test_mylist_xml_parser.cpp)
skip_automoc_for_usagi_sources(test_mylist_xml_parser)

target_link_libraries(test_mylist_xml_parser PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_mylist_xml_parser PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_xml_parser COMMAND test_mylist_xml_parser -v2)

# Test 7: Export template verification in notifications
add_executable(test_export_template_verification test_export_template_verification.cpp)
skip_automoc_for_usagi_sources(test_export_template_verification)

target_link_libraries(test_export_template_verification PRIVATE
    Qt6::Core
    Qt6::Test
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_export_template_verification PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_export_template_verification COMMAND test_export_template_verification -v2)

# Test 8: Episode number type (epno)
add_executable(test_epno test_epno.cpp ../usagi/src/epno.cpp ../usagi/src/logger.cpp ../usagi/src/logger.h)
skip_automoc_for_usagi_sources(test_epno)

target_link_libraries(test_epno PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_epno PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_epno PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_epno COMMAND test_epno -v2)

# Test for file version extraction from AniDB state bits
add_executable(test_file_version_extraction test_file_version_extraction.cpp ../usagi/src/logger.cpp ../usagi/src/logger.h)
skip_automoc_for_usagi_sources(test_file_version_extraction)

target_link_libraries(test_file_version_extraction PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_file_version_extraction PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_file_version_extraction PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_file_version_extraction COMMAND test_file_version_extraction -v2)

# Test 9: Episode column format verification
add_executable(test_episode_column_format test_episode_column_format.cpp)
skip_automoc_for_usagi_sources(test_episode_column_format)

target_link_libraries(test_episode_column_format PRIVATE
    Qt6::Core
    Qt6::Test
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_episode_column_format PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_episode_column_format COMMAND test_episode_column_format -v2)

# Test 10: MyList 221 response fix verification
add_executable(test_mylist_221_fix test_mylist_221_fix.cpp)
skip_automoc_for_usagi_sources(test_mylist_221_fix)

target_link_libraries(test_mylist_221_fix PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_mylist_221_fix PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_221_fix COMMAND test_mylist_221_fix -v2)

# Test 11: Evangelion Ha fix - episode column display for movies with specials
add_executable(test_evangelion_ha_fix test_evangelion_ha_fix.cpp)
skip_automoc_for_usagi_sources(test_evangelion_ha_fix)

target_link_libraries(test_evangelion_ha_fix PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_evangelion_ha_fix PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_evangelion_ha_fix COMMAND test_evangelion_ha_fix -v2)

# Test 12: Aired date type
add_executable(test_aired test_aired.cpp ../usagi/src/aired.cpp ../usagi/src/logger.cpp ../usagi/src/logger.h)
skip_automoc_for_usagi_sources(test_aired)

target_link_libraries(test_aired PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_aired PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_aired PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_aired COMMAND test_aired -v2)

# Test 13: MyList Type and Aired columns integration
add_executable(test_mylist_type_aired test_mylist_type_aired.cpp ../usagi/src/aired.cpp ../usagi/src/logger.cpp ../usagi/src/logger.h)
skip_automoc_for_usagi_sources(test_mylist_type_aired)

target_link_libraries(test_mylist_type_aired PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

target_include_directories(test_mylist_type_aired PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_mylist_type_aired PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_type_aired COMMAND test_mylist_type_aired -v2)

# Test 14: Mylist columns fix test (Type and Aired columns)
add_executable(test_mylist_columns_fix test_mylist_columns_fix.cpp ../usagi/src/aired.cpp ../usagi/src/logger.cpp ../usagi/src/logger.h)
skip_automoc_for_usagi_sources(test_mylist_columns_fix)

target_link_libraries(test_mylist_columns_fix PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Widgets
    Qt6::Test
)
target_include_directories(test_mylist_columns_fix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_mylist_columns_fix PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_columns_fix COMMAND test_mylist_columns_fix -v2)

# Test 15: MyList aired sorting
add_executable(test_mylist_aired_sorting test_mylist_aired_sorting.cpp ../usagi/src/aired.cpp ../usagi/src/logger.cpp ../usagi/src/logger.h)
skip_automoc_for_usagi_sources(test_mylist_aired_sorting)

target_link_libraries(test_mylist_aired_sorting PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)
target_include_directories(test_mylist_aired_sorting PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_mylist_aired_sorting PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_aired_sorting COMMAND test_mylist_aired_sorting -v2)

# Test 16: Directory watcher
add_executable(test_directorywatcher test_directorywatcher.cpp ../usagi/src/directorywatcher.cpp ../usagi/src/directorywatcher.h ../usagi/src/logger.cpp ../usagi/src/logger.h)
skip_automoc_for_usagi_sources(test_directorywatcher)

# Link Qt libraries
target_link_libraries(test_directorywatcher PRIVATE
    Qt6::Test
    Qt6::Sql
    Qt6::Core
)
target_include_directories(test_directorywatcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_directorywatcher PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_directorywatcher COMMAND test_directorywatcher -v2)

# Test 17: Hash storage with ed2k_hash column
set(HASH_STORAGE_TEST_SOURCES
    test_hash_storage.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(HASH_STORAGE_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_hash_storage ${HASH_STORAGE_TEST_SOURCES} ${HASH_STORAGE_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_hash_storage)

target_compile_definitions(test_hash_storage PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_hash_storage PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_hash_storage PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_hash_storage PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hash_storage COMMAND test_hash_storage -v2)


# Test 18: Logger unified logging system
set(LOGGER_TEST_SOURCES
    test_logger.cpp
    ../usagi/src/logger.cpp
)

set(LOGGER_TEST_HEADERS
    ../usagi/src/logger.h
)

add_executable(test_logger ${LOGGER_TEST_SOURCES} ${LOGGER_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_logger)

target_compile_definitions(test_logger PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_logger PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Gui
)

target_include_directories(test_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_logger PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_logger COMMAND test_logger -v2)

# Test 19: Hash reuse functionality
set(HASH_REUSE_TEST_SOURCES
    test_hash_reuse.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(HASH_REUSE_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_hash_reuse ${HASH_REUSE_TEST_SOURCES} ${HASH_REUSE_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_hash_reuse)

target_compile_definitions(test_hash_reuse PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_hash_reuse PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_hash_reuse PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_hash_reuse PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hash_reuse COMMAND test_hash_reuse -v2)

# Test 20: Batch LocalIdentify functionality
set(BATCH_LOCALIDENTIFY_TEST_SOURCES
    test_batch_localidentify.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(BATCH_LOCALIDENTIFY_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_batch_localidentify ${BATCH_LOCALIDENTIFY_TEST_SOURCES} ${BATCH_LOCALIDENTIFY_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_batch_localidentify)

target_compile_definitions(test_batch_localidentify PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_batch_localidentify PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_batch_localidentify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_batch_localidentify PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_batch_localidentify COMMAND test_batch_localidentify -v2)

# Test 21: Hash duplicate reuse functionality
set(HASH_DUPLICATE_REUSE_TEST_SOURCES
    test_hash_duplicate_reuse.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(HASH_DUPLICATE_REUSE_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_hash_duplicate_reuse ${HASH_DUPLICATE_REUSE_TEST_SOURCES} ${HASH_DUPLICATE_REUSE_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_hash_duplicate_reuse)

target_compile_definitions(test_hash_duplicate_reuse PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_hash_duplicate_reuse PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    z
)

target_include_directories(test_hash_duplicate_reuse PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_hash_duplicate_reuse PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hash_duplicate_reuse COMMAND test_hash_duplicate_reuse -v2)

# Test 22: Database connection validation
add_executable(test_database_validation test_database_validation.cpp)
skip_automoc_for_usagi_sources(test_database_validation)

target_link_libraries(test_database_validation PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_database_validation PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_database_validation COMMAND test_database_validation -v2)

# Test 23: Thread safety for getLocalFileHash
set(THREAD_SAFETY_TEST_SOURCES
    test_thread_safety.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(THREAD_SAFETY_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_thread_safety ${THREAD_SAFETY_TEST_SOURCES} ${THREAD_SAFETY_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_thread_safety)

target_compile_definitions(test_thread_safety PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_thread_safety PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_thread_safety PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_thread_safety PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_thread_safety COMMAND test_thread_safety -v2)

# Test 24: Immediate identification after hashing
set(IMMEDIATE_IDENTIFICATION_TEST_SOURCES
    test_immediate_identification.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(IMMEDIATE_IDENTIFICATION_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_immediate_identification ${IMMEDIATE_IDENTIFICATION_TEST_SOURCES} ${IMMEDIATE_IDENTIFICATION_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_immediate_identification)

target_compile_definitions(test_immediate_identification PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_immediate_identification PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_immediate_identification PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_immediate_identification PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_immediate_identification COMMAND test_immediate_identification -v2)

# Test: Batch hash retrieval functionality
set(BATCH_HASH_RETRIEVAL_TEST_SOURCES
    test_batch_hash_retrieval.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(BATCH_HASH_RETRIEVAL_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_batch_hash_retrieval ${BATCH_HASH_RETRIEVAL_TEST_SOURCES} ${BATCH_HASH_RETRIEVAL_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_batch_hash_retrieval)

target_compile_definitions(test_batch_hash_retrieval PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_batch_hash_retrieval PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_batch_hash_retrieval PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_batch_hash_retrieval PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_batch_hash_retrieval COMMAND test_batch_hash_retrieval -v2)

# Test 25: UI freeze fix for pre-hashed files
set(UI_FREEZE_FIX_TEST_SOURCES
    test_ui_freeze_fix.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(UI_FREEZE_FIX_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_ui_freeze_fix ${UI_FREEZE_FIX_TEST_SOURCES} ${UI_FREEZE_FIX_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_ui_freeze_fix)

target_compile_definitions(test_ui_freeze_fix PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_ui_freeze_fix PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_ui_freeze_fix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_ui_freeze_fix PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_ui_freeze_fix COMMAND test_ui_freeze_fix -v2)

# Test 26: HasherThread runs in separate thread
set(HASHER_THREAD_TEST_SOURCES
    test_hasher_thread.cpp
    ../usagi/src/hasherthread.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/myanidbapi.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(HASHER_THREAD_TEST_HEADERS
    ../usagi/src/hasherthread.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/anidbapi.h
    ../usagi/src/main.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_hasher_thread ${HASHER_THREAD_TEST_SOURCES} ${HASHER_THREAD_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_hasher_thread)

target_compile_definitions(test_hasher_thread PRIVATE CRYPTOPP_DEBUG=0)

# Link Qt libraries
# Note: For static Qt builds, order matters. Qt6::Test must come before other Qt modules
# to ensure QSignalSpy::wait() and other Qt Test methods are properly linked.
target_link_libraries(test_hasher_thread PRIVATE
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Core
    z
)

target_include_directories(test_hasher_thread PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_hasher_thread PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hasher_thread COMMAND test_hasher_thread -v2)


# Test: HasherThreadPool multithreading
set(HASHER_THREADPOOL_TEST_SOURCES
    test_hasher_threadpool.cpp
    ../usagi/src/hasherthread.cpp
    ../usagi/src/hasherthreadpool.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/myanidbapi.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(HASHER_THREADPOOL_TEST_HEADERS
    ../usagi/src/hasherthread.h
    ../usagi/src/hasherthreadpool.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/anidbapi.h
    ../usagi/src/main.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_hasher_threadpool ${HASHER_THREADPOOL_TEST_SOURCES} ${HASHER_THREADPOOL_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_hasher_threadpool)

target_compile_definitions(test_hasher_threadpool PRIVATE CRYPTOPP_DEBUG=0)

# Link Qt libraries
target_link_libraries(test_hasher_threadpool PRIVATE
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Core
    z
)

target_include_directories(test_hasher_threadpool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_hasher_threadpool PRIVATE
        "-Wl,--subsystem,console"
    )
    
    # For static Qt builds on Windows, import platform plugins
    get_target_property(qt_core_type Qt6::Core TYPE)
    if(qt_core_type STREQUAL "STATIC_LIBRARY")
        qt_import_plugins(test_hasher_threadpool
            INCLUDE
                Qt::QWindowsIntegrationPlugin
                Qt::QWindowsVistaStylePlugin
                Qt::QSQLiteDriverPlugin
        )
    endif()
endif()

add_test(NAME test_hasher_threadpool COMMAND test_hasher_threadpool -v2)


# Test: Stop non-blocking behavior (UI freeze fix)
set(STOP_NON_BLOCKING_TEST_SOURCES
    test_stop_non_blocking.cpp
    ../usagi/src/hasherthread.cpp
    ../usagi/src/hasherthreadpool.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/myanidbapi.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(STOP_NON_BLOCKING_TEST_HEADERS
    ../usagi/src/hasherthread.h
    ../usagi/src/hasherthreadpool.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/anidbapi.h
    ../usagi/src/main.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_stop_non_blocking ${STOP_NON_BLOCKING_TEST_SOURCES} ${STOP_NON_BLOCKING_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_stop_non_blocking)

target_compile_definitions(test_stop_non_blocking PRIVATE CRYPTOPP_DEBUG=0)

# Link Qt libraries
target_link_libraries(test_stop_non_blocking PRIVATE
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Core
    z
)

target_include_directories(test_stop_non_blocking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_stop_non_blocking PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_stop_non_blocking COMMAND test_stop_non_blocking -v2)


# Test: Truncated API response handling
set(TRUNCATED_RESPONSE_TEST_SOURCES
    test_truncated_response.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(TRUNCATED_RESPONSE_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_truncated_response ${TRUNCATED_RESPONSE_TEST_SOURCES} ${TRUNCATED_RESPONSE_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_truncated_response)

target_compile_definitions(test_truncated_response PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_truncated_response PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_truncated_response PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_truncated_response PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_truncated_response COMMAND test_truncated_response -v2)


# Test: API optimization (ban handling and database checks)
set(API_OPTIMIZATION_TEST_SOURCES
    test_api_optimization.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(API_OPTIMIZATION_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_api_optimization ${API_OPTIMIZATION_TEST_SOURCES} ${API_OPTIMIZATION_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_api_optimization)

target_compile_definitions(test_api_optimization PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_api_optimization PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_api_optimization PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_api_optimization PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_api_optimization COMMAND test_api_optimization -v2)

# Test: MyListCardManager test
set(MYLISTCARDMANAGER_TEST_SOURCES
    test_mylistcardmanager.cpp
    ../usagi/src/mylistcardmanager.cpp
    ../usagi/src/animecard.cpp
    ../usagi/src/flowlayout.cpp
    ../usagi/src/virtualflowlayout.cpp
    ../usagi/src/playbuttondelegate.cpp
    ../usagi/src/epno.cpp
    ../usagi/src/aired.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/myanidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
    ../usagi/src/watchsessionmanager.cpp
    ../usagi/src/watchchunkmanager.cpp
    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/sessioninfo.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
    ../usagi/src/animestats.cpp
    ../usagi/src/cachedanimedata.cpp
    ../usagi/src/taginfo.cpp
    ../usagi/src/cardfileinfo.cpp
    ../usagi/src/cardepisodeinfo.cpp
    ../usagi/src/relationdata.cpp
    ../usagi/src/animechain.cpp
)

set(MYLISTCARDMANAGER_TEST_HEADERS
    ../usagi/src/mylistcardmanager.h
    ../usagi/src/animecard.h
    ../usagi/src/flowlayout.h
    ../usagi/src/virtualflowlayout.h
    ../usagi/src/playbuttondelegate.h
    ../usagi/src/epno.h
    ../usagi/src/aired.h
    ../usagi/src/anidbapi.h
    ../usagi/src/main.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
    ../usagi/src/watchsessionmanager.h
    ../usagi/src/watchchunkmanager.h
    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
    ../usagi/src/sessioninfo.h
    ../usagi/src/animechain.h
    ../usagi/src/animestats.h
    ../usagi/src/cachedanimedata.h
    ../usagi/src/taginfo.h
    ../usagi/src/cardfileinfo.h
    ../usagi/src/cardepisodeinfo.h
    ../usagi/src/relationdata.h
)

add_executable(test_mylistcardmanager ${MYLISTCARDMANAGER_TEST_SOURCES} ${MYLISTCARDMANAGER_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_mylistcardmanager)

target_compile_definitions(test_mylistcardmanager PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_mylistcardmanager PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_mylistcardmanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_mylistcardmanager PRIVATE
        "-Wl,--subsystem,console"
    )
    
    # For static Qt builds on Windows, import platform plugins
    get_target_property(qt_core_type Qt6::Core TYPE)
    if(qt_core_type STREQUAL "STATIC_LIBRARY")
        qt_import_plugins(test_mylistcardmanager
            INCLUDE
                Qt::QWindowsIntegrationPlugin
                Qt::QWindowsVistaStylePlugin
                Qt::QSQLiteDriverPlugin
        )
    endif()
endif()

add_test(NAME test_mylistcardmanager COMMAND test_mylistcardmanager -v2)

# Test: Background loading functionality
set(BACKGROUND_LOADING_TEST_SOURCES
    test_background_loading.cpp
)

add_executable(test_background_loading ${BACKGROUND_LOADING_TEST_SOURCES})
skip_automoc_for_usagi_sources(test_background_loading)

target_compile_definitions(test_background_loading PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_background_loading PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_background_loading PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_background_loading COMMAND test_background_loading -v2)

# Test: WatchChunkManager test
set(WATCHCHUNKMANAGER_TEST_SOURCES
    test_watchchunkmanager.cpp
    ../usagi/src/watchchunkmanager.cpp
    ../usagi/src/logger.cpp
)

set(WATCHCHUNKMANAGER_TEST_HEADERS
    ../usagi/src/watchchunkmanager.h
    ../usagi/src/logger.h
)

add_executable(test_watchchunkmanager ${WATCHCHUNKMANAGER_TEST_SOURCES} ${WATCHCHUNKMANAGER_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_watchchunkmanager)

target_compile_definitions(test_watchchunkmanager PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_watchchunkmanager PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
    z
)

target_include_directories(test_watchchunkmanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_watchchunkmanager PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_watchchunkmanager COMMAND test_watchchunkmanager -v2)

# Test: Unknown files persistence test
set(UNKNOWN_FILES_PERSISTENCE_TEST_SOURCES
    test_unknown_files_persistence.cpp
    ../usagi/src/logger.cpp
)

set(UNKNOWN_FILES_PERSISTENCE_TEST_HEADERS
    ../usagi/src/logger.h
)

add_executable(test_unknown_files_persistence ${UNKNOWN_FILES_PERSISTENCE_TEST_SOURCES} ${UNKNOWN_FILES_PERSISTENCE_TEST_HEADERS})

set_target_properties(test_unknown_files_persistence PROPERTIES AUTOMOC ON)

target_compile_definitions(test_unknown_files_persistence PRIVATE CRYPTOPP_DEBUG=0 USAGI_TEST_MODE=1)

target_link_libraries(test_unknown_files_persistence PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

target_include_directories(test_unknown_files_persistence PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_unknown_files_persistence PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_unknown_files_persistence COMMAND test_unknown_files_persistence -v2)

# Test: WatchSessionManager test
set(WATCHSESSIONMANAGER_TEST_SOURCES
    test_watchsessionmanager.cpp
    ../usagi/src/watchsessionmanager.cpp
    ../usagi/src/logger.cpp
    ../usagi/src/sessioninfo.cpp
)

set(WATCHSESSIONMANAGER_TEST_HEADERS
    ../usagi/src/watchsessionmanager.h
    ../usagi/src/logger.h
    ../usagi/src/sessioninfo.h
)

add_executable(test_watchsessionmanager ${WATCHSESSIONMANAGER_TEST_SOURCES} ${WATCHSESSIONMANAGER_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_watchsessionmanager)

target_compile_definitions(test_watchsessionmanager PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_watchsessionmanager PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

target_include_directories(test_watchsessionmanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_watchsessionmanager PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_watchsessionmanager COMMAND test_watchsessionmanager -v2)

# ==========================================================
# TEST: Duplicate Detection
# ==========================================================
set(DUPLICATE_DETECTION_TEST_SOURCES
    test_duplicate_detection.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(DUPLICATE_DETECTION_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_duplicate_detection ${DUPLICATE_DETECTION_TEST_SOURCES} ${DUPLICATE_DETECTION_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_duplicate_detection)

target_compile_definitions(test_duplicate_detection PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_duplicate_detection PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    z
)

target_include_directories(test_duplicate_detection PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_duplicate_detection PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_duplicate_detection COMMAND test_duplicate_detection -v2)

# ==========================================================
# TEST: Bitrate Preferences
# ==========================================================
set(BITRATE_PREFERENCES_TEST_SOURCES
    test_bitrate_preferences.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/watchsessionmanager.cpp
    ../usagi/src/watchchunkmanager.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp

    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/sessioninfo.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
)

set(BITRATE_PREFERENCES_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/mask.h
    ../usagi/src/watchsessionmanager.h
    ../usagi/src/watchchunkmanager.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h

    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
)

add_executable(test_bitrate_preferences ${BITRATE_PREFERENCES_TEST_SOURCES} ${BITRATE_PREFERENCES_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_bitrate_preferences)

target_compile_definitions(test_bitrate_preferences PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_bitrate_preferences PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    z
)

target_include_directories(test_bitrate_preferences PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_bitrate_preferences PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_bitrate_preferences COMMAND test_bitrate_preferences -v2)

# ==========================================================
# test_anidbfileinfo - AniDBFileInfo class tests
# ==========================================================
add_executable(test_anidbfileinfo
    test_anidbfileinfo.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/anidbfileinfo.cpp
)

target_link_libraries(test_anidbfileinfo PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_anidbfileinfo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_anidbfileinfo PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_anidbfileinfo COMMAND test_anidbfileinfo -v2)

# ==========================================================
# test_card_filtering - Card filtering logic tests
# ==========================================================
add_executable(test_card_filtering
    test_card_filtering.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animemetadatacache.cpp
)

target_link_libraries(test_card_filtering PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_card_filtering PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_card_filtering PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_card_filtering COMMAND test_card_filtering -v2)



# ==========================================================
# test_relationdata - RelationData class tests
# ==========================================================
add_executable(test_relationdata
    test_relationdata.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/relationdata.cpp
)

target_link_libraries(test_relationdata PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_relationdata PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_relationdata PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_relationdata COMMAND test_relationdata -v2)

# ==========================================================
# test_cardcreationdata_api - CardCreationData API tests
# ==========================================================
add_executable(test_cardcreationdata_api
    test_cardcreationdata_api.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/relationdata.cpp
)

target_link_libraries(test_cardcreationdata_api PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_cardcreationdata_api PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_cardcreationdata_api PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_cardcreationdata_api COMMAND test_cardcreationdata_api -v2)

# ==========================================================
# test_animechain - AnimeChain class tests
# ==========================================================
add_executable(test_animechain
    test_animechain.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animechain.cpp
)

target_link_libraries(test_animechain PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_animechain PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_animechain PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_animechain COMMAND test_animechain -v2)

# ==========================================================
# test_chain_sorting - AnimeChain sorting criteria tests
# ==========================================================
add_executable(test_chain_sorting
    test_chain_sorting.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animechain.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animestats.cpp
)

target_link_libraries(test_chain_sorting PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_chain_sorting PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_chain_sorting PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_chain_sorting COMMAND test_chain_sorting -v2)

# ==========================================================
# test_hidden_card_sorting - Hidden card sorting with chains
# ==========================================================
add_executable(test_hidden_card_sorting
    test_hidden_card_sorting.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animechain.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animestats.cpp
)

target_link_libraries(test_hidden_card_sorting PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_hidden_card_sorting PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_hidden_card_sorting PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hidden_card_sorting COMMAND test_hidden_card_sorting -v2)

# ==========================================================
# test_chain_filtering_standalone - Chain filtering with standalone anime
# ==========================================================
set(CHAIN_FILTERING_STANDALONE_TEST_SOURCES
    test_chain_filtering_standalone.cpp
    ../usagi/src/mylistcardmanager.cpp
    ../usagi/src/animecard.cpp
    ../usagi/src/flowlayout.cpp
    ../usagi/src/virtualflowlayout.cpp
    ../usagi/src/playbuttondelegate.cpp
    ../usagi/src/epno.cpp
    ../usagi/src/aired.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/myanidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
    ../usagi/src/watchsessionmanager.cpp
    ../usagi/src/watchchunkmanager.cpp
    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/sessioninfo.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
    ../usagi/src/animestats.cpp
    ../usagi/src/cachedanimedata.cpp
    ../usagi/src/taginfo.cpp
    ../usagi/src/cardfileinfo.cpp
    ../usagi/src/cardepisodeinfo.cpp
    ../usagi/src/relationdata.cpp
    ../usagi/src/animechain.cpp
)

set(CHAIN_FILTERING_STANDALONE_TEST_HEADERS
    ../usagi/src/mylistcardmanager.h
    ../usagi/src/animecard.h
    ../usagi/src/flowlayout.h
    ../usagi/src/virtualflowlayout.h
    ../usagi/src/playbuttondelegate.h
    ../usagi/src/epno.h
    ../usagi/src/aired.h
    ../usagi/src/anidbapi.h
    ../usagi/src/main.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
    ../usagi/src/watchsessionmanager.h
    ../usagi/src/watchchunkmanager.h
    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
    ../usagi/src/sessioninfo.h
    ../usagi/src/animechain.h
    ../usagi/src/animestats.h
    ../usagi/src/cachedanimedata.h
    ../usagi/src/taginfo.h
    ../usagi/src/cardfileinfo.h
    ../usagi/src/cardepisodeinfo.h
    ../usagi/src/relationdata.h
)

add_executable(test_chain_filtering_standalone ${CHAIN_FILTERING_STANDALONE_TEST_SOURCES} ${CHAIN_FILTERING_STANDALONE_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_chain_filtering_standalone)

target_compile_definitions(test_chain_filtering_standalone PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_chain_filtering_standalone PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_chain_filtering_standalone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_chain_filtering_standalone PRIVATE
        "-Wl,--subsystem,console"
    )
    
    # For static Qt builds on Windows, import platform plugins
    get_target_property(qt_core_type Qt6::Core TYPE)
    if(qt_core_type STREQUAL "STATIC_LIBRARY")
        qt_import_plugins(test_chain_filtering_standalone
            INCLUDE
                Qt::QWindowsIntegrationPlugin
                Qt::QWindowsVistaStylePlugin
                Qt::QSQLiteDriverPlugin
        )
    endif()
endif()

add_test(NAME test_chain_filtering_standalone COMMAND test_chain_filtering_standalone -v2)

# ==========================================================
# test_filter_system - Comprehensive filter system tests
# ==========================================================
add_executable(test_filter_system
    test_filter_system.cpp
)

target_link_libraries(test_filter_system PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

target_include_directories(test_filter_system PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_filter_system PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_filter_system COMMAND test_filter_system -v2)

# ==========================================================
# test_filter_classes - Refactored filter classes tests
# ==========================================================
add_executable(test_filter_classes
    test_filter_classes.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animefilter.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animemetadatacache.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animestats.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/cachedanimedata.cpp
)

target_link_libraries(test_filter_classes PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Widgets
)

target_include_directories(test_filter_classes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_filter_classes PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_filter_classes COMMAND test_filter_classes -v2)


# ==========================================================
# test_recent_episode_air_date_sort - Recent episode air date sorting tests
# ==========================================================
add_executable(test_recent_episode_air_date_sort
    test_recent_episode_air_date_sort.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animechain.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animestats.cpp
)

target_link_libraries(test_recent_episode_air_date_sort PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_recent_episode_air_date_sort PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_recent_episode_air_date_sort PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_recent_episode_air_date_sort COMMAND test_recent_episode_air_date_sort -v2)

# ==========================================================
# test_recent_episode_sort_improvements - Recent episode sorting improvements tests
# ==========================================================
add_executable(test_recent_episode_sort_improvements
    test_recent_episode_sort_improvements.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animechain.cpp
    ${CMAKE_SOURCE_DIR}/usagi/src/animestats.cpp
)

target_link_libraries(test_recent_episode_sort_improvements PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_recent_episode_sort_improvements PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_recent_episode_sort_improvements PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_recent_episode_sort_improvements COMMAND test_recent_episode_sort_improvements -v2)

# ==========================================================
# test_hasher_card_update_signal - Signal connection for anime card updates
# ==========================================================
set(HASHER_CARD_UPDATE_SIGNAL_TEST_SOURCES
    test_hasher_card_update_signal.cpp
    test_stubs.cpp
    ../usagi/src/hashercoordinator.cpp
    ../usagi/src/hashingtask.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/myanidbapi.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/hasherthreadpool.cpp
    ../usagi/src/hasherthread.cpp
    ../usagi/src/progresstracker.cpp
)

set(HASHER_CARD_UPDATE_SIGNAL_TEST_HEADERS
    test_hashes_stub.h
    ../usagi/src/hashercoordinator.h
    ../usagi/src/anidbapi.h
    ../usagi/src/logger.h
    ../usagi/src/main.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/hasherthreadpool.h
    ../usagi/src/hasherthread.h
)

add_executable(test_hasher_card_update_signal ${HASHER_CARD_UPDATE_SIGNAL_TEST_SOURCES} ${HASHER_CARD_UPDATE_SIGNAL_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_hasher_card_update_signal)

target_compile_definitions(test_hasher_card_update_signal PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_hasher_card_update_signal PRIVATE
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Core
    z
)

target_include_directories(test_hasher_card_update_signal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_hasher_card_update_signal PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hasher_card_update_signal COMMAND test_hasher_card_update_signal -v2)

# Test: Missing anime data request deduplication
set(MISSING_ANIME_DATA_REQUEST_TEST_SOURCES
    test_missing_anime_data_request.cpp
    ../usagi/src/mylistcardmanager.cpp
    ../usagi/src/animecard.cpp
    ../usagi/src/flowlayout.cpp
    ../usagi/src/virtualflowlayout.cpp
    ../usagi/src/playbuttondelegate.cpp
    ../usagi/src/epno.cpp
    ../usagi/src/aired.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/myanidbapi.cpp
    ../usagi/src/mask.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
    ../usagi/src/watchsessionmanager.cpp
    ../usagi/src/watchchunkmanager.cpp
    ../usagi/src/anidbanimeinfo.cpp
    ../usagi/src/anidbfileinfo.cpp
    ../usagi/src/anidbepisodeinfo.cpp
    ../usagi/src/anidbgroupinfo.cpp
    ../usagi/src/applicationsettings.cpp
    ../usagi/src/sessioninfo.cpp
    ../usagi/src/truncatedresponseinfo.cpp
    ../usagi/src/replywaiter.cpp
    ../usagi/src/filehashinfo.cpp
    ../usagi/src/animestats.cpp
    ../usagi/src/cachedanimedata.cpp
    ../usagi/src/taginfo.cpp
    ../usagi/src/cardfileinfo.cpp
    ../usagi/src/cardepisodeinfo.cpp
    ../usagi/src/relationdata.cpp
    ../usagi/src/animechain.cpp
)

set(MISSING_ANIME_DATA_REQUEST_TEST_HEADERS
    ../usagi/src/mylistcardmanager.h
    ../usagi/src/animecard.h
    ../usagi/src/flowlayout.h
    ../usagi/src/virtualflowlayout.h
    ../usagi/src/playbuttondelegate.h
    ../usagi/src/epno.h
    ../usagi/src/aired.h
    ../usagi/src/anidbapi.h
    ../usagi/src/main.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
    ../usagi/src/watchsessionmanager.h
    ../usagi/src/watchchunkmanager.h
    ../usagi/src/anidbanimeinfo.h
    ../usagi/src/anidbfileinfo.h
    ../usagi/src/anidbepisodeinfo.h
    ../usagi/src/anidbgroupinfo.h
    ../usagi/src/applicationsettings.h
    ../usagi/src/sessioninfo.h
    ../usagi/src/animechain.h
    ../usagi/src/animestats.h
    ../usagi/src/cachedanimedata.h
    ../usagi/src/taginfo.h
    ../usagi/src/cardfileinfo.h
    ../usagi/src/cardepisodeinfo.h
    ../usagi/src/relationdata.h
)

add_executable(test_missing_anime_data_request ${MISSING_ANIME_DATA_REQUEST_TEST_SOURCES} ${MISSING_ANIME_DATA_REQUEST_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_missing_anime_data_request)

target_compile_definitions(test_missing_anime_data_request PRIVATE CRYPTOPP_DEBUG=0)

target_link_libraries(test_missing_anime_data_request PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    z
)

target_include_directories(test_missing_anime_data_request PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_missing_anime_data_request PRIVATE
        "-Wl,--subsystem,console"
    )

    get_target_property(qt_core_type Qt6::Core TYPE)
    if(qt_core_type STREQUAL "STATIC_LIBRARY")
        qt_import_plugins(test_missing_anime_data_request
            INCLUDE
                Qt::QWindowsIntegrationPlugin
                Qt::QWindowsVistaStylePlugin
                Qt::QSQLiteDriverPlugin
        )
    endif()
endif()

add_test(NAME test_missing_anime_data_request COMMAND test_missing_anime_data_request -v2)

# Test: Deletion Candidate Display infrastructure
set(DELETION_CANDIDATE_DISPLAY_TEST_SOURCES
    test_deletion_candidate_display.cpp
    ../usagi/src/deletionlockmanager.cpp
    ../usagi/src/factorweightlearner.cpp
    ../usagi/src/deletionhistorymanager.cpp
    ../usagi/src/logger.cpp
)

set(DELETION_CANDIDATE_DISPLAY_TEST_HEADERS
    ../usagi/src/deletionlockmanager.h
    ../usagi/src/factorweightlearner.h
    ../usagi/src/deletionhistorymanager.h
    ../usagi/src/deletionlock.h
    ../usagi/src/deletionhistoryentry.h
    ../usagi/src/logger.h
)

add_executable(test_deletion_candidate_display ${DELETION_CANDIDATE_DISPLAY_TEST_SOURCES} ${DELETION_CANDIDATE_DISPLAY_TEST_HEADERS})
skip_automoc_for_usagi_sources(test_deletion_candidate_display)

target_link_libraries(test_deletion_candidate_display PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

target_include_directories(test_deletion_candidate_display PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Windows console subsystem
if(WIN32)
    target_link_options(test_deletion_candidate_display PRIVATE
        "-Wl,--subsystem,console"
    )

    get_target_property(qt_core_type Qt6::Core TYPE)
    if(qt_core_type STREQUAL "STATIC_LIBRARY")
        qt_import_plugins(test_deletion_candidate_display
            INCLUDE
                Qt::QWindowsIntegrationPlugin
                Qt::QWindowsVistaStylePlugin
                Qt::QSQLiteDriverPlugin
        )
    endif()
endif()

add_test(NAME test_deletion_candidate_display COMMAND test_deletion_candidate_display -v2)
