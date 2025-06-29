@echo off
setlocal

REM Startup message
echo.
echo _
echo [107m[31mKilling application[0m[0m
echo ------

REM Set application name
set APP_NAME=kbdlayoutmon.exe

REM Kill it
taskkill /F /IM %APP_NAME%
if errorlevel 1 (
    echo [91m[FAILED][0m Kill application, %APP_NAME%.
) else (
    echo [92m[OK][0m Kill application, %APP_NAME%.
)

endlocal