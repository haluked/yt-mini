# Naming Note (NAMING.md)

**English** · [Türkçe](NAMING.tr.md)

## Status

| Place | Name | Description |
|---|---|---|
| GitHub repository (target) | **`yt-mini-lite`** | The new, permanent home of the C version. Versioning starts clean at `v1.0.0`. |
| GitHub repository (old) | `haluked/yt-mini` | Archive of the Python versions (v1.0 → v5.0 + pixel-UI v2.0). The C version branches off `beta` there. |
| Local development folder | **`ff-dlp-viewer`** | The name the code was born with and grew used to. **Deliberately not renamed** (to avoid breaking habits/paths tied to the folder name). |
| Binary name | `ff-dlp-viewer.exe` | Output of `build.bat`. |

## Why are there two names?

The project was first developed under the name `ff-dlp-viewer`; later the product
identity was attached to the `yt-mini` family. Renaming the folder would break habits
and possible path references, so **the folder name stays local**, and only the
repository and product identity become `yt-mini-lite`.

## Rule

1. **The local folder name is never referenced in code** — the code always resolves
   paths relative to the executable's own location (`GetModuleFileNameW` + `bin\`
   lookup, see `engine.c: find_binary`). The folder name is therefore irrelevant and
   can be renamed safely.
2. Identity strings inside the code (mutex, registry, window class, titles) must be
   updated **in a single commit, all at once** during the `yt-mini-lite` transition
   (see TODO.md #6).
3. `FFMPEG_URL` in `bootstrapper.c` still points to the `v2.0.0-beta` release of the old
   `yt-mini` repository; it moves to the new URL as soon as yt-mini-lite publishes its
   first release (see TODO.md #4).
