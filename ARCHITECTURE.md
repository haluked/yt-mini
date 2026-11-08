# System Architecture Specification (ARCHITECTURE.md)

**English** · [Türkçe](ARCHITECTURE.tr.md)

## 1. Architecture Overview

ff-dlp-viewer is a native Win32 application written in C11. It has no runtime
dependencies beyond the Windows system DLLs: no Python, no .NET, no Electron and no
external GUI framework. The core logic (URL detection, codec selection, filename
sanitization, JSON parsing) is pure C, independent of the UI, and covered by unit tests.

```
                 ┌──────────────────────────────────────────────┐
                 │                 main.c (WinMain)             │
                 │  CLI parsing · single instance · bootstrapper│
                 └───────────────┬──────────────────────────────┘
                                 │ creates window, starts UI
                                 ▼
                 ┌──────────────────────────────────────────────┐
                 │              ui.c (UI thread)                │
                 │  window · owner-draw controls · tray · toast │
                 │  state machine · DPI · drag & drop           │
                 └───────┬───────────────────────────▲──────────┘
                         │ engine_*_async()          │ WM_USER_* messages
                         ▼                           │ (PostMessage)
                 ┌──────────────────────────────────────────────┐
                 │            engine.c (worker thread)          │
                 │  CreateThread · CreatePipe · ReadFile        │
                 │  progress parsing · cancel · Job Object      │
                 └───────┬───────────────────────────▲──────────┘
                         │ CreateProcessW (hidden)   │ stdout / stderr
                         ▼                           │
                 ┌──────────────────────────────────────────────┐
                 │        yt-dlp.exe  →  ffmpeg.exe             │
                 │        (bundled under bin\, never PATH)      │
                 └──────────────────────────────────────────────┘
```

---

## 2. Module Structure and Responsibilities

```
ff-dlp-viewer/
├── src/
│   ├── main.c              # Entry point: CLI, mutex, bootstrapper, ui_run()
│   ├── ui.c                # Main window, layout, state machine, tray, toast, drag & drop
│   ├── gb_ui.c             # Game Boy drawing primitives, owner-draw controls, dropdown, message box
│   ├── engine.c            # Worker thread, hidden process pipes, metadata/download, progress, cancel
│   ├── bootstrapper.c      # First-run download of yt-dlp/ffmpeg and yt-dlp updater window
│   ├── codec_selector.c    # Pure deterministic codec/container selection engine
│   ├── detector.c          # URL validation and platform detection
│   ├── sanitization.c      # Windows-safe filename and path handling
│   ├── cjson.c             # Single-file JSON parser (metadata and format tree)
│   ├── *.h                 # Public interfaces and shared constants
├── res/                    # app.ico, app.manifest, res.rc (version info)
├── tests/test_core.c       # Unit tests for detector, sanitization, codec selector, cJSON
├── installer/setup.iss     # Inno Setup installer script
├── build.bat               # windres + gcc build with size verification
├── DESIGN.md               # Game Boy 8-bit design specification
├── PRODUCT.md              # Product specification
├── LIGHT_ARCHITECTURE.md   # Size/runtime rationale of the native rewrite
├── NAMING.md               # Repository/binary naming decisions
├── TODO.md                 # Roadmap
└── README.md               # User and developer documentation
```

---

## 3. Data Flow

1. **URL input and validation**
   - `detector.c` validates the URL (http/https, length, no quotes/backslashes/control
     characters) and detects the platform (`youtube`, `tiktok`, `instagram`, `twitter`,
     `generic`).
2. **Metadata fetch (asynchronous)**
   - `ui.c` calls `engine_fetch_async()`. `engine.c` starts a worker thread with
     `CreateThread` and launches
     `yt-dlp.exe --dump-json --no-playlist --no-warnings "<url>"` with hidden pipes
     (`CreateProcessW` + `CreatePipe`).
   - stdout carries only JSON; stderr is drained on a separate thread so the child
     process can never block on a full pipe.
   - The JSON is parsed with `cjson.c`; format entries are stored in a fixed
     `MAX_FORMATS` (256) array. When the limit is reached, the best formats are kept by
     taking entries from the end of yt-dlp's worst-to-best list.
3. **Codec resolution (pure function)**
   - `select_best_format()` (in `codec_selector.c`) has no side effects. Given the
     format list, mode (`video`/`audio`), target resolution, force-MP4 flag and audio
     preference, it returns the selected format id(s), expected container, whether a
     re-encode is needed, the reason text and an optional warning.
4. **Download and progress**
   - `engine_download_async()` runs in the same worker model. yt-dlp is started with a
     `--progress-template` that prints
     `percent|speed|eta|downloaded|total` lines; each line is parsed and posted to the UI
     as `WM_USER_DOWNLOAD_TICK`.
   - The final file path is written by yt-dlp to a UTF-8 temp file
     (`--print-to-file after_move:filepath`) instead of being guessed from the console
     code page, so Turkish/emoji titles resolve to the correct path.
5. **Cancellation and completion**
   - A cancel request terminates the Job Object holding the whole process tree
     (yt-dlp and any ffmpeg child), so no orphan processes remain.
   - On failure, intermediate files created by this run are cleaned up by timestamp;
     the final/post-processed output is never deleted.
   - The UI receives `finished`, `cancelled` or `error` and updates the status line,
     tray tooltip and dialogs.

