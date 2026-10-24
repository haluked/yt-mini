"""Özel Piksel ve Pastel Arayüz Bileşenleri.

Tasarım kuralları:
- Keskin köşeler (0 radius)
- 1-2px belirgin katı kenarlıklar (BORDER_DARK / BORDER_MUTED)
- Yumuşak pastel dolgular
- Görsel geri bildirim (Hover & Active basılma durumları)
"""

from __future__ import annotations
import tkinter as tk
from typing import Callable, Optional

from ui.theme import (
    BG_PRIMARY,
    BG_SURFACE,
    BG_SUNKEN,
    BORDER_DARK,
    BORDER_MUTED,
    BORDER_THICK,
    BORDER_THIN,
    TEXT_MAIN,
    TEXT_MUTED,
    ACCENT_GREEN,
    ACCENT_BLUE,
    ACCENT_CORAL,
    ACCENT_HOVER,
    FONT_BODY,
    FONT_BODY_BOLD,
    FONT_META,
    FONT_BADGE,
)


class PixelButton(tk.Frame):
    """Piksel estetiğinde, 2px katı kenarlıklı, pastel dolgulu özel buton."""

    def __init__(
        self,
        master,
        text: str,
        command: Optional[Callable[[], None]] = None,
        bg_color: str = BG_SUNKEN,
        fg_color: str = TEXT_MAIN,
        hover_color: str = ACCENT_HOVER,
        font=FONT_BODY_BOLD,
        padx: int = 12,
        pady: int = 6,
        border_width: int = BORDER_THICK,
        border_color: str = BORDER_DARK,
        cursor: str = "hand2",
        **kwargs,
    ):
        super().__init__(
            master,
            bg=border_color,
            padx=border_width,
            pady=border_width,
            **kwargs,
        )
        self.command = command
        self.bg_color = bg_color
        self.fg_color = fg_color
        self.hover_color = hover_color
        self._disabled = False

        self.inner_frame = tk.Frame(self, bg=bg_color, cursor=cursor)
        self.inner_frame.pack(fill="both", expand=True)

        self.label = tk.Label(
            self.inner_frame,
            text=text,
            bg=bg_color,
            fg=fg_color,
            font=font,
            cursor=cursor,
            padx=padx,
            pady=pady,
        )
        self.label.pack(fill="both", expand=True)

        # Olay bağlamaları
        for widget in (self, self.inner_frame, self.label):
            widget.bind("<Enter>", self._on_enter)
            widget.bind("<Leave>", self._on_leave)
            widget.bind("<Button-1>", self._on_click)

    def _on_enter(self, event=None):
        if not self._disabled:
            self.inner_frame.configure(bg=self.hover_color)
            self.label.configure(bg=self.hover_color)

    def _on_leave(self, event=None):
        if not self._disabled:
            self.inner_frame.configure(bg=self.bg_color)
            self.label.configure(bg=self.bg_color)

    def _on_click(self, event=None):
        if not self._disabled and self.command:
            self.command()

    def set_text(self, text: str):
        self.label.configure(text=text)

    def set_enabled(self, enabled: bool):
        self._disabled = not enabled
        if self._disabled:
            self.inner_frame.configure(bg=BG_SUNKEN)
            self.label.configure(bg=BG_SUNKEN, fg=TEXT_MUTED)
        else:
            self.inner_frame.configure(bg=self.bg_color)
            self.label.configure(bg=self.bg_color, fg=self.fg_color)


