import os
import shutil
import subprocess
import threading
import io
import logging
from typing import Optional, Callable
from PIL import Image
import requests
import yt_dlp
from config import add_history_entry, APP_DIR

CREATE_NO_WINDOW = 0x08000000 if os.name == "nt" else 0

class DownloadCancelled(Exception):
    """Raised to gracefully abort an in-progress yt-dlp download."""
    pass


class TranscoderManager:
    """
    Manages FFmpeg discovery, prioritization (Gyan builds),
    and automatic background setup without requiring user intervention.
    """
    _cached_path: Optional[str] = None

    @classmethod
    def get_ffmpeg_path(cls) -> str:
        if cls._cached_path and os.path.exists(cls._cached_path):
            return cls._cached_path

        # 1. Local application bin directory
        local_bin = os.path.join(APP_DIR, "bin", "ffmpeg.exe")
        if os.path.exists(local_bin):
            cls._cached_path = local_bin
            return local_bin

        # 2. Search PATH, WinGet, and standard directories
        candidates = cls._scan_system_ffmpeg()
        for c in candidates:
            if c["score"] > 0 and os.path.exists(c["path"]):
                cls._cached_path = c["path"]
                return c["path"]

        return ""

    @classmethod
    def get_ffmpeg_status(cls) -> dict:
        path = cls.get_ffmpeg_path()
        if not path:
            return {"ready": False, "version": "Not Found", "path": ""}
        try:
            res = subprocess.run(
                [path, "-version"],
                capture_output=True,
                text=True,
                timeout=4,
                creationflags=CREATE_NO_WINDOW
            )
            first_line = (res.stdout or res.stderr or "").strip().splitlines()[0]
            # Clean up version string
            label = "Gyan FFmpeg" if "gyan" in first_line.lower() else "FFmpeg"
            return {"ready": True, "version": f"{label} (Ready)", "path": path}
        except Exception:
            return {"ready": True, "version": "FFmpeg (Active)", "path": path}

    @classmethod
    def _scan_system_ffmpeg(cls) -> list[dict]:
        raw_paths = []
        try:
            res = subprocess.run(
                ["where", "ffmpeg"],
                capture_output=True,
                text=True,
                creationflags=CREATE_NO_WINDOW
            )
            if res.returncode == 0:
                for line in res.stdout.strip().splitlines():
                    if line.strip() and os.path.exists(line.strip()):
                        raw_paths.append(os.path.normpath(line.strip()))
        except Exception:
            pass

        sh_which = shutil.which("ffmpeg")
        if sh_which and os.path.exists(sh_which):
            raw_paths.append(os.path.normpath(sh_which))

        local_app_data = os.environ.get("LOCALAPPDATA", "")
        if local_app_data:
            winget_pkgs = os.path.join(local_app_data, "Microsoft", "WinGet", "Packages")
            if os.path.exists(winget_pkgs):
                try:
                    for root, _, files in os.walk(winget_pkgs):
                        for f in files:
                            if f.lower() == "ffmpeg.exe":
                                raw_paths.append(os.path.normpath(os.path.join(root, f)))
                except Exception:
                    pass

        common_locs = [
            r"C:\ffmpeg\bin\ffmpeg.exe",
            r"C:\Program Files\ffmpeg\bin\ffmpeg.exe",
            r"C:\tools\ffmpeg\bin\ffmpeg.exe"
        ]
        for c in common_locs:
            if os.path.exists(c):
                raw_paths.append(os.path.normpath(c))

        deduped = list(dict.fromkeys(raw_paths))
        scored = []
        for p in deduped:
            p_lower = p.lower()
            # Explicitly exclude SolidWorks / CAD bundled binaries
            if "solidworks" in p_lower or "sldworks" in p_lower or "dassault" in p_lower:
                score = -1000
            elif "gyan" in p_lower and "shared" in p_lower:
                score = 150
            elif "gyan" in p_lower or "full_build" in p_lower:
                score = 100
            elif "yt-dlp.ffmpeg" in p_lower:
                score = 80
            else:
                score = 40
            scored.append({"path": p, "score": score})

        scored.sort(key=lambda x: x["score"], reverse=True)
        return scored


