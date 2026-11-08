# ff-dlp-viewer-light: Ultra-Lightweight Native C Win32 Mimarisi (< 1 MB)

Bu dal (`light`), Python/PyInstaller çalışma zamanı şişkinliğini (bloat) tamamen ortadan kaldırmak ve uygulamayı **saf C (C11) / yerel Win32 API** ile yeniden inşa etmek amacıyla oluşturulmuştur.

---

## 1. Mimari Prensipler ve Tasarım Kararları

1. **Sıfır Ek Çalışma Zamanı (Zero Runtime Bundles):**
   * PyInstaller, Electron, .NET veya Qt bağımlılıkları tamamen kaldırılmıştır.
   * Yalnızca Windows işletim sisteminin yerel DLL kütüphaneleri dinamik bağlanır (`USER32.dll`, `GDI32.dll`, `COMCTL32.dll`, `SHELL32.dll`, `ole32.dll`, `msvcrt.dll`, `KERNEL32.dll`).
   * Harici MinGW çalışma zamanları (`libgcc_s_seh`, `libwinpthread`) `-static` bayrağı ile engellenmiştir.

2. **Dosya Boyutu ve Performans:**
   * **Hedef:** < 1 MB (İdeal: 150 KB – 300 KB).
   * **Gerçekleşen Boyut:** **~337 KB** (345.088 bayt).
   * **Boşta RAM Ayak İzi:** < 10 MB (Python sürümündeki 60-70 MB'a kıyasla ~%85 tasarruf).
   * **Açılış Süresi:** < 15 ms (neredeyse anında).

3. **Modüler Çekirdek (Arayüzden Bağımsız C Katmanı):**
   * `src/detector.c`: YouTube, TikTok, Instagram, X/Twitter ve Genel Web adreslerini tanıyan platform tespit motoru.
   * `src/codec_selector.c`: Deterministik kodek/format matrisi (AV1 > VP9 > H.265 > H.264 ve Opus > AAC > MP3) ve stream copy / remux karar motoru.
   * `src/sanitization.c`: Windows dosya adı kısıtlamaları (yasaklı karakterler, CON/PRN/AUX ayrılmış adları, path traversal engelleme, uzunluk sınırlaması).
   * `src/cjson.c`: Tek dosyalık ultra-hafif JSON ayrıştırıcı (meta verileri ve format ağacını ayrıştırmak için).
   * `src/engine.c`: Asenkron iş parçacığı (`CreateThread`), GUI'yi kilitlemeyen gizli boru hattı (`CreatePipe` / `ReadFile`), yüzde/hız/kalan süre ayrıştırıcısı ve iptal mekanizması.

4. **Game Boy Esintili 8-bit Arayüz (`gb_ui.c` / `ui.c`):**
   * Klasik DMG paleti: dört yeşil ton (`#0F380F`, `#306230`, `#8BAC0F`, `#9BBC0F`); ara tonlar 1 px dama deseniyle.
   * `BS_OWNERDRAW` butonlar, seçim çipleri, onay kutusu, özel açılır liste, segmentli ilerleme çubuğu, kendi başlık çubuğu ve modal mesaj kutusu.
   * `Consolas` sistem fontu + `NONANTIALIASED_QUALITY`; harici font/görsel dosyası yok.
   * Modern COM `IFileOpenDialog` (`FOS_PICKFOLDERS`) ile klasör seçimi.
   * Sürükle & Bırak (`WM_DROPFILES`) desteği.
   * Sistem tepsisi (`Shell_NotifyIconW`) entegrasyonu.

---

## 2. Derleme Komutu

MinGW-w64 GCC ile tek komutla derleme:

```cmd
build.bat
```

veya manuel olarak:

```bash
windres res\res.rc -O coff -o res.o

gcc -O3 -s -flto \
    -ffunction-sections -fdata-sections \
    -Wl,--gc-sections \
    -static -mwindows -municode \
    -Isrc \
    src\main.c src\ui.c src\gb_ui.c src\engine.c src\codec_selector.c src\detector.c src\sanitization.c src\cjson.c res.o \
    -o dist\ff-dlp-viewer.exe \
    -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -luser32 -lurlmon
```

---

## 3. Doğrulama ve Testler

C çekirdek birim testlerini derleyip çalıştırmak için:

```cmd
gcc -O2 -Isrc src/detector.c src/sanitization.c src/codec_selector.c src/cjson.c tests/test_core.c -o tests/test_core.exe
tests\test_core.exe
```

Test çıktısı:
```
==========================================
RUNNING FF-DLP-VIEWER LIGHT C UNIT TESTS
==========================================
[TEST] Detector testing...
  -> Detector tests PASSED!
[TEST] Sanitization testing...
  -> Sanitization tests PASSED!
[TEST] Codec Selector testing...
  -> Codec Selector tests PASSED!

ALL CORE C TESTS PASSED WITH 100% SUCCESS!
```
