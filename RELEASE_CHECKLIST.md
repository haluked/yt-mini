# Pre-Release Checklist

**English** · [Türkçe](RELEASE_CHECKLIST.tr.md)

Checklist to verify before ff-dlp-viewer is published on GitHub or shipped to end users:

- [x] **Design and Interface Review (Game Boy 8-bit)**:
  - Four-shade DMG palette (`#0F380F`, `#306230`, `#8BAC0F`, `#9BBC0F`) applied; no color outside the palette.
  - Pixel-stepped corners, 2 px hard frames/shadows; no gradients, blur, glow or anti-aliased corners.
  - Every control is owner-drawn; the only native control is the URL `EDIT` box.

- [x] **Code Quality and Unit Tests**:
  - All C core tests pass with `tests\test_core.exe` (detector, sanitization, codec selector, cJSON).
  - Force-MP4 behaviour is covered (direct copy, audio-only conversion, full re-encode).
  - File sanitization (Windows reserved characters, path traversal, reserved names, MAX_PATH) is verified.

- [x] **Size and Performance**:
  - `build.bat` verifies the executable stays under 1 MB (current: ~351 KB).
  - Idle RAM measured at ~10–15 MB.
  - No external runtime: only native Windows system DLLs are linked.

- [x] **Background and Tray Behaviour**:
  - Closing the window hides it to the tray and keeps the app running.
  - A one-time "running in the background" notice is shown on the first close.
  - Quitting while a download is running asks for confirmation.
  - A second launch forwards the URL to the existing window (`WM_COPYDATA`) instead of opening a new copy.

- [x] **Security and Isolation**:
  - Processes are started with `CreateProcessW`; no shell command strings are assembled.
  - URLs reject quotes, backslashes and control characters.
  - yt-dlp/ffmpeg are installed in the app's own `bin/` folder; the system `PATH` is not modified.
  - A Windows named mutex guarantees a single instance.
  - A Job Object terminates the whole process tree on cancel.

- [x] **Installation and Packaging**:
  - `installer/setup.iss` provides an English Inno Setup installer (per-user `%LOCALAPPDATA%`, no admin rights).
  - `build.bat` compiles the native executable with size verification.
  - First-run bootstrapper downloads yt-dlp/ffmpeg when missing.

- [x] **Single Active Version**:
  - The current version is `v2.3.2`; only one active release line exists.
  - Only one release and one tag remain on GitHub. Older releases/assets are removed.
  - `ffmpeg.exe` is uploaded as a release asset so
    `releases/latest/download/ffmpeg.exe` keeps working for the bootstrapper.

- [x] **License Compliance**:
  - `LICENSE` contains the MIT License plus the usage terms for yt-dlp (The Unlicense) and FFmpeg (LGPL v2.1+) as standalone binaries.

- [ ] **User Approval & GitHub Push**:
  - No push to GitHub happens until Haluk approves it.
