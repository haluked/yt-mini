# ff-dlp-viewer (v2.3 Native Light)

**English** · [Türkçe](README.tr.md)

> An ultra-light (< 1 MB), runtime-free (zero Python/Electron) video and audio downloader for Windows, built on a native Win32 GUI.

[![C11](https://img.shields.io/badge/Language-C11-00599C?style=flat-square&logo=c)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/Platform-Windows_10_|_11-0078D6?style=flat-square&logo=windows)](https://microsoft.com/windows)
[![Binary Size](https://img.shields.io/badge/Binary_Size-~345_KB-brightgreen?style=flat-square)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-8CBE9C?style=flat-square)](LICENSE)

---

## Overview

**ff-dlp-viewer** was rewritten from scratch in pure C11 using the native Windows API (`user32.dll`, `gdi32.dll`, `comctl32.dll`, `shell32.dll`, `ole32.dll`), removing heavy dependencies such as the Python runtime, PyInstaller and Electron entirely.

The final executable is **only ~345 KB**, with memory usage around 10–15 MB.

---

## Highlights

- **Ultra-light with zero runtime (< 1 MB)**:
  - No external DLL dependencies; it links statically against the native Windows system libraries.
  - Fast startup, zero lag and minimal resource usage (RAM: ~12 MB).

- **Game Boy inspired 8-bit interface**:
  - Classic DMG palette: only four shades of green (`#0F380F`, `#306230`, `#8BAC0F`, `#9BBC0F`).
  - Every button, choice chip, checkbox, progress bar, dropdown, toast and confirmation dialog is custom drawn; there is no external image or font file.
  - Its own title bar (drag / minimize / close), sharp corners, hard 2 px shadows. No gradients, blur or glow.
  - System font `Consolas`, anti-aliasing off; all accented characters are fully supported.
  - Fixed 646×485 design, scaled with Per-Monitor V2 DPI.

- **One-click automatic fetch & download**:
  - Automatic metadata lookup in the background as soon as a link is pasted.
  - Even if the info has not been fetched yet, pressing "DOWNLOAD" first fetches the metadata automatically and then starts the download.
  - Seamless fallback for direct media files (`.mp4`, `.webm`, etc.) and single video/audio streams.

- **Keyboard and accessibility focused**:
  - Complete navigation across all controls with `Tab` / `Shift+Tab` (`WS_TABSTOP` and `IsDialogMessageW`).
  - `Enter`: in the URL box, fetches the info first and starts the download.
  - `Esc`: cancels the running operation or closes the window.

- **Smart process management (Job Object)**:
  - No orphaned `yt-dlp` or `ffmpeg` processes on cancel; the whole process tree is terminated instantly and cleanly through the Windows Job Object boundary.

- **Registry and folder persistence**:
  - The chosen download directory is stored under `HKCU\Software\ff-dlp-viewer\DownloadDir` and survives restarts.
  - Folder selection through the modern COM `IFileOpenDialog`.

- **Steam/WhatsApp style tray lifecycle**:
  - `X` does not quit; it hides the window to the system tray and keeps running in the background.
  - Full exit: right-click the tray icon → `Quit`. If a download is running, it asks for confirmation first.
  - Launching the shortcut again does not start a new copy; the hidden window is brought forward.
  - On the first close, a one-time "running in the background" notice is shown.

- **Single instance and command-line integration**:
  - A named mutex (`Local\ff-dlp-viewer-light`) prevents more than one copy from running at once.
  - When a second instance is started with `--url <URL>` or `--url=URL`, the link is transferred to the open window via `WM_COPYDATA` and the window is brought forward.

---

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Enter` | Starts the download while in the URL box (fetches the info first if needed). |
| `Esc` | Cancels an active operation, otherwise hides the window to the tray. |
| `Tab` / `Shift+Tab` | Moves focus through the interface controls in order. |
| `Space` | Clicks the focused button or toggles a radio/check box. |

---

## Command-line usage

```cmd
# Starts the GUI normally
ff-dlp-viewer.exe

# Opens the given link and fetches its info automatically
ff-dlp-viewer.exe --url "https://www.youtube.com/watch?v=..."

# Short help and version dialogs
ff-dlp-viewer.exe --help
ff-dlp-viewer.exe --version
```

---

## Developer & build guide

### Requirements
- **MinGW-w64 GCC** (with C11 support)
- **windres** (GNU Resource Compiler)

### 1. Build
To build the project with one command:
```cmd
build.bat
```

Or directly with the compiler:
```cmd
windres res\res.rc -O coff -o res.o
gcc -O3 -s -flto -ffunction-sections -fdata-sections -Wl,--gc-sections -static -mwindows -municode -Isrc src\main.c src\ui.c src\gb_ui.c src\engine.c src\codec_selector.c src\detector.c src\sanitization.c src\cjson.c res.o -o dist\ff-dlp-viewer.exe -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -luser32 -lurlmon
```

### 2. Unit tests
To test the core algorithms (platform detector, file-name sanitization, codec selection and direct media handling):
```cmd
gcc -O2 -Isrc src/detector.c src/sanitization.c src/codec_selector.c src/cjson.c tests/test_core.c -o tests/test_core.exe
.\tests\test_core.exe
```

---

## License

This project is licensed under the [MIT License](LICENSE). The bundled `yt-dlp` (The Unlicense) and `FFmpeg` (LGPL v2.1+) components keep their own license terms.
