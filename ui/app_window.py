"""Ana Uygulama Penceresi (AppWindow).

- Tkinter tabanlı özel piksel arayüz
- Asenkron video metadata çekimi ve indirme
- Sekmeli (Video / Ses) format yönetimi
- Boşta ultra düşük RAM tüketimi ve yaşam döngüsü kontrolü
"""

from __future__ import annotations
import gc
import io
import os
import subprocess
import threading
import tkinter as tk
from typing import Callable, Optional
from urllib.request import Request, urlopen

from PIL import Image, ImageTk

from core.codec_selector import (
    SelectionResult,
    select_best_format,
    get_available_resolutions,
)
from core.detector import detect_platform, is_valid_url
from core.downloader import (
    DownloadEngine,
    MediaMetadata,
    ProgressState,
    format_bytes,
)
from ui.dialogs import show_confirm, show_error, show_warning
from ui.strings import (
    APP_NAME,
    APP_SUBTITLE,
    BTN_CANCEL,
    BTN_DOWNLOAD,
    BTN_FETCH,
    BTN_OPEN_FOLDER,
    BTN_PASTE,
    CHECK_FORCE_MP4,
    CONFIRM_EXIT_DOWNLOADING,
    CONFIRM_TITLE,
    ERR_EMPTY_URL,
    ERR_INVALID_URL,
    LABEL_AUDIO_FORMAT,
    LABEL_RESOLUTION,
    MP4_WARNING_OPUS,
    MP4_WARNING_VP9,
    RES_BEST,
    STATUS_CANCELLED,
    STATUS_DOWNLOADING,
    STATUS_FINISHED,
    STATUS_PROCESSING,
    STATUS_READY,
    TAB_AUDIO,
    TAB_VIDEO,
    URL_PLACEHOLDER,
)
from ui.theme import (
    ACCENT_AMBER,
    ACCENT_BLUE,
    ACCENT_CORAL,
    ACCENT_GREEN,
    ACCENT_HOVER,
    BG_PRIMARY,
    BG_SUNKEN,
    BG_SURFACE,
    BORDER_DARK,
    BORDER_MUTED,
    BORDER_THICK,
    BORDER_THIN,
    FONT_BADGE,
    FONT_BODY,
    FONT_BODY_BOLD,
    FONT_CAPTION,
    FONT_META,
    FONT_SECTION,
    FONT_TITLE,
    PAD_GAP,
    PAD_INTERNAL,
    PAD_WINDOW,
    TEXT_MAIN,
    TEXT_MUTED,
    WINDOW_HEIGHT,
    WINDOW_WIDTH,
)
from ui.widgets import (
    PixelBadge,
    PixelButton,
    PixelCard,
    PixelEntry,
    PixelProgressBar,
)


