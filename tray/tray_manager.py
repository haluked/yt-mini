"""Windows Sistem Tepsisi (Tray) Yöneticisi.

- WhatsApp / Steam benzeri arka plan yaşam döngüsü
- Özel çizimli piksel tepsi ikonu (32x32)
- Sol tık ile pencereyi geri getirme
- Sağ tık menüsü (Aç, Güncellemeleri Denetle, Tamamen Çık)
"""

from __future__ import annotations
import threading
from typing import Callable, Optional

from PIL import Image, ImageDraw
import pystray
from pystray import MenuItem as item

from ui.strings import (
    APP_NAME,
    TRAY_CHECK_UPDATES,
    TRAY_DOWNLOADING_TOOLTIP,
    TRAY_EXIT,
    TRAY_IDLE_TOOLTIP,
    TRAY_OPEN,
)


def create_pixel_tray_icon(is_active: bool = False) -> Image.Image:
    """32x32 boyutunda, retro piksel-art indirme ikonu üretir."""
    img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    border_color = (45, 43, 42, 255)       # #2D2B2A Kömür
    fill_color = (125, 186, 132, 255) if is_active else (120, 161, 187, 255) # Pastel Yeşil veya Mavi
    sunken_color = (234, 229, 220, 255)   # #EAE5DC

    # Alt disk/tepsi gövdesi
    draw.rectangle([4, 20, 27, 27], fill=sunken_color, outline=border_color, width=2)
    # Tepsi ön çentiği
    draw.line([12, 20, 19, 20], fill=fill_color, width=2)

    # İndirme oku (piksel ok)
    # Dikey gövde
    draw.rectangle([13, 5, 18, 14], fill=fill_color, outline=border_color, width=2)
    # Ok başı üçgeni
    arrow_points = [(8, 14), (23, 14), (16, 21)]
    draw.polygon(arrow_points, fill=fill_color, outline=border_color)

    return img


class TrayManager:
    """Sistem tepsisi ikonu ve menü yönetim sınıfı."""

    def __init__(
        self,
        on_open_clicked: Callable[[], None],
        on_check_updates_clicked: Callable[[], None],
        on_exit_clicked: Callable[[], None],
    ):
        self.on_open_clicked = on_open_clicked
        self.on_check_updates_clicked = on_check_updates_clicked
        self.on_exit_clicked = on_exit_clicked

        self._icon: Optional[pystray.Icon] = None
        self._thread: Optional[threading.Thread] = None

    def start(self):
        """Tray ikonunu ayrı bir arka plan thread'inde başlatır."""
        menu = pystray.Menu(
            item(TRAY_OPEN, self._on_open, default=True),
            pystray.Menu.SEPARATOR,
            item(TRAY_CHECK_UPDATES, self._on_check_updates),
            pystray.Menu.SEPARATOR,
            item(TRAY_EXIT, self._on_exit),
        )

        self._icon = pystray.Icon(
            name=APP_NAME,
            icon=create_pixel_tray_icon(is_active=False),
            title=TRAY_IDLE_TOOLTIP,
            menu=menu,
        )

        def _run():
            self._icon.run()

        self._thread = threading.Thread(target=_run, daemon=True)
        self._thread.start()

    def set_downloading_state(self, is_downloading: bool):
        """İndirme durumuna göre tepsi ikonunu ve açıklamasını günceller."""
        if self._icon:
            self._icon.icon = create_pixel_tray_icon(is_active=is_downloading)
            self._icon.title = TRAY_DOWNLOADING_TOOLTIP if is_downloading else TRAY_IDLE_TOOLTIP

    def stop(self):
        """Tray ikonunu durdurur ve sistem tepsisinden temizler."""
        if self._icon:
            self._icon.stop()

    def _on_open(self, icon=None, item=None):
        self.on_open_clicked()

    def _on_check_updates(self, icon=None, item=None):
        self.on_check_updates_clicked()

    def _on_exit(self, icon=None, item=None):
        self.on_exit_clicked()
