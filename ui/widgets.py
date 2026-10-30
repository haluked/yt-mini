"""Kırtasiye defteri arayüz bileşenleri.

Tasarım kuralları:
- Keskin köşeler, 1px mürekkep çizgiler (gölge yok; derinlik çizgiyle anlatılır).
- Piksel ikonlar Canvas üzerine 1x1 kare bloklarla çizilir (harici görsel/font yok).
- Seçili/aktif durumlar fosforlu kalem sarısı (HILITE) ile işaretlenir.
- Her etkileşimli bileşen: normal, üzerine gelme, basılı, pasif ve klavye odağı
  durumlarını bilir.
"""

from __future__ import annotations
import tkinter as tk
from typing import Callable, Dict, Optional, Sequence

from ui.theme import (
    AMBER,
    BORDER_THIN,
    CORAL,
    CORAL_DEEP,
    FOCUS,
    FONT_BADGE,
    FONT_BODY,
    FONT_BODY_BOLD,
    FONT_LABEL,
    FONT_META,
    HILITE,
    INK,
    INK_FAINT,
    INK_SOFT,
    PAPER,
    PAPER_DEEP,
    PAPER_LINE,
    SAGE,
    SAGE_DEEP,
    SKY,
    SKY_DEEP,
    WHITE,
)


# --------------------------------------------------------------------- Piksel ikonlar
# Her satır aynı uzunlukta; "X" dolu piksel, "." boş.
GLYPHS: Dict[str, Sequence[str]] = {
    "check": (
        "......X",
        ".....XX",
        "X...XX.",
        "XX.XX..",
        ".XXX...",
        "..X....",
    ),
    "warn": (
        "...X...",
        "..XXX..",
        "..XXX..",
        ".XX.XX.",
        ".XX.XX.",
        "XXX.XXX",
        "XXXXXXX",
    ),
}



def glyph_size(key: str, scale: int = 1) -> tuple[int, int]:
    rows = GLYPHS[key]
    return max(len(r) for r in rows) * scale, len(rows) * scale


def draw_glyph(canvas: tk.Canvas, key: str, x: int, y: int, scale: int, color: str) -> None:
    """Piksel ikonu canvas üzerine bloklar halinde çizer."""
    for ry, row in enumerate(GLYPHS[key]):
        for rx, ch in enumerate(row):
            if ch == "X":
                canvas.create_rectangle(
                    x + rx * scale,
                    y + ry * scale,
                    x + (rx + 1) * scale - 1,
                    y + (ry + 1) * scale - 1,
                    fill=color,
                    outline="",
                )


class GlyphIcon(tk.Canvas):
    """Tek renkli piksel ikon taşıyıcısı."""

    def __init__(self, master, key: str, scale: int = 2, color: str = INK, bg: str = PAPER):
        w, h = glyph_size(key, scale)
        super().__init__(
            master,
            width=w,
            height=h,
            bg=bg,
            highlightthickness=0,
            bd=0,
            takefocus=0,
        )
        self.key = key
        self.scale = scale
        self._color = color
        self._draw()

    def _draw(self):
        self.delete("all")
        draw_glyph(self, self.key, 0, 0, self.scale, self._color)

    def set_colors(self, color: str, bg: str):
        self._color = color
        self.configure(bg=bg)
        self._draw()


# --------------------------------------------------------------------- Buton
# ton -> (dolgu, üzerine gelme, basılı, çerçeve, metin)
BUTTON_TONES = {
    "primary": (SAGE, "#9ACAA9", SAGE_DEEP, INK, INK),
    "info": (SKY, "#A4C8DC", SKY_DEEP, INK, INK),
    "neutral": (PAPER, PAPER_DEEP, PAPER_LINE, INK, INK),
    "danger": (CORAL, "#EBA491", CORAL_DEEP, INK, INK),
    "quiet": (PAPER, PAPER_DEEP, PAPER_LINE, PAPER_LINE, INK_SOFT),
}


