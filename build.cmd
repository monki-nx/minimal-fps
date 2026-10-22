@echo off
rem Compila minimal-fps.exe con las Build Tools de Visual Studio 2022.
setlocal
set SDK=C:\Program Files\Intel\PresentMon\SDK
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /O2 /EHsc /W3 /DUNICODE /D_UNICODE /std:c++17 /I"%SDK%" minimal-fps.cpp ^
   /Fo:out\ /Fe:out\minimal-fps.exe /link /SUBSYSTEM:WINDOWS "%SDK%\PresentMonAPI2Loader.lib" || exit /b 1
copy /y "%SDK%\PresentMonAPI2Loader.dll" out\ >nul
echo OK: out\minimal-fps.exe
