# yt-mini Brave / Chrome Web Extension

This browser extension pairs with **yt-mini v5** to detect downloadable videos in real-time as you browse with **zero video stream bandwidth consumption**, and lets you download with a single click.

---

## 🚀 How to Install in Brave / Chrome / Edge

1. **Open Extensions Page**:
   - In Brave, open `brave://extensions` (in Chrome, `chrome://extensions`).
2. **Enable Developer Mode**:
   - Toggle the **"Developer mode"** switch in the top right corner.
3. **Load the Extension**:
   - Click **"Load unpacked"** in the top left.
   - Select this `extension` folder:
     `C:\Users\haluk\Desktop\projects\Github yt-mini repisotory\yt-mini v5\extension`
4. **Pin to Toolbar**:
   - Click the puzzle icon in your browser toolbar and pin **yt-mini** for easy access.

---

## ⚡ How It Works

- **Live Green Light**: When you open any video on YouTube, TikTok, Reddit, SoundCloud, etc., the extension icon will display a bright green badge (`●`) indicating that downloadable media has been detected.
- **Zero Video Bandwidth**: Pinging yt-dlp only queries the webpage's ~20KB JSON metadata. Zero media stream bytes are downloaded until you actually click download.
- **One-Click Download**: Open the popup, select your format (`MP4 Video`, `MP3 Audio`, `Opus`), and click **"⚡ Download Video Now"**. The download will immediately start in your `yt-mini v5` desktop application.
- **Offline Fallback**: If the desktop app is not running, the popup displays a helpful prompt with a direct link to download the desktop application.
