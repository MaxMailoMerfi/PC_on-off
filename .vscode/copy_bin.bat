@echo off
chcp 65001 >nul
title Copy BIN file

REM --- Перехід до кореня проєкту ---
cd /d "%~dp0\.."
echo Шлях: %cd%

REM --- Пошук BIN файлу у build ---
set "SRC="
for %%f in (build\build_PC_on-off\*.bin) do (
    set "SRC=%%f"
    echo Знайдено: %%f
)

REM --- Перевірка і копіювання ---
if defined SRC (
    setlocal enabledelayedexpansion
    
    REM --- Формуємо ім'я файлу для копіювання ---
    set "FILENAME=%~n1"
    set "DST=!SRC:build\build_PC_on-off\=!"
    set "DST=!DST:.ino.bin=.bin!"
    
    REM --- Перевіряємо чи є новий файл в поточній папці ---
    if exist "!DST!" (
        echo Видаляємо старий файл: !DST!
        del /f /q "!DST!"
    )
    
    REM --- Копіюємо новий файл ---
    copy /Y "%SRC%" "!DST!" >nul
    if errorlevel 1 (
        echo ПОМИЛКА: Не вдалося скопіювати файл!
    ) else (
        echo УСПIШНО: BIN файл скопiйовано як "!DST!"!
    )
    
    endlocal
) else (
    echo ПОМИЛКА: BIN файл не знайдено в build\build_PC_on-off\
)

REM --- Завершуємо без очікування ---
exit