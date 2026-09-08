import os
import sys
import tkinter as tk
from tkinter import filedialog, messagebox
import customtkinter as ctk
from PIL import Image, ImageTk

import config
from engine import TranscoderManager, MediaInspector, DownloadEngine

APP_DIR = os.path.dirname(os.path.abspath(__file__))
ICON_PATH = os.path.join(APP_DIR, "app.ico")

# Modern Styling Constants
FONT_FAMILY = "Segoe UI"
COLOR_BG = "#0B0E14"
COLOR_CARD = "#151922"
COLOR_CARD_BORDER = "#232936"
COLOR_ACCENT = "#00B4D8"
COLOR_ACCENT_HOVER = "#0096C7"
COLOR_PURPLE = "#8338EC"
COLOR_GREEN = "#06D6A0"
COLOR_RED = "#EF476F"
COLOR_TEXT_DIM = "#94A3B8"

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

SUPPORTED_SITES = [
    {
        "rank": 1,
        "name": "YouTube",
        "icon": "🔴",
        "badge": "Videos • Shorts • Playlists • Channels • Music",
        "guide": "Copy URL from your browser address bar or YouTube Share button. For full playlists/albums, yt-mini auto-detects all tracks, creates a dedicated playlist subfolder, and offers sequential numbering (01, 02...).",
        "tips": "For 4K/1080p, MP4 is recommended. For songs, select MP3 or Opus with 'Embed Artwork & Tags'."
    },
    {
        "rank": 2,
        "name": "TikTok",
        "icon": "🎵",
        "badge": "Videos • Sounds • Creator Feeds",
        "guide": "Click Share -> 'Copy Link' on any TikTok video. Paste into yt-mini to download in maximum bitrate without watermarks.",
        "tips": "Select MP3 to extract the original backing sound/music track directly."
    },
    {
        "rank": 3,
        "name": "Instagram",
        "icon": "📸",
        "badge": "Reels • Posts • Stories • IGTV",
        "guide": "Click the three dots (...) or Share icon on any public Reel or Post and click 'Copy Link'.",
        "tips": "Supports all public creator reels and video carousel posts in crystal clear MP4."
    },
    {
        "rank": 4,
        "name": "Twitter / X",
        "icon": "𝕏",
        "badge": "Videos • GIFs • Spaces Recordings",
        "guide": "Click the Share button beneath any post and select 'Copy link to post'.",
        "tips": "yt-mini automatically selects the highest resolution MP4 stream."
    },
    {
        "rank": 5,
        "name": "Reddit",
        "icon": "🟠",
        "badge": "Videos • Audio Clips • DASH Streams",
        "guide": "Copy the post link directly from the browser bar or Share menu. yt-mini automatically uses Gyan FFmpeg to merge Reddit's separate video and audio tracks.",
        "tips": "No more muted Reddit video downloads!"
    },
    {
        "rank": 6,
        "name": "Facebook",
        "icon": "👥",
        "badge": "Public Videos • Reels • Watch Clips",
        "guide": "Click Share -> 'Copy Link' on any public Facebook video or Reel.",
        "tips": "Downloads in highest available HD 1080p/720p quality."
    },
    {
        "rank": 7,
        "name": "SoundCloud",
        "icon": "☁️",
        "badge": "Tracks • Albums • Playlists • Sets",
        "guide": "Paste any track, album, or artist set link. The app detects full albums and downloads every track with embedded cover art and tags.",
        "tips": "Select 'Opus (Audio)' or 'MP3' for the richest acoustic fidelity."
    },
    {
        "rank": 8,
        "name": "Twitch",
        "icon": "🟣",
        "badge": "Clips • Full VODs • Stream Highlights",
        "guide": "Copy the URL of any Clip or past stream broadcast (VOD).",
        "tips": "Supports 60fps source streams without quality degradation."
    },
    {
        "rank": 9,
        "name": "Vimeo",
        "icon": "🎬",
        "badge": "Cinema 4K • Showreels • Portfolios",
        "guide": "Copy the video URL directly from the address bar.",
        "tips": "Pristine high-bitrate video streams up to 4K resolution."
    },
    {
        "rank": 10,
        "name": "Bilibili",
        "icon": "📺",
        "badge": "Videos • Anime • Multi-Part Series",
        "guide": "Paste standard 'bilibili.com/video/BV...' URLs.",
        "tips": "Multi-part video episodes are automatically treated as playlists."
    },
    {
        "rank": 11,
        "name": "Bandcamp",
        "icon": "🎸",
        "badge": "Tracks • Full Discographies • Albums",
        "guide": "Paste any track or full album link. Choose 'FLAC (Lossless)' or 'MP3 (Audio)'.",
        "tips": "Full albums download into a clean album folder with metadata and cover artwork."
    },
    {
        "rank": 12,
        "name": "Pinterest",
        "icon": "📌",
        "badge": "Video Pins • Idea Pins",
        "guide": "Click the Share button on any pin and copy the link.",
        "tips": "Saves original high quality MP4 video pins."
    },
    {
        "rank": 13,
        "name": "Dailymotion",
        "icon": "🎥",
        "badge": "Videos • News Clips",
        "guide": "Copy the video page link directly.",
        "tips": "Supports standard 1080p and 720p feeds."
    },
    {
        "rank": 14,
        "name": "1,000+ Other Sites & Web Streams",
        "icon": "🌐",
        "badge": "BBC • CNN • TED • Kick • Threads • Generic M3U8/MP4",
        "guide": "yt-mini embeds yt-dlp's generic web extractor. If any website streams video or audio in your browser, just paste the page link.",
        "tips": "Direct HLS (.m3u8), DASH (.mpd), and embedded video players are supported."
    }
]


