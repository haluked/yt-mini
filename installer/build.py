"""ff-dlp-viewer Tek Komutla Derleme ve Paketleme Scripti (build.py).

İşlem adımları:
1. ffmpeg ikilisini kontrol eder (yoksa sistemden bin/ altına kopyalar).
2. PyInstaller ile bağımsız .exe ve çalışma ortamını derler (dist/ff-dlp-viewer).
3. Inno Setup (ISCC.exe) ile tek parça Setup.exe kurulum dosyası üretir.
4. Setup.exe için SHA-256 sağlama özeti üretip kaydeder.
"""

from __future__ import annotations
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DIST_DIR = PROJECT_ROOT / "dist"
BIN_DIR = PROJECT_ROOT / "bin"
INSTALLER_DIR = PROJECT_ROOT / "installer"


def print_step(step: str):
    print(f"\n======================================================\n>> {step}\n======================================================")


def ensure_ffmpeg():
    """bin/ffmpeg.exe dosyasının varlığını garanti eder."""
    print_step("1/4: ffmpeg İkilisi Kontrol Ediliyor")
    BIN_DIR.mkdir(exist_ok=True)
    target_ffmpeg = BIN_DIR / "ffmpeg.exe"

    if target_ffmpeg.is_file():
        print(f"ffmpeg mevcut: {target_ffmpeg}")
        return True

    # Sistem PATH kontrolü
    sys_ffmpeg = shutil.which("ffmpeg")
    if sys_ffmpeg and os.path.isfile(sys_ffmpeg):
        print(f"Sistemdeki ffmpeg bulundu: {sys_ffmpeg}")
        print(f"Kopyalanıyor -> {target_ffmpeg}")
        shutil.copy2(sys_ffmpeg, target_ffmpeg)
        return True

    print("[UYARI] ffmpeg.exe ne bin/ klasöründe ne de sistem PATH'inde bulunamadı!")
    print("Lütfen ffmpeg.exe'yi bin/ klasörüne yerleştirin.")
    return False


def build_pyinstaller():
    """PyInstaller ile uygulamayı paketler."""
    print_step("2/4: PyInstaller ile Uygulama Paketleniyor")

    icon_path = INSTALLER_DIR / "app.ico"
    cmd = [
        sys.executable,
        "-m",
        "PyInstaller",
        "--noconfirm",
        "--clean",
        "--onedir",
        "--windowed",
        "--name", "ff-dlp-viewer",
    ]

    if icon_path.is_file():
        cmd.extend(["--icon", str(icon_path)])

    # Gerekli veri klasörleri
    cmd.extend([
        "--add-data", f"{PROJECT_ROOT / 'ui'};ui",
        "--add-data", f"{PROJECT_ROOT / 'core'};core",
        "--add-data", f"{PROJECT_ROOT / 'tray'};tray",
        "--hidden-import", "PIL",
        "--hidden-import", "pystray",
        "--hidden-import", "yt_dlp",
        str(PROJECT_ROOT / "main.py"),
    ])

    print("Komut:", " ".join(cmd))
    res = subprocess.run(cmd, cwd=str(PROJECT_ROOT))
    if res.returncode != 0:
        raise RuntimeError("PyInstaller derlemesi başarısız oldu!")

    # ffmpeg'i paket içi bin/ altına kopyala
    output_bin = DIST_DIR / "ff-dlp-viewer" / "bin"
    output_bin.mkdir(exist_ok=True)
    src_ffmpeg = BIN_DIR / "ffmpeg.exe"
    if src_ffmpeg.is_file():
        shutil.copy2(src_ffmpeg, output_bin / "ffmpeg.exe")
        print(f"ffmpeg paket içi bin/ dizinine yerleştirildi: {output_bin / 'ffmpeg.exe'}")


def find_iscc() -> Path | None:
    """Inno Setup derleyicisini (ISCC.exe) arar."""
    # PATH kontrolü
    iscc_path = shutil.which("ISCC.exe") or shutil.which("iscc")
    if iscc_path:
        return Path(iscc_path)

    # Standart Windows kurulum dizinleri
    candidates = [
        Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Inno Setup 6" / "ISCC.exe",
        Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Inno Setup 6" / "ISCC.exe",
        Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "Inno Setup 6" / "ISCC.exe",
    ]
    for c in candidates:
        if c.is_file():
            return c
    return None


def build_inno_setup():
    """Inno Setup ile Setup.exe üretir."""
    print_step("3/4: Inno Setup ile Setup.exe Üretiliyor")

    iscc = find_iscc()
    if not iscc:
        print("[BİLGİ] Inno Setup (ISCC.exe) sistemde bulunamadı.")
        print("Taşınabilir bağımsız uygulama 'dist/ff-dlp-viewer/' altında hazırlandı.")
        print("Tek parça 'Setup.exe' üretmek için Inno Setup 6 yükleyin (winget install JRSoftware.InnoSetup).")
        return None

    iss_file = INSTALLER_DIR / "setup.iss"
    cmd = [str(iscc), str(iss_file)]
    print(f"ISCC çalıştırılıyor: {iscc}")
    res = subprocess.run(cmd, cwd=str(INSTALLER_DIR))
    if res.returncode != 0:
        raise RuntimeError("Inno Setup derlemesi başarısız oldu!")

    setup_exe = DIST_DIR / "ff-dlp-viewer-Setup-v1.0.0.exe"
    if setup_exe.is_file():
        print(f"[BAŞARILI] Kurulum dosyası üretildi: {setup_exe}")
        return setup_exe
    return None


def compute_sha256(filepath: Path) -> str:
    """Dosyanın SHA-256 özetini hesaplar ve yanına kaydeder."""
    print_step("4/4: SHA-256 Bütünlük Özeti Hesaplanıyor")
    sha256 = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            sha256.update(chunk)
    digest = sha256.hexdigest()

    checksum_file = filepath.with_suffix(filepath.suffix + ".sha256")
    checksum_file.write_text(f"{digest} *{filepath.name}\n", encoding="utf-8")
    print(f"SHA-256 ({filepath.name}): {digest}")
    print(f"Sağlama dosyası: {checksum_file}")
    return digest


def main():
    print("ff-dlp-viewer Otomatik Derleme Süreci Başlıyor...")
    ensure_ffmpeg()
    build_pyinstaller()
    setup_exe = build_inno_setup()
    if setup_exe and setup_exe.is_file():
        compute_sha256(setup_exe)

    print("\n======================================================")
    print("TÜM İŞLEMLER TAMAMLANDI!")
    print(f"Çıktı Dizini: {DIST_DIR}")
    print("======================================================")


if __name__ == "__main__":
    main()
