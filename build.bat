@echo off
setlocal EnableExtensions
rem =========================================================================
rem  SPCT build - double-click to build spct.exe and its installer.
rem
rem    build.bat            build, stage dist\SPCT, make dist\SPCT-Setup-<ver>.exe
rem    build.bat check      run the regression tests (tests\check.sh) first
rem    build.bat nopause    don't wait for a key at the end (for scripts)
rem
rem  Needs MSYS2 with the UCRT64 gcc (gcc on PATH, or MSYS2_ROOT set, or
rem  C:\msys64), and Inno Setup 6 for the installer (found on PATH, in its
rem  usual install folders, or via ISCC=<path to ISCC.exe>). Without Inno
rem  Setup it still builds the portable dist\SPCT folder.
rem =========================================================================
cd /d "%~dp0"

set "RUNCHECK="
set "NOPAUSE="
for %%A in (%*) do (
    if /i "%%~A"=="check" set "RUNCHECK=1"
    if /i "%%~A"=="nopause" set "NOPAUSE=1"
)
set /p VERSION=<VERSION
echo === SPCT %VERSION%

rem --- toolchain: MSYS2's UCRT64 gcc, and MSYS2's bash to run build.sh ------
set "GCC="
if defined MSYS2_ROOT if exist "%MSYS2_ROOT%\ucrt64\bin\gcc.exe" set "GCC=%MSYS2_ROOT%\ucrt64\bin\gcc.exe"
if not defined GCC for /f "delims=" %%G in ('where gcc 2^>nul') do if not defined GCC set "GCC=%%G"
if not defined GCC if exist "C:\msys64\ucrt64\bin\gcc.exe" set "GCC=C:\msys64\ucrt64\bin\gcc.exe"
if not defined GCC (
    echo ERROR: gcc not found. Install MSYS2 with mingw-w64-ucrt-x86_64-gcc,
    echo        or set MSYS2_ROOT to your MSYS2 folder.
    goto :fail
)
for %%G in ("%GCC%") do set "GCCDIR=%%~dpG"
rem gcc is <msys2>\ucrt64\bin\gcc.exe, so bash is <msys2>\usr\bin\bash.exe
for %%B in ("%GCCDIR%..\..\usr\bin\bash.exe") do set "BASH=%%~fB"
if not exist "%BASH%" (
    echo ERROR: %GCC% is not in an MSYS2 UCRT64 layout ^(no %BASH%^).
    echo        Set MSYS2_ROOT to your MSYS2 folder.
    goto :fail
)
for %%B in ("%BASH%") do set "USRBIN=%%~dpB"
set "PATH=%GCCDIR%;%USRBIN%;%PATH%"
echo     gcc:  %GCC%

rem --- optional regression tests (these build a normal, non-static exe) -----
if defined RUNCHECK (
    echo === Regression tests
    "%BASH%" tests/check.sh
    if errorlevel 1 goto :fail
)

rem --- build: static, so the exe needs no MSYS2 DLLs ------------------------
echo === Building spct.exe
set "SPCT_STATIC=1"
"%BASH%" ./build.sh spct
if errorlevel 1 goto :fail
"%BASH%" ./build.sh smoke
if errorlevel 1 goto :fail
set "SPCT_STATIC="

echo === Smoke test
build\smoke.exe tests\data\sample.pgn >nul
if errorlevel 1 goto :fail
build\smoke.exe tests\data\tags.pgn >nul
if errorlevel 1 goto :fail
echo     ok

rem --- stage the installable tree ------------------------------------------
echo === Staging dist\SPCT
set "STAGE=dist\SPCT"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
copy /y build\spct.exe "%STAGE%\" >nul || goto :fail
xcopy data "%STAGE%\data\" /e /i /q /y >nul || goto :fail
copy /y README.md "%STAGE%\README.txt" >nul || goto :fail
copy /y LICENSE "%STAGE%\LICENSE.txt" >nul || goto :fail
copy /y NOTICE.md "%STAGE%\NOTICE.txt" >nul || goto :fail

rem The staged exe must find its data\ folder from anywhere: run it on the
rem sample file in an empty temp folder.
set "SELFTEST=%TEMP%\spct-selftest-%RANDOM%"
mkdir "%SELFTEST%"
copy /y tests\data\sample.pgn "%SELFTEST%\" >nul
pushd "%SELFTEST%"
"%~dp0%STAGE%\spct.exe" sgs sample.pgn >nul 2>&1
set "SELFRC=%ERRORLEVEL%"
popd
rmdir /s /q "%SELFTEST%"
if not "%SELFRC%"=="0" (
    echo ERROR: the staged spct.exe failed its self-test.
    goto :fail
)
echo     %STAGE% ready ^(portable: copy the folder anywhere and run spct.exe^)

rem --- installer ------------------------------------------------------------
if not defined ISCC for /f "delims=" %%I in ('where iscc 2^>nul') do if not defined ISCC set "ISCC=%%I"
if not defined ISCC if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not defined ISCC (
    echo === Inno Setup 6 not found - skipping the installer.
    echo     Install it ^(winget install JRSoftware.InnoSetup^) or set ISCC.
    goto :done
)
echo === Building the installer
if exist "dist\SPCT-Setup-%VERSION%.exe" del /q "dist\SPCT-Setup-%VERSION%.exe"
"%ISCC%" /Q /DAppVersion=%VERSION% installer\spct.iss
if errorlevel 1 goto :fail
echo     dist\SPCT-Setup-%VERSION%.exe

:done
echo.
echo === Done.
if not defined NOPAUSE (
    start "" explorer "%~dp0dist"
    pause
)
exit /b 0

:fail
echo.
echo === BUILD FAILED
if not defined NOPAUSE pause
exit /b 1
