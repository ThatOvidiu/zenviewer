@echo off
rem Makes ZenViewer-Setup-<version>.exe in the project folder from the app that
rem windows\build.bat put in windows\build. Run build.bat first.
rem Needs Inno Setup 6 or newer: winget install JRSoftware.InnoSetup
setlocal

rem Work from the project folder, one level up from this script
cd /d "%~dp0.."

if not exist windows\build\zenviewer.exe (
    echo The app isn't built yet. Run windows\build.bat first.
    goto :fail
)
rem The version, from the line "project(zenviewer VERSION x.y.z ...)" in CMakeLists.txt
set "VERSION="
for /f "tokens=3" %%v in ('findstr /b /c:"project(zenviewer VERSION " CMakeLists.txt') do set "VERSION=%%v"
if defined VERSION set "VERSION=%VERSION:)=%"
if not defined VERSION (
    echo Could not find "project(zenviewer VERSION ..." in CMakeLists.txt.
    goto :fail
)
echo ZenViewer %VERSION%

set "ISCC="
for %%p in ("%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" "%ProgramFiles%\Inno Setup 6\ISCC.exe" "%LOCALAPPDATA%\Programs\Inno Setup 7\ISCC.exe" "%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe" "%ProgramFiles%\Inno Setup 7\ISCC.exe") do if not defined ISCC if exist "%%~p" set "ISCC=%%~p"
if not defined ISCC for %%i in (ISCC.exe) do set "ISCC=%%~$PATH:i"
if not defined ISCC (
    echo Inno Setup not found. Install it from https://jrsoftware.org/isdl.php
    echo or with: winget install JRSoftware.InnoSetup
    goto :fail
)

"%ISCC%" /Q "/DAppVersion=%VERSION%" windows\installer.iss || goto :fail
echo.
for %%f in (ZenViewer-Setup-*.exe) do echo Installer: %CD%\%%f
endlocal
if "%~1"=="" pause
exit /b 0

:fail
echo.
echo Making the installer failed.
endlocal
if "%~1"=="" pause
exit /b 1
