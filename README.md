# yt-mini

A lightweight, high-efficiency GUI wrapper for yt-dlp. Designed for archiving video and audio with a focus on minimal storage footprint and maximum ease of use.

## Installation & Setup

### Option A: Windows Executable (Recommended)

1. Download `yt-mini.exe` from the [Releases](../../releases) page.
2. Run the application. (Windows will try to block it because I don'thave ≈400$ license, run it anyway)
3. Click **Setup** (Top Left).
4. In the settings menu:
   - Click **Install yt-dlp**.
   - Click **Install FFmpeg**.
   - Click **Auto-Detect Paths**.
   - Click **Save & Return**.

### Option B: Run from Source

1. Ensure Python 3.13+ is installed.
2. Clone the repository:

```bash
git clone https://github.com/YOUR_USERNAME/yt-mini.git
cd yt-mini
```

3. Install dependencies:

```bash
pip install customtkinter
```

4. Run the script:

```bash
python main.py
```

## Usage

1. **Paste Link**: Use `Ctrl+V` or the Paste button.
2. **Select Mode**:
   - **Video**: Choose Container (WebM/MP4) and Resolution.
   - **Sound**: Choose Format (MP3/Opus) and Quality.
3. **Options**:
   - **Playlist**: Check to automatically create a subfolder and number files.
   - **Subtitles**: Check to download .vtt files.
4. **Execute**: Click EXECUTE DOWNLOAD.

## Key Features

- **Smart Quality**: Automatically grabs the best available resolution if your target is missing.
- **Storage Efficient**: Defaults to VP9/Opus for high quality at smaller file sizes.
- **Audio Metadata**: Embeds Artist and Album tags directly into music files.
- **Batch Mode**: Import a `.txt` file to download multiple links at once.
- **History**: Tracks downloads with options to open folders or delete files.

## Supported Platforms & Sites

**Operating Systems:**
- Windows 10/11 (Native)
- Linux/macOS (compatible via source)

**Supported Sites:**
YouTube, Twitch, Vimeo, Twitter (X), Instagram, TikTok, SoundCloud, Bandcamp, and more.

## Configuration

Settings are saved in `config.txt` and history in `history.json` in the application folder.

To reset the application, go to **Setup** > **Factory Reset App**.

## License

Licensed under the [GNU General Public License v3.0](LICENSE).
