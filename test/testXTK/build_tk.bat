@echo off
REM ============================================================
REM Script biên dịch test_tk.cpp với DirectX 11 + DirectXTK
REM Chạy file này từ thư mục gốc của workspace
REM ============================================================

REM --- Đường dẫn ---
set MSVC_DIR=D:\VisualStudio\2022\BuildTools\VC\Tools\MSVC\14.40.33807
set WINSDK_DIR=C:\Program Files (x86)\Windows Kits\10
set VCPKG_DIR=D:\lab\vscworkplace\directX\vcpkg\installed\x64-windows

REM --- VCPKG_DIR+\bin ---
set BIN_DIR=%VCPKG_DIR%\bin

REM --- Tìm phiên bản Windows SDK mới nhất ---
for /f "delims=" %%i in ('dir /b /ad "%WINSDK_DIR%\Include" 2^>nul') do set WINSDK_VER=%%i

REM --- Thiết lập PATH cho compiler và linker ---
set PATH=%MSVC_DIR%\bin\Hostx64\x64;%PATH%

REM --- Thư mục output ---
if not exist bin mkdir bin

echo [BUILD] Dang bien dich test_tk.cpp...
echo.

REM --- Lệnh biên dịch ---
cl.exe ^
    /std:c++17 ^
    /EHsc ^
    /W3 ^
    /MD ^
    /Zi ^
    /Fe:bin\test_tk.exe ^
    /Fo:bin\ ^
    .\test_tk.cpp ^
    /I "%MSVC_DIR%\include" ^
    /I "%WINSDK_DIR%\Include\%WINSDK_VER%\um" ^
    /I "%WINSDK_DIR%\Include\%WINSDK_VER%\shared" ^
    /I "%WINSDK_DIR%\Include\%WINSDK_VER%\ucrt" ^
    /I "%VCPKG_DIR%\include" ^
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
    echo [OK] Bien dich thanh cong! File: bin\test_tk.exe
    echo [RUN] Dang chay...
    echo.
    REM Copy DLL cần thiết vào bin/ nếu chưa có
    if not exist %BIN_DIR%\DirectXTK.dll (
        copy "%VCPKG_DIR%\bin\*.dll" bin\ >nul 2>&1
    )
    start "" bin\test_tk.exe
) else (
    echo.
    echo [LOI] Bien dich that bai! Xem loi o tren.
)
