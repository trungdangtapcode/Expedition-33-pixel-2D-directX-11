@echo off
REM ============================================================
REM build_src.bat — Compile the full src/ project.
REM Run from workspace root: D:\lab\vscworkplace\directX\
REM
REM Usage:
REM   build_src.bat          -> Debug build   (default)
REM   build_src.bat Release  -> Release build
REM
REM Debug build:   /MDd + /D_DEBUG + /Zi  (debug CRT, symbols, LOG() active)
REM Release build: /MD  + /DNDEBUG + /O2  (release CRT, optimized, LOG() = no-op)
REM ============================================================

set MSVC_DIR=D:\VisualStudio\2022\BuildTools\VC\Tools\MSVC\14.40.33807
set WINSDK_DIR=C:\Program Files (x86)\Windows Kits\10
set VCPKG_DIR=D:\lab\vscworkplace\directX\vcpkg\installed\x64-windows
set OUT_DIR=bin
set OBJ_DIR=bin\obj

REM --- Determine build configuration ---
set BUILD_TYPE=Debug
if /I "%1"=="Release" set BUILD_TYPE=Release

REM /MDd links against msvcrtd.lib (debug CRT) — required when _DEBUG is defined,
REM because STL headers use _CrtDbgReport and _invalid_parameter from that library.
REM /MD  links against msvcrt.lib  (release CRT) — these symbols do not exist there.
REM Mixing /MD with /D_DEBUG is the root cause of LNK2019 on _CrtDbgReport.
if /I "%BUILD_TYPE%"=="Release" (
    set CRT_FLAG=/MD
    set OPT_FLAG=/O2 /DNDEBUG
) else (
    set CRT_FLAG=/MDd
    set OPT_FLAG=/Zi /D_DEBUG
)

REM Tìm phiên bản Windows SDK
for /f "delims=" %%i in ('dir /b /ad "%WINSDK_DIR%\Include" 2^>nul') do set WINSDK_VER=%%i

set PATH=%MSVC_DIR%\bin\Hostx64\x64;%PATH%

if not exist %OUT_DIR% mkdir %OUT_DIR%
if not exist %OBJ_DIR% mkdir %OBJ_DIR%

echo ============================================================
echo  Building: My Game - DirectX 11
echo ============================================================
echo.

cl.exe ^
    /std:c++17 ^
    /EHsc ^
    /W3 ^
    %CRT_FLAG% ^
    %OPT_FLAG% ^
    /DUNICODE /D_UNICODE ^
    /Fe:%OUT_DIR%\game.exe ^
    /Fo:%OBJ_DIR%\ ^
    src\main.cpp ^
    src\Core\GameApp.cpp ^
    src\Core\GameTimer.cpp ^
    src\Renderer\D3DContext.cpp ^
    src\States\StateManager.cpp ^
    src\States\MenuState.cpp ^
    src\States\PlayState.cpp ^
    src\Events\EventManager.cpp ^
    /I "%MSVC_DIR%\include" ^
    /I "%WINSDK_DIR%\Include\%WINSDK_VER%\um" ^
    /I "%WINSDK_DIR%\Include\%WINSDK_VER%\shared" ^
    /I "%WINSDK_DIR%\Include\%WINSDK_VER%\ucrt" ^
    /I "%WINSDK_DIR%\Include\%WINSDK_VER%\winrt" ^
    /I "%VCPKG_DIR%\include" ^
    /I "src" ^
    /link ^
    /LIBPATH:"%MSVC_DIR%\lib\x64" ^
    /LIBPATH:"%WINSDK_DIR%\Lib\%WINSDK_VER%\um\x64" ^
    /LIBPATH:"%WINSDK_DIR%\Lib\%WINSDK_VER%\ucrt\x64" ^
    /LIBPATH:"%VCPKG_DIR%\lib" ^
    user32.lib ^
    gdi32.lib ^
    d3d11.lib ^
    dxgi.lib ^
    DirectXTK.lib ^
    /SUBSYSTEM:WINDOWS

if %ERRORLEVEL% == 0 (
    echo.
    echo [OK] Build succeeded ^> %OUT_DIR%\game.exe  [%BUILD_TYPE%]
    REM Copy the DirectXTK DLL next to the executable if not already present.
    if not exist %OUT_DIR%\DirectXTK.dll (
        copy "%VCPKG_DIR%\bin\DirectXTK.dll" %OUT_DIR%\ >nul
    )
) else (
    echo.
    echo [ERROR] Build failed. See errors above.
    exit /b 1
)
