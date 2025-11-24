cmake_minimum_required(VERSION 3.16)

project(usagi_tests LANGUAGES CXX)

enable_testing()

# Enable automatic MOC for test files that include .moc files
set(CMAKE_AUTOMOC ON)

# Macro to add a test with usagi_core library
# Usage: add_usagi_test(test_name [EXTRA_QT_LIBS...])
macro(add_usagi_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
    
    target_link_libraries(${TEST_NAME} PRIVATE
        usagi_core
        Qt6::Core
        Qt6::Test
        ${ARGN}  # Additional Qt libraries if needed
    )
    
    if(WIN32)
        target_link_options(${TEST_NAME} PRIVATE
            "-Wl,--subsystem,console"
        )
    endif()
    
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} -v2)
endmacro()

# Define all tests
add_usagi_test(test_hash)
add_usagi_test(test_mask)
add_usagi_test(test_anime_mask_parsing Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_crashlog Qt6::Gui)
add_usagi_test(test_anidbapi Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_compression Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_anidb_timeout_retry Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_anime_titles Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_url_extraction)
add_usagi_test(test_mylist_xml_parser Qt6::Sql)
add_usagi_test(test_export_template_verification)
add_usagi_test(test_epno)
add_usagi_test(test_episode_column_format)
add_usagi_test(test_mylist_221_fix Qt6::Sql)
add_usagi_test(test_evangelion_ha_fix Qt6::Sql)
add_usagi_test(test_aired)
add_usagi_test(test_mylist_type_aired Qt6::Sql)
add_usagi_test(test_mylist_columns_fix Qt6::Sql Qt6::Widgets)
add_usagi_test(test_mylist_aired_sorting Qt6::Widgets)
add_usagi_test(test_directorywatcher Qt6::Sql)
add_usagi_test(test_hash_storage Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_logger Qt6::Gui)
add_usagi_test(test_hash_reuse Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_batch_localidentify Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_hash_duplicate_reuse Qt6::Network Qt6::Sql)
add_usagi_test(test_database_validation Qt6::Sql)
add_usagi_test(test_thread_safety Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_immediate_identification Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_batch_hash_retrieval Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_ui_freeze_fix Qt6::Network Qt6::Sql Qt6::Widgets)

# test_hasher_thread needs myanidbapi.cpp and main.h (uses myAniDBApi class)
add_executable(test_hasher_thread test_hasher_thread.cpp ../usagi/src/myanidbapi.cpp)

target_link_libraries(test_hasher_thread PRIVATE
    usagi_core
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Core
)

if(WIN32)
    target_link_options(test_hasher_thread PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_hasher_thread COMMAND test_hasher_thread -v2)

# test_hasher_threadpool needs myanidbapi.cpp and main.h (uses myAniDBApi class)
add_executable(test_hasher_threadpool test_hasher_threadpool.cpp ../usagi/src/myanidbapi.cpp)

target_link_libraries(test_hasher_threadpool PRIVATE
    usagi_core
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Core
)

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

# Continue with remaining tests
# test_stop_non_blocking needs myanidbapi.cpp and main.h (uses myAniDBApi class)
add_executable(test_stop_non_blocking test_stop_non_blocking.cpp ../usagi/src/myanidbapi.cpp)

target_link_libraries(test_stop_non_blocking PRIVATE
    usagi_core
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
)

if(WIN32)
    target_link_options(test_stop_non_blocking PRIVATE
        "-Wl,--subsystem,console"
    )
endif()

add_test(NAME test_stop_non_blocking COMMAND test_stop_non_blocking -v2)

add_usagi_test(test_truncated_response Qt6::Network Qt6::Sql Qt6::Widgets)
add_usagi_test(test_api_optimization Qt6::Network Qt6::Sql Qt6::Widgets)

# test_mylistcardmanager needs myanidbapi.cpp and main.h (uses myAniDBApi class)
add_executable(test_mylistcardmanager test_mylistcardmanager.cpp ../usagi/src/myanidbapi.cpp)

target_link_libraries(test_mylistcardmanager PRIVATE
    usagi_core
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
)

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

# Continue with final tests
add_usagi_test(test_background_loading Qt6::Sql)
add_usagi_test(test_watchchunkmanager Qt6::Sql)
