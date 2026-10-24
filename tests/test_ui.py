"""Kullanıcı arayüzü başlatma ve bileşen testleri."""

import pytest
import tkinter as tk
from core.downloader import DownloadEngine
from ui.app_window import AppWindow
from ui.widgets import PixelButton, PixelEntry, PixelProgressBar


def test_ui_initialization_and_widgets():
    engine = DownloadEngine()
    try:
        app = AppWindow(engine)
    except tk.TclError:
        pytest.skip("Grafik ekranı bulunamayan ortam (headless)")
        return

    # Başlık ve başlangıç durumu kontrolü
    assert "ff-dlp-viewer" in app.title()
    assert app.url_entry is not None
    assert app.btn_download is not None

    # Sekme geçişi testi
    app.switch_tab("audio")
    assert app.active_tab == "audio"
    app.switch_tab("video")
    assert app.active_tab == "video"

    # Pencereyi kapatıp serbest bırak
    app.destroy()
