@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ======================================================
echo [1/3] Compiling Windows resources (windres)
echo ======================================================
if not exist "dist" mkdir "dist"

windres res\res.rc -O coff -o res.o
if errorlevel 1 (
    echo [ERROR] windres resource compilation failed!
    exit /b 1
)

echo ======================================================
echo [2/3] Compiling the native C binary (ultra-lightweight)
echo ======================================================
gcc -O3 -s -flto ^
    -ffunction-sections -fdata-sections ^
    -Wl,--gc-sections ^
    -static -mwindows -municode ^
    -Isrc ^
    src\main.c ^
    src\ui.c ^
    src\gb_ui.c ^
    src\bootstrapper.c ^
    src\engine.c ^
    src\codec_selector.c ^
    src\detector.c ^
    src\sanitization.c ^
    src\cjson.c ^
    res.o ^
    -o dist\ff-dlp-viewer.exe ^
    -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -luser32 -lurlmon

if errorlevel 1 (
    echo [ERROR] GCC compilation failed!
    exit /b 1
)

echo ======================================================
echo [3/3] Build complete - size verification
echo ======================================================
powershell -Command "$size = (Get-Item 'dist\ff-dlp-viewer.exe').Length; $kb = [math]::Round($size / 1KB, 2); Write-Host \"Executable: dist\ff-dlp-viewer.exe\"; Write-Host \"File size: $size bytes ($kb KB)\"; if ($size -lt 1MB) { Write-Host '>> OK: the < 1 MB target is met!' -ForegroundColor Green } else { Write-Host '>> ERROR: the size exceeded 1 MB!' -ForegroundColor Red }"
