@echo off
set MSVC_DIR=D:\VisualStudio\2022\BuildTools\VC\Tools\MSVC\14.40.33807
set WINSDK_DIR=C:\Program Files (x86)\Windows Kits\10
set VCPKG_DIR=D:\lab\vscworkplace\directX\vcpkg\installed\x64-windows-static
for /f "delims=" %%i in ('dir /b /ad "%WINSDK_DIR%\Include" 2^>nul') do set WINSDK_VER=%%i
set PATH=%MSVC_DIR%\bin\Hostx64\x64;%PATH%

mkdir bin\obj_test 2>nul
cl /c src\main.cpp src\Core\GameApp.cpp /Fo:bin\obj_test\ /MP /nologo /std:c++17 /EHsc /W3 /MTd /Zi /D_DEBUG /DUNICODE /D_UNICODE /I "%MSVC_DIR%\include" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\um" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\shared" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\ucrt" /I "%WINSDK_DIR%\Include\%WINSDK_VER%\winrt" /I "%VCPKG_DIR%\include" /I "%VCPKG_DIR%\include\directxtk" /I "src"
