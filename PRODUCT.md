# Product: ff-dlp-viewer (Lightweight Media Downloader)

**English** · [Türkçe](PRODUCT.tr.md)

<!-- impeccable:product-schema 1 -->

## Platform

windows

## Stack

Pure C11 (Win32 API: user32/gdi32/comctl32/shell32/ole32), standalone yt-dlp and ffmpeg binaries, custom-drawn GDI interface (gb_ui.c), system tray. No external GUI framework, Python or Electron.

## Users

- Primary user: Haluk and family members / everyday computer users who do not know terminal or technical commands.
- Situation: users who want to save a video or audio link they watched or want to keep on the web to their computer in the highest quality, with a single click and no effort.
- Goal: a fast desktop tool that waits quietly in the background, does not waste resources, and never makes them deal with complex ffmpeg/yt-dlp parameters.

## Product Purpose

Download video and audio from platforms such as YouTube, Instagram, TikTok and X (Twitter) with the most suitable codec/container the source offers, without unnecessary re-encoding, using the minimum amount of system resources (RAM/CPU), and without opening a terminal window.

## Positioning

Unlike bloated downloaders full of unnecessary "AI dashboard" decoration, hundreds of megabytes of Electron or heavy Qt dependencies, ads and telemetry; this is a standalone Windows tool with under 20 MB idle RAM, a quiet Steam/WhatsApp-style tray lifecycle, deterministic codec selection and an 8-bit interface in the Game Boy DMG palette.

## Operating Context

- Windows 10/11 desktop environment.
- The user copies a URL, opens the app window (or pastes from the clipboard), and sees the video information and available formats with one key press.
- A download starts; even if the window is closed while it runs in the background, the app shrinks to the tray icon and keeps working.
- When the download finishes it notifies the user, who can open the file location with one click.

## Capabilities and Constraints

- **Modular Platform Parser**: Detects YouTube, Instagram, X/Twitter and TikTok URL formats and applies the matching platform rules.
- **Deterministic Codec & Format Selection**: Pure mathematical priority (Video: AV1 > VP9 > H.265 > H.264; Audio: Opus > AAC > MP3). Stream copy (remux) is used whenever possible; no CPU-hungry, unnecessary re-encoding.
- **Container Transparency**: The target container (.webm, .mp4, .mkv, .opus, .m4a, .mp3) is clearly reported before downloading.
- **MP4 Compatibility Guard**: When the user forces MP4 and the codecs are incompatible (e.g. VP9/Opus), a clear warning and a conversion/remux option are offered.
- **Real Windows Installation**: A single `Setup.exe` built with Inno Setup, installs into `%LOCALAPPDATA%` without administrator rights, creates Start Menu/Desktop shortcuts, and ships a clean uninstaller.
- **Tool Bootstrapper and yt-dlp Updater**: On first run it downloads yt-dlp and ffmpeg into the app's own `bin/` folder (never touching the system `PATH`), and the tray menu can update yt-dlp with one click.

## Brand Commitments

- **Style**: Game Boy DMG 8-bit aesthetic: only 4 shades of green, pixel-stepped (chamfered) corners, hard 2 px frames/shadows, system `Consolas` font (anti-aliasing off). No external image/font files.
- **Anti-pattern bans**: No gradients, glow, blurred shadows, anti-aliased (blurry) corners, OS-drawn controls (combobox/radio/checkbox), emoji icons or generic AI-style cards, ever.
- **Language**: Fully English, plain terminology. The Turkish interface is preserved on the `design/gb-retro` branch.

## Product Principles

1. **Invisible and Lightweight**: Barely present in memory when unused (~15–20 MB RAM), never holds the CPU hostage.
2. **Respect the Source (No Needless Re-encode)**: If the source already provides a quality stream, it is copied directly instead of being encoded again.
3. **Deterministic and Testable**: Given the same URL and format list, the codec engine always produces the same output.
4. **Plain for Someone Starting from Zero**: Family members should be able to get a working file by just pasting a URL and pressing "Download".
