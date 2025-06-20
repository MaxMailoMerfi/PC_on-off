@echo off
REM --- Перехід до кореня проєкту ---
cd /d "%~dp0\.."

REM --- Задати шляхи ---
set "SRC=build\PC_on-off.ino.bin"
set "DST=PC_on-off.ino.bin"

REM --- Копіювання ---
if exist "%SRC%" (
    copy /Y "%SRC%" "%DST%"
    echo The BIN file has been copied!
) else (
    echo BIN file not found!
)
