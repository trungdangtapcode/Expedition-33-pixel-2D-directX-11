@echo off
REM ============================================================
REM build_src.bat - Biên dịch toàn bộ project src/
REM Chạy từ thư mục gốc: D:\lab\vscworkplace\directX\
REM ============================================================

set MSVC_DIR=D:\VisualStudio\2022\BuildTools\VC\Tools\MSVC\14.40.33807
set WINSDK_DIR=C:\Program Files (x86)\Windows Kits\10
set VCPKG_DIR=D:\lab\vscworkplace\directX\vcpkg\installed\x64-windows
set OUT_DIR=bin
set OBJ_DIR=bin\obj

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
    /MD ^
    /Zi ^
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
    echo [OK] Build thanh cong ^> %OUT_DIR%\game.exe
    REM Copy DLL cần thiết
    if not exist %OUT_DIR%\DirectXTK.dll (
        copy "%VCPKG_DIR%\bin\DirectXTK.dll" %OUT_DIR%\ >nul
    )
) else (
    echo.
    echo [LOI] Build that bai. Xem loi o tren.
    exit /b 1
)
