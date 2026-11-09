"""ff-dlp-viewer one-command build and packaging script (build.py).

Steps:
1. Checks the optional binaries (ffmpeg.exe, yt-dlp.exe).
2. Builds the native C11/Win32 binary (runs build.bat, verifies the < 1 MB limit).
3. Produces the portable bundle (exe + tools + README + LICENSE) and its ZIP.
4. Produces the single-file Setup.exe with Inno Setup when available.
5. Computes SHA-256 checksums for every artifact.
"""

from __future__ import annotations
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
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

APP_NAME = "ff-dlp-viewer"
APP_VERSION = "2.4.0"
PORTABLE_NAME = f"{APP_NAME}-v{APP_VERSION}-portable"


def print_step(step: str):
    print(f"\n======================================================\n>> {step}\n======================================================")


def ensure_binaries():
    """Checks the binaries (ffmpeg, yt-dlp) under bin/."""
    print_step("1/5: Checking external binaries (ffmpeg, yt-dlp)")
    BIN_DIR.mkdir(exist_ok=True)

    for binary in ["ffmpeg", "yt-dlp"]:
        target = BIN_DIR / f"{binary}.exe"
        if target.is_file():
            print(f"{binary} found: {target}")
            continue

        sys_path = shutil.which(binary)
        if sys_path and os.path.isfile(sys_path):
            print(f"Found {binary} on the system: {sys_path}")
            print(f"Copying -> {target}")
            shutil.copy2(sys_path, target)
        else:
            print(f"[INFO] {binary}.exe was not found in the local bin/ folder (the app also searches the system PATH).")


def build_native_c():
    """Builds the native C11/Win32 binary through build.bat."""
    print_step("2/5: Building the native C11 binary (ultra-lightweight < 1 MB)")

    build_bat = PROJECT_ROOT / "build.bat"
    if not build_bat.is_file():
        raise FileNotFoundError(f"build.bat not found: {build_bat}")

    res = subprocess.run(["cmd.exe", "/c", str(build_bat)], cwd=str(PROJECT_ROOT))
    if res.returncode != 0:
        raise RuntimeError("Native C build failed!")

    exe_path = DIST_DIR / f"{APP_NAME}.exe"
    if not exe_path.is_file():
        raise FileNotFoundError(f"Compiled executable not found: {exe_path}")

    size = exe_path.stat().st_size
    kb = round(size / 1024, 2)
    print(f"Executable: {exe_path} ({size} bytes / {kb} KB)")
    if size >= 1024 * 1024:
        raise ValueError(f"WARNING: the binary exceeded the 1 MB limit! ({size} bytes)")
    print(f">> VERIFICATION PASSED: {kb} KB (< 1 MB)")
    return exe_path


def build_portable_package() -> Path:
    """Builds the standalone portable bundle (exe, ffmpeg, yt-dlp, README, LICENSE)."""
    print_step("3/5: Building the portable bundle")

    bundle_dir = DIST_DIR / PORTABLE_NAME
    if bundle_dir.exists():
        shutil.rmtree(bundle_dir)
    bundle_dir.mkdir(parents=True, exist_ok=True)

    # 1. Main application
    shutil.copy2(DIST_DIR / f"{APP_NAME}.exe", bundle_dir / f"{APP_NAME}.exe")

    # 2. External binaries (bin/)
    bundle_bin = bundle_dir / "bin"
    bundle_bin.mkdir(exist_ok=True)
    for b in ["ffmpeg.exe", "yt-dlp.exe"]:
        src = BIN_DIR / b
        if src.is_file():
            shutil.copy2(src, bundle_bin / b)
            print(f"Added to the bundle: bin/{b}")
        else:
            print(f"[WARNING] {b} not found!")

    # 3. Quick start guide
    readme_txt = bundle_dir / "README.txt"
    readme_txt.write_text(
        f"{APP_NAME} v{APP_VERSION} (Portable)\n"
        "===========================================\n\n"
        "This package contains every component (ff-dlp-viewer, yt-dlp, ffmpeg) in one folder.\n"
        "You do not need Python, pip or any external installation.\n\n"
        "To run:\n"
        "  - Double-click 'ff-dlp-viewer.exe'.\n\n"
        "Closing:\n"
        "  - The X button hides the window to the system tray; the app keeps running in the background.\n"
        "  - To quit completely, right-click the tray icon and choose 'Quit'.\n\n"
        "Downloads are saved to your Videos folder by default.\n"
        "Use 'Choose Folder' to pick another folder.\n\n"
        "License: GNU GPLv3 or later. The bundled yt-dlp (The Unlicense) and\n"
        "FFmpeg (GPLv3, Gyan essentials) keep their own terms; see\n"
        "THIRD_PARTY_NOTICES.md.\n\n"
        "Enjoy!\n",
        encoding="utf-8"
    )

    # 4. License and third-party notices
    license_src = PROJECT_ROOT / "LICENSE"
    if license_src.is_file():
        shutil.copy2(license_src, bundle_dir / "LICENSE")
    notices_src = PROJECT_ROOT / "THIRD_PARTY_NOTICES.md"
    if notices_src.is_file():
        shutil.copy2(notices_src, bundle_dir / "THIRD_PARTY_NOTICES.md")

    # 5. ZIP archive (keeps the top-level folder name)
    zip_base = DIST_DIR / PORTABLE_NAME
    archive_file = Path(shutil.make_archive(
        str(zip_base), "zip", root_dir=str(DIST_DIR), base_dir=bundle_dir.name))
    print(f"[SUCCESS] Portable bundle ready: {archive_file}")
    return archive_file


