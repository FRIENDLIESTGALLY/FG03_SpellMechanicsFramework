@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem Builds FG03_SpellMechanicsFramework.dll (Release with debug info) with Visual Studio 2022 and xmake.
rem
rem Finds Visual Studio through vswhere. To use a specific install instead, set VSDEVCMD to the full
rem path of its VsDevCmd.bat. If xmake is not on PATH, set XMAKE_DIR to the folder holding xmake.exe.
rem Output: build\windows\x64\releasedbg\FG03_SpellMechanicsFramework.dll
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not defined VSDEVCMD if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDEVCMD=%%i\Common7\Tools\VsDevCmd.bat"
)
if not defined VSDEVCMD goto :novs
if not exist "%VSDEVCMD%" goto :novs
call "%VSDEVCMD%" -arch=x64 -host_arch=x64 >nul 2>nul
if errorlevel 1 exit /b 1

if defined XMAKE_DIR set "PATH=%XMAKE_DIR%;%PATH%"
where xmake >nul 2>&1
if errorlevel 1 goto :noxmake

rem Keep the builder's folder names out of the DLL: the compiler trims this folder off the source
rem paths that logging records. Passed through the compiler's CL variable because xmake splits
rem flags at spaces.
set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "CL=/d1trimfile:"%ROOT%""

rem Lets xmake run from an elevated prompt.
set XMAKE_ROOT=y
cd /d "%~dp0"
xmake config -m releasedbg -y -c
if errorlevel 1 exit /b 2
rem Full rebuild, so every file is compiled with the CL setting above.
xmake build -r -y
exit /b %errorlevel%

:novs
echo Could not find Visual Studio 2022 with the C++ tools. Set VSDEVCMD to the path of VsDevCmd.bat.
exit /b 1

:noxmake
echo xmake was not found. Install it from https://xmake.io or set XMAKE_DIR to its folder.
exit /b 1
