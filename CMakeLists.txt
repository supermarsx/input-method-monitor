cmake_minimum_required(VERSION 3.15)
if(WIN32)
    project(InputMethodMonitor LANGUAGES CXX RC)
else()
    project(InputMethodMonitor LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    source/configuration.cpp
    source/config_parser.cpp
    source/log.cpp
    source/app_state.cpp
)

# Core static library for shared sources
add_library(core STATIC
    ${COMMON_SOURCES}
)

target_include_directories(core PRIVATE source)

target_compile_definitions(core PRIVATE UNICODE _UNICODE)

# kbdlayoutmon executable
if(WIN32)
add_executable(kbdlayoutmon WIN32
    source/kbdlayoutmon.cpp
    source/cli_utils.cpp
    source/config_watcher.cpp
    source/tray_icon.cpp
    source/hotkey_registry.cpp
    source/hotkey_cli.cpp
    resources/res-icon.rc
    resources/res-versioninfo.rc
)

# Include directory for rc file header
target_include_directories(kbdlayoutmon PRIVATE resources source)

target_compile_definitions(kbdlayoutmon PRIVATE UNICODE _UNICODE)

target_link_libraries(kbdlayoutmon PRIVATE
    core
    shlwapi
    user32
    gdi32
    ole32
    advapi32
    shell32
    version
)

# Embed manifest after build on Windows
# (Disabled: mt.exe manifest embedding causes quoting issues on CI)
endif()

# kbdlayoutmonhook DLL
if(WIN32)
add_library(kbdlayoutmonhook SHARED
    source/kbdlayoutmonhook.cpp
)

set_target_properties(kbdlayoutmonhook PROPERTIES PREFIX "")
 
target_include_directories(kbdlayoutmonhook PRIVATE source)
 
target_compile_definitions(kbdlayoutmonhook PRIVATE UNICODE _UNICODE)
 
target_link_libraries(kbdlayoutmonhook PRIVATE
    core
    shlwapi
    user32
    gdi32
    ole32
    advapi32
 )
endif()

# Unit tests
# Attempt to find Catch2; if missing, fetch it with FetchContent
include(FetchContent)
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found: Fetching Catch2 via FetchContent...")
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    # Prefer FetchContent_MakeAvailable (automatically populates and adds subdirectory)
    FetchContent_MakeAvailable(catch2)
    # After this, Catch2 targets (Catch2::Catch2WithMain) should be available
    set(Catch2_FOUND TRUE)
endif()


set(TEST_SOURCES
    tests/test_configuration.cpp
    tests/test_log.cpp
    tests/bench_pipe.cpp
)

set(RUN_SOURCES
    # linking against core static library provides: log, configuration, config_parser, app_state
)

if(WIN32)
    list(APPEND TEST_SOURCES
        tests/test_utils.cpp
        tests/test_config_watcher.cpp
        tests/test_worker_thread.cpp
        tests/test_hotkey_registry.cpp
        tests/stubs.cpp
    )
    # For Windows unit tests, compile the runtime pieces (but exclude the hook DLL implementation)
    list(APPEND RUN_SOURCES
        source/config_watcher.cpp
        source/hotkey_registry.cpp
        source/hotkey_cli.cpp
    )
else()
    list(APPEND RUN_SOURCES source/config_watcher_posix.cpp tests/stubs.cpp)
endif()

if(Catch2_FOUND)
    add_executable(run_tests_exe EXCLUDE_FROM_ALL ${TEST_SOURCES})
    target_include_directories(run_tests_exe PRIVATE tests source)
    target_link_libraries(run_tests_exe PRIVATE Catch2::Catch2WithMain core)
    target_compile_definitions(run_tests_exe PRIVATE UNIT_TEST UNICODE _UNICODE)

    if(RUN_SOURCES)
        add_library(test_runtime STATIC ${RUN_SOURCES} "tests/test_runtime_helpers.cpp")
        target_include_directories(test_runtime PRIVATE source tests)
        target_compile_definitions(test_runtime PRIVATE UNICODE _UNICODE)
        target_link_libraries(run_tests_exe PRIVATE test_runtime)
    endif()

    if(WIN32)
        target_link_libraries(run_tests_exe PRIVATE shlwapi user32 gdi32 ole32 advapi32)
    endif()

    enable_testing()
    add_test(NAME run_tests COMMAND run_tests_exe)
else()
    message(STATUS "Skipping unit test target: Catch2 not available.")
endif()