class PixelButton(tk.Frame):
    """Düz dolgulu, 1px mürekkep çerçeveli buton; basılınca koyulaşır."""

    def __init__(
        self,
        master,
        text: str = "",
        command: Optional[Callable[[], None]] = None,
        tone: str = "neutral",
        glyph: Optional[str] = None,
        font=FONT_BODY_BOLD,
        padx: int = 14,
        pady: int = 6,
        min_width: int = 0,
        **kwargs,
    ):
        fill, hover, press, border, fg = BUTTON_TONES.get(tone, BUTTON_TONES["neutral"])
        super().__init__(
            master,
            bg=border,
            padx=BORDER_THIN,
            pady=BORDER_THIN,
            highlightthickness=2,
            highlightbackground=PAPER,
            highlightcolor=FOCUS,
            **kwargs,
        )

        self.command = command
        self._fill, self._hover, self._press = fill, hover, press
        self._border, self._fg = border, fg
        self._disabled = False
        self._enabled_look = True
        self._pressed = False
        self._padx, self._pady = padx, pady
        self._glyph_key = glyph
        self._glyph_scale = 2

        self.inner = tk.Frame(self, bg=fill)
        self.inner.pack(fill="both", expand=True)

        self.body = tk.Frame(self.inner, bg=fill)
        self.body.pack(fill="both", expand=True, padx=padx, pady=pady)

        self.icon: Optional[GlyphIcon] = None
        if glyph:
            self.icon = GlyphIcon(self.body, glyph, scale=self._glyph_scale, color=fg, bg=fill)
            self.icon.pack(side="left", padx=(0, 6))

        self.label = tk.Label(
            self.body,
            text=text,
            bg=fill,
            fg=fg,
            font=font,
            bd=0,
            highlightthickness=0,
        )
        self.label.pack(side="left")
        if min_width:
            self.body.configure(width=min_width)
            self.body.pack_propagate(False)

        self.configure(takefocus=True, cursor="hand2")
        self._bind_tree(self)
        self.bind("<Return>", lambda e: self.invoke())
        self.bind("<space>", lambda e: self.invoke())

    # -- olaylar ---------------------------------------------------------
    def _iter_widgets(self):
        yield self
        for child in self.winfo_children():
            yield from self._descendants(child)

    def _descendants(self, widget):
        yield widget
        for child in widget.winfo_children():
            yield from self._descendants(child)

    def _bind_tree(self, widget):
        for w in self._descendants(widget):
            w.bind("<Enter>", self._on_enter)
            w.bind("<Leave>", self._on_leave)
            w.bind("<ButtonPress-1>", self._on_press)
            w.bind("<ButtonRelease-1>", self._on_release)

    def _paint(self, fill: str, border: Optional[str] = None, fg: Optional[str] = None):
        self.configure(bg=border or self._border)
        self.inner.configure(bg=fill)
        self.body.configure(bg=fill)
        self.label.configure(bg=fill, fg=fg or self._fg)
        if self.icon:
            self.icon.set_colors(fg or self._fg, fill)

    def _on_enter(self, _event=None):
        if self._disabled or self._pressed:
            return
        self._paint(self._hover)

    def _on_leave(self, _event=None):
        if self._disabled:
            return
        self._pressed = False
        self._paint(self._fill)

    def _on_press(self, _event=None):
        if self._disabled:
            return
        self._pressed = True
        self._paint(self._press)
        self.focus_set()

    def _on_release(self, event):
        if self._disabled:
            return
        self._pressed = False
        self._paint(self._fill)
        inside = self.winfo_containing(event.x_root, event.y_root)
        if inside is not None and self._owns(inside):
            self.invoke()

    def _owns(self, widget) -> bool:
        while widget is not None:
            if widget is self:
                return True
            widget = getattr(widget, "master", None)
        return False

    # -- genel API -------------------------------------------------------
    def invoke(self):
        if not self._disabled and self.command:
            self.command()

    def set_text(self, text: str):
        self.label.configure(text=text)

    def set_glyph(self, key: Optional[str]):
        if self.icon:
            self.icon.destroy()
            self.icon = None
        if key:
            self._glyph_key = key
            self.icon = GlyphIcon(self.body, key, scale=self._glyph_scale, color=self._fg, bg=self._fill)
            self.icon.pack(side="left", padx=(0, 6), before=self.label)
            self._bind_tree(self.icon)

    def set_enabled(self, enabled: bool):
        if enabled == self._enabled_look:
            return
        self._enabled_look = enabled
        self._disabled = not enabled
        if self._disabled:
            self._paint(PAPER_DEEP, border=PAPER_LINE, fg=INK_FAINT)
            self.configure(cursor="", takefocus=False)
            if self.focus_get() is self:
                self.master.focus_set()
        else:
            self._paint(self._fill, border=self._border, fg=self._fg)
            self.configure(cursor="hand2", takefocus=True)

    def set_tone(self, tone: str):
        fill, hover, press, border, fg = BUTTON_TONES.get(tone, BUTTON_TONES["neutral"])
        self._fill, self._hover, self._press = fill, hover, press
        self._border, self._fg = border, fg
        if not self._disabled:
            self._paint(fill, border=border, fg=fg)


