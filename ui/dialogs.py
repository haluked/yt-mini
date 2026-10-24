"""Piksel temalı açılır pencereler (diyaloglar)."""

from __future__ import annotations
import tkinter as tk
from typing import Callable, Optional

from ui.theme import (
    BG_PRIMARY,
    BG_SURFACE,
    BG_SUNKEN,
    BORDER_DARK,
    BORDER_THICK,
    TEXT_MAIN,
    TEXT_MUTED,
    ACCENT_AMBER,
    ACCENT_CORAL,
    ACCENT_GREEN,
    FONT_SECTION,
    FONT_BODY,
    FONT_BODY_BOLD,
)
from ui.widgets import PixelButton, PixelCard


class PixelMessageBox(tk.Toplevel):
    """Piksel estetiğinde özel mesaj ve onay penceresi."""

    def __init__(
        self,
        parent,
        title: str,
        message: str,
        dialog_type: str = "info",  # "info", "warning", "confirm"
        on_confirm: Optional[Callable[[], None]] = None,
        confirm_text: str = "Tamam",
        cancel_text: str = "İptal",
    ):
        super().__init__(parent)
        self.title(title)
        self.resizable(False, False)
        self.configure(bg=BG_PRIMARY)
        self.transient(parent)
        self.grab_set()

        self.on_confirm = on_confirm
        self.result = False

        card = PixelCard(self, bg=BG_SURFACE, border_color=BORDER_DARK, padx=16, pady=16)
        card.pack(fill="both", expand=True, padx=12, pady=12)

        # Başlık ve Rozet rengi
        accent = ACCENT_AMBER if dialog_type == "warning" else (ACCENT_CORAL if dialog_type == "error" else ACCENT_GREEN)
        header_frame = tk.Frame(card.inner, bg=BG_SURFACE)
        header_frame.pack(fill="x", pady=(0, 10))

        indicator = tk.Frame(header_frame, bg=accent, width=12, height=12)
        indicator.pack(side="left", padx=(0, 8))

        lbl_title = tk.Label(header_frame, text=title, font=FONT_SECTION, bg=BG_SURFACE, fg=TEXT_MAIN)
        lbl_title.pack(side="left")

        # Mesaj
        lbl_msg = tk.Label(
            card.inner,
            text=message,
            font=FONT_BODY,
            bg=BG_SURFACE,
            fg=TEXT_MAIN,
            justify="left",
            wraplength=360,
        )
        lbl_msg.pack(fill="x", pady=(0, 16))

        # Butonlar
        btn_frame = tk.Frame(card.inner, bg=BG_SURFACE)
        btn_frame.pack(fill="x")

        if dialog_type == "confirm":
            btn_cancel = PixelButton(
                btn_frame,
                text=cancel_text,
                command=self._on_cancel,
                bg_color=BG_SUNKEN,
                padx=12,
                pady=4,
            )
            btn_cancel.pack(side="right", padx=(8, 0))

        btn_ok = PixelButton(
            btn_frame,
            text=confirm_text,
            command=self._on_ok,
            bg_color=accent,
            padx=16,
            pady=4,
        )
        btn_ok.pack(side="right")

        # Pencereyi ortala
        self.update_idletasks()
        w = self.winfo_reqwidth()
        h = self.winfo_reqheight()
        pw = parent.winfo_width()
        ph = parent.winfo_height()
        px = parent.winfo_rootx()
        py = parent.winfo_rooty()
        self.geometry(f"{max(380, w)}x{h}+{px + (pw - w)//2}+{py + (ph - h)//2}")

    def _on_ok(self):
        self.result = True
        self.destroy()
        if self.on_confirm:
            self.on_confirm()

    def _on_cancel(self):
        self.result = False
        self.destroy()


def show_error(parent, title: str, message: str):
    PixelMessageBox(parent, title=title, message=message, dialog_type="error")


def show_warning(parent, title: str, message: str):
    PixelMessageBox(parent, title=title, message=message, dialog_type="warning")


def show_confirm(parent, title: str, message: str, on_confirm: Callable[[], None]):
    PixelMessageBox(parent, title=title, message=message, dialog_type="confirm", on_confirm=on_confirm)
