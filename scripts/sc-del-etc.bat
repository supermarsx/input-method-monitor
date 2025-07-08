@echo off
setlocal

REM Delete exp files
del *.exp 2>&1 || set ERRORLEVEL=1
if errorlevel 1 (
    echo [91m[FAILED][0m Delete *.exp files.
) else (
    echo [92m[OK][0m Delete *.exp files.
)

REM Delete lib files
del *.lib 2>&1 || set ERRORLEVEL=1
if errorlevel 1 (
    echo [91m[FAILED][0m Delete *.lib files.
) else (
    echo [92m[OK][0m Delete *.lib files.
)

REM Delete obj files
del *.obj 2>&1 || set ERRORLEVEL=1
if errorlevel 1 (
    echo [91m[FAILED][0m Delete *.obj files.
) else (
    echo [92m[OK][0m Delete *.obj files.
)
