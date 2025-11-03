cmake_minimum_required(VERSION 3.16)

project(usagi_tests LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug symbols for crash log stack traces
# This allows crash logs to show function names from Usagi codebase
# Using DWARF debug format for all platforms
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# Note: Qt6 is already found in the root CMakeLists.txt, no need to find it again

# Enable automatic MOC
set(CMAKE_AUTOMOC ON)

# Ensure MOC sees QT_STATIC for static Qt builds (inherited from parent CMakeLists.txt)
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    set(CMAKE_AUTOMOC_MOC_OPTIONS "-DQT_STATIC" "-DQT_STATICPLUGIN")
endif()

# Enable testing
enable_testing()

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

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_hash PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_hash PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_hash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Force console subsystem for LLVM MinGW on Windows
# Note: -fuse-ld=lld is already set globally in root CMakeLists.txt
if(WIN32)
    target_link_options(test_hash PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hash COMMAND test_hash -v2)

# Test 2: Crash log encoding test
set(CRASHLOG_TEST_SOURCES
    test_crashlog.cpp
    ../usagi/src/crashlog.cpp
)

set(CRASHLOG_TEST_HEADERS
    ../usagi/src/crashlog.h
)

add_executable(test_crashlog ${CRASHLOG_TEST_SOURCES} ${CRASHLOG_TEST_HEADERS})

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_crashlog PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_crashlog PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(test_crashlog PRIVATE dbghelp)
    
    # Force console subsystem for LLVM MinGW
    # Note: -fuse-ld=lld is already set globally in root CMakeLists.txt
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
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
)

set(ANIDBAPI_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
)

add_executable(test_anidbapi ${ANIDBAPI_TEST_SOURCES} ${ANIDBAPI_TEST_HEADERS})

target_compile_definitions(test_anidbapi PRIVATE CRYPTOPP_DEBUG=0)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_anidbapi PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

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

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_anidbapi PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_anidbapi COMMAND test_anidbapi -v2)

# Test 4: Anime titles import tests
set(ANIME_TITLES_TEST_SOURCES
    test_anime_titles.cpp
    ../usagi/src/anidbapi.cpp
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
)

set(ANIME_TITLES_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
)

add_executable(test_anime_titles ${ANIME_TITLES_TEST_SOURCES} ${ANIME_TITLES_TEST_HEADERS})

target_compile_definitions(test_anime_titles PRIVATE CRYPTOPP_DEBUG=0)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_anime_titles PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

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

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_anime_titles PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_anime_titles COMMAND test_anime_titles -v2)

# Test 5: URL extraction from notifications
add_executable(test_url_extraction test_url_extraction.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_url_extraction PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_url_extraction PRIVATE
    Qt6::Core
    Qt6::Test
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_url_extraction PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_url_extraction COMMAND test_url_extraction -v2)

# Test 6: MyList XML Parser
add_executable(test_mylist_xml_parser test_mylist_xml_parser.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_mylist_xml_parser PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_mylist_xml_parser PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_mylist_xml_parser PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_xml_parser COMMAND test_mylist_xml_parser -v2)

# Test 7: Export template verification in notifications
add_executable(test_export_template_verification test_export_template_verification.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_export_template_verification PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_export_template_verification PRIVATE
    Qt6::Core
    Qt6::Test
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_export_template_verification PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_export_template_verification COMMAND test_export_template_verification -v2)

# Test 8: Episode number type (epno)
add_executable(test_epno test_epno.cpp ../usagi/src/epno.cpp ../usagi/src/logger.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_epno PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_epno PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_epno PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_epno PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_epno COMMAND test_epno -v2)

# Test 9: Episode column format verification
add_executable(test_episode_column_format test_episode_column_format.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_episode_column_format PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_episode_column_format PRIVATE
    Qt6::Core
    Qt6::Test
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_episode_column_format PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_episode_column_format COMMAND test_episode_column_format -v2)

# Test 10: MyList 221 response fix verification
add_executable(test_mylist_221_fix test_mylist_221_fix.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_mylist_221_fix PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_mylist_221_fix PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_mylist_221_fix PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_221_fix COMMAND test_mylist_221_fix -v2)

# Test 11: Evangelion Ha fix - episode column display for movies with specials
add_executable(test_evangelion_ha_fix test_evangelion_ha_fix.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_evangelion_ha_fix PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_evangelion_ha_fix PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_evangelion_ha_fix PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_evangelion_ha_fix COMMAND test_evangelion_ha_fix -v2)

# Test 12: Aired date type
add_executable(test_aired test_aired.cpp ../usagi/src/aired.cpp ../usagi/src/logger.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_aired PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_aired PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_aired PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_aired PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_aired COMMAND test_aired -v2)

