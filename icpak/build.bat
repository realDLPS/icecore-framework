@echo off

SET BuildRelease=1
SET Build=1
SET Clean=0

SET infLoop=0

:parse
IF "%~1"=="" GOTO endparse
IF "%~1"=="Debug" SET BuildRelease=0
IF "%~1"=="debug" SET BuildRelease=0
IF "%~1"=="DEBUG" SET BuildRelease=0
IF "%~1"=="c" SET Clean=1
IF "%~1"=="C" SET Clean=1
IF "%~1"=="clean" SET Clean=1
IF "%~1"=="Clean" SET Clean=1
IF "%~1"=="CLEAN" SET Clean=1

SET /A infLoop+=1
IF %infLoop% GTR 10 GOTO endparse

SHIFT
GOTO parse
:endparse

:build
IF %BuildRelease%==1 IF %Build%==1 GOTO buildRelease
IF %BuildRelease%==0 IF %Build%==1 GOTO buildDebug
GOTO end


:buildRelease
SET Build=0
echo Building Release, this might take a while!
IF %Clean%==1 rd /s /q build\release\
cmake -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR=C:/msys64/ucrt64 -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -S %cd% -B %cd%\build\release -G Ninja
cmake --build build\release
xcopy /s /y %cd%\build\release\ICPAK.exe %cd%
echo Finished Building Windows Release
GOTO :build

:buildDebug
SET Build=0
SET Clean=1
echo Building Debug, this might take a while!
IF %Clean%==1 rd /s /q build\debug\
cmake -DCMAKE_BUILD_TYPE=Debug -DOPENSSL_ROOT_DIR=C:/msys64/ucrt64 -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -S %cd% -B %cd%\build\debug -G Ninja
cmake --build build\debug
echo Finished Building Debug
GOTO :build

:end
echo Finished all builds