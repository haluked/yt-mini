"""Ana Uygulama Penceresi (AppWindow) - Kırtasiye defteri düzeni.

Yapı:
1. Başlık: piksel logo + uygulama adı + platform rozeti
2. Bağlantı: URL kutusu, "Yapıştır" ve "Bilgi Getir"
3. Medya: önizleme, başlık, teknik veri, Video/Ses sekmeleri, seçenekler, not şeridi
4. İndirme: ilerleme çubuğu, durum satırı, eylem butonları

Tasarım notları:
- Sol tarafta defterin cilt kenarı: zımba delikleri + kırmızı kenar çizgisi.
- Seçili/aktif her şey fosforlu kalem sarısı (HILITE) ile işaretlenir.
- Medya bölümünün yüksekliği sabittir; içerik değişince pencere zıplamaz.
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
    AUDIO_CHOICES,
    BTN_CANCEL,
    BTN_DOWNLOAD,
    BTN_FETCH,
    BTN_FETCH_BUSY,
    BTN_OPEN_FOLDER,
    BTN_PASTE,
    CHECK_FORCE_MP4,
    CONFIRM_EXIT_DOWNLOADING,
    CONFIRM_TITLE,
    EMPTY_STEPS,
    EMPTY_TITLE,
    ERR_EMPTY_URL,
    ERR_INVALID_URL,
    HINT_SUPPORTED,
    LABEL_AUDIO_FORMAT,
    LABEL_RESOLUTION,
    NOTE_AUDIO_COPY,
    NOTE_AUDIO_ENCODE_AAC,
    NOTE_AUDIO_ENCODE_MP3,
    NOTE_STREAM_COPY,
    RES_BEST,
    STATUS_CANCELLED,
    STATUS_DOWNLOADING,
    STATUS_FINISHED,
    STATUS_PROCESSING,
    STATUS_READY,
    STATUS_READY_MEDIA,
    TAB_AUDIO,
    TAB_VIDEO,
    URL_PLACEHOLDER,
)
from ui.theme import (
    AMBER,
    CONTENT_HEIGHT,
    CORAL,
    FONT_BODY,
    FONT_BODY_BOLD,
    FONT_CAPTION,
    FONT_LABEL,
    FONT_META,
    FONT_SECTION,
    FONT_TITLE,
    HILITE,
    INK,
    INK_FAINT,
    INK_SOFT,
    MARGIN_RAIL,
    MARGIN_RED,
    PAD_GAP,
    PAD_INTERNAL,
    PAD_WINDOW,
    PAPER,
    PAPER_DEEP,
    PAPER_LINE,
    SAGE_DEEP,
    WHITE,
    WINDOW_WIDTH,
)
from ui.widgets import (
    GlyphIcon,
    PixelBadge,
    PixelButton,
    PixelCheck,
    PixelChip,
    PixelEntry,
    PixelProgressBar,
    PixelThumb,
    SectionHeading,
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
        self.configure(bg=PAPER)
        self.resizable(False, False)

        # Durum değişkenleri
        self.current_metadata: Optional[MediaMetadata] = None
        self.current_selection: Optional[SelectionResult] = None
        self.is_downloading = False
        self.active_tab = "video"
        self.thumb_photo: Optional[ImageTk.PhotoImage] = None
        self.last_downloaded_path = ""

        # Form değişkenleri
        self.var_resolution = tk.StringVar(value=RES_BEST)
        self.var_force_mp4 = tk.BooleanVar(value=False)
        self.var_audio_format = tk.StringVar(value="opus")

        self.res_chips: list[PixelChip] = []
        self.audio_chips: list[PixelChip] = []

        self.protocol("WM_DELETE_WINDOW", self.on_window_close)

        self._build_ui()
        self.status_label.configure(text=STATUS_READY)
        self._center_on_screen()

        # Pencereyi öne getir
        self.deiconify()
        self.lift()
        self.focus_force()
        self.attributes("-topmost", True)
        self.after(300, lambda: self.attributes("-topmost", False))

    # ------------------------------------------------------------------ düzen
    def _center_on_screen(self):
        """Pencereyi içeriğin gerçek yüksekliğine oturtup ekranın ortasına koyar."""
        self.update_idletasks()
        height = self.winfo_reqheight()
        sw = self.winfo_screenwidth()
        sh = self.winfo_screenheight()
        cx = max(20, (sw - WINDOW_WIDTH) // 2)
        cy = max(20, (sh - height) // 3)
        self.geometry(f"{WINDOW_WIDTH}x{height}+{cx}+{cy}")

    def _build_ui(self):
        shell = tk.Frame(self, bg=PAPER)
        shell.pack(fill="both", expand=True)

        self._build_header(shell)

        # Kağıdın üst kuralı (başlık ile gövdeyi ayırır)
        tk.Frame(shell, bg=PAPER_LINE, height=1).pack(fill="x")

        page = tk.Frame(shell, bg=PAPER)
        page.pack(fill="both", expand=True)

        self._build_margin(page)

        self.content = tk.Frame(page, bg=PAPER, padx=18, pady=12)
        self.content.pack(side="left", fill="both", expand=True)

        self._build_link_section()
        self._build_media_section()
        self._build_action_section()

    # -- 1. başlık -------------------------------------------------------
    def _build_header(self, parent):
        header = tk.Frame(parent, bg=PAPER)
        header.pack(fill="x", padx=(PAD_WINDOW, PAD_WINDOW), pady=(12, 10))

        # Monogram: fosforlu kalemle işaretlenmiş "ff" damgası
        logo_box = tk.Frame(header, bg=HILITE, padx=6, pady=3)
        logo_box.pack(side="left")
        tk.Label(
            logo_box,
            text="ff",
            font=(FONT_TITLE[0], 11, "bold"),
            bg=HILITE,
            fg=INK,
            bd=0,
            highlightthickness=0,
        ).pack()

        name_box = tk.Frame(header, bg=PAPER)
        name_box.pack(side="left", padx=(10, 0))

        tk.Label(
            name_box,
            text=APP_NAME,
            font=FONT_TITLE,
            bg=PAPER,
            fg=INK,
            bd=0,
            highlightthickness=0,
        ).pack(anchor="w")

        tk.Label(
            name_box,
            text=APP_SUBTITLE,
            font=FONT_CAPTION,
            bg=PAPER,
            fg=INK_SOFT,
            bd=0,
            highlightthickness=0,
        ).pack(anchor="w")

        self.platform_badge = PixelBadge(header, text="WEB")
        self.platform_badge.pack(side="right", pady=(0, 14))

    # -- 2. sol kenar (defter cildi) -------------------------------------
    def _build_margin(self, parent):
        rail = tk.Frame(parent, bg=PAPER, width=MARGIN_RAIL)
        rail.pack(side="left", fill="y")
        rail.pack_propagate(False)

        # Kırmızı kenar çizgisi
        tk.Frame(parent, bg=MARGIN_RED, width=1).pack(side="left", fill="y")

    # -- 3. bağlantı bölümü ----------------------------------------------
    def _build_link_section(self):
        SectionHeading(self.content, text="BAĞLANTI").pack(anchor="w")

        row = tk.Frame(self.content, bg=PAPER)
        row.pack(fill="x", pady=(8, 0))

        self.url_entry = PixelEntry(row, placeholder=URL_PLACEHOLDER)
        self.url_entry.pack(side="left", fill="x", expand=True)
        self.url_entry.entry.bind("<KeyRelease>", self._on_url_changed)
        self.url_entry.entry.bind("<Return>", lambda e: self.on_fetch_clicked())

        btn_paste = PixelButton(
            row,
            text=BTN_PASTE,
            command=self.on_paste_clicked,
            tone="neutral",
            padx=10,
            pady=6,
        )
        btn_paste.pack(side="left", padx=(8, 0))

        self.btn_fetch = PixelButton(
            row,
            text=BTN_FETCH,
            command=self.on_fetch_clicked,
            tone="info",
            padx=12,
            pady=6,
        )
        self.btn_fetch.pack(side="left", padx=(6, 0))

        tk.Label(
            self.content,
            text=HINT_SUPPORTED,
            font=FONT_META,
            bg=PAPER,
            fg=INK_FAINT,
            bd=0,
            highlightthickness=0,
        ).pack(anchor="w", pady=(6, 0))

        tk.Frame(self.content, bg=PAPER_LINE, height=1).pack(fill="x", pady=(PAD_GAP, 0))

    # -- 4. medya bölümü -------------------------------------------------
    def _build_media_section(self):
        SectionHeading(self.content, text="MEDYA").pack(anchor="w", pady=(PAD_GAP, 0))

        self.media_region = tk.Frame(self.content, bg=PAPER, height=CONTENT_HEIGHT)
        self.media_region.pack(fill="x", pady=(8, 0))
        self.media_region.pack_propagate(False)

        self._build_guide_state()
        self._build_media_state()

        tk.Frame(self.content, bg=PAPER_LINE, height=1).pack(fill="x", pady=(PAD_GAP, 0))

    def _build_guide_state(self):
        """Bağlantı gelmeden önce görünen, sırayı öğreten not kutusu."""
        self.guide_frame = tk.Frame(self.media_region, bg=PAPER_DEEP, padx=18, pady=16)
        self.guide_frame.pack(fill="x", expand=True)

        tk.Label(
            self.guide_frame,
            text=EMPTY_TITLE,
            font=FONT_BODY_BOLD,
            bg=PAPER_DEEP,
            fg=INK,
            bd=0,
            highlightthickness=0,
        ).pack(anchor="w")

        for index, step in enumerate(EMPTY_STEPS, start=1):
            row = tk.Frame(self.guide_frame, bg=PAPER_DEEP)
            row.pack(fill="x", pady=(6, 0))

            tk.Label(
                row,
                text=f"{index}",
                font=FONT_LABEL,
                bg=PAPER_DEEP,
                fg=INK_SOFT,
                width=2,
                anchor="w",
                bd=0,
                highlightthickness=0,
            ).pack(side="left")

            tk.Label(
                row,
                text=step,
                font=FONT_BODY,
                bg=PAPER_DEEP,
                fg=INK_SOFT,
                anchor="w",
                bd=0,
                highlightthickness=0,
            ).pack(side="left")

    def _build_media_state(self):
        self.media_frame = tk.Frame(self.media_region, bg=PAPER)

        # 4A. Önizleme + künye
        top = tk.Frame(self.media_frame, bg=PAPER)
        top.pack(fill="x")

        thumb_border = tk.Frame(top, bg=PAPER_LINE, padx=1, pady=1)
        thumb_border.pack(side="left", anchor="n")
        self.thumb = PixelThumb(thumb_border)
        self.thumb.pack()

        info = tk.Frame(top, bg=PAPER)
        info.pack(side="left", fill="both", expand=True, padx=(10, 0))

        self.lbl_media_title = tk.Label(
            info,
            text="",
            font=FONT_BODY_BOLD,
            bg=PAPER,
            fg=INK,
            anchor="w",
            justify="left",
            wraplength=300,
            height=2,
            bd=0,
            highlightthickness=0,
        )
        self.lbl_media_title.pack(fill="x")

        self.lbl_media_meta = tk.Label(
            info,
            text="",
            font=FONT_META,
            bg=PAPER,
            fg=INK_SOFT,
            anchor="w",
            bd=0,
            highlightthickness=0,
        )
        self.lbl_media_meta.pack(fill="x", pady=(2, 0))

        self.lbl_codec_meta = tk.Label(
            info,
            text="",
            font=FONT_META,
            bg=PAPER,
            fg=INK,
            anchor="w",
            bd=0,
            highlightthickness=0,
        )
        self.lbl_codec_meta.pack(fill="x", pady=(1, 0))

        # 4B. Video / Ses sekmeleri
        tabs = tk.Frame(self.media_frame, bg=PAPER)
        tabs.pack(fill="x", pady=(8, 0))

        self.tab_video = PixelChip(
            tabs,
            text=TAB_VIDEO,
            value="video",
            on_click=lambda chip: self.switch_tab("video"),
            selected=True,
            font=FONT_BODY_BOLD,
            padx=10,
            pady=3,
        )
        self.tab_video.pack(side="left")

        self.tab_audio = PixelChip(
            tabs,
            text=TAB_AUDIO,
            value="audio",
            on_click=lambda chip: self.switch_tab("audio"),
            selected=False,
            font=FONT_BODY_BOLD,
            padx=10,
            pady=3,
        )
        self.tab_audio.pack(side="left", padx=(6, 0))

        # 4C. Seçenek satırı (video ya da ses)
        self.options_row = tk.Frame(self.media_frame, bg=PAPER)
        self.options_row.pack(fill="x", pady=(8, 0))

        self.options_video = tk.Frame(self.options_row, bg=PAPER)
        tk.Label(
            self.options_video,
            text=LABEL_RESOLUTION,
            font=FONT_CAPTION,
            bg=PAPER,
            fg=INK_SOFT,
            bd=0,
            highlightthickness=0,
        ).pack(side="left", padx=(0, 8))

        self.res_chip_box = tk.Frame(self.options_video, bg=PAPER)
        self.res_chip_box.pack(side="left")
        self.options_video.pack(fill="x")

        self.options_audio = tk.Frame(self.options_row, bg=PAPER)
        tk.Label(
            self.options_audio,
            text=LABEL_AUDIO_FORMAT,
            font=FONT_CAPTION,
            bg=PAPER,
            fg=INK_SOFT,
            bd=0,
            highlightthickness=0,
        ).pack(side="left", padx=(0, 8))

        self.audio_chip_box = tk.Frame(self.options_audio, bg=PAPER)
        self.audio_chip_box.pack(side="left")

        for value, label in AUDIO_CHOICES:
            chip = PixelChip(
                self.audio_chip_box,
                text=label,
                value=value,
                on_click=self._on_audio_chip,
                selected=(value == self.var_audio_format.get()),
            )
            chip.pack(side="left", padx=(0, 6))
            self.audio_chips.append(chip)

        # 4D. MP4 onay kutusu
        self.mp4_row = tk.Frame(self.media_frame, bg=PAPER)
        self.mp4_row.pack(fill="x", pady=(6, 0))

        # Esnek boşluk: ses sekmesinde alt bloklar bölümün dibine yaslanır
        self.media_spacer = tk.Frame(self.media_frame, bg=PAPER, height=1)
        self.media_spacer.pack(fill="both", expand=True)
        self.chk_mp4 = PixelCheck(
            self.mp4_row,
            text=CHECK_FORCE_MP4,
            variable=self.var_force_mp4,
            command=self._on_options_changed,
        )
        self.chk_mp4.pack(side="left")

        # 4E. Not / uyarı şeridi (yüksekliği sabit)
        self.note_box = tk.Frame(self.media_frame, bg=PAPER_DEEP, height=36, padx=8, pady=5)
        self.note_box.pack(fill="x", pady=(8, 0))
        self.note_box.pack_propagate(False)

        self.note_icon = GlyphIcon(self.note_box, "check", scale=1, color=INK_SOFT, bg=PAPER_DEEP)
        self.note_icon.pack(side="left", padx=(0, 6))

        self.lbl_note = tk.Label(
            self.note_box,
            text="",
            font=FONT_CAPTION,
            bg=PAPER_DEEP,
            fg=INK_SOFT,
            anchor="w",
            justify="left",
            wraplength=380,
            bd=0,
            highlightthickness=0,
        )
        self.lbl_note.pack(side="left", fill="x", expand=True)

    # -- 5. eylem bölümü -------------------------------------------------
    def _build_action_section(self):
        footer = tk.Frame(self.content, bg=PAPER)
        footer.pack(fill="x", pady=(PAD_GAP, 0))

        # İlerleme yuvası: çubuk yalnızca indirme sırasında çizilir; yuva sabit
        # yükseklikte durur, böylece çubuk görünüp kaybolurken yerleşim kaymaz.
        self.progress_slot = tk.Frame(footer, bg=PAPER, height=13)
        self.progress_slot.pack(fill="x")
        self.progress_slot.pack_propagate(False)

        self.progress_bar = PixelProgressBar(self.progress_slot, height=11)

        status_row = tk.Frame(footer, bg=PAPER)
        status_row.pack(fill="x", pady=(8, 10))

        self.status_label = tk.Label(
            status_row,
            text=STATUS_READY,
            font=FONT_META,
            bg=PAPER,
            fg=INK_SOFT,
            anchor="w",
            bd=0,
            highlightthickness=0,
        )
        self.status_label.pack(side="left")

        self.progress_meta_label = tk.Label(
            status_row,
            text="",
            font=FONT_META,
            bg=PAPER,
            fg=INK_FAINT,
            anchor="e",
            bd=0,
            highlightthickness=0,
        )
        self.progress_meta_label.pack(side="right")

        row = tk.Frame(footer, bg=PAPER)
        row.pack(fill="x")

        self.btn_download = PixelButton(
            row,
            text=BTN_DOWNLOAD,
            command=self.on_download_clicked,
            tone="primary",
            padx=22,
            pady=7,
        )
        self.btn_download.pack(side="left")

        self.btn_cancel = PixelButton(
            row,
            text=BTN_CANCEL,
            command=self.on_cancel_clicked,
            tone="danger",
            padx=12,
            pady=7,
        )
        self.btn_cancel.pack(side="left", padx=(8, 0))
        self.btn_cancel.set_enabled(False)

        self.btn_open_folder = PixelButton(
            row,
            text=BTN_OPEN_FOLDER,
            command=self.on_open_folder_clicked,
            tone="neutral",
            padx=14,
            pady=7,
        )
        self.btn_open_folder.pack(side="right")
        self.btn_open_folder.set_enabled(False)

    # ------------------------------------------------------------------ durum
    def _show_guide(self):
        self.media_frame.pack_forget()
        self.guide_frame.pack(fill="both", expand=True)

    def _show_media(self):
        self.guide_frame.pack_forget()
        self.media_frame.pack(fill="both", expand=True)

    def _set_note(self, kind: str, text: str):
        """Not şeridini bilgi (sakin) veya uyarı (amber) moduna alır."""
        if kind == "warn":
            bg, fg, glyph, color = AMBER, INK, "warn", INK
        elif kind == "error":
            bg, fg, glyph, color = CORAL, INK, "warn", INK
        else:
            bg, fg, glyph, color = PAPER_DEEP, INK_SOFT, "check", INK_SOFT

        self.note_box.configure(bg=bg)
        self.lbl_note.configure(bg=bg, fg=fg, text=text)
        self.note_icon.set_colors(color, bg)
        if self.note_icon.key != glyph:
            self.note_icon.destroy()
            self.note_icon = GlyphIcon(self.note_box, glyph, scale=1, color=color, bg=bg)
            self.note_icon.pack(side="left", padx=(0, 6), before=self.lbl_note)

    # ------------------------------------------------------------------ sekmeler
    def switch_tab(self, tab: str):
        """Video ve Ses sekmeleri arasında geçiş yapar."""
        self.active_tab = tab
        self.tab_video.set_selected(tab == "video")
        self.tab_audio.set_selected(tab == "audio")

        if tab == "video":
            self.options_audio.pack_forget()
            self.options_video.pack(fill="x")
            self.mp4_row.pack(fill="x", pady=(6, 0), before=self.note_box)
        else:
            self.options_video.pack_forget()
            self.options_audio.pack(fill="x")
            self.mp4_row.pack_forget()

        self._update_selection()

    # ------------------------------------------------------------------ URL
    def _on_url_changed(self, event=None):
        url = self.url_entry.get()
        platform = detect_platform(url)
        self.platform_badge.set_text(platform.badge_label)

    def on_paste_clicked(self):
        try:
            clipboard = self.clipboard_get().strip()
        except Exception:
            clipboard = ""
        if clipboard:
            self.url_entry.set(clipboard)
            self._on_url_changed()
            self.on_fetch_clicked()

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
        self.btn_fetch.set_text(BTN_FETCH_BUSY)
        self.btn_download.set_enabled(False)

        def _success(meta: MediaMetadata):
            self.after(0, lambda: self._apply_fetched_metadata(meta))

        def _error(err: str):
            self.after(0, lambda: self._handle_fetch_error(err))

        self.engine.fetch_info_async(url, on_success=_success, on_error=_error)

    def _apply_fetched_metadata(self, meta: MediaMetadata):
        self.current_metadata = meta
        self.btn_fetch.set_enabled(True)
        self.btn_fetch.set_text(BTN_FETCH)
        self.btn_download.set_enabled(True)
        self.status_label.configure(text=STATUS_READY)

        self.lbl_media_title.configure(text=meta.title)
        self.lbl_media_meta.configure(
            text=f"{meta.platform_name} · {meta.duration_str} · {len(meta.available_resolutions)} kalite"
        )
        self.platform_badge.set_text(detect_platform(self.url_entry.get()).badge_label)

        self._rebuild_resolution_chips(meta.available_resolutions)
        self.var_resolution.set(RES_BEST)

        if meta.thumbnail_url:
            self.thumb.clear()
            threading.Thread(target=self._load_thumbnail_async, args=(meta.thumbnail_url,), daemon=True).start()
        else:
            self.thumb.clear()

        self._show_media()
        self._update_selection()

    def _rebuild_resolution_chips(self, resolutions):
        for chip in self.res_chips:
            chip.destroy()
        self.res_chips.clear()

        choices = [(RES_BEST, "En İyi")] + [(f"{res}p", f"{res}p") for res in resolutions]
        for value, label in choices:
            chip = PixelChip(
                self.res_chip_box,
                text=label,
                value=value,
                on_click=self._on_resolution_chip,
                selected=(value == self.var_resolution.get()),
            )
            chip.pack(side="left", padx=(0, 6))
            self.res_chips.append(chip)

    def _on_resolution_chip(self, chip: PixelChip):
        self.var_resolution.set(chip.value)
        for c in self.res_chips:
            c.set_selected(c is chip)
        self._update_selection()

    def _on_audio_chip(self, chip: PixelChip):
        self.var_audio_format.set(chip.value)
        for c in self.audio_chips:
            c.set_selected(c is chip)
        self._update_selection()

    def _load_thumbnail_async(self, thumb_url: str):
        try:
            req = Request(thumb_url, headers={"User-Agent": "Mozilla/5.0"})
            with urlopen(req, timeout=5) as resp:
                data = resp.read()
            img = Image.open(io.BytesIO(data))
            img.thumbnail((PixelThumb.WIDTH, PixelThumb.HEIGHT), Image.Resampling.LANCZOS)
            photo = ImageTk.PhotoImage(img)

            def _update():
                self.thumb_photo = photo
                self.thumb.set_photo(photo)

            self.after(0, _update)
        except Exception:
            pass

    def _handle_fetch_error(self, err: str):
        self.btn_fetch.set_enabled(True)
        self.btn_fetch.set_text(BTN_FETCH)
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

        selection = select_best_format(
            self.current_metadata.formats,
            mode=self.active_tab,
            target_res=target_res,
            force_mp4=self.var_force_mp4.get(),
            audio_format_choice=self.var_audio_format.get(),
        )
        self.current_selection = selection

        codec_text = (
            f"{selection.audio_codec.upper()} → .{selection.expected_container}"
            if self.active_tab == "audio"
            else (
                f"{selection.video_codec.upper()} + {selection.audio_codec.upper()}"
                f" → .{selection.expected_container}"
            )
        )
        codec_text += " · " + ("dönüştürme var" if selection.needs_reencode else "kopyalama")
        self.lbl_codec_meta.configure(text=codec_text)

        if self.active_tab == "video" and selection.warning:
            self._set_note("warn", selection.warning)
        elif self.active_tab == "video":
            self._set_note("info", NOTE_STREAM_COPY)
        else:
            self._set_note("info", NOTE_AUDIO)

    # ------------------------------------------------------------------ indirme
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
            self.status_label.configure(text=f"%{state.percent:.0f} · {spd}")
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
        self.progress_meta_label.configure(text=os.path.basename(filepath))

    def _on_download_error(self, err: str):
        self.is_downloading = False
        self.btn_download.set_enabled(True)
        self.btn_cancel.set_enabled(False)
        self.btn_fetch.set_enabled(True)
        self.status_label.configure(text="Hata oluştu.")
        self.progress_bar.set_percent(0)
        show_error(self, "İndirme Hatası", err)

    # ------------------------------------------------------------------ pencere
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
        """Pencereyi gizler ve ağır görselleri bellekten boşaltır."""
        self.withdraw()
        self.thumb_photo = None
        self.thumb.clear()
        gc.collect()

        if os.name == "nt":
            try:
                import ctypes
                ctypes.windll.kernel32.SetProcessWorkingSetSize(-1, -1, -1)
            except Exception:
                pass

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
