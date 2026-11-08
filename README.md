# ff-dlp-viewer (v2.0 Native Light)

> Windows için ultra hafif (< 1 MB), çalışma zamanı gerektirmeyen (sıfır Python/Electron), yerel Win32 GUI tabanlı video ve ses indirme aracı.

[![C11](https://img.shields.io/badge/Language-C11-00599C?style=flat-square&logo=c)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/Platform-Windows_10_|_11-0078D6?style=flat-square&logo=windows)](https://microsoft.com/windows)
[![Binary Size](https://img.shields.io/badge/Binary_Size-~340_KB-brightgreen?style=flat-square)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-8CBE9C?style=flat-square)](LICENSE)

---

## Genel Bakış

**ff-dlp-viewer**, Python çalışma zamanı, PyInstaller veya Electron gibi ağır bağımlılıkları tamamen ortadan kaldırarak saf C11 ve yerel Windows API (`user32.dll`, `gdi32.dll`, `comctl32.dll`, `shell32.dll`, `ole32.dll`) kullanılarak sıfırdan yeniden yazılmıştır.

Nihai çalıştırılabilir dosya boyutu **yalnızca ~340 KB** olup, bellek tüketimi 10-15 MB civarındadır.

---

## Öne Çıkan Özellikler

- **Ultra Hafif ve Sıfır Runtime (< 1 MB)**:
  - Harici DLL bağımlılığı yoktur; doğrudan Windows yerel sistem kütüphanelerine statik olarak bağlanır.
  - Hızlı açılış, sıfır gecikme ve minimum kaynak kullanımı (RAM: ~12 MB).

- **Game Boy Esintili 8-bit Arayüz**:
  - Klasik DMG paleti: yalnızca 4 ton yeşil (`#0F380F`, `#306230`, `#8BAC0F`, `#9BBC0F`).
  - Tüm butonlar, seçim çipleri, onay kutusu, ilerleme çubuğu, açılır listeler, bildirim ve onay pencereleri özel çizilir; harici görsel veya font dosyası yoktur.
  - Kendi başlık çubuğu (sürükle / küçült / kapat), keskin köşeler, 2 px sert gölgeler. Gradient, blur ve parlama yoktur.
  - Sistem fontu `Consolas`, anti-aliasing kapalı; Türkçe karakterler tam desteklenir.
  - Sabit 646×485 tasarım, Per-Monitor V2 DPI ile ölçeklenir.

- **Tek Tıkla Otomatik İndirme (Auto-Fetch & Download)**:
  - Bağlantı yapıştırıldığında arka planda otomatik bilgi sorgulama.
  - Bilgi henüz çekilmemiş olsa bile "İNDİR" butonuna basıldığında önce otomatik bilgi çekip ardından indirmeyi başlatma.
  - Doğrudan medya dosyaları (`.mp4`, `.webm` vb.) ve tekil video/ses akışları için kusursuz geri dönüş (fallback) desteği.

- **Klavye ve Erişilebilirlik Odaklı**:
  - `Tab` / `Shift+Tab` ile tüm kontroller arasında eksiksiz gezinme (`WS_TABSTOP` ve `IsDialogMessageW`).
  - `Enter`: URL kutusundayken önce bilgi çekip indirmeyi başlatır.
  - `Esc`: Devam eden işlemi iptal eder veya pencereyi kapatır.

- **Akıllı Süreç Yönetimi (Job Object)**:
  - İptal durumunda yetim `yt-dlp` veya `ffmpeg` süreçleri kalmaz; Windows Job Object sınırları ile tüm süreç ağacı anında ve temiz şekilde sonlandırılır.

- **Kayıt Defteri & Klasör Kalıcılığı**:
  - Seçilen indirme dizini `HKCU\Software\ff-dlp-viewer\DownloadDir` altında saklanır ve bir sonraki açılışta korunur.
  - Modern COM `IFileOpenDialog` ile klasör seçimi.

- **Steam/WhatsApp Tarzı Tepsi Yaşam Döngüsü**:
  - `X` uygulamayı kapatmaz; pencereyi sistem tepsisine gizler ve arka planda çalışmaya devam eder.
  - Tam çıkış: tepsi simgesine sağ tık → `Çıkış`. İndirme sürüyorsa önce onay ister.
  - Kısayol tekrar açılırsa yeni kopya başlatılmaz; gizli pencere öne getirilir.
  - İlk kapatmada bir kerelik "arka planda çalışıyor" bilgilendirmesi gösterilir.

- **Tekil Örnek (Single Instance) ve Komut Satırı Entegrasyonu**:
  - Named Mutex (`Local\ff-dlp-viewer-light`) ile aynı anda birden fazla kopya açılmaz.
  - `--url <URL>` veya `--url=URL` ile ikinci bir örnek çağrıldığında bağlantı açık olan pencereye `WM_COPYDATA` ile aktarılır ve pencere öne getirilir.

---

## Klavye Kısayolları

| Kısayol | Eylem |
|---|---|
| `Enter` | URL kutusundayken indirmeyi başlatır (bilgi çekilmemişse önce bilgiyi alıp indirir). |
| `Esc` | Aktif bir işlem varsa iptal eder, yoksa pencereyi tepsiye gizler. |
| `Tab` / `Shift+Tab` | Arayüz kontrolleri arasında sırayla gezinir. |
| `Space` | Odaklanan butona tıklar veya radyo/onay kutusunu değiştirir. |

---

## Komut Satırı Kullanımı

```cmd
# Grafik arayüzü normal başlatır
ff-dlp-viewer.exe

# Belirtilen bağlantıyı açıp otomatik olarak bilgilerini getirir
ff-dlp-viewer.exe --url "https://www.youtube.com/watch?v=..."

# Kısa yardım ve sürüm pencereleri
ff-dlp-viewer.exe --help
ff-dlp-viewer.exe --version
```

---

## Geliştirici & Derleme Kılavuzu

### Gereksinimler
- **MinGW-w64 GCC** (C11 destekli)
- **windres** (GNU Resource Compiler)

### 1. Derleme
Projeyi tek komutla derlemek için:
```cmd
build.bat
```

Veya doğrudan derleyici komutu:
```cmd
windres res\res.rc -O coff -o res.o
gcc -O3 -s -flto -ffunction-sections -fdata-sections -Wl,--gc-sections -static -mwindows -municode -Isrc src\main.c src\ui.c src\gb_ui.c src\engine.c src\codec_selector.c src\detector.c src\sanitization.c src\cjson.c res.o -o dist\ff-dlp-viewer.exe -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -luser32 -lurlmon
```

### 2. Birim Testleri
Çekirdek algoritmaları (platform dedektörü, dosya adı sanitizasyonu, kodek seçimi ve doğrudan medya yönetimi) test etmek için:
```cmd
gcc -O2 -Isrc src/detector.c src/sanitization.c src/codec_selector.c src/cjson.c tests/test_core.c -o tests/test_core.exe
.\tests\test_core.exe
```

---

## Lisans

Bu proje [MIT Lisansı](LICENSE) ile lisanslanmıştır. Dahili kullanılan `yt-dlp` (The Unlicense) ve `FFmpeg` (LGPL v2.1+) kütüphanelerinin lisans şartlarına saygı duyulur.
