@echo off
rem Compila minimal-fps con Visual Studio 2022 (Build Tools o completo) y CMake.
rem Resultado: build\Release\minimal-fps.exe
setlocal
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo No se encontro Visual Studio 2022. Instala "Build Tools for Visual Studio 2022" con el workload de C++.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo No se encontro el toolset de C++ de Visual Studio 2022.
    exit /b 1
)
call "%VSDIR%\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo >nul || exit /b 1

where cmake >nul 2>&1 || (
    echo No se encontro cmake. Instala el componente "C++ CMake tools for Windows" de Visual Studio o CMake desde cmake.org.
    exit /b 1
)

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build build --config Release || exit /b 1
echo.
echo OK: build\Release\minimal-fps.exe
