import os
import json
import logging
import uuid
import urllib.parse
from http.server import HTTPServer, BaseHTTPRequestHandler
import threading
from typing import Callable, Optional
import yt_dlp
from engine import TranscoderManager

HOST = "127.0.0.1"
PORT = 48123

# In-memory tracking of active jobs
# job_id -> { "status": "downloading"|"complete"|"error"|"cancelled", "percent": 0.0, "speed": "", "eta": "", "total": "", "filepath": "", "filename": "", "error": "" }
active_jobs = {}


class BridgeRequestHandler(BaseHTTPRequestHandler):
    download_callback: Optional[Callable[[dict], None]] = None

    def _set_cors_headers(self, status_code=200, content_type="application/json"):
        self.send_response(status_code)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Access-Control-Allow-Private-Network", "true")
        self.send_header("Content-Type", content_type)

    def do_OPTIONS(self):
        self._set_cors_headers(200)
        self.end_headers()

    def do_HEAD(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        query = urllib.parse.parse_qs(parsed.query)

        if path == "/serve":
            job_id = query.get("id", [""])[0]
            job = active_jobs.get(job_id)
            if not job or not job.get("filepath") or not os.path.exists(job["filepath"]):
                self._set_cors_headers(404)
                self.end_headers()
                return

            filepath = job["filepath"]
            filename = job.get("filename") or os.path.basename(filepath)
            filesize = os.path.getsize(filepath)

            ext = os.path.splitext(filename)[1].lower()
            ct = "video/mp4" if ext in [".mp4", ".m4v"] else "application/octet-stream"
            self.send_response(200)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Private-Network", "true")
            self.send_header("Content-Type", ct)
            safe_fname = urllib.parse.quote(filename)
            self.send_header("Content-Disposition", f"attachment; filename=\"{filename}\"; filename*=UTF-8''{safe_fname}")
            self.send_header("Content-Length", str(filesize))
            self.end_headers()
        else:
            self._set_cors_headers(200)
            self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        query = urllib.parse.parse_qs(parsed.query)

        if path == "/status":
            self._set_cors_headers(200)
            self.end_headers()
            payload = {
                "status": "ok",
                "app": "yt-mini v5",
                "version": "5.0",
                "message": "Companion bridge is active"
            }
            self.wfile.write(json.dumps(payload).encode("utf-8"))

        elif path == "/job_status":
            job_id = query.get("id", [""])[0]
            job = active_jobs.get(job_id)
            if not job:
                self._set_cors_headers(404)
                self.end_headers()
                self.wfile.write(b'{"error": "Job not found"}')
                return

            self._set_cors_headers(200)
            self.end_headers()
            self.wfile.write(json.dumps(job).encode("utf-8"))

        elif path == "/serve":
            job_id = query.get("id", [""])[0]
            job = active_jobs.get(job_id)
            if not job or not job.get("filepath") or not os.path.exists(job["filepath"]):
                self._set_cors_headers(404)
                self.end_headers()
                self.wfile.write(b'{"error": "File not found or still processing"}')
                return

            filepath = job["filepath"]
            filename = job.get("filename") or os.path.basename(filepath)
            filesize = os.path.getsize(filepath)

            # Determine content type
            ext = os.path.splitext(filename)[1].lower()
            ct = "application/octet-stream"
            if ext in [".mp4", ".m4v"]: ct = "video/mp4"
            elif ext in [".mkv"]: ct = "video/x-matroska"
            elif ext in [".webm"]: ct = "video/webm"
            elif ext in [".mp3"]: ct = "audio/mpeg"
            elif ext in [".opus"]: ct = "audio/opus"
            elif ext in [".flac"]: ct = "audio/flac"

            self.send_response(200)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Private-Network", "true")
            self.send_header("Content-Type", ct)
            # URL-encode filename for Content-Disposition header
            safe_fname = urllib.parse.quote(filename)
            self.send_header("Content-Disposition", f"attachment; filename=\"{filename}\"; filename*=UTF-8''{safe_fname}")
            self.send_header("Content-Length", str(filesize))
            self.end_headers()

            # Stream the file locally
            try:
                with open(filepath, "rb") as f:
                    while chunk := f.read(64 * 1024):
                        self.wfile.write(chunk)
            except Exception as e:
                logging.error(f"Error serving file: {e}")

        else:
            self._set_cors_headers(404)
            self.end_headers()
            self.wfile.write(b'{"error": "Not Found"}')

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode("utf-8") if content_length > 0 else "{}"

        try:
            data = json.loads(body)
        except Exception:
            data = {}

        if self.path == "/inspect":
            url = data.get("url", "").strip()
            if not url:
                self._set_cors_headers(400)
                self.end_headers()
                self.wfile.write(b'{"downloadable": false, "error": "No URL provided"}')
                return

            info = self._quick_inspect_url(url)
            self._set_cors_headers(200)
            self.end_headers()
            self.wfile.write(json.dumps(info).encode("utf-8"))

        elif self.path == "/download":
            url = data.get("url", "").strip()
            if not url:
                self._set_cors_headers(400)
                self.end_headers()
                self.wfile.write(b'{"status": "error", "message": "No URL provided"}')
                return

            job_id = str(uuid.uuid4())[:8]
            active_jobs[job_id] = {
                "status": "initializing",
                "percent": 0.0,
                "speed": "",
                "eta": "",
                "total": "",
                "filepath": "",
                "filename": "",
                "error": ""
            }

            # Start download in background thread
            threading.Thread(target=self._run_job_download, args=(job_id, data), daemon=True).start()

            # Also notify desktop UI if callback is attached
            if BridgeRequestHandler.download_callback:
                BridgeRequestHandler.download_callback(data)

            self._set_cors_headers(200)
            self.end_headers()
            self.wfile.write(json.dumps({
                "status": "started",
                "job_id": job_id,
                "message": "Download started"
            }).encode("utf-8"))

        else:
            self._set_cors_headers(404)
            self.end_headers()
            self.wfile.write(b'{"error": "Endpoint not found"}')

    def _quick_inspect_url(self, url: str) -> dict:
        """Inspect URL without downloading any video stream bytes."""
        if not (url.startswith("http://") or url.startswith("https://")):
            url = "https://" + url

        ydl_opts = {
            "extract_flat": "in_playlist",
            "skip_download": True,
            "quiet": True,
            "no_warnings": True
        }

        try:
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(url, download=False)
                if not info:
                    return {"downloadable": False}

                is_pl = (info.get("_type") == "playlist" or "entries" in info)
                entries = list(info.get("entries", [])) if is_pl else []
                count = len(entries) if is_pl else 1

                title = info.get("title") or "Unknown Media"
                uploader = info.get("uploader") or info.get("channel") or "Unknown Creator"
                duration = info.get("duration_string") or "N/A"
                thumb = info.get("thumbnail") or ""

                if is_pl and not thumb and entries:
                    first = entries[0]
                    if isinstance(first, dict):
                        thumb = first.get("thumbnail") or ""

                return {
                    "downloadable": True,
                    "url": url,
                    "title": title,
                    "uploader": uploader,
                    "duration": duration,
                    "thumbnail_url": thumb,
                    "is_playlist": is_pl,
                    "playlist_count": count
                }
        except Exception as e:
            logging.error(f"Bridge inspection failed: {e}")
            return {"downloadable": False, "error": str(e)}

    def _run_job_download(self, job_id: str, data: dict):
        url = data["url"]
        mode = data.get("mode", "video")
        fmt_choice = data.get("format", "MP4 (H.264)").upper()
        quality = data.get("quality", "1080p")
        is_playlist = data.get("is_playlist", False)

        # Temporary staging folder for browser download serving
        temp_dir = os.path.join(os.environ.get("TEMP", os.getcwd()), "yt_mini_downloads")
        os.makedirs(temp_dir, exist_ok=True)

        out_tmpl = "%(playlist_title)s/%(playlist_index)02d - %(title)s.%(ext)s" if is_playlist else "%(title)s.%(ext)s"

        def progress_hook(d):
            if d.get("status") == "downloading":
                active_jobs[job_id]["status"] = "downloading"
                active_jobs[job_id]["percent"] = float(d.get("_percent") or 0.0)
                active_jobs[job_id]["speed"] = d.get("_speed_str") or ""
                active_jobs[job_id]["eta"] = d.get("_eta_str") or ""
                active_jobs[job_id]["total"] = d.get("_total_bytes_str") or d.get("_total_bytes_estimate_str") or ""

        def postprocessor_hook(d):
            if d.get("status") == "finished":
                info = d.get("info_dict", {})
                fp = d.get("filepath") or info.get("filepath") or info.get("_filename")
                if fp and os.path.exists(fp):
                    active_jobs[job_id]["filepath"] = os.path.abspath(fp)
                    active_jobs[job_id]["filename"] = os.path.basename(fp)

        ydl_opts = {
            "outtmpl": os.path.join(temp_dir, out_tmpl),
            "noplaylist": not is_playlist,
            "quiet": True,
            "no_warnings": True,
            "ignoreerrors": True,
            "progress_hooks": [progress_hook],
            "postprocessor_hooks": [postprocessor_hook]
        }

        ff = TranscoderManager.get_ffmpeg_path()
        if ff:
            ydl_opts["ffmpeg_location"] = ff

        if mode == "video":
            if "MKV" in fmt_choice:
                ydl_opts["merge_output_format"] = "mkv"
                ydl_opts["format"] = "bv*[height<=1080]+ba/b[height<=1080]"
            else:
                ydl_opts["merge_output_format"] = "mp4"
                ydl_opts["format"] = "bv*[ext=mp4][height<=1080]+ba[ext=m4a]/b[ext=mp4][height<=1080]/bv*[height<=1080]+ba/b[height<=1080]"
                ydl_opts["format_sort"] = ["vcodec:h264,res,acodec:m4a"]
        else:
            a_fmt = "mp3"
            for cand in ["opus", "aac", "m4a", "flac", "wav", "mp3"]:
                if cand in fmt_choice.lower():
                    a_fmt = cand
                    break

            ydl_opts["format"] = "bestaudio/best"
            ydl_opts["postprocessors"] = [{
                "key": "FFmpegExtractAudio",
                "preferredcodec": a_fmt,
                "preferredquality": "320k"
            }, {
                "key": "FFmpegMetadata",
                "add_metadata": True
            }, {
                "key": "EmbedThumbnail",
                "already_have_thumbnail": False
            }]
            ydl_opts["writethumbnail"] = True

        try:
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                ydl.download([url])

            active_jobs[job_id]["status"] = "complete"
            active_jobs[job_id]["percent"] = 100.0
            logging.info(f"Job {job_id} complete! File: {active_jobs[job_id].get('filepath')}")
        except Exception as e:
            active_jobs[job_id]["status"] = "error"
            active_jobs[job_id]["error"] = str(e)
            logging.error(f"Job {job_id} error: {e}")

    def log_message(self, format, *args):
        pass


class BridgeServer:
    def __init__(self, on_download: Optional[Callable[[dict], None]] = None):
        BridgeRequestHandler.download_callback = on_download
        self.server = None
        self._thread = None

    def start(self):
        try:
            self.server = HTTPServer((HOST, PORT), BridgeRequestHandler)
            self._thread = threading.Thread(target=self.server.serve_forever, daemon=True)
            self._thread.start()
            logging.info(f"Bridge server running on http://{HOST}:{PORT}")
            return True
        except Exception as e:
            logging.error(f"Failed to start bridge: {e}")
            return False

    def stop(self):
        if self.server:
            self.server.shutdown()
            self.server.server_close()
            self.server = None
