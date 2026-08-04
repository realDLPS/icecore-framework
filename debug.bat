@echo off

SET debugWindows=0
SET debugWeb=0

SET infLoop=0

:parse
IF "%~1"=="" GOTO endparse
IF "%~1"=="windows" SET debugWindows=1
IF "%~1"=="Windows" SET debugWindows=1
IF "%~1"=="WINDOWS" SET debugWindows=1
IF "%~1"=="win" SET debugWindows=1
IF "%~1"=="Win" SET debugWindows=1
IF "%~1"=="WIN" SET debugWindows=1
IF "%~1"=="web" SET debugWeb=1
IF "%~1"=="Web" SET debugWeb=1
IF "%~1"=="WEB" SET debugWeb=1

SET /A infLoop+=1
IF %infLoop% GTR 10 GOTO endparse

SHIFT
GOTO parse
:endparse

for /f "tokens=1,2 delims==" %%A in (project.ini) do (
    if /i "%%A"=="project-name" (
        set "PROJECT_NAME=%%B"
    )
)

IF %debugWindows%==1 GOTO debugWinBuild
IF %debugWeb%==1 GOTO debugWebBuild


:debugWinBuild
echo Please start debugging inside vscode by pressing F5
GOTO end

:debugWebBuild
start "" http://localhost:8080/%PROJECT_NAME%.html
emrun %cd%\build\web\debug\%PROJECT_NAME%.html
GOTO end

:end
echo Finished debugging