# --------------------------------------------------------------------- Giriş kutusu
class PixelEntry(tk.Frame):
    """Defter üzerine yazılmış satır hissi veren, 1px çerçeveli metin kutusu."""

    def __init__(self, master, placeholder: str = "", font=FONT_BODY, **kwargs):
        super().__init__(master, bg=INK_SOFT, padx=BORDER_THIN, pady=BORDER_THIN, **kwargs)
        self.placeholder = placeholder
        self._has_placeholder = False

        self.entry = tk.Entry(
            self,
            bg=WHITE,
            fg=INK,
            font=font,
            relief="flat",
            bd=0,
            highlightthickness=0,
            insertbackground=INK,
            selectbackground=HILITE,
            selectforeground=INK,
        )
        self.entry.pack(fill="both", expand=True, padx=9, pady=7)

        self.entry.bind("<FocusIn>", self._on_focus_in)
        self.entry.bind("<FocusOut>", self._on_focus_out)
        self.entry.bind("<Button-1>", lambda e: self._on_focus_in())

        if placeholder:
            self._set_placeholder()

    def _set_placeholder(self):
        if not self.entry.get():
            self._has_placeholder = True
            self.entry.insert(0, self.placeholder)
            self.entry.configure(fg=INK_SOFT)

    def _on_focus_in(self, _event=None):
        self.configure(bg=FOCUS)
        if self._has_placeholder:
            self.entry.delete(0, tk.END)
            self.entry.configure(fg=INK)
            self._has_placeholder = False

    def _on_focus_out(self, _event=None):
        self.configure(bg=INK_SOFT)
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
        self.entry.configure(fg=INK)

    def focus_entry(self):
        self.entry.focus_set()


# --------------------------------------------------------------------- Seçim çipi
class PixelChip(tk.Frame):
    """Tek seçimlik küçük etiket (çözünürlük, ses formatı)."""

    def __init__(
        self,
        master,
        text: str,
        value: str,
        on_click: Callable[["PixelChip"], None],
        selected: bool = False,
        font=FONT_META,
        padx: int = 8,
        pady: int = 3,
        **kwargs,
    ):
        super().__init__(
            master,
            bg=PAPER_LINE,
            padx=BORDER_THIN,
            pady=BORDER_THIN,
            highlightthickness=2,
            highlightbackground=PAPER,
            highlightcolor=FOCUS,
            **kwargs,
        )
        self.value = value
        self._on_click = on_click
        self._selected = selected
        self._font = font
        self._padx, self._pady = padx, pady

        self.label = tk.Label(self, text=text, font=font, bd=0, highlightthickness=0, padx=padx, pady=pady)
        self.label.pack(fill="both", expand=True)

        for w in (self, self.label):
            w.bind("<Button-1>", self._click)
            w.bind("<Enter>", self._enter)
            w.bind("<Leave>", self._leave)
        self.configure(cursor="hand2", takefocus=True)
        self.bind("<Return>", lambda e: self._click())
        self.bind("<space>", lambda e: self._click())
        self._apply()

    def _apply(self, hover: bool = False):
        if self._selected:
            bg, fg, border = HILITE, INK, INK
        elif hover:
            bg, fg, border = PAPER_DEEP, INK, INK
        else:
            bg, fg, border = PAPER, INK_SOFT, INK_SOFT
        self.configure(bg=border)
        self.label.configure(bg=bg, fg=fg, font=(self._font[0], self._font[1], "bold" if self._selected else self._font[2]))

    def _enter(self, _e=None):
        self._apply(hover=True)

    def _leave(self, _e=None):
        self._apply(hover=False)

    def _click(self, _e=None):
        self.focus_set()
        self._on_click(self)

    def set_selected(self, selected: bool):
        self._selected = selected
        self._apply()


