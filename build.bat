@echo off
setlocal
set "target=build\CardIDViewer.exe"
if not "%~1"=="" set "target=%~1"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build mkdir build
set TEMP=%CD%\build
set TMP=%CD%\build
cl /nologo /c /O2 /DUNICODE /D_UNICODE /I src /I vendor\libaribb25\aribb25 /FIpcsc_bridge.h /utf-8 vendor\libaribb25\aribb25\b_cas_card.c /Fobuild\card.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /O2 /W4 /DUNICODE /D_UNICODE /I src /I vendor\libaribb25\aribb25 /utf-8 src\main.cpp /Fe"%target%" /Fobuild\main.obj /link build\card.obj user32.lib gdi32.lib /SUBSYSTEM:WINDOWS


