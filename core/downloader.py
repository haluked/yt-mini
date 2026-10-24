"""yt-dlp Python kütüphane API'sini güvenli şekilde kullanan indirme motoru.

- Subprocess DEĞİL, doğrudan yt-dlp Python API'si kullanılır.
- UI iş parçacığını asla bloke etmez; arka plan thread'lerinde çalışır.
- İptal mekanizması threading.Event ile yönetilir.
- ffmpeg uygulamanın kendi bin/ klasöründen veya sistemden çözümlenir.
- Hatalar anlaşılır Türkçe mesajlara dönüştürülür.
"""

from __future__ import annotations
import os
import shutil
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Dict, Optional

from core.detector import detect_platform, is_valid_url
from core.codec_selector import SelectionResult, select_best_format, get_available_resolutions
from core.sanitization import sanitize_filename


@dataclass
class ProgressState:
    status: str             # "fetching", "downloading", "processing", "finished", "cancelled", "error"
    percent: float          # 0.0 - 100.0
    downloaded_bytes: int   # İndirilen byte
    total_bytes: int        # Toplam byte (tahmini veya kesin)
    speed_bytes_sec: float  # Byte/saniye hız
    eta_seconds: int        # Kalan saniye
    filename: str           # İndirilen dosya adı
    error_message: Optional[str] = None


@dataclass
class MediaMetadata:
    title: str
    duration: int
    duration_str: str
    thumbnail_url: str
    platform_key: str
    platform_name: str
    available_resolutions: list[int]
    formats: list[dict[str, Any]]
    raw_info: dict[str, Any]


def get_ffmpeg_path() -> Optional[str]:
    """ffmpeg çalıştırılabilir dosyasının konumunu belirler.

    Öncelik:
    1. PyInstaller _MEIPASS / bin / ffmpeg.exe
    2. Uygulama çalışma dizini / bin / ffmpeg.exe
    3. Sistem PATH'indeki ffmpeg
    """
    candidates = []

    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        meipass_bin = Path(sys._MEIPASS) / "bin" / "ffmpeg.exe"
        candidates.append(meipass_bin)

    app_dir = Path(__file__).resolve().parent.parent
    candidates.append(app_dir / "bin" / "ffmpeg.exe")
    candidates.append(app_dir / "ffmpeg.exe")

    for c in candidates:
        if c.is_file():
            return str(c.parent)

    # Sistem PATH kontrolü
    sys_ffmpeg = shutil.which("ffmpeg")
    if sys_ffmpeg:
        return str(Path(sys_ffmpeg).parent)

    return None


def format_duration(seconds: Optional[int]) -> str:
    """Saniyeyi SS:DD veya DD:SS formatına dönüştürür."""
    if not seconds or seconds < 0:
        return "Bilinmiyor"
    m, s = divmod(int(seconds), 60)
    h, m = divmod(m, 60)
    if h > 0:
        return f"{h}:{m:02d}:{s:02d}"
    return f"{m:02d}:{s:02d}"


def format_bytes(num_bytes: Optional[int | float]) -> str:
    """Byte sayısını okunabilir MB/GB metnine çevirir."""
    if not num_bytes or num_bytes <= 0:
        return "0.0 MB"
    mb = num_bytes / (1024 * 1024)
    if mb >= 1024:
        return f"{mb / 1024:.2f} GB"
    return f"{mb:.1f} MB"


class DownloadCancelledException(Exception):
    """Kullanıcı indirmeyi iptal ettiğinde fırlatılır."""
    pass


def _get_ydl_class():
    """yt-dlp kütüphanesini yalnızca ihtiyaç duyulduğunda lazy import eder."""
    import yt_dlp
    return yt_dlp.YoutubeDL