def find_iscc() -> Path | None:
    """Locates the Inno Setup compiler (ISCC.exe)."""
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
    """Produces Setup.exe with Inno Setup."""
    print_step("4/5: Producing Setup.exe with Inno Setup")

    iscc = find_iscc()
    if not iscc:
        print("[INFO] Inno Setup (ISCC.exe) was not found on this system.")
        print("The portable standalone app under 'dist/' is ready.")
        print("Install Inno Setup 6 to produce a single-file 'Setup.exe'.")
        return None

    iss_file = INSTALLER_DIR / "setup.iss"
    setup_name = f"{APP_NAME}-Setup-v{APP_VERSION}.exe"
    setup_exe = DIST_DIR / setup_name

    # Build into a private temp folder first. Some antivirus products lock the
    # output file while Inno Setup updates its resources, which makes the
    # compiler fail with "EndUpdateResource failed". A temp folder plus a final
    # copy avoids that race and never leaves a partial Setup.exe in dist/.
    temp_out = Path(tempfile.mkdtemp(prefix="ffdlp-setup-"))
    cmd = [
        str(iscc),
        f"/O{temp_out}",
        f"/F{Path(setup_name).stem}",
        str(iss_file),
    ]
    print(f"Running ISCC: {iscc}")
    try:
        res = subprocess.run(cmd, cwd=str(INSTALLER_DIR))
        if res.returncode != 0:
            raise RuntimeError("Inno Setup build failed!")
        built = temp_out / setup_name
        if not built.is_file():
            raise RuntimeError("Inno Setup did not produce the expected Setup.exe")
        temp_setup = DIST_DIR / (setup_name + ".tmp")
        shutil.copy2(built, temp_setup)
        os.replace(temp_setup, setup_exe)
    finally:
        shutil.rmtree(temp_out, ignore_errors=True)

    if setup_exe.is_file():
        print(f"[SUCCESS] Installer produced: {setup_exe}")
        return setup_exe
    return None


def compute_sha256(filepath: Path) -> str:
    """Computes the file's SHA-256 checksum and saves it next to the file."""
    print_step(f"5/5: SHA-256 integrity checksum ({filepath.name})")
    sha256 = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            sha256.update(chunk)
    digest = sha256.hexdigest()

    checksum_file = filepath.with_suffix(filepath.suffix + ".sha256")
    checksum_file.write_text(f"{digest} *{filepath.name}\n", encoding="utf-8")
    print(f"SHA-256 ({filepath.name}): {digest}")
    print(f"Checksum file: {checksum_file}")
    return digest


def main():
    print("ff-dlp-viewer native C automated build and packaging starting...")
    ensure_binaries()
    exe_path = build_native_c()
    compute_sha256(exe_path)

    portable_zip = build_portable_package()
    if portable_zip.is_file():
        compute_sha256(portable_zip)

    setup_exe = build_inno_setup()
    if setup_exe and setup_exe.is_file():
        compute_sha256(setup_exe)

    print("\n======================================================")
    print("ALL STEPS COMPLETED SUCCESSFULLY!")
    print(f"Output directory: {DIST_DIR}")
    print("======================================================")


if __name__ == "__main__":
    main()
