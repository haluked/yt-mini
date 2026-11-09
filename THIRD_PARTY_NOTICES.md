# Third-Party Notices

`ff-dlp-viewer` is licensed under the **GNU General Public License v3.0 or later**
(see `LICENSE`).

Copyright (C) 2026 Haluk.

This distribution contains the following separate components:

## yt-dlp

- License: The Unlicense
- Project: https://github.com/yt-dlp/yt-dlp
- Distributed as the unmodified standalone `yt-dlp.exe` release binary.

## FFmpeg

- License: GNU General Public License v3.0
- Build: Gyan Doshi's `ffmpeg-release-essentials` static Windows build
  (currently FFmpeg 9.0.2).
- Source code for that build:
  https://github.com/FFmpeg/FFmpeg/commit/946fcce07b
- Build downloads: https://www.gyan.dev/ffmpeg/builds/
- The full GPLv3 text is in `LICENSE`. FFmpeg is distributed as a separate
  executable, not linked into `ff-dlp-viewer.exe`.

## cJSON

- License: MIT
- Copyright (C) Dave Gamble and cJSON contributors.
- The parser in `src/cjson.c` and `src/cjson.h` is derived from cJSON.

## MinGW-w64 runtime

- The static C runtime and compiler support code are covered by the
  MinGW-w64 / GCC runtime exception. They are linked into the executable but do
  not impose a different license on the application source.
