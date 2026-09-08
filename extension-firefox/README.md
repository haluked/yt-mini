# yt-mini Mozilla Firefox Add-on

This extension pairs with **yt-mini v5** on **Mozilla Firefox** to detect downloadable videos in real-time as you browse with **zero video stream bandwidth consumption**, and lets you download with a single click.

---

## 🚀 How to Install in Mozilla Firefox

1. **Open Firefox Debugging Page**:
   - In Firefox address bar, navigate to `about:debugging#/runtime/this-firefox`.
   - (Or navigate to `about:debugging` and click **"This Firefox"** in the left sidebar).
2. **Load the Add-on**:
   - Under the **"Temporary Extensions"** section, click **"Load Temporary Add-on..."**.
   - Browse to the `extension-firefox` folder and select `manifest.json`.
3. **Pin to Toolbar**:
   - Click the puzzle piece (Extensions) icon in the Firefox toolbar, right-click **yt-mini**, and select **"Pin to Toolbar"**.

---

## ⚡ How It Works

- **Live Green Light**: When you open any video on YouTube, TikTok, Reddit, SoundCloud, etc., the extension icon will display a bright green badge (`●`) indicating that downloadable media has been detected.
- **Zero Video Bandwidth**: Pinging yt-dlp only queries the webpage's ~20KB JSON metadata. Zero media stream bytes are downloaded until you actually click download.
- **One-Click Download**: Open the popup, select your format (`MP4 Video`, `MP3 Audio`, `Opus`), and click **"⚡ Download Video Now"**. The download will immediately start in your `yt-mini v5` desktop application.
- **Offline Fallback**: If the desktop app is not running, the popup displays a helpful prompt with a direct link to download the desktop application.
