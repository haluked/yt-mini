"""ff-dlp-viewer ana uygulama başlangıç noktası.

- Windows tekil örnek kontrolü (Named Mutex)
- Sistem tepsisi (pystray) ve Tkinter yaşam döngüsü senkronizasyonu
- WhatsApp / Steam benzeri tray davranışı
"""

from __future__ import annotations
import ctypes
import os
import sys
import tkinter as tk

from core.downloader import DownloadEngine
from core.updater import UpdateInfo, check_updates_async, CURRENT_APP_VERSION
from tray.tray_manager import TrayManager
from ui.app_window import AppWindow
from ui.dialogs import show_confirm, show_error, show_warning


MUTEX_NAME = "Global\\ff_dlp_viewer_single_instance_mutex"


def acquire_single_instance_mutex():
    """Windows Named Mutex ile uygulamanın tekil çalışmasını sağlar."""
    if sys.platform != "win32":
        return None

    kernel32 = ctypes.windll.kernel32
    mutex = kernel32.CreateMutexW(None, False, MUTEX_NAME)
    last_error = kernel32.GetLastError()

    # ERROR_ALREADY_EXISTS = 183
    if last_error == 183:
        kernel32.CloseHandle(mutex)
        return None

    return mutex


def release_single_instance_mutex(mutex):
    """Mutex kaynağını serbest bırakır."""
    if mutex and sys.platform == "win32":
        ctypes.windll.kernel32.CloseHandle(mutex)


def main():
    # 1. Tekil örnek kontrolü
    mutex = acquire_single_instance_mutex()
    if not mutex and sys.platform == "win32":
        # Halihazırda bir kopya çalışıyor
        sys.exit(0)

    start_in_tray = "--tray" in sys.argv

    # 2. Çekirdek motorlar
    engine = DownloadEngine()

    tray_mgr: TrayManager | None = None

    def on_minimize_to_tray():
        app.hide_to_tray()

    def on_open_from_tray():
        app.after(0, app.restore_from_tray)

    def on_check_updates_from_tray():
        def _check_callback():
            app.restore_from_tray()
            app.status_label.configure(text="Güncellemeler denetleniyor...")

            def _on_result(info: UpdateInfo):
                def _ui():
                    app.status_label.configure(text=f"Mevcut Sürüm: v{CURRENT_APP_VERSION}")
                    if info.has_update:
                        msg = (
                            f"Yeni bir sürüm mevcut!\n\n"
                            f"Mevcut: v{info.current_version}\n"
                            f"Yeni: v{info.latest_version}\n\n"
                            f"Yenilikler:\n{info.release_notes[:300]}\n\n"
                            f"İndirme bağlantısı: {info.download_url or 'GitHub Releases'}"
                        )
                        show_warning(app, "Güncelleme Mevcut", msg)
                    else:
                        show_warning(
                            app,
                            "Güncelleme Denetimi",
                            f"ff-dlp-viewer en güncel sürümde (v{CURRENT_APP_VERSION}).",
                        )

                app.after(0, _ui)

            check_updates_async(_on_result)

        app.after(0, _check_callback)

    def on_exit_app():
        if tray_mgr:
            tray_mgr.stop()
        release_single_instance_mutex(mutex)
        app.destroy()
        sys.exit(0)

    # 3. Ana Arayüz Penceresi
    app = AppWindow(
        engine=engine,
        on_minimize_to_tray=on_minimize_to_tray,
        on_exit_app=on_exit_app,
    )

    # 4. Tray Yöneticisi
    tray_mgr = TrayManager(
        on_open_clicked=on_open_from_tray,
        on_check_updates_clicked=on_check_updates_from_tray,
        on_exit_clicked=lambda: app.after(0, app.confirm_and_exit),
    )
    tray_mgr.start()

    if start_in_tray:
        app.hide_to_tray()

    try:
        app.mainloop()
    finally:
        if tray_mgr:
            tray_mgr.stop()
        release_single_instance_mutex(mutex)


if __name__ == "__main__":
    main()
