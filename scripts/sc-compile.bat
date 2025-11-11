@echo off
setlocal

cls

REM Startup message
echo.
echo [107m[31mCompile kbdlayoutmon[0m[0m
echo.

echo [36m[STARTED][0m Start compilation script.

REM Set the Visual Studio environment variables, using Visual Studio 2022 Community
REM echo.
REM echo [46mSetting up Visual Studio environment variables.[0m
REM echo.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul

echo [92m[OK][0m Initialize environment.

REM Define the source file and outputs
REM echo.
REM echo [46mDefining source files, outputs and make directory[0m
REM echo.
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

mkdir dist 2>&1 >nul
if errorlevel 1 (
	echo [93m[WARN][0m Failed to create output subdirectory or already exists.
) else (
	echo [92m[OK][0m Create '%OUTPUT_DIR%' output subdirectory.
)

echo [92m[OK][0m Define variables and make directory.

REM Compile resource files
REM echo.
REM echo [46mCompiling resource file[0m
REM echo.

REM Icon resource
rc /nologo /v /fo %OUTPUT_RES_ICO% %SOURCE_RES_ICO% 2>&1 >nul
if errorlevel 1 (
	echo.
    echo [91m[FAILED][0m Compile resource, %SOURCE_RES_ICO%.
) else (
    echo [92m[OK][0m Compile resource, %SOURCE_RES_ICO%.
)

REM Version resource
rc /nologo /v /fo %OUTPUT_RES_VER% %SOURCE_RES_VER% 2>&1 >nul
if errorlevel 1 (
    echo [91m[FAILED][0m Compile resource, %SOURCE_RES_VER%.
) else (
    echo [92m[OK][0m Compile resource, %SOURCE_RES_VER%.
)

REM Compile kbdlayoutmon.exe
REM echo.
REM echo [46mCompiling %OUTPUT_EXE%[0m
REM echo.

cl /nologo /EHsc /DUNICODE /D_UNICODE /W4 /MD %SOURCE_EXE% source\log.cpp source\configuration.cpp %OUTPUT_RES_ICO% %OUTPUT_RES_VER% /link /out:%OUTPUT_DIR%\%OUTPUT_EXE% shlwapi.lib user32.lib gdi32.lib ole32.lib advapi32.lib shell32.lib version.lib 2>&1 >nul
if errorlevel 1 (
	echo [91m[FAILED][0m Compile executable: %OUTPUT_EXE%
) else (
    echo [92m[OK][0m Compile executable: %OUTPUT_EXE%
)

REM Compile kbdlayoutmonhook.dll
REM echo.
REM echo [46mCompiling %OUTPUT_DLL%.[0m
REM echo.
cl /nologo /EHsc /DUNICODE /D_UNICODE /W4 /MD /LD %SOURCE_LIB% source\configuration.cpp /link /out:%OUTPUT_DIR%\%OUTPUT_DLL% shlwapi.lib user32.lib gdi32.lib ole32.lib advapi32.lib 2>&1 >nul
if errorlevel 1 (
    echo [91m[FAILED][0m Compile dll, %OUTPUT_DLL%.
) else (
    echo [92m[OK][0m Compile dll, %OUTPUT_DLL%.
)

REM Add manifest to executable
REM echo.
REM echo [46mAdding manifest to %OUTPUT_EXE%.[0m
REM echo.
mt.exe -nologo -manifest %MANIFEST_FILE% -outputresource:%OUTPUT_DIR%\%OUTPUT_EXE%;1 2>&1 >nul
if errorlevel 1 (
    echo [91m[FAILED][0m Add manifest to executable, %OUTPUT_EXE%.
) else (
    echo [92m[OK][0m Add manifest to executable, %OUTPUT_EXE%.
)

REM Remove compiled resource .res files
REM echo.
REM echo [46mRemoving compiled resource .res files.[0m
REM echo.
del resources\*.res
if errorlevel 1 (
    echo [91m[FAILED][0m Delete *.res files.
) else (
    echo [92m[OK][0m Delete *.res files.
)

REM Remove remaining .obj .lib .exp files
REM echo.
REM echo [46mRemoving remaining .obj .lib .exp files.[0m
REM echo.

REM Delete exp files
del *.exp
if errorlevel 1 (
    echo [91m[FAILED][0m Delete *.exp files.
) else (
    echo [92m[OK][0m Delete *.exp files.
)

REM Delete lib files
del *.lib
if errorlevel 1 (
    echo [91m[FAILED][0m Delete *.lib files.
) else (
    echo [92m[OK][0m Delete *.lib files.
)

REM Delete obj files
del *.obj
if errorlevel 1 (
    echo [91m[FAILED][0m Delete *.obj files.
) else (
    echo [92m[OK][0m Delete *.obj files.
)

REM Copy .config file
REM echo.
REM echo [46mCopying config file.[0m
REM echo.
copy .\%CONFIG_FILE% .\%OUTPUT_DIR%\%CONFIG_FILE% 2>&1 >nul
if errorlevel 1 (
    echo [91m[FAILED][0m Copy .config file, %CONFIG_FILE%.
) else (
    echo [92m[OK][0m Copy .config file, %CONFIG_FILE%.
)

REM Copy sc-taildebug-cur.bat file
REM echo.
REM echo _________
REM echo [46mCopying %SC_DEBUGTAIL_FILE% file.[0m
REM echo.
copy .\scripts\%SC_DEBUGTAIL_FILE% .\%OUTPUT_DIR%\%SC_DEBUGTAIL_FILE% 2>&1 >nul
if errorlevel 1 (
    echo [91m[FAILED][0m Copy %SC_DEBUGTAIL_FILE% file.
) else (
    echo [92m[OK][0m Copy %SC_DEBUGTAIL_FILE% file.
)

REM echo.
REM echo [46mFinished.[0m

echo [36m[FINISHED][0m Finished compilation script.

endlocal
