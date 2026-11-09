# Naming Note (NAMING.md)

**English** · [Türkçe](NAMING.tr.md)

## Status

| Place | Name | Description |
|---|---|---|
| GitHub repository | **`haluked/yt-mini`** | The only home of the project. The current release is `v2.4.0`. |
| Product | **`ff-dlp-viewer`** | The name shown in the window, installer and documentation. |
| Local development folder | **`ff-dlp-viewer`** | The folder name is kept as-is so existing paths and habits keep working. |
| Binary name | `ff-dlp-viewer.exe` | Output of `build.bat`. |

## Single active version

The native C11/Win32 build is the only maintained version. Older Python/PyInstaller
prototypes are retired and exist only in the git history; they are not built, shipped
or documented anywhere in the current tree.

The version number moves forward from `v2.4.0`. There is only one active release line.

## Why are there two names?

The project started as `ff-dlp-viewer`; the repository was later created as `yt-mini`.
Renaming the local folder would break paths and habits, so only the repository keeps the
`yt-mini` name. The product itself is always `ff-dlp-viewer`.

## Rules

1. **The local folder name is never referenced in code** — the code resolves paths
   relative to the executable's own location (`GetModuleFileNameW` + `bin\` lookup, see
   `engine.c: find_binary`). The folder can therefore be renamed safely.
2. Identity strings inside the code (mutex, registry key, window class, titles) all use
   **`ff-dlp-viewer`**. They must stay consistent across the whole codebase.
3. `FFMPEG_URL` in `bootstrapper.c` points to the `ffmpeg.exe` asset of the latest GitHub
   release. Every release must include that asset (see `RELEASE_CHECKLIST.md`).
