@echo off
setlocal

REM Compile the project
call "%~dp0sc-compile.bat"
if errorlevel 1 (
    echo [FAILED] Compilation failed. Exiting.
    endlocal
    exit /b 1
)

REM Launch the built executable
pushd "%~dp0dist" >nul
kbdlayoutmon.exe
popd >nul

REM Optional: tail the debug log
set /p TAIL_LOG="Tail log file? (y/N): "
if /I "%TAIL_LOG%"=="Y" (
    call "%~dp0sc-taildebug.bat"
)

endlocal
