@echo off
setlocal

cls

REM Startup message
echo.
echo Compile kbdlayoutmon
echo.
echo [STARTED] Start compilation script.
echo.

REM Attempt to locate Visual Studio via vswhere and set environment, with several fallbacks
set VSINSTALLDIR=

REM Try vswhere on PATH first
where vswhere >nul 2>&1nif %ERRORLEVEL%==0 (
    for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSINSTALLDIR=%%i
)

REM If vswhere wasn't on PATH, try the common install location for vswhere
if not defined VSINSTALLDIR (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
        for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSINSTALLDIR=%%i
    )
)

REM Final fallbacks: common Visual Studio install paths (2022 & 2019)
if not defined VSINSTALLDIR (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Community
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2019\Community
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community
    )
)

if defined VSINSTALLDIR (
    if exist "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat" (
        call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
        if errorlevel 1 (
            echo Failed to run vcvars64.bat from "%VSINSTALLDIR%". Continuing but the compiler may not be available.
        ) else (
            echo [OK] Initialized Visual Studio environment from "%VSINSTALLDIR%".
        )
    ) else (
        echo VSINSTALLDIR was set to "%VSINSTALLDIR%" but vcvars64.bat was not found there.
    )
) else (
    echo Could not locate Visual Studio or vswhere. Ensure Visual Studio with C++ workload is installed and vswhere is available.
)

REM Check that cl.exe is available
where cl >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [OK] cl.exe found on PATH.
) else (
    echo [WARN] cl.exe not found. The build steps may fail without a configured MSVC environment.
)

REM Define the source file and outputs
set SOURCE_LIB=source\kbdlayoutmonhook.cpp
set OUTPUT_DLL=kbdlayoutmonhook.dll
set SOURCE_EXE=source\kbdlayoutmon.cpp
set OUTPUT_EXE=kbdlayoutmon.exe
set MANIFEST_FILE=manifest.xml
set SOURCE_RES_ICO=resources\res-icon.rc
set OUTPUT_RES_ICO=resources\res-icon.res
set SOURCE_RES_VER=resources\res-versioninfo.rc
set OUTPUT_RES_VER=resources\res-versioninfo.res
set CONFIG_FILE=kbdlayoutmon.config
set SC_DEBUGTAIL_FILE=sc-taildebug-cur.bat
set OUTPUT_DIR=dist

mkdir "%OUTPUT_DIR%" 2>nul >nul
if errorlevel 1 (
    echo [WARN] Failed to create output subdirectory or it already exists.
) else (
    echo [OK] Create '%OUTPUT_DIR%' output subdirectory.
)

echo [OK] Define variables and make directory.

REM Compile resource files
rc /nologo /v /fo %OUTPUT_RES_ICO% %SOURCE_RES_ICO% 2>nul >nul
if errorlevel 1 (
    echo [FAILED] Compile resource, %SOURCE_RES_ICO%.
) else (
    echo [OK] Compile resource, %SOURCE_RES_ICO%.
)

rc /nologo /v /fo %OUTPUT_RES_VER% %SOURCE_RES_VER% 2>nul >nul
if errorlevel 1 (
    echo [FAILED] Compile resource, %SOURCE_RES_VER%.
) else (
    echo [OK] Compile resource, %SOURCE_RES_VER%.
)

REM Compile kbdlayoutmon.exe
cl /nologo /EHsc /DUNICODE /D_UNICODE /W4 /MD %SOURCE_EXE% source\log.cpp source\configuration.cpp %OUTPUT_RES_ICO% %OUTPUT_RES_VER% /link /out:%OUTPUT_DIR%\%OUTPUT_EXE% shlwapi.lib user32.lib gdi32.lib ole32.lib advapi32.lib shell32.lib version.lib
if errorlevel 1 (
    echo [FAILED] Compile executable: %OUTPUT_EXE%
) else (
    echo [OK] Compile executable: %OUTPUT_EXE%
)

REM Compile kbdlayoutmonhook.dll
cl /nologo /EHsc /DUNICODE /D_UNICODE /W4 /MD /LD %SOURCE_LIB% source\configuration.cpp /link /out:%OUTPUT_DIR%\%OUTPUT_DLL% shlwapi.lib user32.lib gdi32.lib ole32.lib advapi32.lib
if errorlevel 1 (
    echo [FAILED] Compile dll, %OUTPUT_DLL%.
) else (
    echo [OK] Compile dll, %OUTPUT_DLL%.
)

REM Add manifest to executable (optional)
if exist "%MANIFEST_FILE%" (
    mt.exe -nologo -manifest %MANIFEST_FILE% -outputresource:%OUTPUT_DIR%\%OUTPUT_EXE%;1 2>nul >nul
    if errorlevel 1 (
        echo [WARN] Add manifest to executable failed. Continuing.
    ) else (
        echo [OK] Add manifest to executable, %OUTPUT_EXE%.
    )
) else (
    echo [WARN] Manifest file %MANIFEST_FILE% not found; skipping manifest embedding.
)

REM Cleanup intermediate files
del resources\*.res 2>nul >nul
del *.exp 2>nul >nul
del *.lib 2>nul >nul
del *.obj 2>nul >nul

REM Copy .config file
if exist "%CONFIG_FILE%" (
    copy /Y "%CONFIG_FILE%" "%OUTPUT_DIR%\%CONFIG_FILE%" >nul
    echo [OK] Copy .config file, %CONFIG_FILE%.
) else (
    echo [WARN] Config file %CONFIG_FILE% not found; skipping copy.
)

REM Copy sc-taildebug-cur.bat file
if exist "scripts\%SC_DEBUGTAIL_FILE%" (
    copy /Y "scripts\%SC_DEBUGTAIL_FILE%" "%OUTPUT_DIR%\%SC_DEBUGTAIL_FILE%" >nul
    echo [OK] Copy %SC_DEBUGTAIL_FILE% file.
) else (
    echo [WARN] %SC_DEBUGTAIL_FILE% not found in scripts; skipping copy.
)

echo [FINISHED] Finished compilation script.

endlocal