class MediaInspector:
    """Extracts media metadata and downloads thumbnails asynchronously."""

    @staticmethod
    def inspect(url: str, callback: Callable[[Optional[dict]], None]):
        def _worker():
            clean_url = url.strip()
            if not clean_url:
                callback(None)
                return

            if not (clean_url.startswith("http://") or clean_url.startswith("https://")):
                clean_url = "https://" + clean_url

            ydl_opts = {
                "extract_flat": "in_playlist",
                "quiet": True,
                "no_warnings": True,
                "skip_download": True
            }

            try:
                with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                    info = ydl.extract_info(clean_url, download=False)
                    if not info:
                        callback(None)
                        return

                    is_pl = (info.get("_type") == "playlist" or "entries" in info)
                    entries = list(info.get("entries", [])) if is_pl else []
                    count = len(entries) if is_pl else 1

                    title = info.get("title") or "Unknown Title"
                    uploader = info.get("uploader") or info.get("channel") or "Unknown Creator"
                    duration = info.get("duration_string") or "N/A"
                    thumb_url = info.get("thumbnail") or ""

                    # If playlist without thumb, pick first entry thumb
                    if is_pl and not thumb_url and entries:
                        first = entries[0]
                        if isinstance(first, dict):
                            thumb_url = first.get("thumbnail") or ""

                    media_info = {
                        "url": clean_url,
                        "title": title,
                        "uploader": uploader,
                        "duration": duration,
                        "thumbnail_url": thumb_url,
                        "is_playlist": is_pl,
                        "playlist_count": count
                    }
                    callback(media_info)
            except Exception as e:
                logging.error(f"Failed to inspect URL {clean_url}: {e}")
                callback(None)

        threading.Thread(target=_worker, daemon=True).start()

    @staticmethod
    def fetch_thumbnail_image(url: str, size: tuple[int, int] = (160, 90)) -> Optional[Image.Image]:
        """Fetch remote thumbnail and resize it to given dimensions."""
        if not url:
            return None
        try:
            resp = requests.get(url, timeout=5)
            if resp.status_code == 200:
                img = Image.open(io.BytesIO(resp.content))
                img = img.convert("RGBA")
                img.thumbnail(size, Image.Resampling.LANCZOS)
                return img
        except Exception:
            pass
        return None


