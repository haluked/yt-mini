# Future Plans and Roadmap (TODO)

**English** · [Türkçe](TODO.tr.md)

## 1. Clipboard Fetch Memory Cache and Timeout
- **Description:** When the user copies a YouTube link, the video/media info fetched in
  the background is not discarded immediately; it is cached safely in memory for a few
  minutes (e.g. 5–10 minutes).
- **Use case:**
  - If the user is busy when the 4-second toast appears, or misses it, the video info is
    not lost.
  - When the user later clicks the tray icon or taskbar button, they see
    "Last Copied Media: [Video Name]" and can continue the download with one click.
  - When the timeout expires or a new link is copied, the old cache is cleared to avoid
    a memory leak.

## 2. Extend Clipboard Auto-Fetch to All Platforms ("Can we download this link?")
- **Description:** Today **only YouTube** links copied to the clipboard are auto-queried
  and show a toast. In the future, any copied link could be passed to yt-dlp with the
  question "can we download this?" (1000+ supported sites).
- **Design notes:**
  - If the copied URL's platform cannot be detected (generic/web), try silently and show
    no notification when the result fails (avoid bothering the user with needless
    toasts).
  - The toast badge currently says a fixed "▶ YOUTUBE" (`ui.c` WM_PAINT) — once platforms
    are extended, the badge should come from `meta.platform.badge_label`.
  - A small blocklist (e.g. google.com, bing.com) could be considered for URLs that
    cannot be downloaded, such as search pages.

## 3. Passive Update Check (RAM-friendly model)
- **Description:** Today the tray menu's "Check for Updates (yt-dlp)" downloads ~17–30 MB
  on every use **without checking the version first**. Target: compare versions at app
  startup and at most once a day while idle in the tray, using a single HTTP request to
  the PyPI/GitHub API.
- **Behavior:** If an update exists, the tray balloon says "Update available, click to
  install"; clicking starts the download. No background service or scheduled task (zero
  extra RAM).
- **Related work:** Add **SHA-256 verification** for downloaded yt-dlp/ffmpeg files at
  this stage (the GitHub Releases API `digest` field can be used). The update must keep
  targeting the copy the app actually uses (see the bootstrapper fix).

## 4. ffmpeg Distribution URL
- **Description:** `FFMPEG_URL` in `bootstrapper.c` still points to the old `yt-mini`
  repository's v2.0.0-beta release. When the yt-mini-lite repository is opened and its
  first release is published, this URL must move to the new repository.
- **Alternative:** Use gyan.dev or BtbN official builds as the ffmpeg source with a
  pinned SHA-256 (with the LGPL/GPL license note). Decision pending in the distribution
  architecture discussion.

## 5. Installation Strategy (architecture — to be discussed)
- **Decision (for now):** yt-dlp + ffmpeg are **always installed into the app's own
  folder** (app-dir\bin or %LOCALAPPDATA%\Programs\...\bin); the system PATH is NEVER
  touched. The app therefore always behaves "portable", and uninstalling is clean.
- **Later:** adding to PATH could be offered as an **optional checkbox** in the installer
  (for advanced users). Any registry/environment change must be reversible.

## 6. Identity/Brand Fix (yt-mini-lite transition)
- The mutex (`Local\ff-dlp-viewer`), registry key
  (`HKCU\Software\ff-dlp-viewer`), window class and titles still carry the
  **ff-dlp-viewer** name. They must all be updated at once when moving to the
  yt-mini-lite repository (a half-done rebrand mixes two names).
- Note: if the window class name changes, `FindWindowW(UI_WINDOW_CLASS, ...)` in
  `main.c` picks up the new name automatically for second-instance forwarding — changing
  both sides in the same commit is enough.

## 7. Format List Limit
- MAX_FORMATS = 256 (fixed for memory reasons). Because yt-dlp lists formats from worst
  to best, the limit now keeps the **best** ones by taking from the end (see
  `parse_json_metadata` in engine.c). A dynamic split could be considered for rare sites
  with more than 256 formats, but it is not needed in practice.
