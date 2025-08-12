@echo off
REM --- Перехід до кореня проєкту ---
cd /d "%~dp0\.."

REM --- Пошук BIN файлу у build ---
for %%f in (build\*.bin) do (
    set "SRC=%%f"
    set "DST=%%~nxf"
)

REM --- Перевірка і копіювання ---
if defined SRC (
    copy /Y "%SRC%" "%DST%"
    echo The BIN file has been copied!
) else (
    echo BIN file not found!
)