class ModernYtMiniApp(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("yt-mini v5 • Next-Gen Downloader")
        self.geometry("700x860")
        self.minsize(640, 750)

        if os.path.exists(ICON_PATH):
            try:
                self.iconbitmap(ICON_PATH)
            except Exception:
                pass

        # Load persisted settings
        self.cfg = config.load_config()
        ctk.set_appearance_mode(self.cfg.get("theme", "Dark"))

        # Engine state
        self.active_engine: DownloadEngine | None = None
        self.inspected_media: dict | None = None
        self.current_thumbnail_img: ctk.CTkImage | None = None

        # Thread-safe event dispatcher queue
        import queue
        self.event_queue = queue.Queue()
        self._process_event_queue()

        # Browser Extension Companion Bridge Server
        from bridge import BridgeServer
        self.bridge = BridgeServer(on_download=self._on_external_download)
        self.bridge.start()

        # Build Main Layout
        self._build_header()
        self._build_navigation()
        self._build_views()

        # Start on Downloader view
        self._switch_view("downloader")
        self._check_clipboard_on_start()

    def _on_external_download(self, payload: dict):
        """Handle download requests dispatched from browser extension."""
        url = payload.get("url", "").strip()
        fmt = payload.get("format", "MP4 (H.264)")

        def _apply():
            self.nav_seg.set("⚡ Downloader")
            self._switch_view("downloader")
            self.entry_url.delete(0, "end")
            self.entry_url.insert(0, url)
            if fmt in self.seg_format.cget("values"):
                self.seg_format.set(fmt)
                self._on_format_changed(fmt)
            try:
                self.deiconify()
                self.lift()
                self.focus_force()
            except Exception:
                pass
            self._inspect_url()
            self._toggle_download_action()

        self.dispatch(_apply)

    def dispatch(self, fn, *args):
        """Thread-safe dispatch of UI updates to the Tkinter main loop."""
        self.event_queue.put((fn, args))

    def _process_event_queue(self):
        try:
            while True:
                fn, args = self.event_queue.get_nowait()
                try:
                    fn(*args)
                except Exception as e:
                    import logging
                    logging.error(f"Error in UI dispatch: {e}")
        except Exception:
            pass
        self.after(50, self._process_event_queue)

    # -------------------------------------------------------------
    # Header & Status
    # -------------------------------------------------------------
    def _build_header(self):
        self.frame_top = ctk.CTkFrame(self, fg_color="transparent")
        self.frame_top.pack(fill="x", padx=24, pady=(16, 6))

        # Brand / Title
        f_brand = ctk.CTkFrame(self.frame_top, fg_color="transparent")
        f_brand.pack(side="left")

        lbl_logo = ctk.CTkLabel(
            f_brand,
            text="⚡",
            font=ctk.CTkFont(family=FONT_FAMILY, size=24, weight="bold"),
            text_color=COLOR_ACCENT
        )
        lbl_logo.pack(side="left", padx=(0, 6))

        lbl_title = ctk.CTkLabel(
            f_brand,
            text="yt-mini",
            font=ctk.CTkFont(family=FONT_FAMILY, size=22, weight="bold")
        )
        lbl_title.pack(side="left")

        lbl_ver = ctk.CTkLabel(
            f_brand,
            text="v5.0",
            font=ctk.CTkFont(family=FONT_FAMILY, size=11, weight="bold"),
            fg_color=COLOR_CARD,
            corner_radius=6,
            text_color=COLOR_ACCENT
        )
        lbl_ver.pack(side="left", padx=(8, 0), pady=(4, 0), ipadx=6, ipady=1)

        # Transcoder Status Indicator
        ff_info = TranscoderManager.get_ffmpeg_status()
        dot_color = COLOR_GREEN if ff_info["ready"] else COLOR_RED
        status_text = f"● {ff_info['version']}" if ff_info["ready"] else "● Transcoder Missing"

        self.lbl_engine_status = ctk.CTkLabel(
            self.frame_top,
            text=status_text,
            font=ctk.CTkFont(family=FONT_FAMILY, size=11),
            text_color=dot_color
        )
        self.lbl_engine_status.pack(side="right", pady=(6, 0))

    # -------------------------------------------------------------
    # Navigation Bar
    # -------------------------------------------------------------
    def _build_navigation(self):
        self.frame_nav = ctk.CTkFrame(self, fg_color="transparent")
        self.frame_nav.pack(fill="x", padx=24, pady=(0, 10))

        self.nav_seg = ctk.CTkSegmentedButton(
            self.frame_nav,
            values=["⚡ Downloader", "📚 Library", "⚙️ Settings"],
            font=ctk.CTkFont(family=FONT_FAMILY, size=13, weight="bold"),
            height=36,
            command=self._on_nav_change
        )
        self.nav_seg.pack(fill="x")
        self.nav_seg.set("⚡ Downloader")

    def _on_nav_change(self, value: str):
        if "Downloader" in value:
            self._switch_view("downloader")
        elif "Library" in value:
            self._switch_view("library")
            self._refresh_library()
        elif "Settings" in value:
            self._switch_view("settings")

    def _switch_view(self, name: str):
        self.view_downloader.pack_forget()
        self.view_library.pack_forget()
        self.view_settings.pack_forget()

        if name == "downloader":
            self.view_downloader.pack(fill="both", expand=True, padx=20, pady=(0, 16))
        elif name == "library":
            self.view_library.pack(fill="both", expand=True, padx=20, pady=(0, 16))
        elif name == "settings":
            self.view_settings.pack(fill="both", expand=True, padx=20, pady=(0, 16))

    # -------------------------------------------------------------
    # Main Views Construction
    # -------------------------------------------------------------
    def _build_views(self):
        self.view_downloader = ctk.CTkScrollableFrame(self, fg_color="transparent")
        self.view_library = ctk.CTkScrollableFrame(self, fg_color="transparent")
        self.view_settings = ctk.CTkScrollableFrame(self, fg_color="transparent")

        self._init_downloader_view()
        self._init_library_view()
        self._init_settings_view()

    # -------------------------------------------------------------
    # VIEW 1: Modern Downloader
    # -------------------------------------------------------------
    def _init_downloader_view(self):
        # 1. URL Hero Input Card
        card_input = ctk.CTkFrame(self.view_downloader, fg_color=COLOR_CARD, corner_radius=12, border_width=1, border_color=COLOR_CARD_BORDER)
        card_input.pack(fill="x", pady=(0, 12))

        lbl_hero = ctk.CTkLabel(
            card_input,
            text="Enter Media Link",
            font=ctk.CTkFont(family=FONT_FAMILY, size=13, weight="bold")
        )
        lbl_hero.pack(anchor="w", padx=16, pady=(12, 4))

        f_url_row = ctk.CTkFrame(card_input, fg_color="transparent")
        f_url_row.pack(fill="x", padx=14, pady=(0, 14))

        self.entry_url = ctk.CTkEntry(
            f_url_row,
            placeholder_text="Paste YouTube, SoundCloud, or playlist link...",
            height=40,
            font=ctk.CTkFont(family=FONT_FAMILY, size=12),
            corner_radius=8
        )
        self.entry_url.pack(side="left", fill="x", expand=True, padx=(0, 8))
        self.entry_url.bind("<Return>", lambda e: self._inspect_url())

        self.btn_inspect = ctk.CTkButton(
            f_url_row,
            text="📋 Paste & Fetch",
            width=120,
            height=40,
            font=ctk.CTkFont(family=FONT_FAMILY, size=12, weight="bold"),
            fg_color=COLOR_ACCENT,
            hover_color=COLOR_ACCENT_HOVER,
            corner_radius=8,
            command=self._paste_and_inspect
        )
        self.btn_inspect.pack(side="left")

        # 2. Live Media Preview Card
        self.card_preview = ctk.CTkFrame(self.view_downloader, fg_color=COLOR_CARD, corner_radius=12, border_width=1, border_color=COLOR_CARD_BORDER)
        self.card_preview.pack(fill="x", pady=(0, 12))

        # Preview Container
        self.f_preview_body = ctk.CTkFrame(self.card_preview, fg_color="transparent")
        self.f_preview_body.pack(fill="x", padx=14, pady=12)

        self.lbl_thumb = ctk.CTkLabel(
            self.f_preview_body,
            text="🎬",
            font=ctk.CTkFont(family=FONT_FAMILY, size=32),
            width=140,
            height=80,
            fg_color="#1E222D",
            corner_radius=8
        )
        self.lbl_thumb.pack(side="left", padx=(0, 12))

        self.f_meta_col = ctk.CTkFrame(self.f_preview_body, fg_color="transparent")
        self.f_meta_col.pack(side="left", fill="both", expand=True)

        self.lbl_media_title = ctk.CTkLabel(
            self.f_meta_col,
            text="No link loaded",
            font=ctk.CTkFont(family=FONT_FAMILY, size=13, weight="bold"),
            anchor="w",
            wraplength=420
        )
        self.lbl_media_title.pack(fill="x", pady=(0, 2))

        self.lbl_media_author = ctk.CTkLabel(
            self.f_meta_col,
            text="Paste a URL above to inspect resolution, duration, and track information.",
            font=ctk.CTkFont(family=FONT_FAMILY, size=11),
            text_color=COLOR_TEXT_DIM,
            anchor="w",
            wraplength=420
        )
        self.lbl_media_author.pack(fill="x")

        # Dynamic Playlist Controls Frame inside Preview Card
        self.f_playlist_bar = ctk.CTkFrame(self.card_preview, fg_color="#1A1528", corner_radius=8, border_width=1, border_color=COLOR_PURPLE)
        
        self.lbl_playlist_badge = ctk.CTkLabel(
            self.f_playlist_bar,
            text="📁 PLAYLIST DETECTED",
            font=ctk.CTkFont(family=FONT_FAMILY, size=12, weight="bold"),
            text_color="#C084FC"
        )
        self.lbl_playlist_badge.pack(anchor="w", padx=12, pady=(8, 4))

        f_pl_options = ctk.CTkFrame(self.f_playlist_bar, fg_color="transparent")
        f_pl_options.pack(fill="x", padx=12, pady=(0, 8))

        self.var_is_playlist = ctk.BooleanVar(value=True)
        self.chk_playlist_mode = ctk.CTkCheckBox(
            f_pl_options,
            text="Download Full Playlist",
            variable=self.var_is_playlist,
            font=ctk.CTkFont(family=FONT_FAMILY, size=11)
        )
        self.chk_playlist_mode.pack(side="left", padx=(0, 16))

        self.var_auto_folder = ctk.BooleanVar(value=self.cfg.get("auto_playlist_folder", True))
        self.chk_auto_folder = ctk.CTkCheckBox(
            f_pl_options,
            text="Create Playlist Subfolder",
            variable=self.var_auto_folder,
            font=ctk.CTkFont(family=FONT_FAMILY, size=11)
        )
        self.chk_auto_folder.pack(side="left", padx=(0, 16))

        self.var_enumerate = ctk.BooleanVar(value=self.cfg.get("enumerate_playlist", True))
        self.chk_enumerate = ctk.CTkCheckBox(
            f_pl_options,
            text="Number Tracks (01, 02...)",
            variable=self.var_enumerate,
            font=ctk.CTkFont(family=FONT_FAMILY, size=11)
        )
        self.chk_enumerate.pack(side="left")

        # 3. Format & Quality Selector Card
        card_format = ctk.CTkFrame(self.view_downloader, fg_color=COLOR_CARD, corner_radius=12, border_width=1, border_color=COLOR_CARD_BORDER)
        card_format.pack(fill="x", pady=(0, 12))

        lbl_format_sec = ctk.CTkLabel(
            card_format,
            text="Output Format & Quality",
            font=ctk.CTkFont(family=FONT_FAMILY, size=13, weight="bold")
        )
        lbl_format_sec.pack(anchor="w", padx=16, pady=(12, 6))

        # Format Pills
        self.var_format = ctk.StringVar(value=self.cfg.get("default_format", "MP4"))
        self.seg_format = ctk.CTkSegmentedButton(
            card_format,
            values=["MP4 (H.264)", "MKV (Pro)", "MP3 (Audio)", "Opus (Audio)", "FLAC (Lossless)"],
            variable=self.var_format,
            font=ctk.CTkFont(family=FONT_FAMILY, size=11, weight="bold"),
            height=34,
            command=self._on_format_changed
        )
        self.seg_format.pack(fill="x", padx=14, pady=(0, 10))

        # Dynamic Quality Controls Row
        self.f_dynamic_options = ctk.CTkFrame(card_format, fg_color="transparent")
        self.f_dynamic_options.pack(fill="x", padx=14, pady=(0, 12))

        self.lbl_qual_tag = ctk.CTkLabel(self.f_dynamic_options, text="Resolution:", font=ctk.CTkFont(family=FONT_FAMILY, size=11))
        self.lbl_qual_tag.pack(side="left", padx=(0, 6))

        self.combo_quality = ctk.CTkOptionMenu(
            self.f_dynamic_options,
            values=["Best (4K/1080p)", "1080p (Full HD)", "720p (HD)", "480p (SD)"],
            font=ctk.CTkFont(family=FONT_FAMILY, size=11),
            width=160
        )
        self.combo_quality.pack(side="left", padx=(0, 16))

        self.var_extra = ctk.BooleanVar(value=True)
        self.chk_extra = ctk.CTkCheckBox(
            self.f_dynamic_options,
            text="Download Subtitles (.srt)",
            variable=self.var_extra,
            font=ctk.CTkFont(family=FONT_FAMILY, size=11)
        )
        self.chk_extra.pack(side="left")

        self._on_format_changed(self.var_format.get())

        # 4. Destination Folder Card
        card_dest = ctk.CTkFrame(self.view_downloader, fg_color=COLOR_CARD, corner_radius=12, border_width=1, border_color=COLOR_CARD_BORDER)
        card_dest.pack(fill="x", pady=(0, 12))

        f_dest_row = ctk.CTkFrame(card_dest, fg_color="transparent")
        f_dest_row.pack(fill="x", padx=14, pady=10)

        lbl_folder_icon = ctk.CTkLabel(f_dest_row, text="📁", font=ctk.CTkFont(family=FONT_FAMILY, size=16))
        lbl_folder_icon.pack(side="left", padx=(0, 8))

        self.lbl_folder_path = ctk.CTkLabel(
            f_dest_row,
            text=self.cfg.get("download_path", ""),
            font=ctk.CTkFont(family=FONT_FAMILY, size=11),
            anchor="w"
        )
        self.lbl_folder_path.pack(side="left", fill="x", expand=True, padx=(0, 8))

        btn_browse = ctk.CTkButton(
            f_dest_row,
            text="Change Folder",
            width=100,
            height=30,
            font=ctk.CTkFont(family=FONT_FAMILY, size=11),
            fg_color="#2A2F3D",
            hover_color="#363C4E",
            command=self._change_download_folder
        )
        btn_browse.pack(side="right")

        # 5. Live Progress & Big Action Button Card
        card_action = ctk.CTkFrame(self.view_downloader, fg_color=COLOR_CARD, corner_radius=12, border_width=1, border_color=COLOR_CARD_BORDER)
        card_action.pack(fill="x", pady=(0, 12))

        self.progress_bar = ctk.CTkProgressBar(card_action, height=10, corner_radius=5)
        self.progress_bar.pack(fill="x", padx=16, pady=(16, 6))
        self.progress_bar.set(0)

        f_metrics_row = ctk.CTkFrame(card_action, fg_color="transparent")
        f_metrics_row.pack(fill="x", padx=16, pady=(0, 12))

        self.lbl_prog_status = ctk.CTkLabel(
            f_metrics_row,
            text="Ready to download",
            font=ctk.CTkFont(family=FONT_FAMILY, size=11)
        )
        self.lbl_prog_status.pack(side="left")

        self.lbl_prog_metrics = ctk.CTkLabel(
            f_metrics_row,
            text="",
            font=ctk.CTkFont(family=FONT_FAMILY, size=11, weight="bold"),
            text_color=COLOR_ACCENT
        )
        self.lbl_prog_metrics.pack(side="right")

        self.btn_download = ctk.CTkButton(
            card_action,
            text="⚡ START DOWNLOAD",
            height=46,
            font=ctk.CTkFont(family=FONT_FAMILY, size=14, weight="bold"),
            fg_color=COLOR_ACCENT,
            hover_color=COLOR_ACCENT_HOVER,
            corner_radius=8,
            command=self._toggle_download_action
        )
        self.btn_download.pack(fill="x", padx=16, pady=(0, 16))

    # -------------------------------------------------------------
    # Format and Quality Switching
    # -------------------------------------------------------------
    def _on_format_changed(self, fmt_val: str):
        is_audio = any(a in fmt_val for a in ["MP3", "Opus", "FLAC"])
        if is_audio:
            self.lbl_qual_tag.configure(text="Bitrate:")
            self.combo_quality.configure(values=["320 kbps (High Quality)", "256 kbps", "192 kbps", "128 kbps"])
            self.combo_quality.set("320 kbps (High Quality)")
            self.chk_extra.configure(text="Embed Artwork & Tags")
            self.chk_extra.select()
        else:
            self.lbl_qual_tag.configure(text="Resolution:")
            self.combo_quality.configure(values=["Best (4K/1080p)", "1080p (Full HD)", "720p (HD)", "480p (SD)"])
            self.combo_quality.set("1080p (Full HD)")
            self.chk_extra.configure(text="Download Subtitles (.srt)")
            self.chk_extra.deselect()

    # -------------------------------------------------------------
    # URL Paste & Inspection
    # -------------------------------------------------------------
    def _check_clipboard_on_start(self):
        try:
            clip = self.clipboard_get().strip()
            if any(s in clip for s in ["youtube.com", "youtu.be", "soundcloud.com"]):
                self.entry_url.insert(0, clip)
                self._inspect_url()
        except Exception:
            pass

    def _paste_and_inspect(self):
        try:
            clip = self.clipboard_get().strip()
            if clip:
                self.entry_url.delete(0, "end")
                self.entry_url.insert(0, clip)
                self._inspect_url()
        except Exception:
            pass

    def _inspect_url(self):
        url = self.entry_url.get().strip()
        if not url:
            return

        self.btn_inspect.configure(text="⏳ Inspecting...", state="disabled")
        self.lbl_media_title.configure(text="Analyzing link details...")
        self.lbl_media_author.configure(text="Fetching stream formats and track information from server...")
        self.f_playlist_bar.pack_forget()

        MediaInspector.inspect(url, lambda info: self.dispatch(self._apply_media_inspection, info))

    def _apply_media_inspection(self, info: dict | None):
        self.btn_inspect.configure(text="📋 Paste & Fetch", state="normal")
        if not info:
            self.lbl_media_title.configure(text="Unable to load media details")
            self.lbl_media_author.configure(text="Check internet connection or verify the link is public.")
            return

        self.inspected_media = info
        self.lbl_media_title.configure(text=info["title"])
        self.lbl_media_author.configure(text=f"By {info['uploader']}  •  Duration: {info['duration']}")

        # Show Playlist Banner if detected
        if info["is_playlist"]:
            self.f_playlist_bar.pack(fill="x", padx=14, pady=(0, 10))
            self.lbl_playlist_badge.configure(
                text=f"📁 PLAYLIST DETECTED • {info['playlist_count']} TRACKS"
            )
            self.var_is_playlist.set(True)
        else:
            self.f_playlist_bar.pack_forget()
            self.var_is_playlist.set(False)

        # Asynchronously fetch thumbnail
        if info.get("thumbnail_url"):
            threading.Thread(target=self._load_thumbnail_async, args=(info["thumbnail_url"],), daemon=True).start()

    def _load_thumbnail_async(self, thumb_url: str):
        img = MediaInspector.fetch_thumbnail_image(thumb_url, (140, 80))
        if img:
            ctk_img = ctk.CTkImage(light_image=img, dark_image=img, size=(140, 80))
            self.dispatch(self._display_thumbnail, ctk_img)

    def _display_thumbnail(self, ctk_img: ctk.CTkImage):
        self.current_thumbnail_img = ctk_img
        self.lbl_thumb.configure(image=ctk_img, text="")

    def _change_download_folder(self):
        cur = self.cfg.get("download_path", os.path.expanduser("~"))
        chosen = filedialog.askdirectory(initialdir=cur, title="Select Download Folder")
        if chosen:
            self.cfg["download_path"] = chosen
            self.lbl_folder_path.configure(text=chosen)
            config.save_config(self.cfg)

    # -------------------------------------------------------------
    # Download Execution
    # -------------------------------------------------------------
    def _toggle_download_action(self):
        if self.active_engine:
            # Cancel active download
            self.btn_download.configure(text="Cancelling...", state="disabled")
            self.lbl_prog_status.configure(text="Aborting download cleanly...", text_color="#FFA500")
            self.active_engine.cancel()
            return

        url = self.entry_url.get().strip()
        if not url:
            messagebox.showwarning("Missing URL", "Please enter or paste a valid link.")
            return

        fmt = self.var_format.get()
        is_audio = any(a in fmt for a in ["MP3", "Opus", "FLAC"])

        options = {
            "url": url,
            "download_path": self.cfg.get("download_path"),
            "mode": "audio" if is_audio else "video",
            "format": fmt,
            "quality": self.combo_quality.get(),
            "is_playlist": self.var_is_playlist.get(),
            "auto_playlist_folder": self.var_auto_folder.get(),
            "enumerate_playlist": self.var_enumerate.get(),
            "download_subtitles": self.var_extra.get() if not is_audio else False,
            "embed_metadata": self.var_extra.get() if is_audio else True
        }

        self.btn_download.configure(
            text="🛑 CANCEL DOWNLOAD",
            fg_color=COLOR_RED,
            hover_color="#C0392B",
            state="normal"
        )
        self.progress_bar.set(0)
        self.lbl_prog_status.configure(text="Initializing engine...", text_color=ctk.ThemeManager.theme["CTkLabel"]["text_color"])
        self.lbl_prog_metrics.configure(text="")

        callbacks = {
            "on_progress": lambda p, s, e, t: self.dispatch(self._on_engine_progress, p, s, e, t),
            "on_status": lambda msg: self.dispatch(self._update_prog_status, msg),
            "on_complete": lambda ok, msg: self.dispatch(self._on_engine_complete, ok, msg),
            "on_item_saved": lambda item: self.dispatch(self._refresh_library)
        }

        self.active_engine = DownloadEngine(options, callbacks)
        self.active_engine.start()

    def _update_prog_status(self, msg: str):
        self.lbl_prog_status.configure(text=msg)

    def _on_engine_progress(self, percent: float, speed: str, eta: str, total_str: str):
        safe_p = max(0.0, min(100.0, percent)) / 100.0
        self.progress_bar.set(safe_p)

        pills = []
        if speed:
            pills.append(f"⚡ {speed}")
        if eta:
            pills.append(f"⏳ ETA {eta}")
        if total_str:
            pills.append(f"📦 {total_str}")

        self.lbl_prog_metrics.configure(text="   ".join(pills))

    def _on_engine_complete(self, success: bool, message: str):
        self.active_engine = None
        self.progress_bar.set(1.0 if success else 0.0)
        self.lbl_prog_status.configure(
            text=message,
            text_color=COLOR_GREEN if success else COLOR_RED
        )
        self.lbl_prog_metrics.configure(text="")
        self.btn_download.configure(
            text="⚡ START DOWNLOAD",
            fg_color=COLOR_ACCENT,
            hover_color=COLOR_ACCENT_HOVER,
            state="normal"
        )
        self._refresh_library()

    # -------------------------------------------------------------
    # VIEW 2: Library / History
    # -------------------------------------------------------------
    def _init_library_view(self):
        f_top = ctk.CTkFrame(self.view_library, fg_color="transparent")
        f_top.pack(fill="x", pady=(0, 10))

        lbl_lib = ctk.CTkLabel(
            f_top,
            text="Downloaded Media Library",
            font=ctk.CTkFont(family=FONT_FAMILY, size=15, weight="bold")
        )
        lbl_lib.pack(side="left")

        btn_clear_all = ctk.CTkButton(
            f_top,
            text="Clear History",
            width=90,
            height=28,
            fg_color="transparent",
            border_width=1,
            border_color="#C9302C",
            text_color="#FF6666",
            hover_color="#441111",
            command=self._clear_library_action
        )
        btn_clear_all.pack(side="right")

        self.frame_lib_cards = ctk.CTkFrame(self.view_library, fg_color="transparent")
        self.frame_lib_cards.pack(fill="both", expand=True)

    def _refresh_library(self):
        for widget in self.frame_lib_cards.winfo_children():
            widget.destroy()

        records = config.load_history()
        if not records:
            lbl_empty = ctk.CTkLabel(
                self.frame_lib_cards,
                text="No downloads in library yet.\nDownloaded files will appear here with instant play and folder access.",
                font=ctk.CTkFont(family=FONT_FAMILY, size=12, slant="italic"),
                text_color=COLOR_TEXT_DIM
            )
            lbl_empty.pack(pady=60)
            return

        for item in records:
            card = ctk.CTkFrame(self.frame_lib_cards, fg_color=COLOR_CARD, corner_radius=10, border_width=1, border_color=COLOR_CARD_BORDER)
            card.pack(fill="x", pady=4)

            f_inner = ctk.CTkFrame(card, fg_color="transparent")
            f_inner.pack(fill="both", expand=True, padx=12, pady=10)

            # Left info
            f_info = ctk.CTkFrame(f_inner, fg_color="transparent")
            f_info.pack(side="left", fill="both", expand=True)

            t_str = item.get("title") or os.path.basename(item.get("path", "Unknown"))
            lbl_t = ctk.CTkLabel(
                f_info,
                text=t_str,
                font=ctk.CTkFont(family=FONT_FAMILY, size=12, weight="bold"),
                anchor="w"
            )
            lbl_t.pack(fill="x")

            meta_str = f"Duration: {item.get('duration', 'N/A')}  •  Size: {item.get('size', 'N/A')}"
            lbl_m = ctk.CTkLabel(
                f_info,
                text=meta_str,
                font=ctk.CTkFont(family=FONT_FAMILY, size=10),
                text_color=COLOR_TEXT_DIM,
                anchor="w"
            )
            lbl_m.pack(fill="x")

            # Right action buttons
            f_acts = ctk.CTkFrame(f_inner, fg_color="transparent")
            f_acts.pack(side="right", padx=(8, 0))

            p = item.get("path", "")
            btn_p = ctk.CTkButton(
                f_acts,
                text="▶ Play",
                width=60,
                height=28,
                fg_color=COLOR_ACCENT,
                hover_color=COLOR_ACCENT_HOVER,
                command=lambda target=p: config.open_file_safely(target)
            )
            btn_p.pack(side="left", padx=2)

            btn_f = ctk.CTkButton(
                f_acts,
                text="📂 Folder",
                width=68,
                height=28,
                fg_color="#2A2F3D",
                hover_color="#363C4E",
                command=lambda target=p: config.open_folder_safely(target)
            )
            btn_f.pack(side="left", padx=2)

            btn_d = ctk.CTkButton(
                f_acts,
                text="✕",
                width=28,
                height=28,
                fg_color="transparent",
                text_color="#FF6666",
                hover_color="#441111",
                command=lambda target=p: self._remove_library_item(target)
            )
            btn_d.pack(side="left", padx=2)

    def _remove_library_item(self, path: str):
        config.remove_history_entry(path)
        self._refresh_library()

    def _clear_library_action(self):
        if not config.load_history():
            return
        if messagebox.askyesno("Clear Library", "Clear download history list?"):
            config.clear_history()
            self._refresh_library()

    # -------------------------------------------------------------
    # VIEW 3: Settings (Self-Managing & Zero Manual Paths)
    # -------------------------------------------------------------
    def _init_settings_view(self):
        # Engine Health Card
        card_diag = ctk.CTkFrame(self.view_settings, fg_color=COLOR_CARD, corner_radius=12, border_width=1, border_color=COLOR_CARD_BORDER)
        card_diag.pack(fill="x", pady=(0, 12))

        lbl_d = ctk.CTkLabel(
            card_diag,
            text="Engine Architecture",
            font=ctk.CTkFont(family=FONT_FAMILY, size=13, weight="bold")
        )
        lbl_d.pack(anchor="w", padx=16, pady=(12, 6))

        # yt-dlp Status Row
        f_diag_yt = ctk.CTkFrame(card_diag, fg_color="transparent")
        f_diag_yt.pack(fill="x", padx=16, pady=4)
        ctk.CTkLabel(f_diag_yt, text="Downloader Core:", font=ctk.CTkFont(family=FONT_FAMILY, size=12, weight="bold")).pack(side="left")
        ctk.CTkLabel(f_diag_yt, text="● Embedded Python Core (Active)", font=ctk.CTkFont(family=FONT_FAMILY, size=11), text_color=COLOR_GREEN).pack(side="right")

        # FFmpeg Status Row
        ff_info = TranscoderManager.get_ffmpeg_status()
        f_diag_ff = ctk.CTkFrame(card_diag, fg_color="transparent")
        f_diag_ff.pack(fill="x", padx=16, pady=4)
        ctk.CTkLabel(f_diag_ff, text="Media Transcoder:", font=ctk.CTkFont(family=FONT_FAMILY, size=12, weight="bold")).pack(side="left")
        ctk.CTkLabel(f_diag_ff, text=f"● {ff_info['version']}", font=ctk.CTkFont(family=FONT_FAMILY, size=11), text_color=COLOR_GREEN if ff_info['ready'] else COLOR_RED).pack(side="right")

        # Browser Extension Bridge Row
        f_diag_bridge = ctk.CTkFrame(card_diag, fg_color="transparent")
        f_diag_bridge.pack(fill="x", padx=16, pady=(4, 12))
        ctk.CTkLabel(f_diag_bridge, text="Browser Extension Bridge:", font=ctk.CTkFont(family=FONT_FAMILY, size=12, weight="bold")).pack(side="left")
        ctk.CTkLabel(f_diag_bridge, text="● Active (127.0.0.1:48123)", font=ctk.CTkFont(family=FONT_FAMILY, size=11), text_color=COLOR_GREEN).pack(side="right")

        # Preferences Card
        card_pref = ctk.CTkFrame(self.view_settings, fg_color=COLOR_CARD, corner_radius=12, border_width=1, border_color=COLOR_CARD_BORDER)
        card_pref.pack(fill="x", pady=(0, 12))

        lbl_pref = ctk.CTkLabel(
            card_pref,
            text="Preferences",
            font=ctk.CTkFont(family=FONT_FAMILY, size=13, weight="bold")
        )
        lbl_pref.pack(anchor="w", padx=16, pady=(12, 8))

        # Default Folder Row
        f_fold_row = ctk.CTkFrame(card_pref, fg_color="transparent")
        f_fold_row.pack(fill="x", padx=16, pady=6)
        ctk.CTkLabel(f_fold_row, text="Default Download Folder:", font=ctk.CTkFont(family=FONT_FAMILY, size=11)).pack(side="left")
        btn_pref_fold = ctk.CTkButton(f_fold_row, text="Browse", width=80, height=28, command=self._change_download_folder)
        btn_pref_fold.pack(side="right")

        # Theme Row
        f_th_row = ctk.CTkFrame(card_pref, fg_color="transparent")
        f_th_row.pack(fill="x", padx=16, pady=(6, 14))
        ctk.CTkLabel(f_th_row, text="Interface Theme:", font=ctk.CTkFont(family=FONT_FAMILY, size=11)).pack(side="left")
        self.seg_pref_theme = ctk.CTkSegmentedButton(
            f_th_row,
            values=["Dark", "Light"],
            height=28,
            command=self._on_theme_changed
        )
        self.seg_pref_theme.set(self.cfg.get("theme", "Dark"))
        self.seg_pref_theme.pack(side="right")

        # 3. Supported Platforms & Download Guide Card
        card_sites = ctk.CTkFrame(self.view_settings, fg_color=COLOR_CARD, corner_radius=12, border_width=1, border_color=COLOR_CARD_BORDER)
        card_sites.pack(fill="x", pady=(0, 16))

        lbl_sites_title = ctk.CTkLabel(
            card_sites,
            text="🌐 Supported Sites & Quick Guide (Most to Least Popular)",
            font=ctk.CTkFont(family=FONT_FAMILY, size=13, weight="bold")
        )
        lbl_sites_title.pack(anchor="w", padx=16, pady=(12, 4))

        lbl_sites_sub = ctk.CTkLabel(
            card_sites,
            text="yt-mini supports over 1,000+ websites. Here is the ranked list with specific download instructions for each platform.",
            font=ctk.CTkFont(family=FONT_FAMILY, size=11),
            text_color=COLOR_TEXT_DIM,
            wraplength=600,
            justify="left"
        )
        lbl_sites_sub.pack(anchor="w", padx=16, pady=(0, 8))

        # Search / Filter Bar for Sites
        f_search_row = ctk.CTkFrame(card_sites, fg_color="transparent")
        f_search_row.pack(fill="x", padx=16, pady=(0, 10))

        self.entry_site_search = ctk.CTkEntry(
            f_search_row,
            placeholder_text="🔍 Filter platform (e.g. YouTube, TikTok, Reddit, SoundCloud)...",
            height=34,
            font=ctk.CTkFont(family=FONT_FAMILY, size=11)
        )
        self.entry_site_search.pack(fill="x")
        self.entry_site_search.bind("<KeyRelease>", lambda e: self._filter_sites_guide())

        # Container for Site Cards
        self.f_site_cards_container = ctk.CTkFrame(card_sites, fg_color="transparent")
        self.f_site_cards_container.pack(fill="x", padx=16, pady=(0, 12))

        self._render_site_cards(SUPPORTED_SITES)

        # Official yt-dlp link button
        import webbrowser
        btn_all_sites = ctk.CTkButton(
            card_sites,
            text="🔗 View All 1,000+ Supported Extractors on GitHub",
            height=32,
            font=ctk.CTkFont(family=FONT_FAMILY, size=11),
            fg_color="#1E222D",
            hover_color="#2B3040",
            command=lambda: webbrowser.open("https://github.com/yt-dlp/yt-dlp/blob/master/supportedsites.md")
        )
        btn_all_sites.pack(fill="x", padx=16, pady=(0, 14))

    def _render_site_cards(self, sites_list: list):
        for widget in self.f_site_cards_container.winfo_children():
            widget.destroy()

        if not sites_list:
            lbl_no = ctk.CTkLabel(
                self.f_site_cards_container,
                text="No matching platforms found.",
                font=ctk.CTkFont(family=FONT_FAMILY, size=11, slant="italic"),
                text_color=COLOR_TEXT_DIM
            )
            lbl_no.pack(pady=12)
            return

        for s in sites_list:
            card = ctk.CTkFrame(self.f_site_cards_container, fg_color="#1A1E29", corner_radius=8, border_width=1, border_color="#262C3D")
            card.pack(fill="x", pady=4)

            f_c_head = ctk.CTkFrame(card, fg_color="transparent")
            f_c_head.pack(fill="x", padx=12, pady=(8, 2))

            # Rank & Name
            lbl_title = ctk.CTkLabel(
                f_c_head,
                text=f"#{s['rank']}  {s['icon']} {s['name']}",
                font=ctk.CTkFont(family=FONT_FAMILY, size=12, weight="bold"),
                anchor="w"
            )
            lbl_title.pack(side="left")

            # Media Tag Pill
            lbl_badge = ctk.CTkLabel(
                f_c_head,
                text=s["badge"],
                font=ctk.CTkFont(family=FONT_FAMILY, size=10),
                fg_color="#242A38",
                text_color=COLOR_ACCENT,
                corner_radius=4
            )
            lbl_badge.pack(side="right", ipadx=6, ipady=1)

            # How to download guide
            lbl_g = ctk.CTkLabel(
                card,
                text=f"• How to download: {s['guide']}",
                font=ctk.CTkFont(family=FONT_FAMILY, size=10),
                text_color="#CBD5E1",
                anchor="w",
                justify="left",
                wraplength=590
            )
            lbl_g.pack(fill="x", padx=12, pady=(2, 2))

            # Pro tip
            lbl_tip = ctk.CTkLabel(
                card,
                text=f"• Pro tip: {s['tips']}",
                font=ctk.CTkFont(family=FONT_FAMILY, size=10),
                text_color=COLOR_TEXT_DIM,
                anchor="w",
                justify="left",
                wraplength=590
            )
            lbl_tip.pack(fill="x", padx=12, pady=(0, 8))

    def _filter_sites_guide(self):
        query = self.entry_site_search.get().strip().lower()
        if not query:
            self._render_site_cards(SUPPORTED_SITES)
            return

        filtered = [
            s for s in SUPPORTED_SITES
            if query in s["name"].lower() or query in s["badge"].lower() or query in s["guide"].lower() or query in s["tips"].lower()
        ]
        self._render_site_cards(filtered)

    def _on_theme_changed(self, theme_val: str):
        ctk.set_appearance_mode(theme_val)
        self.cfg["theme"] = theme_val
        config.save_config(self.cfg)


if __name__ == "__main__":
    app = ModernYtMiniApp()
    app.mainloop()
