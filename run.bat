@echo off
chcp 65001 >nul
cd /d "%~dp0"

set "PY=python"
where python >nul 2>&1
if errorlevel 1 set "PY=py"

%PY% main.py
if errorlevel 1 (
    echo.
    echo [HATA] Uygulama beklenmedik bir sekilde sonlandi.
    pause
)