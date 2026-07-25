@echo off

SET BuildRelease=0
SET BuildWindows=0
SET BuildWeb=0

SET infLoop=0

:parse
IF "%~1"=="" GOTO endparse
IF "%~1"=="Release" SET BuildRelease=1
IF "%~1"=="release" SET BuildRelease=1
IF "%~1"=="RELEASE" SET BuildRelease=1
IF "%~1"=="Debug" SET BuildRelease=0
IF "%~1"=="debug" SET BuildRelease=0
IF "%~1"=="DEBUG" SET BuildRelease=0
IF "%~1"=="windows" SET BuildWindows=1
IF "%~1"=="Windows" SET BuildWindows=1
IF "%~1"=="WINDOWS" SET BuildWindows=1
IF "%~1"=="win" SET BuildWindows=1
IF "%~1"=="Win" SET BuildWindows=1
IF "%~1"=="WIN" SET BuildWindows=1
IF "%~1"=="web" SET BuildWeb=1
IF "%~1"=="Web" SET BuildWeb=1
IF "%~1"=="WEB" SET BuildWeb=1

SET /A infLoop+=1
IF %infLoop% GTR 10 GOTO endparse

SHIFT
GOTO parse
:endparse

:build
IF %BuildRelease%==1 IF %BuildWindows%==1 GOTO buildWinRelease
IF %BuildRelease%==0 IF %BuildWindows%==1 GOTO buildWinDebug
IF %BuildRelease%==1 IF %BuildWeb%==1 GOTO buildWebRelease
IF %BuildRelease%==0 IF %BuildWeb%==1 GOTO buildWebDebug
GOTO end


:buildWinRelease
SET BuildWindows=0
echo Building Windows Release, this might take a while!
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -S %cd% -B %cd%\build\win\release -G Ninja
cmake --build build\win\release
echo Finished Building Windows Release
GOTO :build

:buildWinDebug
SET BuildWindows=0
echo Building Windows Debug, this might take a while!
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -S %cd% -B %cd%\build\win\debug -G Ninja
cmake --build build\win\debug
echo Finished Building Windows Debug
GOTO :build

:buildWebRelease
SET BuildWeb=0
echo Building Web Release, this might take a while!
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=%cd%\emsdk\upstream\emscripten\cmake\Modules\Platform\EMscripten.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -S %cd% -B %cd%\build\web\release -G Ninja
cmake --build build\web\release
echo Finished Building Web Release
GOTO :build

:buildWebDebug
SET BuildWeb=0
echo Building Web Debug, this might take a while!
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=%cd%\emsdk\upstream\emscripten\cmake\Modules\Platform\EMscripten.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -S %cd% -B %cd%\build\web\debug -G Ninja
cmake --build build\web\debug
echo Finished Building Web Debug
GOTO :build

:end
echo Finished all builds