# Test 13: MyList Type and Aired columns integration
add_executable(test_mylist_type_aired test_mylist_type_aired.cpp ../usagi/src/aired.cpp ../usagi/src/logger.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_mylist_type_aired PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_mylist_type_aired PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

target_include_directories(test_mylist_type_aired PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_mylist_type_aired PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_type_aired COMMAND test_mylist_type_aired -v2)

# Test 14: Mylist columns fix test (Type and Aired columns)
add_executable(test_mylist_columns_fix test_mylist_columns_fix.cpp ../usagi/src/aired.cpp ../usagi/src/logger.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_mylist_columns_fix PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_mylist_columns_fix PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Widgets
    Qt6::Test
)
target_include_directories(test_mylist_columns_fix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_mylist_columns_fix PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_columns_fix COMMAND test_mylist_columns_fix -v2)

# Test 15: MyList aired sorting
add_executable(test_mylist_aired_sorting test_mylist_aired_sorting.cpp ../usagi/src/aired.cpp ../usagi/src/logger.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_mylist_aired_sorting PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_mylist_aired_sorting PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)
target_include_directories(test_mylist_aired_sorting PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_mylist_aired_sorting PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_mylist_aired_sorting COMMAND test_mylist_aired_sorting -v2)

# Test 16: Directory watcher
add_executable(test_directorywatcher test_directorywatcher.cpp ../usagi/src/directorywatcher.cpp ../usagi/src/logger.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_directorywatcher PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

# Link Qt libraries
# Note: For static Qt builds, order matters. Qt6::Concurrent depends on Qt6::Core,
# so Qt6::Concurrent must come before Qt6::Core in the link order.
target_link_libraries(test_directorywatcher PRIVATE
    Qt6::Test
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Core
)
target_include_directories(test_directorywatcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Force console subsystem for LLVM MinGW on Windows
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
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
)

set(HASH_STORAGE_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
)

add_executable(test_hash_storage ${HASH_STORAGE_TEST_SOURCES} ${HASH_STORAGE_TEST_HEADERS})

target_compile_definitions(test_hash_storage PRIVATE CRYPTOPP_DEBUG=0)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_hash_storage PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

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

# Force console subsystem for LLVM MinGW on Windows
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

target_compile_definitions(test_logger PRIVATE CRYPTOPP_DEBUG=0)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_logger PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_logger PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Gui
)

target_include_directories(test_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../usagi/src
)

# Force console subsystem for LLVM MinGW on Windows
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
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
)

set(HASH_REUSE_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
)

add_executable(test_hash_reuse ${HASH_REUSE_TEST_SOURCES} ${HASH_REUSE_TEST_HEADERS})

target_compile_definitions(test_hash_reuse PRIVATE CRYPTOPP_DEBUG=0)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_hash_reuse PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

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

# Force console subsystem for LLVM MinGW on Windows
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
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
)

set(BATCH_LOCALIDENTIFY_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
)

add_executable(test_batch_localidentify ${BATCH_LOCALIDENTIFY_TEST_SOURCES} ${BATCH_LOCALIDENTIFY_TEST_HEADERS})

target_compile_definitions(test_batch_localidentify PRIVATE CRYPTOPP_DEBUG=0)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_batch_localidentify PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

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

# Force console subsystem for LLVM MinGW on Windows
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
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
)

set(HASH_DUPLICATE_REUSE_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
)

add_executable(test_hash_duplicate_reuse ${HASH_DUPLICATE_REUSE_TEST_SOURCES} ${HASH_DUPLICATE_REUSE_TEST_HEADERS})

target_compile_definitions(test_hash_duplicate_reuse PRIVATE CRYPTOPP_DEBUG=0)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_hash_duplicate_reuse PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

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

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_hash_duplicate_reuse PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hash_duplicate_reuse COMMAND test_hash_duplicate_reuse -v2)

# Test 22: Database connection validation
add_executable(test_database_validation test_database_validation.cpp)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_database_validation PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

target_link_libraries(test_database_validation PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Sql
)

# Force console subsystem for LLVM MinGW on Windows
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
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
)

set(THREAD_SAFETY_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
)

add_executable(test_thread_safety ${THREAD_SAFETY_TEST_SOURCES} ${THREAD_SAFETY_TEST_HEADERS})

target_compile_definitions(test_thread_safety PRIVATE CRYPTOPP_DEBUG=0)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_thread_safety PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

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

# Force console subsystem for LLVM MinGW on Windows
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
    ../usagi/src/anidbapi_settings.cpp
    ../usagi/src/hash/ed2k.cpp
    ../usagi/src/hash/md4.cpp
    ../usagi/src/Qt-AES-master/qaesencryption.cpp
    ../usagi/src/logger.cpp
)

set(IMMEDIATE_IDENTIFICATION_TEST_HEADERS
    ../usagi/src/anidbapi.h
    ../usagi/src/hash/ed2k.h
    ../usagi/src/hash/md4.h
    ../usagi/src/Qt-AES-master/qaesencryption.h
    ../usagi/src/logger.h
)

add_executable(test_immediate_identification ${IMMEDIATE_IDENTIFICATION_TEST_SOURCES} ${IMMEDIATE_IDENTIFICATION_TEST_HEADERS})

target_compile_definitions(test_immediate_identification PRIVATE CRYPTOPP_DEBUG=0)

# For static Qt builds, ensure QT_STATIC is defined
get_target_property(qt_core_type Qt6::Core TYPE)
if(qt_core_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(test_immediate_identification PRIVATE QT_STATIC QT_STATICPLUGIN)
endif()

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

# Force console subsystem for LLVM MinGW on Windows
if(WIN32)
    target_link_options(test_immediate_identification PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_immediate_identification COMMAND test_immediate_identification -v2)


