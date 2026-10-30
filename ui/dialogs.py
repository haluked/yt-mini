"""Kırtasiye defteri temalı açılır pencereler.

Kural: kart içinde kart yok. Diyalog tek bir kağıt yaprağıdır; 1px mürekkep
çerçeve, üstte renkli durum karesi, altta sağa yaslı butonlar.
"""

from __future__ import annotations
import tkinter as tk
from typing import Callable, Optional

from ui.theme import (
    AMBER,
    CORAL,
    FONT_BODY,
    FONT_SECTION,
    HILITE,
    INK,
    INK_SOFT,
    PAPER,
    PAPER_LINE,
    SAGE,
)
from ui.widgets import PixelButton


class PixelMessageBox(tk.Toplevel):
    """Sade kağıt diyaloğu: bilgi, uyarı ve onay için tek bileşen."""

    def __init__(
        self,
        parent,
        title: str,
        message: str,
        dialog_type: str = "info",  # "info", "warning", "error", "confirm"
        on_confirm: Optional[Callable[[], None]] = None,
        confirm_text: str = "Tamam",
        cancel_text: str = "Vazgeç",
    ):
        super().__init__(parent)
        self.title(title)
        self.resizable(False, False)
        self.configure(bg=PAPER)
        self.transient(parent)
        self.grab_set()

        self.on_confirm = on_confirm
        self.result = False

        if dialog_type == "warning":
            accent, tone = AMBER, "primary"
        elif dialog_type == "error":
            accent, tone = CORAL, "danger"
        elif dialog_type == "confirm":
            accent, tone = HILITE, "primary"
        else:
            accent, tone = SAGE, "primary"

        # 1px mürekkep çerçeve
        border = tk.Frame(self, bg=INK, padx=1, pady=1)
        border.pack(fill="both", expand=True)

        body = tk.Frame(border, bg=PAPER, padx=18, pady=16)
        body.pack(fill="both", expand=True)

        head = tk.Frame(body, bg=PAPER)
        head.pack(fill="x")

        tk.Frame(head, bg=accent, width=9, height=9).pack(side="left", padx=(0, 8))
        tk.Label(
            head,
            text=title,
            font=FONT_SECTION,
            bg=PAPER,
            fg=INK,
            bd=0,
            highlightthickness=0,
        ).pack(side="left")

        tk.Label(
            body,
            text=message,
            font=FONT_BODY,
            bg=PAPER,
            fg=INK_SOFT,
            justify="left",
            anchor="w",
            wraplength=340,
            bd=0,
            highlightthickness=0,
        ).pack(fill="x", pady=(10, 0))

        tk.Frame(body, bg=PAPER_LINE, height=1).pack(fill="x", pady=(14, 12))

        buttons = tk.Frame(body, bg=PAPER)
        buttons.pack(fill="x")

        btn_ok = PixelButton(
            buttons,
            text=confirm_text,
            command=self._on_ok,
            tone=tone,
            padx=16,
            pady=6,
        )
        btn_ok.pack(side="right")

        if dialog_type == "confirm":
            btn_cancel = PixelButton(
                buttons,
                text=cancel_text,
                command=self._on_cancel,
                tone="neutral",
                padx=12,
                pady=6,
            )
            btn_cancel.pack(side="right", padx=(0, 8))

        self.bind("<Escape>", lambda e: self._on_cancel())
        self.bind("<Return>", lambda e: self._on_ok())

        # Pencereyi ebeveynin ortasına yerleştir
        self.update_idletasks()
        w = max(360, self.winfo_reqwidth())
        h = self.winfo_reqheight()
        pw = parent.winfo_width()
        ph = parent.winfo_height()
        px = parent.winfo_rootx()
        py = parent.winfo_rooty()
        x = px + max(0, (pw - w) // 2)
        y = py + max(0, (ph - h) // 2)
        self.geometry(f"{w}x{h}+{x}+{y}")

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
    PixelMessageBox(
        parent,
        title=title,
        message=message,
        dialog_type="confirm",
        on_confirm=on_confirm,
    )
