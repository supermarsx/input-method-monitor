@echo off
setlocal

REM Startup message
echo.
echo +-
echo [107m[31mTail log debug file[0m[0m
echo +-----

REM Define the log file to tail
set LOG_DIRECTORY=
set LOG_FILE=kbdlayoutmon.log

REM Check if the log file exists
if not exist %LOG_DIRECTORY%%LOG_FILE% (
    echo Log file %LOG_DIRECTORY%%LOG_FILE% does not exist.
    endlocal
    pause
    exit /b
)

REM Tail the log file
echo.
echo +-
echo [46mTailing log file: %LOG_DIRECTORY%%LOG_FILE%[0m
echo [46mPress Ctrl+C to stop.[0m
echo +-----

REM Continuously read the log file for new lines
echo.
powershell -command "Get-Content -Path '%LOG_DIRECTORY%%LOG_FILE%' -Wait"

endlocal
pause
