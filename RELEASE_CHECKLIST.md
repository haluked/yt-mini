# Yayın Öncesi Kontrol Listesi (Pre-Release Checklist)

ff-dlp-viewer GitHub'a veya son kullanıcılara yayınlanmadan önce doğrulanması gereken kontrol listesi:

- [x] **Tasarım ve Arayüz Denetimi (Impeccable & Pixel Estetiği)**:
  - Keskin köşeler (0 radius), 2px katı kenarlıklar uygulandı.
  - Pastel renk paleti (`#F4F1EA`, `#7DBA84`, `#78A1BB`, `#E5B869`, `#DE7A68`) uygulandı.
  - Gradient, blur, glow ve emoji içermeyen sade retro masaüstü dili korundu.

- [x] **Kod Kalitesi ve Birim Testleri**:
  - `python -m pytest tests/` ile 42 birim testinin tamamı başarıyla geçiyor (`42 passed in 0.24s`).
  - YouTube 4K, 1080p, TikTok, Instagram ve X format JSON örnekleriyle saf kodek seçici test edildi.
  - Dosya sanitization (Windows yasak karakterler, path traversal, MAX_PATH) doğrulandı.

- [x] **Bellek ve Performans Testi**:
  - Windows Working Set bellek tüketimi ölçüldü.
  - `yt_dlp` import'ları lazy hale getirildi.
  - Sistem tepsisine küçültüldüğünde nesneler serbest bırakılıyor ve `SetProcessWorkingSetSize` ile bellek optimize ediliyor.

- [x] **Sistem Tepsisi (Tray) Davranışı**:
  - Pencere kapatıldığında (`WM_DELETE_WINDOW`) uygulama kapanmıyor, tray'de sessizce bekliyor.
  - Çift/sol tık pencereyi öne getiriyor.
  - İndirme sürerken çıkış istendiğinde kullanıcıdan açık onay isteniyor.

- [x] **Güvenlik ve İzolasyon**:
  - Subprocess yerine doğrudan `yt_dlp` Python kütüphane API'si kullanılıyor.
  - ffmpeg ikilisi bağımsız olarak uygulamanın kendi `bin/` klasörüne yerleşiyor, sistem `PATH` değiştirilmiyor.
  - Windows Tekil Örnek (Named Mutex) ile çift kopya açılması engelleniyor.

- [x] **Kurulum ve Paketleme**:
  - `installer/setup.iss` ile Inno Setup kurulum scripti hazırlandı (Türkçe dil, `%LOCALAPPDATA%` kurulumu, yönetici izni gerektirmeme).
  - `installer/build.py` ve `build.bat` ile tek tıkla PyInstaller + Inno Setup derleme hattı hazırlandı.
  - Geliştirici için `dev_setup.bat` ve `requirements.txt` hazırlandı.

- [x] **Lisans Uyumluluğu**:
  - `LICENSE` dosyası oluşturuldu: MIT Lisansı + yt-dlp (The Unlicense) ve FFmpeg (LGPL v2.1+) bağımsız binary kullanım şartları belirtildi.

- [ ] **Kullanıcı Onayı & GitHub Push**:
  - Haluk tarafından onaylanana kadar GitHub'a hiçbir push işlemi yapılmayacaktır.
