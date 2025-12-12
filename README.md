# yt-mini

A lightweight, high-efficiency GUI wrapper for **yt-dlp**. Designed for archiving video and audio with a focus on minimal storage footprint (VP9/Opus) and maximum ease of use.

![Python](https://img.shields.io/badge/Python-3.13.9-blue?style=flat-square)
![License](https://img.shields.io/badge/License-GPLv3-green?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey?style=flat-square)

## 🌟 Features

* **Compact & Responsive UI:** Resizable interface that fits on any screen, with a history tab that adapts to your layout.
* **Smart Quality Selection:** Choose your target resolution (e.g., 720p). If unavailable, it automatically grabs the next best quality.
* **Storage Efficient:** Defaults to **VP9 (WebM)** for video and **Opus/AAC** for audio to save disk space without losing quality.
* **Advanced Audio Mode:** Embed custom **Artist** and **Album** metadata tags directly into your files.
* **Smart Playlist Handling:** Automatically creates sub-folders for playlists and numbers files (`01 - Title.webm`).
* **Advanced Features:**
    * **Clipboard Auto-Paste:** One-click button to paste links.
    * **Batch Mode:** Import a `.txt` file to download multiple links at once.
    * **Subtitles:** Option to download English and Original language subtitles.
* **Persistent History:** Tracks your recent downloads with options to Open Folder, Hide from List, or Delete File.
 ## 💻 Supported Platforms & OS

### Operating System
* **Windows 10 / 11** (Native EXE support)
* *Linux/macOS:* Not officially supported via EXE, but source code can be run with minor modifications.

### Supported Websites
While optimized for **YouTube**, yt-mini uses the powerful `yt-dlp` engine, meaning it supports thousands of websites, including:
* **Video:** YouTube, Twitch, Vimeo, Dailymotion
* **Social:** Twitter (X), Instagram, TikTok, Reddit, Facebook
* **Audio:** SoundCloud, Bandcamp, Mixcloud

*Note: Some websites may require specific formats or user authentication which are not yet fully implemented in the GUI.*

## 🛠️ Prerequisites

This app acts as a GUI for command-line tools. You can install them manually, or use the app's **Built-in Setup Menu** to install them for you.

1.  **Windows 10/11**
2.  **[yt-dlp](https://github.com/yt-dlp/yt-dlp):** Handles the downloading.
3.  **[FFmpeg](https://ffmpeg.org/):** Required for merging video/audio and converting formats.

## 🚀 Installation & Setup

### Option A: Download the App (Recommended)
1.  Go to the **[Releases](../../releases)** page.
2.  Download `yt-mini.exe`.
3.  Run the app.
4.  Click **⚙️ Setup** (Top Left).
5.  Click **"Install yt-dlp"** and **"Install FFmpeg"** (This uses Winget).
6.  Click **"Auto-Detect Paths"**.
7.  Click **"Save & Return"**.

### Option B: Run from Source (Python)
If you prefer to run the raw Python code:

1.  Ensure you have **Python 3.13+** installed.
2.  Clone this repository:
    ```bash
    git clone [https://github.com/YOUR_USERNAME/yt-mini.git](https://github.com/YOUR_USERNAME/yt-mini.git)
    cd yt-mini
    ```
3.  Install dependencies (Standard libraries only, but you need PyInstaller to build):
    ```bash
    pip install pyinstaller
    ```
4.  Run the script:
    ```bash
    python yt-mini.py
    ```

## 📖 Usage Guide

1.  **Paste Link:** Use `Ctrl+V` or the **📋 Paste** button.
2.  **Select Mode:**
    * **Video:** Choose Format (WebM/MP4) and Max Quality (e.g., 720p).
    * **Sound:** Choose Format (MP3/Opus/AAC) and Quality (High/Med/Low).
3.  **Check Options:**
    * *Playlist:* Creates a folder named after the playlist.
    * *Subtitles:* Downloads `.vtt` files.
4.  **Advanced Options:** Click "Show Advanced Options" to import a batch list `.txt` or change the filename template.
5.  **Click EXECUTE DOWNLOAD.**

## ⚙️ Configuration
The app saves your settings in `config.txt` and your download history in `history.json` in the same folder as the executable.

* To reset everything: Go to **Setup** -> **⚠️ Factory Reset App**.

## 📄 License
This project is licensed under the **GNU General Public License v3.0** - see the [LICENSE](LICENSE) file for details.
