@echo off
:  AI:W (0rwx.com)
:  This script was written by Claude Opus 5.5 but reviewed by me manually.
: 
:  Installs everything needed to build icfw
:    Tools (system-wide)
:      - Git                         (winget: Git.Git)
:      - MSYS2                       (winget: MSYS2.MSYS2, default C:\msys64)
:          UCRT64 packages: gcc/g++, gdb, cmake, ninja, openssl, upx
:          -> gcc/gdb/ninja are used by build.bat and .vscode/launch.json
:          -> openssl is required by icpak (find_package(OpenSSL), OpenSSL::Crypto)
:          -> upx is optional, used to compress Windows release builds
:    Libraries
:      - raylib 6.0   -> raylib-6.0\   (main.cpp, src\icecore_framework.hpp)
:      - doctest      -> doctest\      (icpak\main.cpp, icpak\src\icpak_types.hpp)
:      - emsdk        -> emsdk\        (web builds, launch-vscode.bat, debug/run web)

:  Usage:
:      install-dependencies              install whatever is missing
:      install-dependencies update       also update MSYS2 packages and re-pull emsdk
:      install-dependencies noweb        skip emsdk (Windows-only development)
:      install-dependencies nomsys       skip MSYS2
:  Overrides (set before running):
:      MSYS2_ROOT     MSYS2 install folder        (default C:\msys64)
:      EMSDK_VERSION  emsdk version to install    (default latest)

setlocal EnableExtensions
cd /d "%~dp0"

SET RAYLIB_TAG=6.0
SET DOCTEST_TAG=v2.5.3
IF NOT DEFINED EMSDK_VERSION SET EMSDK_VERSION=latest
IF NOT DEFINED MSYS2_ROOT SET MSYS2_ROOT=C:\msys64

SET Update=0
SET SkipWeb=0
SET SkipMsys=0
SET Failed=0

:parse
IF "%~1"=="" GOTO endparse
IF /I "%~1"=="update" SET Update=1
IF /I "%~1"=="noweb"  SET SkipWeb=1
IF /I "%~1"=="nomsys" SET SkipMsys=1
SHIFT
GOTO parse
:endparse

echo.
echo ==== Icecore Framework dependency installer ====
echo.

:: ---------------------------------------------------------------------------
:: winget (used to install Git and MSYS2 if they are missing)
:: ---------------------------------------------------------------------------
SET HaveWinget=0
where winget >nul 2>nul && SET HaveWinget=1

:: ---------------------------------------------------------------------------
:: Git
:: ---------------------------------------------------------------------------
echo [1/5] Git
where git >nul 2>nul
IF NOT ERRORLEVEL 1 (
    echo       already installed
    GOTO gitDone
)
IF %HaveWinget%==0 (
    echo       ERROR: git not found and winget is unavailable.
    echo       Install Git from https://git-scm.com/download/win and run this again.
    GOTO fatal
)
echo       installing Git with winget...
winget install --id Git.Git -e --source winget --accept-package-agreements --accept-source-agreements
SET "PATH=%ProgramFiles%\Git\cmd;%PATH%"
where git >nul 2>nul
IF ERRORLEVEL 1 (
    echo       ERROR: Git installed but not found on PATH. Open a new terminal and run this again.
    GOTO fatal
)
:gitDone

:: ---------------------------------------------------------------------------
:: MSYS2 + UCRT64 toolchain, OpenSSL, UPX
:: ---------------------------------------------------------------------------
echo [2/5] MSYS2 UCRT64 toolchain (gcc, gdb, cmake, ninja, openssl, upx)
IF %SkipMsys%==1 (
    echo       skipped ^(nomsys^)
    GOTO msysDone
)

IF EXIST "%MSYS2_ROOT%\usr\bin\bash.exe" GOTO msysInstalled
IF %HaveWinget%==0 (
    echo       ERROR: MSYS2 not found at %MSYS2_ROOT% and winget is unavailable.
    echo       Install MSYS2 from https://www.msys2.org and run this again.
    GOTO fatal
)
echo       installing MSYS2 with winget...
winget install --id MSYS2.MSYS2 -e --source winget --accept-package-agreements --accept-source-agreements
IF NOT EXIST "%MSYS2_ROOT%\usr\bin\bash.exe" (
    echo       ERROR: MSYS2 was not found at %MSYS2_ROOT% after installing.
    echo       If you installed it elsewhere, run: set MSYS2_ROOT=^<path^> and try again.
    GOTO fatal
)
:: A fresh MSYS2 needs a full system update first. The first -Syu may update the
:: core runtime and stop, so it is run twice.
SET Update=1

