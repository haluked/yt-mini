@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo ======================================================
echo ff-dlp-viewer - Gelistirici Ortami Kurulumu
echo ======================================================

where python >nul 2>&1
if errorlevel 1 (
    echo [HATA] Python bulunamadi! Lutfen Python 3.11+ kurun ve PATH'e ekleyin.
    pause
    exit /b 1
)

echo [1/3] Python sanal ortami (venv) kontrol ediliyor...
if not exist ".venv" (
    echo Sanal ortam olusturuluyor (.venv)...
    python -m venv .venv
    if errorlevel 1 (
        echo [HATA] Sanal ortam olusturulamadi.
        pause
        exit /b 1
    )
)

echo [2/3] Bagimliliklar yukleniyor...
call ".venv\Scripts\activate.bat"
python -m pip install --upgrade pip
pip install -r requirements.txt

echo [3/3] Birim testleri calistiriliyor...
python -m pytest tests/

echo ======================================================
echo [BASARILI] Gelistirici ortami hazir!
echo Calistirmak icin: run.bat  (veya: python main.py)
echo ======================================================
pause