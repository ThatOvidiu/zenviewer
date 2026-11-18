@echo off
rem Builds ZenViewer with Microsoft's compiler (MSVC) and Qt for MSVC.
rem Run it from any Command Prompt (windows\build.bat) or by double-clicking. Result:
rem   windows\build\zenviewer.exe  (that folder is the app, apart from scripts)
rem CMake's generated build scripts go to windows\build\scripts; deleting that
rem folder is always safe.
rem
rem   windows\build.bat          build
rem   windows\build.bat clean    delete windows\build\scripts and start over
rem
rem The installer is made separately, with windows\installer.bat.
rem
rem Qt location: set QT_DIR before running, or edit the default below.
setlocal
if not defined QT_DIR set "QT_DIR=C:\Qt\6.12.0\msvc2022_64"

rem Work from the project folder, one level up from this script
cd /d "%~dp0.."

set "WORK=windows\build\scripts"
if /i "%~1"=="clean" if exist "%WORK%" rmdir /s /q "%WORK%"

if not exist "%QT_DIR%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo Qt for MSVC not found in "%QT_DIR%".
    echo Install it with the Qt Online Installer, or set QT_DIR, for example:
    echo     set QT_DIR=C:\Qt\6.12.0\msvc2022_64
    goto :fail
)

rem Find Visual Studio / Build Tools with the C++ workload and load its x64 environment
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Installer not found. Install Visual Studio Build Tools 2022 or 2026
    echo with the "Desktop development with C++" workload.
    goto :fail
)
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo No Visual Studio with the C++ x64 compiler found. In the Visual Studio
    echo Installer, add the "Desktop development with C++" workload.
    goto :fail
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || goto :fail

cmake --preset windows -DCMAKE_PREFIX_PATH="%QT_DIR%" || goto :fail
cmake --build --preset windows || goto :fail
rem Clear the previous app files, but keep the scripts folder
for /d %%d in ("windows\build\*") do if /i not "%%~nxd"=="scripts" rmdir /s /q "%%d"
del /q "windows\build\*" 2>nul
cmake --install "%WORK%" || goto :fail
rem The compiled object files (~80 MB) are only a cache; the next build
rem recompiles everything, which takes about a minute
if exist "%WORK%\CMakeFiles\zenviewer.dir" rmdir /s /q "%WORK%\CMakeFiles\zenviewer.dir"
del /q "%WORK%\zenviewer.exe" 2>nul

echo.
echo Done: %CD%\windows\build\zenviewer.exe
echo To share it, zip windows\build without the scripts folder.
endlocal
exit /b 0

:fail
echo.
echo Build failed.
endlocal
if "%~1"=="" pause
exit /b 1