class PixelEntry(tk.Frame):
    """Piksel tarzında keskin kenarlıklı metin giriş kutusu."""

    def __init__(
        self,
        master,
        placeholder: str = "",
        font=FONT_BODY,
        **kwargs,
    ):
        super().__init__(
            master,
            bg=BORDER_DARK,
            padx=BORDER_THICK,
            pady=BORDER_THICK,
            **kwargs,
        )
        self.placeholder = placeholder
        self._has_placeholder = False

        self.inner_frame = tk.Frame(self, bg=BG_SURFACE)
        self.inner_frame.pack(fill="both", expand=True)

        self.entry = tk.Entry(
            self.inner_frame,
            bg=BG_SURFACE,
            fg=TEXT_MAIN,
            font=font,
            relief="flat",
            bd=0,
            highlightthickness=0,
            insertbackground=TEXT_MAIN,
        )
        self.entry.pack(fill="both", expand=True, padx=8, pady=6)

        self.entry.bind("<FocusIn>", self._on_focus_in)
        self.entry.bind("<FocusOut>", self._on_focus_out)

        if placeholder:
            self._set_placeholder()

    def _set_placeholder(self):
        if not self.entry.get():
            self._has_placeholder = True
            self.entry.insert(0, self.placeholder)
            self.entry.configure(fg=TEXT_MUTED)

    def _on_focus_in(self, event=None):
        self.configure(bg=ACCENT_BLUE)
        if self._has_placeholder:
            self.entry.delete(0, tk.END)
            self.entry.configure(fg=TEXT_MAIN)
            self._has_placeholder = False

    def _on_focus_out(self, event=None):
        self.configure(bg=BORDER_DARK)
        if not self.entry.get():
            self._set_placeholder()

    def get(self) -> str:
        if self._has_placeholder:
            return ""
        return self.entry.get().strip()

    def set(self, text: str):
        self._has_placeholder = False
        self.entry.delete(0, tk.END)
        self.entry.insert(0, text)
        self.entry.configure(fg=TEXT_MAIN)


class PixelProgressBar(tk.Frame):
    """Piksel estetiğinde, blok dolgulu keskin ilerleme çubuğu."""

    def __init__(
        self,
        master,
        height: int = 18,
        bar_color: str = ACCENT_GREEN,
        bg_color: str = BG_SUNKEN,
        border_color: str = BORDER_DARK,
        **kwargs,
    ):
        super().__init__(
            master,
            bg=border_color,
            padx=BORDER_THICK,
            pady=BORDER_THICK,
            **kwargs,
        )
        self.bar_color = bar_color
        self.bg_color = bg_color
        self.canvas_height = height
        self._percent = 0.0

        self.canvas = tk.Canvas(
            self,
            height=height,
            bg=bg_color,
            highlightthickness=0,
            bd=0,
        )
        self.canvas.pack(fill="x", expand=True)
        self.canvas.bind("<Configure>", self._redraw)

    def set_percent(self, percent: float):
        self._percent = max(0.0, min(100.0, float(percent)))
        self._redraw()

    def _redraw(self, event=None):
        self.canvas.delete("all")
        width = self.canvas.winfo_width()
        height = self.canvas.winfo_height() or self.canvas_height

        if width <= 1:
            return

        fill_width = int(width * (self._percent / 100.0))
        if fill_width > 0:
            self.canvas.create_rectangle(
                0, 0, fill_width, height,
                fill=self.bar_color,
                outline="",
            )


class PixelCard(tk.Frame):
    """2px keskin kenarlıklı, pastel yüzey kart bileşeni."""

    def __init__(
        self,
        master,
        bg: str = BG_SURFACE,
        border_color: str = BORDER_DARK,
        border_width: int = BORDER_THICK,
        padx: int = 10,
        pady: int = 10,
        **kwargs,
    ):
        super().__init__(
            master,
            bg=border_color,
            padx=border_width,
            pady=border_width,
            **kwargs,
        )
        self.inner = tk.Frame(self, bg=bg, padx=padx, pady=pady)
        self.inner.pack(fill="both", expand=True)


class PixelBadge(tk.Frame):
    """Platform veya kodek için küçük retro piksel etiket."""

    def __init__(
        self,
        master,
        text: str,
        bg_color: str = BG_SUNKEN,
        fg_color: str = TEXT_MAIN,
        border_color: str = BORDER_MUTED,
        **kwargs,
    ):
        super().__init__(
            master,
            bg=border_color,
            padx=BORDER_THIN,
            pady=BORDER_THIN,
            **kwargs,
        )
        self.label = tk.Label(
            self,
            text=text,
            bg=bg_color,
            fg=fg_color,
            font=FONT_BADGE,
            padx=4,
            pady=1,
        )
        self.label.pack()

    def set_text(self, text: str, bg_color: Optional[str] = None):
        self.label.configure(text=text)
        if bg_color:
            self.label.configure(bg=bg_color)
