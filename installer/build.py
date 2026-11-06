"""ff-dlp-viewer Tek Komutla Derleme ve Paketleme Scripti (build.py).

İşlem adımları:
1. İsteğe bağlı ikilileri (ffmpeg.exe, yt-dlp.exe) kontrol eder.
2. Native C11/Win32 binary'sini derler (build.bat çalıştırır, < 1 MB boyut kontrolü yapar).
3. Inno Setup (ISCC.exe) mevcutsa tek parça Setup.exe kurulum dosyası üretir.
4. Çıktılar için SHA-256 sağlama özeti üretip kaydeder.
"""

from __future__ import annotations
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DIST_DIR = PROJECT_ROOT / "dist"
BIN_DIR = PROJECT_ROOT / "bin"
INSTALLER_DIR = PROJECT_ROOT / "installer"


def print_step(step: str):
    print(f"\n======================================================\n>> {step}\n======================================================")


def ensure_binaries():
    """bin/ altındaki ikilileri (ffmpeg, yt-dlp) kontrol eder."""
    print_step("1/4: Harici İkililer (ffmpeg, yt-dlp) Kontrol Ediliyor")
    BIN_DIR.mkdir(exist_ok=True)

    for binary in ["ffmpeg", "yt-dlp"]:
        target = BIN_DIR / f"{binary}.exe"
        if target.is_file():
            print(f"{binary} mevcut: {target}")
            continue

        sys_path = shutil.which(binary)
        if sys_path and os.path.isfile(sys_path):
            print(f"Sistemdeki {binary} bulundu: {sys_path}")
            print(f"Kopyalanıyor -> {target}")
            shutil.copy2(sys_path, target)
        else:
            print(f"[BİLGİ] {binary}.exe yerel bin/ klasöründe bulunamadı (sistem PATH'inde aranacak).")


def build_native_c():
    """Native C11 Win32 ikilisini build.bat ile derler."""
    print_step("2/4: Native C11 Binary Derleniyor (Ultra-Lightweight < 1 MB)")

    build_bat = PROJECT_ROOT / "build.bat"
    if not build_bat.is_file():
        raise FileNotFoundError(f"build.bat bulunamadı: {build_bat}")

    res = subprocess.run(["cmd.exe", "/c", str(build_bat)], cwd=str(PROJECT_ROOT))
    if res.returncode != 0:
        raise RuntimeError("Native C derlemesi başarısız oldu!")

    exe_path = DIST_DIR / "ff-dlp-viewer.exe"
    if not exe_path.is_file():
        raise FileNotFoundError(f"Derlenen executable bulunamadı: {exe_path}")

    size = exe_path.stat().st_size
    kb = round(size / 1024, 2)
    print(f"Executable: {exe_path} ({size} bayt / {kb} KB)")
    if size >= 1024 * 1024:
        raise ValueError(f"UYARI: Binary boyutu 1 MB sınırını aştı! ({size} bayt)")
    print(f">> DOĞRULAMA BAŞARILI: {kb} KB (< 1 MB)")
    return exe_path


def find_iscc() -> Path | None:
    """Inno Setup derleyicisini (ISCC.exe) arar."""
    iscc_path = shutil.which("ISCC.exe") or shutil.which("iscc")
    if iscc_path:
        return Path(iscc_path)

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
        print("Taşınabilir bağımsız uygulama 'dist/ff-dlp-viewer.exe' hazırlandı.")
        print("Tek parça 'Setup.exe' üretmek için Inno Setup 6 yükleyebilirsiniz.")
        return None

    iss_file = INSTALLER_DIR / "setup.iss"
    cmd = [str(iscc), str(iss_file)]
    print(f"ISCC çalıştırılıyor: {iscc}")
    res = subprocess.run(cmd, cwd=str(INSTALLER_DIR))
    if res.returncode != 0:
        raise RuntimeError("Inno Setup derlemesi başarısız oldu!")

    setup_exe = DIST_DIR / "ff-dlp-viewer-Setup-v2.0.0.exe"
    if setup_exe.is_file():
        print(f"[BAŞARILI] Kurulum dosyası üretildi: {setup_exe}")
        return setup_exe
    return None


def compute_sha256(filepath: Path) -> str:
    """Dosyanın SHA-256 özetini hesaplar ve yanına kaydeder."""
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
    print("ff-dlp-viewer Native C Otomatik Derleme ve Paketleme Başlıyor...")
    ensure_binaries()
    exe_path = build_native_c()
    compute_sha256(exe_path)

    setup_exe = build_inno_setup()
    if setup_exe and setup_exe.is_file():
        compute_sha256(setup_exe)

    print("\n======================================================")
    print("TÜM İŞLEMLER BAŞARIYLA TAMAMLANDI!")
    print(f"Çıktı Dizini: {DIST_DIR}")
    print("======================================================")


if __name__ == "__main__":
    main()