---

## 4. Codec and Container Selection Engine

### Video priority
$$\text{AV1} > \text{VP9} > \text{H.265 (HEVC)} > \text{H.264 (AVC)}$$

### Audio priority
$$\text{Opus} > \text{AAC} > \text{MP3}$$

### Rules
1. **Stream copy first**: if the selected streams can be packaged into MP4, WebM or MKV
   without re-encoding, yt-dlp merges them with `-c copy` (`--merge-output-format`).
   No CPU time is wasted.
2. **Force MP4**:
   - H.264 + AAC sources are copied directly (zero loss, zero CPU).
   - VP9/AV1 video with Opus audio: only the audio is converted to AAC while the video
     is copied (`--ppa "Merger:-c:v copy -c:a aac"`).
   - VP9 video that must become H.264 is re-encoded with an explicit warning shown in
     the UI before the download starts.
3. **TikTok / Instagram**: these platforms normally provide a single joined H.264+AAC
   MP4 stream; it is downloaded as-is with no split/merge step.
4. **Direct media files** (`.mp4`, `.webm`, ...): when yt-dlp reports no codec info for
   both streams, the file is saved directly; audio mode extracts/converts it with
   ffmpeg.

---

## 5. Threading, UI and Rendering

- **UI thread**: owns every window and control. All drawing is GDI on the UI thread.
- **Worker threads**: one metadata/download worker at a time. `engine_is_busy()`
  reports the state so the UI never starts overlapping jobs.
- **Message marshalling**: the worker never touches UI objects; it calls
  `PostMessage(WM_USER_*)`, and the UI thread reads the result.
- **Owner-draw interface**: buttons, chips, checkbox, dropdown, progress bar, title bar
  and modal dialogs are drawn by `gb_ui.c`. The only native control is the URL `EDIT`,
  for IME/clipboard/selection reliability.
- **Fixed layout**: 646×485 design units, scaled with Per-Monitor V2 DPI
  (`gb_scale` / `S()`). The media area has a fixed height so the window never jumps.
- **Tray lifecycle**: closing hides the window (`SW_HIDE`) and shows a one-time
  notification; the tray menu restores or exits. Explorer restarts re-register the icon
  via the `TaskbarCreated` message.
- **Toast window**: a separate `WS_POPUP` window shows the fetched media for 4 seconds;
  hovering resets the timer.

---

## 6. Single Instance and Command Line

- `CreateMutexW(Local\ff-dlp-viewer-light)` guarantees one instance; a second launch
  finds the existing window (`FindWindowW(UI_WINDOW_CLASS)`) and brings it forward.
- `--url <URL>` / `--url=URL` sends the link to the open window through `WM_COPYDATA`.
- `--help` / `--version` show message boxes and exit.
- `-DGB_PREVIEW` builds use a separate mutex so a preview can run next to the installed
  app.

---

## 7. Tool Bootstrapper and Updates

- On startup `bootstrapper_ensure_tools()` checks for `yt-dlp.exe` and `ffmpeg.exe`
  in this order: app directory → app\bin → ..\bin → `%LOCALAPPDATA%\ff-dlp-viewer\bin`
  → system `PATH`.
- Missing tools are downloaded with the URLMon `IBindStatusCallback` API into the app's
  own `bin/` folder. Files are written to `.tmp` and moved into place only after the
  transfer completes; a failed move is reported instead of claiming success.
- The system `PATH` is never modified.
- The tray menu's "Check for Updates (yt-dlp)" downloads the latest `yt-dlp.exe`
  release and targets the copy the app actually uses.

---

## 8. Error Handling and Security

1. **No shell command assembly**: tools are started with `CreateProcessW` and an
   argument string built from validated values; quotes, backslashes and control
   characters are rejected in URLs.
2. **Path safety**: `sanitization.c` removes Windows-reserved characters, blocks path
   traversal (`..`), handles reserved names (CON, PRN, AUX, NUL, COM1–9, LPT1–9),
   collapses whitespace and enforces length limits.
3. **Human-readable errors**: engine failures are translated into plain messages
   (private video, sign-in/age requirement, HTTP 403, network unreachable, missing
   ffmpeg, unavailable format) and shown in the Game Boy message box.
4. **Bounded resources**: fixed-size format table (256), capped progress buffers,
   stderr drained continuously, and intentional leaks only where a thread may outlive
   a timeout (documented in code).

---

## 9. Packaging and Size Budget

- `build.bat` runs `windres`, compiles with `gcc -O3 -s -flto -static -mwindows`,
  and fails the size check above 1 MB.
- The installer is produced with Inno Setup (`installer/setup.iss`) for a per-user
  install under `%LOCALAPPDATA%` without administrator rights.
- Current output: ~345 KB executable, ~10–15 MB idle RAM.

---

## 10. Tests

`tests/test_core.c` covers:
- platform detection and host matching,
- filename sanitization (reserved characters, traversal, reserved names, length),
- deterministic codec selection across YouTube/TikTok/Instagram/X-style format sets,
- force-MP4 behavior (copy vs. audio-only conversion vs. full re-encode),
- cJSON parsing including Unicode escapes, surrogate pairs, depth limits and truncated
  input.
