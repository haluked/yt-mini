@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ======================================================
echo [1/3] Windows Kaynak Dosyalari Derleniyor (windres)
echo ======================================================
if not exist "dist" mkdir "dist"

windres res\res.rc -O coff -o res.o
if errorlevel 1 (
    echo [HATA] windres kaynak derlemesi basarisiz oldu!
    exit /b 1
)

echo ======================================================
echo [2/3] Native C Binary Derleniyor (Ultra-Lightweight)
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
    echo [HATA] GCC derlemesi basarisiz oldu!
    exit /b 1
)

echo ======================================================
echo [3/3] Derleme Tamamlandi - Boyut Dogrulamasi
echo ======================================================
powershell -Command "$size = (Get-Item 'dist\ff-dlp-viewer.exe').Length; $kb = [math]::Round($size / 1KB, 2); Write-Host \"Executable Konumu: dist\ff-dlp-viewer.exe\"; Write-Host \"Dosya Boyutu: $size bayt ($kb KB)\"; if ($size -lt 1MB) { Write-Host '>> BASARILI: Hedef < 1 MB kurali saglandi!' -ForegroundColor Green } else { Write-Host '>> HATA: Boyut 1 MB sinirini asti!' -ForegroundColor Red }"