# --------------------------------------------------------------------- Onay kutusu
class PixelCheck(tk.Frame):
    """Piksel onay kutusu: 14x14 kare, işaretliyken fosforlu zemin."""

    def __init__(
        self,
        master,
        text: str,
        variable: tk.BooleanVar,
        command: Optional[Callable[[], None]] = None,
        font=FONT_BODY,
        **kwargs,
    ):
        super().__init__(
            master,
            bg=PAPER,
            highlightthickness=2,
            highlightbackground=PAPER,
            highlightcolor=FOCUS,
            **kwargs,
        )
        self.variable = variable
        self.command = command
        self._font = font

        self.box = tk.Canvas(self, width=14, height=14, bg=PAPER, highlightthickness=0, bd=0)
        self.box.pack(side="left", padx=(0, 8))

        self.label = tk.Label(self, text=text, font=font, bg=PAPER, fg=INK, bd=0, highlightthickness=0)
        self.label.pack(side="left")

        for w in (self, self.box, self.label):
            w.bind("<Button-1>", self._toggle)
            w.bind("<Enter>", self._enter)
            w.bind("<Leave>", self._leave)
        self.configure(cursor="hand2", takefocus=True)
        self.bind("<Return>", lambda e: self._toggle())
        self.bind("<space>", lambda e: self._toggle())
        self.variable.trace_add("write", lambda *_: self._draw())
        self._draw()

    def _draw(self, hover: bool = False):
        checked = bool(self.variable.get())
        self.box.delete("all")
        border = INK if (checked or hover) else INK_FAINT
        fill = HILITE if checked else (PAPER_DEEP if hover else PAPER)
        self.box.create_rectangle(0, 0, 13, 13, fill=fill, outline=border)
        self.box.configure(bg=PAPER)
        if checked:
            draw_glyph(self.box, "check", 3, 3, 1, INK)
        self.label.configure(fg=INK if checked else INK_SOFT)

    def _enter(self, _e=None):
        self._draw(hover=True)

    def _leave(self, _e=None):
        self._draw(hover=False)

    def _toggle(self, _e=None):
        self.focus_set()
        self.variable.set(not self.variable.get())
        if self.command:
            self.command()


