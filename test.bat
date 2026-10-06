@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\test mkdir build\test
set TEMP=%CD%\build
set TMP=%CD%\build
cl /nologo /LD /EHsc tests\mock_pcsc.cpp /Fobuild\test\mock.obj /Febuild\test\WinSCard.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /I src /I vendor\libaribb25\aribb25 /utf-8 tests\integration.cpp /Fobuild\test\integration.obj /Febuild\test\integration.exe /link build\card.obj user32.lib gdi32.lib
if errorlevel 1 exit /b 1
build\test\integration.exe
