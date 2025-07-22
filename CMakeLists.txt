cmake_minimum_required(VERSION 3.15)
project(InputMethodMonitor LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    source/configuration.cpp
)

# kbdlayoutmon executable
add_executable(kbdlayoutmon
    source/kbdlayoutmon.cpp
    ${COMMON_SOURCES}
    source/log.cpp
    resources/res-icon.rc
    resources/res-versioninfo.rc
)

# Include directory for rc file header
target_include_directories(kbdlayoutmon PRIVATE resources source)

target_link_libraries(kbdlayoutmon
    shlwapi
    user32
    gdi32
    ole32
    advapi32
    shell32
    version
)

# Embed manifest after build on Windows
if(MSVC)
    add_custom_command(TARGET kbdlayoutmon POST_BUILD
        COMMAND mt.exe -nologo -manifest "${CMAKE_CURRENT_SOURCE_DIR}/manifest.xml" -outputresource:"$<TARGET_FILE:kbdlayoutmon>";1
        COMMENT "Embedding application manifest")
endif()

# kbdlayoutmonhook DLL
add_library(kbdlayoutmonhook SHARED
    source/kbdlayoutmonhook.cpp
    ${COMMON_SOURCES}
)

set_target_properties(kbdlayoutmonhook PROPERTIES PREFIX "")

target_include_directories(kbdlayoutmonhook PRIVATE source)

target_link_libraries(kbdlayoutmonhook
    shlwapi
    user32
    gdi32
    ole32
    advapi32
)

# Unit tests
find_package(Catch2 3 REQUIRED)
add_executable(run_tests tests/test_configuration.cpp)
target_include_directories(run_tests PRIVATE tests)
target_link_libraries(run_tests PRIVATE Catch2::Catch2WithMain)

enable_testing()
add_test(NAME run_tests COMMAND run_tests)