:msysInstalled
SET "MSYS_BASH=%MSYS2_ROOT%\usr\bin\bash.exe"
SET MSYSTEM=UCRT64
SET CHERE_INVOKING=1

IF %Update%==1 (
    echo       updating MSYS2 packages...
    "%MSYS_BASH%" -lc "pacman -Syu --noconfirm"
    "%MSYS_BASH%" -lc "pacman -Syu --noconfirm"
)

echo       installing packages (already installed ones are skipped)...
"%MSYS_BASH%" -lc "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-gdb mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-upx"
IF ERRORLEVEL 1 (
    echo       ERROR: pacman failed to install packages. Try: install-dependencies update
    SET Failed=1
    GOTO msysDone
)

:: Make sure the UCRT64 bin folder is on the user PATH so build.bat finds
:: gcc/cmake/ninja/gdb. Done through PowerShell because setx truncates PATH.
SET "UCRT_BIN=%MSYS2_ROOT%\ucrt64\bin"
powershell -NoProfile -Command ^
    "$b='%UCRT_BIN%'; $p=[Environment]::GetEnvironmentVariable('Path','User'); if(-not $p){$p=''};" ^
    "if(($p -split ';') -notcontains $b){ [Environment]::SetEnvironmentVariable('Path', ($b + ';' + $p).TrimEnd(';'), 'User'); Write-Host '      added' $b 'to your user PATH (open a new terminal to pick it up)' }" ^
    "else { Write-Host '      ' $b 'is already on your user PATH' }"
SET "PATH=%UCRT_BIN%;%PATH%"
:msysDone

:: ---------------------------------------------------------------------------
:: raylib
:: ---------------------------------------------------------------------------
echo [3/5] raylib %RAYLIB_TAG%
IF EXIST "raylib-%RAYLIB_TAG%\src\raylib.h" (
    echo       already present in raylib-%RAYLIB_TAG%\
    GOTO raylibDone
)
git clone --branch %RAYLIB_TAG% --depth 1 https://github.com/raysan5/raylib.git raylib-%RAYLIB_TAG%
IF ERRORLEVEL 1 (
    echo       ERROR: failed to clone raylib
    SET Failed=1
)
:raylibDone

:: ---------------------------------------------------------------------------
:: doctest
:: ---------------------------------------------------------------------------
echo [4/5] doctest %DOCTEST_TAG%
IF EXIST "doctest\doctest\doctest.h" (
    echo       already present in doctest\
    GOTO doctestDone
)
git clone --branch %DOCTEST_TAG% --depth 1 https://github.com/doctest/doctest.git doctest
IF ERRORLEVEL 1 (
    echo       ERROR: failed to clone doctest
    SET Failed=1
)
:doctestDone

:: ---------------------------------------------------------------------------
:: Emscripten SDK
:: ---------------------------------------------------------------------------
echo [5/5] Emscripten SDK (%EMSDK_VERSION%)
IF %SkipWeb%==1 (
    echo       skipped ^(noweb^)
    GOTO emsdkDone
)
IF EXIST "emsdk\emsdk.bat" (
    IF %Update%==1 (
        echo       updating emsdk...
        git -C emsdk pull --ff-only
    )
) ELSE (
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git emsdk
    IF ERRORLEVEL 1 (
        echo       ERROR: failed to clone emsdk
        SET Failed=1
        GOTO emsdkDone
    )
)
pushd emsdk
call emsdk.bat install %EMSDK_VERSION%
IF ERRORLEVEL 1 (
    echo       ERROR: emsdk install failed
    SET Failed=1
    popd
    GOTO emsdkDone
)
call emsdk.bat activate %EMSDK_VERSION%
IF ERRORLEVEL 1 (
    echo       ERROR: emsdk activate failed
    SET Failed=1
)
popd
:emsdkDone

:: ---------------------------------------------------------------------------
echo.
IF %Failed%==1 (
    echo ==== Finished with errors - see messages above ====
    endlocal
    exit /b 1
)
echo ==== All dependencies installed ====
echo Open a new terminal so PATH changes take effect, then run:
echo     build win debug      or      launch-vscode
endlocal
exit /b 0

:fatal
echo.
echo ==== Aborted ====
endlocal
exit /b 1
