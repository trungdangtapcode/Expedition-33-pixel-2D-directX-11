@echo off
call "D:\VisualStudio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /EHsc /std:c++17 d:\lab\vscworkplace\directX\test\QteMathTest.cpp /Fe:d:\lab\vscworkplace\directX\test\QteMathTest.exe
if %ERRORLEVEL% equ 0 (
    d:\lab\vscworkplace\directX\test\QteMathTest.exe
)
