# yt-mini v5 • Next-Gen Media & Playlist Downloader

A commercial-grade, modern desktop media downloader built with Python and CustomTkinter, powered by an **embedded native yt-dlp core** and **intelligent FFmpeg integration**.

---

## ⚡ What Makes v5 Different

1. **Embedded & Zero-Configuration Engine**:
   - **No external `yt-dlp.exe` needed**: Runs directly inside Python using the official `yt_dlp` native API for faster speeds, lower latency, and zero process spawning issues.
   - **Smart Gyan FFmpeg Transcoder**: Automatically detects your system's best FFmpeg build (`Gyan.FFmpeg.Shared` / `Gyan.FFmpeg.Full`) and excludes incompatible CAD/SolidWorks binaries.
   - **Zero Path Management**: Users never need to manually paste or browse for executable paths.

2. **Live Media & Playlist Inspection**:
   - Paste any link to instantly fetch and display:
     - High-resolution 16:9 thumbnail preview.
     - Video title, creator, and duration.
     - **Playlist Intelligence**: Automatically detects playlists/albums and displays a vibrant track count badge (e.g. `📁 PLAYLIST DETECTED • 17 TRACKS`).
     - **Dedicated Playlist Folder**: Automatically organizes downloads into a folder named after the playlist (`%(playlist_title)s/...`).
     - **Sequential Track Numbering**: Toggle item enumeration (`01 - Title`, `02 - Title`).
     - Option to download the entire playlist or just the single selected video.

3. **Universal Format & Quality Selection**:
   - **Video**: MP4 (H.264 / AAC for 100% universal playback on phones, TVs, and PCs), MKV (Pro), or WebM (VP9).
   - **Audio**: High-bitrate MP3 (320k), Opus (Hi-Fi), or Lossless FLAC with automatic cover art and tag embedding.
   - **Subtitles**: Toggle subtitle downloading (`.srt` / `.vtt`).

4. **Interactive Supported Platforms Guide (Settings Tab)**:
   - Includes a ranked list of major platforms from most popular to least popular (YouTube, TikTok, Instagram, Twitter/X, Reddit, Facebook, SoundCloud, Twitch, Vimeo, Bilibili, Bandcamp, Pinterest, Dailymotion, and 1,000+ others).
   - Features specific "How to download" steps, pro tips, and a real-time live search filter.

5. **Modern, Responsive CustomTkinter Interface**:
   - Card-based dark obsidian layout with electric cyan and neon violet accents.
   - Smooth animated progress bar with live download speed (MB/s), ETA countdown, and item progress.
   - Instant playback and "Reveal in Explorer" buttons right from the app.
   - Graceful thread-safe cancellation without crashing or leaving orphaned processes.

---

## 🚀 Running yt-mini v5

### 1. Install Requirements
```bash
pip install -r requirements.txt
```

### 2. Launch
```bash
python main.py
```

### 3. Build a Standalone Windows .EXE
To build a single, portable `.exe` file with all theme assets and icon bundled:
```bash
python -m PyInstaller --noconsole --onefile --icon=app.ico --collect-all customtkinter --collect-all yt_dlp --add-data "app.ico;." main.py -n "yt-mini-v5"
```

---

## 🧩 Browser Extensions

yt-mini v5 comes with companion browser extensions that detect downloadable video/audio streams in real time with **zero bandwidth consumption**:

- **Brave / Chrome / Edge**: Located in [`extension/`](extension/). Load unpacked via `brave://extensions` or `chrome://extensions`.
- **Mozilla Firefox**: Located in [`extension-firefox/`](extension-firefox/). Load temporary add-on via `about:debugging#/runtime/this-firefox`.