class DownloadEngine:
    """
    High-performance, native download engine powered by the official yt-dlp API.
    Supports playlist subfolders, enumeration, multiple formats, and safe cancellation.
    """

    def __init__(self, options: dict, callbacks: dict):
        self.options = options
        self.callbacks = callbacks  # on_progress, on_status, on_complete, on_item_saved
        self.is_cancelled = False
        self._thread = None

    def start(self):
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def cancel(self):
        self.is_cancelled = True

    def _run(self):
        url = self.options["url"]
        download_dir = self.options.get("download_path") or os.getcwd()
        os.makedirs(download_dir, exist_ok=True)

        mode = self.options.get("mode", "video").lower()
        fmt_choice = self.options.get("format", "MP4").upper()
        quality_choice = self.options.get("quality", "1080p").lower()

        is_playlist = self.options.get("is_playlist", False)
        auto_folder = self.options.get("auto_playlist_folder", True)
        enumerate_items = self.options.get("enumerate_playlist", True)

        # Build Output Template
        if is_playlist:
            pl_dir = "%(playlist_title,playlist|Playlists)s"
            if auto_folder:
                out_tmpl = f"{pl_dir}/%(playlist_index&{{:02d}} - |)s%(title)s.%(ext)s"
            else:
                out_tmpl = "%(playlist_index&{:02d} - |)s%(title)s.%(ext)s" if enumerate_items else "%(title)s.%(ext)s"
        else:
            out_tmpl = "%(title)s.%(ext)s"

        ydl_opts = {
            "outtmpl": os.path.join(download_dir, out_tmpl),
            "noplaylist": not is_playlist,
            "quiet": True,
            "no_warnings": True,
            "ignoreerrors": True,
            "progress_hooks": [self._progress_hook],
            "postprocessor_hooks": [self._postprocessor_hook]
        }

        ff_path = TranscoderManager.get_ffmpeg_path()
        if ff_path:
            ydl_opts["ffmpeg_location"] = ff_path

        # Configure Formats & Postprocessors
        if mode == "video":
            # Map quality height
            h_map = {"4k": "2160", "2160": "2160", "1440": "1440", "2k": "1440",
                     "1080": "1080", "720": "720", "480": "480", "360": "360"}
            h_limit = None
            if "best" not in quality_choice:
                for k, v in h_map.items():
                    if k in quality_choice:
                        h_limit = v
                        break

            if "MKV" in fmt_choice:
                ydl_opts["merge_output_format"] = "mkv"
                ydl_opts["format"] = f"bv*[height<={h_limit}]+ba/b[height<={h_limit}]" if h_limit else "bv*+ba/b"
            elif "WEBM" in fmt_choice:
                ydl_opts["merge_output_format"] = "webm"
                ydl_opts["format"] = f"bv*[height<={h_limit}]+ba/b[height<={h_limit}]" if h_limit else "bv*+ba/b"
                ydl_opts["format_sort"] = ["vcodec:vp9"]
            else:
                # MP4 Universal
                # Note: Do not restrict bv* to [ext=mp4] because 1440p/4K YouTube streams
                # are exclusively VP9/AV1. FFmpeg will remux/merge them into an MP4 container seamlessly.
                ydl_opts["merge_output_format"] = "mp4"
                if h_limit:
                    ydl_opts["format"] = f"bv*[height<={h_limit}]+ba/b[height<={h_limit}]"
                else:
                    ydl_opts["format"] = "bv*+ba/b"
                ydl_opts["format_sort"] = ["res", "vcodec:h264,vp9", "acodec:m4a,aac"]

            if self.options.get("download_subtitles", False):
                ydl_opts["writesubtitles"] = True
                ydl_opts["writeautomaticsub"] = True
                ydl_opts["subtitleslangs"] = ["en.*", "en"]
                if "postprocessors" not in ydl_opts:
                    ydl_opts["postprocessors"] = []
                ydl_opts["postprocessors"].append({"key": "FFmpegSubtitlesConvertor", "format": "srt"})

        else:
            # Audio Extraction Mode
            a_fmt = "mp3"
            for cand in ["opus", "aac", "m4a", "flac", "wav", "mp3"]:
                if cand in fmt_choice.lower():
                    a_fmt = cand
                    break

            q_val = "0"
            for val in ["320", "256", "192", "128", "96"]:
                if val in quality_choice:
                    q_val = f"{val}k"
                    break

            ydl_opts["format"] = "bestaudio/best"
            pps = [{
                "key": "FFmpegExtractAudio",
                "preferredcodec": a_fmt,
                "preferredquality": q_val
            }]
            if self.options.get("embed_metadata", True):
                pps.append({"key": "FFmpegMetadata", "add_metadata": True})
                # Convert WebP/PNG thumbnails to JPEG before embedding into MP3 ID3 tags
                pps.append({"key": "FFmpegThumbnailsConvertor", "format": "jpg"})
                pps.append({"key": "EmbedThumbnail", "already_have_thumbnail": False})
                ydl_opts["writethumbnail"] = True

            ydl_opts["postprocessors"] = pps

        # Execute Download
        self.callbacks.get("on_status", lambda s: None)("Connecting to media stream...")
        try:
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                ydl.download([url])

            if self.is_cancelled:
                self.callbacks.get("on_complete", lambda ok, msg: None)(False, "Download cancelled.")
            else:
                self.callbacks.get("on_complete", lambda ok, msg: None)(True, "Download completed successfully!")
        except DownloadCancelled:
            self.callbacks.get("on_complete", lambda ok, msg: None)(False, "Download cancelled.")
        except Exception as e:
            logging.error(f"Download exception: {e}")
            if self.is_cancelled:
                self.callbacks.get("on_complete", lambda ok, msg: None)(False, "Download cancelled.")
            else:
                self.callbacks.get("on_complete", lambda ok, msg: None)(False, f"Download failed: {e}")

    def _progress_hook(self, d: dict):
        if self.is_cancelled:
            raise DownloadCancelled("Aborted by user")

        status = d.get("status")
        if status == "downloading":
            pct = d.get("_percent")
            if pct is None:
                downloaded = d.get("downloaded_bytes", 0)
                total = d.get("total_bytes") or d.get("total_bytes_estimate") or 0
                if total > 0:
                    pct = (downloaded / total) * 100.0
                else:
                    pct = 0.0

            speed = d.get("_speed_str") or ""
            eta = d.get("_eta_str") or ""
            total_str = d.get("_total_bytes_str") or d.get("_total_bytes_estimate_str") or ""

            # Playlist index info if available
            info = d.get("info_dict", {})
            cur = info.get("playlist_index")
            total = info.get("n_entries")
            prefix = f"Item {cur}/{total} • " if cur and total else ""

            self.callbacks.get("on_progress", lambda p, s, e, t: None)(pct, speed, eta, total_str)
            self.callbacks.get("on_status", lambda s: None)(f"{prefix}Downloading media...")

    def _postprocessor_hook(self, d: dict):
        if d.get("status") == "finished":
            info = d.get("info_dict", {})
            file_path = d.get("filepath") or info.get("filepath") or info.get("_filename")
            if file_path and os.path.exists(file_path):
                size_mb = os.path.getsize(file_path) / (1024 * 1024)
                entry = {
                    "path": os.path.abspath(file_path),
                    "title": info.get("title") or os.path.basename(file_path),
                    "duration": info.get("duration_string") or "N/A",
                    "size": f"{size_mb:.1f} MB",
                    "format": info.get("ext", "").upper()
                }
                add_history_entry(entry)
                self.callbacks.get("on_item_saved", lambda e: None)(entry)
