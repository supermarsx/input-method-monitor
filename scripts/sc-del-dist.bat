@echo off
setlocal

set OUTPUT_DIR=dist

REM Remove distribution folder
REM echo.
REM echo [46mRemoving compiled resource .res files.[0m
REM echo.
REM cmd /c rd %OUTPUT_DIR% || exit /b 1
rd /S /Q %OUTPUT_DIR% 2>&1 || set ERRORLEVEL=1
if errorlevel 1 (
    echo [91m[FAILED][0m Deleted '%OUTPUT_DIR%' folder.
) else (
    echo [92m[OK][0m Deleted '%OUTPUT_DIR%' folder.
)
