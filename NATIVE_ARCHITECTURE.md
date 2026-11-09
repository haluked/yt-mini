# ff-dlp-viewer: Native C Win32 Architecture (< 1 MB)

**English** · [Türkçe](NATIVE_ARCHITECTURE.tr.md)

The app removes the Python/PyInstaller runtime bloat entirely and is built with
**pure C (C11) / native Win32 API**, using a Game Boy-inspired 8-bit interface.

---

## 1. Architectural principles and design decisions

1. **Zero Runtime Bundles:**
   * PyInstaller, Electron, .NET and Qt dependencies are removed entirely.
   * Only the Windows operating system's own DLLs are linked dynamically (`USER32.dll`, `GDI32.dll`, `COMCTL32.dll`, `SHELL32.dll`, `ole32.dll`, `msvcrt.dll`, `KERNEL32.dll`).
   * External MinGW runtimes (`libgcc_s_seh`, `libwinpthread`) are excluded with the `-static` flag.

2. **Binary size and performance:**
   * **Target:** < 1 MB (ideal: 150 KB – 300 KB).
   * **Achieved size:** **~343 KB** (351,232 bytes).
   * **Idle RAM footprint:** < 15 MB (about 85% less than 60–70 MB in the Python version).
   * **Startup time:** < 15 ms (almost instant).

3. **Modular core (UI-independent C layer):**
   * `src/detector.c`: platform detection engine that recognizes YouTube, TikTok, Instagram, X/Twitter and generic web addresses.
   * `src/codec_selector.c`: deterministic codec/format matrix (AV1 > VP9 > H.265 > H.264 and Opus > AAC > MP3) and the stream copy / remux decision engine.
   * `src/sanitization.c`: Windows file-name constraints (reserved characters, CON/PRN/AUX reserved names, path traversal prevention, length limits).
   * `src/cjson.c`: single-file, ultra-light JSON parser (for parsing metadata and the format tree).
   * `src/engine.c`: asynchronous worker thread (`CreateThread`), hidden pipes that never block the GUI (`CreatePipe` / `ReadFile`), percentage/speed/ETA parser and cancel mechanism.

4. **Game Boy inspired 8-bit interface (`gb_ui.c` / `ui.c`):**
   * Classic DMG palette: four shades of green (`#0F380F`, `#306230`, `#8BAC0F`, `#9BBC0F`); intermediate tones use a 1 px dither.
   * `BS_OWNERDRAW` buttons, choice chips, checkbox, custom dropdown, segmented progress bar, its own title bar and modal message box.
   * `Consolas` system font + `NONANTIALIASED_QUALITY`; no external font/image files.
   * Folder selection through the modern COM `IFileOpenDialog` (`FOS_PICKFOLDERS`).
   * Drag & drop (`WM_DROPFILES`) support.
   * System tray (`Shell_NotifyIconW`) integration.

---

## 2. Build command

One-command build with MinGW-w64 GCC:

```cmd
build.bat
```

Or manually:

```bash
windres res\res.rc -O coff -o res.o

gcc -O3 -s -flto \
    -ffunction-sections -fdata-sections \
    -Wl,--gc-sections \
    -static -mwindows -municode \
    -Isrc \
    src\main.c src\ui.c src\gb_ui.c src\engine.c src\codec_selector.c src\detector.c src\sanitization.c src\cjson.c res.o \
    -o dist\ff-dlp-viewer.exe \
    -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -luser32 -lurlmon
```

---

## 3. Verification and tests

To compile and run the C core unit tests:

```cmd
gcc -O2 -Isrc src/detector.c src/sanitization.c src/codec_selector.c src/cjson.c tests/test_core.c -o tests/test_core.exe
tests\test_core.exe
```

Test output:
```
==========================================
RUNNING FF-DLP-VIEWER C UNIT TESTS
==========================================
[TEST] Detector testing...
  -> Detector tests PASSED!
[TEST] Sanitization testing...
  -> Sanitization tests PASSED!
[TEST] Codec Selector testing...
  -> Codec Selector tests PASSED!
[TEST] cJSON testing...
  -> cJSON tests PASSED!

ALL CORE C TESTS PASSED WITH 100% SUCCESS!
```
