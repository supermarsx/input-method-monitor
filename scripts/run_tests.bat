@echo off
rem Windows test runner: prefer sh, then cmake, then g++ (MinGW)
setlocal

where sh >nul 2>&1
if %ERRORLEVEL%==0 (
  echo Found sh, delegating to scripts\run_tests.sh
  sh scripts/run_tests.sh
  exit /b %ERRORLEVEL%
)

where cmake >nul 2>&1
if %ERRORLEVEL%==0 (
  echo Found cmake, configuring tests with CMake
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  if %ERRORLEVEL% NEQ 0 (
    echo CMake configure failed
    goto try_gpp
  )
  echo Attempting to build CMake project and test targets
  cmake --build build --config Debug
  if %ERRORLEVEL%==0 (
    echo Build finished. Checking for test binary...
    if exist build\run_tests.exe (
      echo Running CMake-built tests...
      build\run_tests.exe
      exit /b %ERRORLEVEL%
    ) else if exist build\tests\run_tests.exe (
      echo Running CMake-built tests from build/tests...
      build\tests\run_tests.exe
      exit /b %ERRORLEVEL%
    ) else (
      echo Test binary not found after build
      goto try_gpp
    )
  ) else (
    echo Build failed. Falling back to g++ compilation.
    goto try_gpp
  )
)

:try_gpp
where g++ >nul 2>&1
if %ERRORLEVEL%==0 (
  echo Using g++ to compile tests (MinGW)

  rem If Catch2 headers/libs are missing, try to download single-header Catch2 for compilation
  set CATCH_VENDOR_DIR=tests\vendor
  set CATCH_HEADER=%CATCH_VENDOR_DIR%\catch2\catch.hpp
  if not exist "%CATCH_HEADER%" (
    echo Catch2 header not found. Attempting to download Catch2 v3 single-header into %CATCH_HEADER%...
    mkdir "%CATCH_VENDOR_DIR%\catch2" >nul 2>&1
    where curl >nul 2>&1
    if %ERRORLEVEL%==0 (
      curl -L -o "%CATCH_HEADER%" "https://raw.githubusercontent.com/catchorg/Catch2/v3.4.0/single_include/catch2/catch.hpp" || del /q "%CATCH_HEADER%" >nul 2>&1
    ) else (
      powershell -Command "try { Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/catchorg/Catch2/v3.4.0/single_include/catch2/catch.hpp' -OutFile '%CATCH_HEADER%'; exit 0 } catch { exit 1 }" || del /q "%CATCH_HEADER%" >nul 2>&1
    )
    if not exist "%CATCH_HEADER%" (
      echo Failed to download Catch2 header. You can install Catch2 or ensure libraries are available.
      goto no_catch2
    ) else (
      echo Downloaded Catch2 header to %CATCH_HEADER%
    )
  ) else (
    echo Found Catch2 header at %CATCH_HEADER%
  )

  rem Ensure a catch main file exists to provide test runner main when using single-header
  if not exist tests\catch_main.cpp (
    echo Creating tests\catch_main.cpp...
    (echo #define CATCH_CONFIG_MAIN) > tests\catch_main.cpp
    (echo #include "catch2/catch.hpp") >> tests\catch_main.cpp
  )

  rem Compile tests using the downloaded single-header Catch2 (if present)
  g++ -std=c++17 -DUNIT_TEST -I tests -I resources -I %CATCH_VENDOR_DIR% ^
    tests\catch_main.cpp ^
    tests\test_configuration.cpp tests\test_log.cpp tests\test_utils.cpp tests\test_timer_guard.cpp ^
    tests\test_tray_icon.cpp tests\test_tray_icon_integration.cpp tests\test_tray_icon_update.cpp ^
    tests\test_hotkey_registry.cpp tests\test_unknown_option.cpp tests\test_config_watcher_posix.cpp tests\stubs.cpp ^
    source\configuration.cpp source\log.cpp source\config_parser.cpp source\tray_icon.cpp ^
    source\hotkey_registry.cpp source\hotkey_cli.cpp source\config_watcher.cpp source\config_watcher_posix.cpp source\cli_utils.cpp ^
    source\app_state.cpp ^
    -o tests\run_tests -pthread
  if %ERRORLEVEL% NEQ 0 (
    echo g++ compilation failed. Ensure MinGW and a working Catch2 header are available.
    exit /b %ERRORLEVEL%
  )
  echo Running tests...
  tests\run_tests
  exit /b %ERRORLEVEL%
)

:no_catch2
echo No suitable Catch2 available and download failed. Install Git for Windows (sh), CMake, or MinGW/g++ and Catch2.
exit /b 1
