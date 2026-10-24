@echo off
chcp 65001 >nul
echo ======================================================
echo ff-dlp-viewer - Geliştirici Ortamı Kurulumu
echo ======================================================

where python >nul 2>&1
if %errorlevel% neq 0 (
    echo [HATA] Python bulunamadı! Lütfen Python 3.11+ kurun ve PATH'e ekleyin.
    pause
    exit /b 1
)

echo [1/3] Python sanal ortamı (venv) kontrol ediliyor...
if not exist ".venv" (
    echo Sanal ortam oluşturuluyor (.venv)...
    python -m venv .venv
)

echo [2/3] Bağımlılıklar yükleniyor...
call .venv\Scripts\activate.bat
python -m pip install --upgrade pip
pip install -r requirements.txt

echo [3/3] Birim testleri çalıştırılıyor...
python -m pytest tests/

echo ======================================================
echo [BAŞARILI] Geliştirici ortamı hazır!
echo Çalıştırmak için: python main.py
echo ======================================================
pause