# --------------------------------------------------------------------- İlerleme çubuğu
class PixelProgressBar(tk.Frame):
    """Blok blok dolan, 1px çerçeveli piksel ilerleme çubuğu."""

    def __init__(
        self,
        master,
        height: int = 12,
        bar_color: str = SAGE_DEEP,
        bg_color: str = PAPER_DEEP,
        **kwargs,
    ):
        super().__init__(master, bg=PAPER_LINE, padx=BORDER_THIN, pady=BORDER_THIN, **kwargs)
        self.bar_color = bar_color
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

    def _redraw(self, _event=None):
        self.canvas.delete("all")
        width = self.canvas.winfo_width()
        height = self.canvas.winfo_height() or self.canvas_height
        if width <= 1 or height <= 1:
            return

        block, gap = 6, 2
        filled_ratio = self._percent / 100.0
        x = 0
        index = 0
        total_blocks = max(1, (width + gap) // (block + gap))
        filled_blocks = int(total_blocks * filled_ratio + 0.0001)

        while x < width:
            w = min(block, width - x)
            color = self.bar_color if index < filled_blocks else ""
            if color:
                self.canvas.create_rectangle(x, 1, x + w - 1, height - 1, fill=color, outline="")
            x += block + gap
            index += 1


# --------------------------------------------------------------------- Kart / rozet
class PixelCard(tk.Frame):
    """1px mürekkep çerçeveli düz kağıt kart (diyaloglar için)."""

    def __init__(
        self,
        master,
        bg: str = PAPER,
        border_color: str = INK,
        border_width: int = BORDER_THIN,
        padx: int = 12,
        pady: int = 12,
        **kwargs,
    ):
        super().__init__(master, bg=border_color, padx=border_width, pady=border_width, **kwargs)
        self.inner = tk.Frame(self, bg=bg, padx=padx, pady=pady)
        self.inner.pack(fill="both", expand=True)


class PixelBadge(tk.Frame):
    """Fosforlu kalemle işaretlenmiş küçük etiket."""

    def __init__(
        self,
        master,
        text: str,
        bg_color: str = HILITE,
        fg_color: str = INK,
        border_color: str = HILITE,
        **kwargs,
    ):
        super().__init__(master, bg=border_color, padx=BORDER_THIN, pady=BORDER_THIN, **kwargs)
        self.label = tk.Label(
            self,
            text=text,
            bg=bg_color,
            fg=fg_color,
            font=FONT_BADGE,
            padx=5,
            pady=1,
            bd=0,
            highlightthickness=0,
        )
        self.label.pack()

    def set_text(self, text: str, bg_color: Optional[str] = None):
        self.label.configure(text=text)
        if bg_color:
            self.label.configure(bg=bg_color)
            self.configure(bg=bg_color)


# --------------------------------------------------------------------- Önizleme kutusu
class PixelThumb(tk.Canvas):
    """Kesikli çerçeveli önizleme alanı; görsel gelince onu gösterir."""

    WIDTH = 118
    HEIGHT = 66

    def __init__(self, master, **kwargs):
        super().__init__(
            master,
            width=self.WIDTH,
            height=self.HEIGHT,
            bg=PAPER_DEEP,
            highlightthickness=0,
            bd=0,
            **kwargs,
        )
        self._photo = None
        self._empty()

    def _empty(self):
        self.delete("all")
        self.create_rectangle(
            1, 1, self.WIDTH - 2, self.HEIGHT - 2,
            outline=INK_FAINT, dash=(2, 2),
        )
        self.create_text(
            self.WIDTH // 2, self.HEIGHT // 2,
            text="önizleme yok",
            fill=INK_SOFT,
            font=(FONT_META[0], 8),
        )

    def set_photo(self, photo):
        self._photo = photo
        self.delete("all")
        self.create_image(self.WIDTH // 2, self.HEIGHT // 2, image=photo)

    def clear(self):
        self._photo = None
        self._empty()


# --------------------------------------------------------------------- Bölüm başlığı
class SectionHeading(tk.Frame):
    """Bölüm başlığı: büyük harf mono etiket + altında kelime boyu ince çizgi."""

    def __init__(self, master, text: str, bg: str = PAPER, **kwargs):
        super().__init__(master, bg=bg, **kwargs)
        self.text = text
        self._bg = bg

        self.label = tk.Label(
            self,
            text=text,
            font=FONT_LABEL,
            bg=bg,
            fg=INK,
            bd=0,
            highlightthickness=0,
        )
        self.label.pack(anchor="w")

        # Çizgi yalnızca etiket genişliğinde kalır (bölüm genişliğinde değil).
        self.underline = tk.Frame(self, bg=INK_SOFT, height=2)
        self.underline.pack(fill="x", pady=(2, 0))

    def set_text(self, text: str):
        self.label.configure(text=text)