class AppWindow(tk.Tk):
    """ff-dlp-viewer ana masaüstü penceresi."""

    def __init__(
        self,
        engine: DownloadEngine,
        on_minimize_to_tray: Optional[Callable[[], None]] = None,
        on_exit_app: Optional[Callable[[], None]] = None,
    ):
        super().__init__()
        self.engine = engine
        self.on_minimize_to_tray = on_minimize_to_tray
        self.on_exit_app = on_exit_app

        self.title(f"{APP_NAME} - {APP_SUBTITLE}")
        self.geometry(f"{WINDOW_WIDTH}x{WINDOW_HEIGHT}")
        self.minsize(WINDOW_WIDTH, WINDOW_HEIGHT)
        self.configure(bg=BG_PRIMARY)

        # Durum değişkenleri
        self.current_metadata: Optional[MediaMetadata] = None
        self.current_selection: Optional[SelectionResult] = None
        self.is_downloading = False
        self.active_tab = "video"  # "video" veya "audio"
        self.thumb_photo: Optional[ImageTk.PhotoImage] = None
        self.last_downloaded_path = ""

        # Form değişkenleri
        self.var_resolution = tk.StringVar(value=RES_BEST)
        self.var_force_mp4 = tk.BooleanVar(value=False)
        self.var_audio_format = tk.StringVar(value="opus")
        self.var_custom_container = tk.StringVar(value="auto")

        self.protocol("WM_DELETE_WINDOW", self.on_window_close)

        self._build_ui()
        self.status_label.configure(text=STATUS_READY)

    def _build_ui(self):
        """Tüm arayüz elemanlarını 8px ızgara ritminde inşa eder."""
        container = tk.Frame(self, bg=BG_PRIMARY, padx=PAD_WINDOW, pady=PAD_WINDOW)
        container.pack(fill="both", expand=True)

        # 1. Başlık Çubuğu
        header = tk.Frame(container, bg=BG_PRIMARY)
        header.pack(fill="x", pady=(0, PAD_GAP))

        title_lbl = tk.Label(
            header,
            text=APP_NAME,
            font=FONT_TITLE,
            bg=BG_PRIMARY,
            fg=TEXT_MAIN,
        )
        title_lbl.pack(side="left")

        self.platform_badge = PixelBadge(header, text="WEB", bg_color=BG_SUNKEN)
        self.platform_badge.pack(side="right")

        # 2. URL Giriş Çubuğu (Giriş + Yapıştır + Getir)
        url_frame = tk.Frame(container, bg=BG_PRIMARY)
        url_frame.pack(fill="x", pady=(0, PAD_GAP))

        self.url_entry = PixelEntry(url_frame, placeholder=URL_PLACEHOLDER)
        self.url_entry.pack(side="left", fill="x", expand=True, padx=(0, 6))
        self.url_entry.entry.bind("<KeyRelease>", self._on_url_changed)
        self.url_entry.entry.bind("<Return>", lambda e: self.on_fetch_clicked())

        btn_paste = PixelButton(
            url_frame,
            text=BTN_PASTE,
            command=self.on_paste_clicked,
            bg_color=BG_SUNKEN,
            padx=8,
            pady=4,
        )
        btn_paste.pack(side="left", padx=(0, 6))

        self.btn_fetch = PixelButton(
            url_frame,
            text=BTN_FETCH,
            command=self.on_fetch_clicked,
            bg_color=ACCENT_BLUE,
            padx=12,
            pady=4,
        )
        self.btn_fetch.pack(side="left")

        # 3. Medya Bilgi Kartı (Thumbnail + Başlık + Süre + Kodek)
        self.media_card = PixelCard(container, bg=BG_SURFACE, padx=PAD_INTERNAL, pady=PAD_INTERNAL)
        self.media_card.pack(fill="x", pady=(0, PAD_GAP))

        thumb_frame = tk.Frame(self.media_card.inner, bg=BORDER_DARK, padx=1, pady=1)
        thumb_frame.pack(side="left", padx=(0, 10))

        self.thumb_label = tk.Label(
            thumb_frame,
            text="[ Önizleme ]",
            bg=BG_SUNKEN,
            fg=TEXT_MUTED,
            font=FONT_CAPTION,
            width=15,
            height=4,
        )
        self.thumb_label.pack()

        info_text_frame = tk.Frame(self.media_card.inner, bg=BG_SURFACE)
        info_text_frame.pack(side="left", fill="both", expand=True)

        self.lbl_media_title = tk.Label(
            info_text_frame,
            text="Bağlantı bekleniyor...",
            font=FONT_BODY_BOLD,
            bg=BG_SURFACE,
            fg=TEXT_MAIN,
            anchor="w",
            justify="left",
            wraplength=340,
        )
        self.lbl_media_title.pack(fill="x")

        self.lbl_media_meta = tk.Label(
            info_text_frame,
            text="Süre: --:-- | Kaynak: --",
            font=FONT_META,
            bg=BG_SURFACE,
            fg=TEXT_MUTED,
            anchor="w",
        )
        self.lbl_media_meta.pack(fill="x", pady=(2, 0))

        self.lbl_codec_meta = tk.Label(
            info_text_frame,
            text="Seçilen Akış: --",
            font=FONT_META,
            bg=BG_SURFACE,
            fg=ACCENT_BLUE,
            anchor="w",
        )
        self.lbl_codec_meta.pack(fill="x", pady=(2, 0))

        # 4. Sekme Çubuğu (Video İndir / Sadece Ses)
        tab_bar = tk.Frame(container, bg=BG_PRIMARY)
        tab_bar.pack(fill="x", pady=(0, 2))

        self.btn_tab_video = PixelButton(
            tab_bar,
            text=f"[ {TAB_VIDEO} ]",
            command=lambda: self.switch_tab("video"),
            bg_color=BG_PRIMARY,
            padx=14,
            pady=4,
        )
        self.btn_tab_video.pack(side="left", padx=(0, 4))

        self.btn_tab_audio = PixelButton(
            tab_bar,
            text=TAB_AUDIO,
            command=lambda: self.switch_tab("audio"),
            bg_color=BG_SUNKEN,
            padx=14,
            pady=4,
        )
        self.btn_tab_audio.pack(side="left")

        # 5. Sekme İçerik Alanı
        self.tab_content_card = PixelCard(container, bg=BG_SURFACE, padx=PAD_INTERNAL, pady=PAD_INTERNAL)
        self.tab_content_card.pack(fill="both", expand=True, pady=(0, PAD_GAP))

        # 5A. Video Sekme İçeriği
        self.video_tab_frame = tk.Frame(self.tab_content_card.inner, bg=BG_SURFACE)

        res_row = tk.Frame(self.video_tab_frame, bg=BG_SURFACE)
        res_row.pack(fill="x", pady=(0, 6))

        lbl_res = tk.Label(res_row, text=LABEL_RESOLUTION, font=FONT_SECTION, bg=BG_SURFACE, fg=TEXT_MAIN)
        lbl_res.pack(side="left", padx=(0, 8))

        self.res_menu_btn = tk.Menubutton(
            res_row,
            textvariable=self.var_resolution,
            font=FONT_BODY_BOLD,
            bg=BG_SUNKEN,
            fg=TEXT_MAIN,
            relief="solid",
            bd=1,
            padx=8,
            pady=2,
            cursor="hand2",
        )
        self.res_menu = tk.Menu(self.res_menu_btn, tearoff=0)
        self.res_menu_btn["menu"] = self.res_menu
        self.res_menu_btn.pack(side="left")

        mp4_check = tk.Checkbutton(
            self.video_tab_frame,
            text=CHECK_FORCE_MP4,
            variable=self.var_force_mp4,
            command=self._on_options_changed,
            font=FONT_BODY,
            bg=BG_SURFACE,
            fg=TEXT_MAIN,
            activebackground=BG_SURFACE,
            cursor="hand2",
        )
        mp4_check.pack(anchor="w", pady=(4, 6))

        self.warning_box = tk.Frame(self.video_tab_frame, bg=ACCENT_AMBER, padx=8, pady=6)
        self.lbl_warning = tk.Label(
            self.warning_box,
            text="",
            font=FONT_CAPTION,
            bg=ACCENT_AMBER,
            fg=TEXT_MAIN,
            wraplength=460,
            justify="left",
        )
        self.lbl_warning.pack(fill="x")

        # 5B. Ses Sekme İçeriği
        self.audio_tab_frame = tk.Frame(self.tab_content_card.inner, bg=BG_SURFACE)

        lbl_audio = tk.Label(
            self.audio_tab_frame,
            text=LABEL_AUDIO_FORMAT,
            font=FONT_SECTION,
            bg=BG_SURFACE,
            fg=TEXT_MAIN,
        )
        lbl_audio.pack(anchor="w", pady=(0, 6))

        for fmt_key, fmt_label in [
            ("opus", "Opus (En Yüksek / Orijinal)"),
            ("aac", "AAC (m4a / Geniş Uyum)"),
            ("mp3", "MP3 (Evrensel Uyum)"),
        ]:
            rb = tk.Radiobutton(
                self.audio_tab_frame,
                text=fmt_label,
                value=fmt_key,
                variable=self.var_audio_format,
                command=self._on_options_changed,
                font=FONT_BODY,
                bg=BG_SURFACE,
                fg=TEXT_MAIN,
                activebackground=BG_SURFACE,
                cursor="hand2",
            )
            rb.pack(anchor="w", pady=2)

        # Başlangıçta video sekmesini göster
        self.video_tab_frame.pack(fill="both", expand=True)

        # 6. İlerleme ve İndirme Kontrolleri
        bottom_frame = tk.Frame(container, bg=BG_PRIMARY)
        bottom_frame.pack(fill="x")

        self.progress_bar = PixelProgressBar(bottom_frame, height=18)
        self.progress_bar.pack(fill="x", pady=(0, 6))

        # İlerleme Bilgi Satırı
        status_row = tk.Frame(bottom_frame, bg=BG_PRIMARY)
        status_row.pack(fill="x", pady=(0, 8))

        self.status_label = tk.Label(
            status_row,
            text=STATUS_READY,
            font=FONT_BODY,
            bg=BG_PRIMARY,
            fg=TEXT_MAIN,
            anchor="w",
        )
        self.status_label.pack(side="left", fill="x", expand=True)

        self.progress_meta_label = tk.Label(
            status_row,
            text="",
            font=FONT_META,
            bg=BG_PRIMARY,
            fg=TEXT_MUTED,
            anchor="e",
        )
        self.progress_meta_label.pack(side="right")

        # Butonlar Satırı
        action_row = tk.Frame(bottom_frame, bg=BG_PRIMARY)
        action_row.pack(fill="x")

        self.btn_download = PixelButton(
            action_row,
            text=BTN_DOWNLOAD,
            command=self.on_download_clicked,
            bg_color=ACCENT_GREEN,
            padx=20,
            pady=8,
        )
        self.btn_download.pack(side="left", padx=(0, 8))

        self.btn_cancel = PixelButton(
            action_row,
            text=BTN_CANCEL,
            command=self.on_cancel_clicked,
            bg_color=ACCENT_CORAL,
            padx=14,
            pady=8,
        )
        self.btn_cancel.pack(side="left", padx=(0, 8))
        self.btn_cancel.set_enabled(False)

        self.btn_open_folder = PixelButton(
            action_row,
            text=BTN_OPEN_FOLDER,
            command=self.on_open_folder_clicked,
            bg_color=BG_SUNKEN,
            padx=12,
            pady=8,
        )
        self.btn_open_folder.pack(side="right")
        self.btn_open_folder.set_enabled(False)

    def switch_tab(self, tab: str):
        """Video ve Ses sekmeleri arasında geçiş yapar."""
        self.active_tab = tab
        if tab == "video":
            self.audio_tab_frame.pack_forget()
            self.video_tab_frame.pack(fill="both", expand=True)
            self.btn_tab_video.inner_frame.configure(bg=BG_PRIMARY)
            self.btn_tab_video.label.configure(text=f"[ {TAB_VIDEO} ]")
            self.btn_tab_audio.inner_frame.configure(bg=BG_SUNKEN)
            self.btn_tab_audio.label.configure(text=TAB_AUDIO)
        else:
            self.video_tab_frame.pack_forget()
            self.audio_tab_frame.pack(fill="both", expand=True)
            self.btn_tab_audio.inner_frame.configure(bg=BG_PRIMARY)
            self.btn_tab_audio.label.configure(text=f"[ {TAB_AUDIO} ]")
            self.btn_tab_video.inner_frame.configure(bg=BG_SUNKEN)
            self.btn_tab_video.label.configure(text=TAB_VIDEO)

        self._update_selection()

    def _on_url_changed(self, event=None):
        url = self.url_entry.get()
        platform = detect_platform(url)
        self.platform_badge.set_text(platform.badge_label)

    def on_paste_clicked(self):
        try:
            clipboard = self.clipboard_get().strip()
            if clipboard:
                self.url_entry.set(clipboard)
                self._on_url_changed()
                self.on_fetch_clicked()
        except Exception:
            pass

    def on_fetch_clicked(self):
        url = self.url_entry.get()
        if not url:
            show_warning(self, "Uyarı", ERR_EMPTY_URL)
            return
        if not is_valid_url(url):
            show_warning(self, "Uyarı", ERR_INVALID_URL)
            return

        self.status_label.configure(text="Medya bilgileri alınıyor...")
        self.btn_fetch.set_enabled(False)
        self.btn_download.set_enabled(False)

        def _success(meta: MediaMetadata):
            self.after(0, lambda: self._apply_fetched_metadata(meta))

        def _error(err: str):
            self.after(0, lambda: self._handle_fetch_error(err))

        self.engine.fetch_info_async(url, on_success=_success, on_error=_error)

    def _apply_fetched_metadata(self, meta: MediaMetadata):
        self.current_metadata = meta
        self.btn_fetch.set_enabled(True)
        self.btn_download.set_enabled(True)
        self.status_label.configure(text=STATUS_READY)

        self.lbl_media_title.configure(text=meta.title)
        self.lbl_media_meta.configure(text=f"Süre: {meta.duration_str} | Platform: {meta.platform_name}")
        self.platform_badge.set_text(detect_platform(self.url_entry.get()).badge_label)

        # Çözünürlük menüsünü doldur
        self.res_menu.delete(0, "end")
        self.res_menu.add_radiobutton(
            label=RES_BEST,
            variable=self.var_resolution,
            value=RES_BEST,
            command=self._on_options_changed,
        )
        for res in meta.available_resolutions:
            label = f"{res}p"
            self.res_menu.add_radiobutton(
                label=label,
                variable=self.var_resolution,
                value=str(res),
                command=self._on_options_changed,
            )
        self.var_resolution.set(RES_BEST)

        # Thumbnail yükle
        if meta.thumbnail_url:
            threading.Thread(target=self._load_thumbnail_async, args=(meta.thumbnail_url,), daemon=True).start()

        self._update_selection()

    def _load_thumbnail_async(self, thumb_url: str):
        try:
            req = Request(thumb_url, headers={"User-Agent": "Mozilla/5.0"})
            with urlopen(req, timeout=5) as resp:
                data = resp.read()
            img = Image.open(io.BytesIO(data))
            img.thumbnail((120, 68), Image.Resampling.LANCZOS)
            photo = ImageTk.PhotoImage(img)

            def _update():
                self.thumb_photo = photo
                self.thumb_label.configure(image=photo, text="", width=120, height=68)

            self.after(0, _update)
        except Exception:
            pass

    def _handle_fetch_error(self, err: str):
        self.btn_fetch.set_enabled(True)
        self.btn_download.set_enabled(False)
        self.status_label.configure(text="Bilgi alma başarısız.")
        show_error(self, "Hata", err)

    def _on_options_changed(self):
        self._update_selection()

    def _update_selection(self):
        if not self.current_metadata or not self.current_metadata.formats:
            return

        target_res = "best"
        res_val = self.var_resolution.get()
        if res_val != RES_BEST:
            try:
                target_res = int(res_val.replace("p", ""))
            except ValueError:
                target_res = "best"

        force_mp4 = self.var_force_mp4.get()
        audio_choice = self.var_audio_format.get()

        selection = select_best_format(
            self.current_metadata.formats,
            mode=self.active_tab,
            target_res=target_res,
            force_mp4=force_mp4,
            audio_format_choice=audio_choice,
        )
        self.current_selection = selection

        # Arayüz bilgilendirmeleri
        reencode_tag = " [Dönüştürülecek]" if selection.needs_reencode else " [Stream Copy]"
        self.lbl_codec_meta.configure(
            text=f"Seçim: {selection.video_codec.upper()}+{selection.audio_codec.upper()} -> .{selection.expected_container}{reencode_tag}"
        )

        # Uyarı kutusu
        if selection.warning and self.active_tab == "video":
            self.lbl_warning.configure(text=selection.warning)
            self.warning_box.pack(fill="x", pady=(4, 0))
        else:
            self.warning_box.pack_forget()

    def on_download_clicked(self):
        if not self.current_metadata or not self.current_selection:
            show_warning(self, "Uyarı", "Lütfen önce geçerli bir bağlantı bilgisi getirin.")
            return

        url = self.url_entry.get()
        self.is_downloading = True
        self.btn_download.set_enabled(False)
        self.btn_cancel.set_enabled(True)
        self.btn_fetch.set_enabled(False)
        self.btn_open_folder.set_enabled(False)
        self.progress_bar.set_percent(0)
        self.status_label.configure(text=STATUS_DOWNLOADING)

        def _progress(state: ProgressState):
            self.after(0, lambda: self._on_progress_update(state))

        def _finished(filepath: str):
            self.after(0, lambda: self._on_download_finished(filepath))

        def _error(err: str):
            self.after(0, lambda: self._on_download_error(err))

        self.engine.download_async(
            url=url,
            selection=self.current_selection,
            on_progress=_progress,
            on_finished=_finished,
            on_error=_error,
        )

    def _on_progress_update(self, state: ProgressState):
        if state.status == "downloading":
            self.progress_bar.set_percent(state.percent)
            spd = format_bytes(state.speed_bytes_sec) + "/s"
            downloaded = format_bytes(state.downloaded_bytes)
            total = format_bytes(state.total_bytes) if state.total_bytes > 0 else "??"
            eta_str = f"{state.eta_seconds} sn" if state.eta_seconds > 0 else "--"

            self.status_label.configure(text=f"%{state.percent:.1f} | {spd} | Kalan: {eta_str}")
            self.progress_meta_label.configure(text=f"{downloaded} / {total}")
        elif state.status == "processing":
            self.progress_bar.set_percent(100)
            self.status_label.configure(text=STATUS_PROCESSING)
            self.progress_meta_label.configure(text="")
        elif state.status == "cancelled":
            self.is_downloading = False
            self.btn_download.set_enabled(True)
            self.btn_cancel.set_enabled(False)
            self.btn_fetch.set_enabled(True)
            self.status_label.configure(text=STATUS_CANCELLED)
            self.progress_meta_label.configure(text="")

    def _on_download_finished(self, filepath: str):
        self.is_downloading = False
        self.last_downloaded_path = filepath
        self.btn_download.set_enabled(True)
        self.btn_cancel.set_enabled(False)
        self.btn_fetch.set_enabled(True)
        self.btn_open_folder.set_enabled(True)
        self.progress_bar.set_percent(100)
        self.status_label.configure(text=STATUS_FINISHED)
        self.progress_meta_label.configure(text="Tamamlandı")

    def _on_download_error(self, err: str):
        self.is_downloading = False
        self.btn_download.set_enabled(True)
        self.btn_cancel.set_enabled(False)
        self.btn_fetch.set_enabled(True)
        self.status_label.configure(text="Hata oluştu.")
        show_error(self, "İndirme Hatası", err)

    def on_cancel_clicked(self):
        if self.is_downloading:
            self.engine.cancel_download()
            self.btn_cancel.set_enabled(False)
            self.status_label.configure(text="İptal ediliyor...")

    def on_open_folder_clicked(self):
        folder = self.engine.download_dir
        if self.last_downloaded_path and os.path.exists(self.last_downloaded_path):
            subprocess.run(["explorer", f"/select,{self.last_downloaded_path}"])
        elif os.path.exists(folder):
            subprocess.run(["explorer", folder])

    def on_window_close(self):
        """Pencere kapatıldığında doğrudan tray'e gizlenir ve bellek optimize edilir."""
        if self.on_minimize_to_tray:
            self.on_minimize_to_tray()
        else:
            self.hide_to_tray()

    def hide_to_tray(self):
        """Pencereyi gizler ve thumbnail nesnesini bellekten boşaltır."""
        self.withdraw()
        # Bellek boşaltma (Garbage Collection)
        self.thumb_photo = None
        self.thumb_label.configure(image="", text="[ Tray Modu ]")
        gc.collect()

    def restore_from_tray(self):
        """Pencereyi tray'den geri getirir."""
        self.deiconify()
        self.lift()
        self.focus_force()

    def confirm_and_exit(self):
        """Uygulamadan çıkmadan önce devam eden indirme varsa onay ister."""
        if self.is_downloading:
            show_confirm(
                self,
                title=CONFIRM_TITLE,
                message=CONFIRM_EXIT_DOWNLOADING,
                on_confirm=self._force_exit,
            )
        else:
            self._force_exit()

    def _force_exit(self):
        if self.is_downloading:
            self.engine.cancel_download()
        if self.on_exit_app:
            self.on_exit_app()
        else:
            self.destroy()