class DownloadEngine:
    """yt-dlp indirme ve bilgi çekme motoru."""

    def __init__(self, download_dir: Optional[str] = None):
        self.download_dir = download_dir or str(Path.home() / "Downloads")
        self.ffmpeg_dir = get_ffmpeg_path()
        self._active_thread: Optional[threading.Thread] = None
        self._cancel_event = threading.Event()

    def fetch_info_async(
        self,
        url: str,
        on_success: Callable[[MediaMetadata], None],
        on_error: Callable[[str], None],
    ) -> threading.Thread:
        """URL'nin video meta verilerini arka planda çeker."""
        def _worker():
            try:
                meta = self.fetch_info(url)
                on_success(meta)
            except Exception as e:
                on_error(self._translate_error(e))

        thread = threading.Thread(target=_worker, daemon=True)
        thread.start()
        return thread

    def fetch_info(self, url: str) -> MediaMetadata:
        """URL'nin video meta verilerini senkron çeker."""
        if not is_valid_url(url):
            raise ValueError("Geçersiz URL formatı.")

        platform = detect_platform(url)

        ydl_opts: Dict[str, Any] = {
            "extract_flat": False,
            "skip_download": True,
            "quiet": True,
            "no_warnings": True,
        }
        if self.ffmpeg_dir:
            ydl_opts["ffmpeg_location"] = self.ffmpeg_dir

        YoutubeDL = _get_ydl_class()
        with YoutubeDL(ydl_opts) as ydl:
            info = ydl.extract_info(url, download=False)
            if not info:
                raise ValueError("Medya bilgisi alınamadı.")

        title = sanitize_filename(info.get("title") or "medya", default_name="medya")
        duration = info.get("duration") or 0
        thumbnail = info.get("thumbnail") or ""
        formats = info.get("formats") or []
        available_res = get_available_resolutions(formats)

        return MediaMetadata(
            title=title,
            duration=duration,
            duration_str=format_duration(duration),
            thumbnail_url=thumbnail,
            platform_key=platform.key,
            platform_name=platform.display_name,
            available_resolutions=available_res,
            formats=formats,
            raw_info=info,
        )

    def download_async(
        self,
        url: str,
        selection: SelectionResult,
        output_dir: Optional[str] = None,
        on_progress: Optional[Callable[[ProgressState], None]] = None,
        on_finished: Optional[Callable[[str], None]] = None,
        on_error: Optional[Callable[[str], None]] = None,
    ) -> threading.Event:
        """Medya indirme işlemini ayrı bir thread'de başlatır ve iptal Event'i döner."""
        self._cancel_event.clear()
        target_dir = output_dir or self.download_dir

        def _worker():
            try:
                final_path = self.download(
                    url=url,
                    selection=selection,
                    output_dir=target_dir,
                    on_progress=on_progress,
                    cancel_event=self._cancel_event,
                )
                if on_finished:
                    on_finished(final_path)
            except DownloadCancelledException:
                if on_progress:
                    on_progress(
                        ProgressState(
                            status="cancelled",
                            percent=0.0,
                            downloaded_bytes=0,
                            total_bytes=0,
                            speed_bytes_sec=0.0,
                            eta_seconds=0,
                            filename="",
                            error_message="İndirme kullanıcı tarafından iptal edildi.",
                        )
                    )
            except Exception as e:
                err_msg = self._translate_error(e)
                if on_error:
                    on_error(err_msg)

        thread = threading.Thread(target=_worker, daemon=True)
        self._active_thread = thread
        thread.start()
        return self._cancel_event

    def cancel_download(self):
        """Aktif indirmeyi iptal eder."""
        self._cancel_event.set()

    def download(
        self,
        url: str,
        selection: SelectionResult,
        output_dir: str,
        on_progress: Optional[Callable[[ProgressState], None]] = None,
        cancel_event: Optional[threading.Event] = None,
    ) -> str:
        """yt-dlp API ile indirme işlemini yürütür."""
        os.makedirs(output_dir, exist_ok=True)
        cancel_evt = cancel_event or self._cancel_event

        outtmpl = os.path.join(output_dir, "%(title).180B.%(ext)s")
        downloaded_file = [""]

        def hook(d: Dict[str, Any]):
            if cancel_evt.is_set():
                raise DownloadCancelledException("İndirme iptal edildi.")

            status = d.get("status")
            if status == "downloading":
                total = d.get("total_bytes") or d.get("total_bytes_estimate") or 0
                downloaded = d.get("downloaded_bytes") or 0
                speed = d.get("speed") or 0.0
                eta = d.get("eta") or 0
                percent = (downloaded / total * 100.0) if total > 0 else 0.0
                filename = os.path.basename(d.get("filename") or "")

                if on_progress:
                    on_progress(
                        ProgressState(
                            status="downloading",
                            percent=min(100.0, max(0.0, percent)),
                            downloaded_bytes=downloaded,
                            total_bytes=total,
                            speed_bytes_sec=speed,
                            eta_seconds=int(eta) if eta else 0,
                            filename=filename,
                        )
                    )
            elif status == "finished":
                filename = d.get("filename") or ""
                downloaded_file[0] = filename
                if on_progress:
                    on_progress(
                        ProgressState(
                            status="processing",
                            percent=100.0,
                            downloaded_bytes=0,
                            total_bytes=0,
                            speed_bytes_sec=0.0,
                            eta_seconds=0,
                            filename=os.path.basename(filename),
                        )
                    )

        ydl_opts: Dict[str, Any] = {
            "format": selection.format_id,
            "outtmpl": outtmpl,
            "progress_hooks": [hook],
            "quiet": True,
            "no_warnings": True,
            "windowsfilenames": True,
            "restrictfilenames": False,
        }

        if self.ffmpeg_dir:
            ydl_opts["ffmpeg_location"] = self.ffmpeg_dir

        # Konteyner remux / post-processor ayarları
        if selection.resolution != "Audio Only":
            if selection.expected_container:
                ydl_opts["merge_output_format"] = selection.expected_container
        else:
            # Sadece ses modu
            postprocessors = []
            if selection.expected_container == "mp3":
                postprocessors.append({
                    "key": "FFmpegExtractAudio",
                    "preferredcodec": "mp3",
                    "preferredquality": "0",  # VBR best
                })
            elif selection.expected_container == "opus":
                postprocessors.append({
                    "key": "FFmpegExtractAudio",
                    "preferredcodec": "opus",
                })
            elif selection.expected_container in ("m4a", "aac"):
                postprocessors.append({
                    "key": "FFmpegExtractAudio",
                    "preferredcodec": "m4a",
                })
            if postprocessors:
                ydl_opts["postprocessors"] = postprocessors

        YoutubeDL = _get_ydl_class()
        with YoutubeDL(ydl_opts) as ydl:
            ydl.download([url])

        return downloaded_file[0]

    def _translate_error(self, exc: Exception) -> str:
        """Teknik hataları anlaşılır Türkçe mesajlara dönüştürür."""
        if isinstance(exc, DownloadCancelledException):
            return "İndirme kullanıcı tarafından iptal edildi."

        err_str = str(exc)
        err_lower = err_str.lower()

        if "private video" in err_lower or "login required" in err_lower:
            return "Bu video gizli veya giriş gerektiriyor."
        if "video unavailable" in err_lower or "has been removed" in err_lower or "deleted" in err_lower:
            return "Bu video yayından kaldırılmış veya mevcut değil."
        if "unable to download webpage" in err_lower or "connection refused" in err_lower or "nodename nor servname" in err_lower or "network is unreachable" in err_lower:
            return "İnternet bağlantısı kurulamadı. Lütfen ağınızı kontrol edin."
        if "ffmpeg is not installed" in err_lower or "ffmpeg not found" in err_lower:
            return "Gerekli medya işleme aracı (ffmpeg) bulunamadı. Lütfen kurulumu kontrol edin."
        if "requested format is not available" in err_lower:
            return "İstenen format bu video için mevcut değil. Lütfen farklı bir çözünürlük deneyin."

        return f"Bir hata oluştu: {err_str[:120]}"
