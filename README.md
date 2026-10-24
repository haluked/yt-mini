# ff-dlp-viewer

> Windows için hafif, sistem tepsisinde (tray) yaşayan, yt-dlp tabanlı video ve ses indirme aracı.

[![Python](https://img.shields.io/badge/Python-3.11+-4B8BBE?style=flat-square&logo=python&logoColor=white)](https://www.python.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-7DBA84?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows_10_|_11-78A1BB?style=flat-square&logo=windows)](https://microsoft.com/windows)
[![Tests](https://img.shields.io/badge/Tests-42_Passed-success?style=flat-square)](tests/)

---

## Ekran Görüntüsü

```
+--------------------------------------------------------------+
| ff-dlp-viewer                                        [YOUTUBE] |
+--------------------------------------------------------------+
| [ https://www.youtube.com/watch?v=... ] [Yapıştır] [Bilgi Getir]
+--------------------------------------------------------------+
| +----------+  Örnek Video Başlığı 4K                         |
| |  [Öniz]  |  Süre: 04:35 | Platform: YouTube                |
| +----------+  Seçim: AV1+OPUS -> .webm [Stream Copy]         |
+--------------------------------------------------------------+
| [ [Video İndir] ]  [ Sadece Ses ]                            |
| +----------------------------------------------------------+ |
| | Çözünürlük: [ En İyi (Otomatik) v ]                      | |
| | [ ] MP4 Olarak Kaydet                                    | |
| +----------------------------------------------------------+ |
| [======================================            ] %64.2   |
| %64.2 | 12.4 MB/s | Kalan: 18 sn               45 MB / 70 MB |
| [ İNDİR ]  [ İPTAL ]                         [ Klasörü Aç ]  |
+--------------------------------------------------------------+
```

---

## Öne Çıkan Özellikler

- **Ultra Düşük Bellek (RAM) ve CPU Tüketimi**:
  - Ağır kütüphaneler yerine saf Python Tkinter tabanlı özel piksel arayüz.
  - Pencere açıkken ~55 MB, sistem tepsisine küçültüldüğünde nesneler serbest bırakılır ve Windows Working Set trimming ile RAM ayak izi minimize edilir.
- **Sistem Tepsisi Yaşam Döngüsü (WhatsApp / Steam Modeli)**:
  - Pencere kapatıldığında arka planda sessizce bekler, çalışmayı kesmez.
  - Tepsi ikonuna sol tık ile pencere öne gelir; sağ tık ile güncelleme denetimi veya çıkış yapılır.
  - İndirme devam ederken kazara çıkışları önlemek için onay penceresi devreye girer.
- **Deterministik ve Kaynağa Saygılı Kodek Seçimi (No Needless Re-encode)**:
  - Matematiksel kodek önceliği: Video için `AV1 > VP9 > H.265 > H.264`, Ses için `Opus > AAC > MP3`.
  - Kaynak ne sunuyorsa (WebM, MP4, MKV) doğrudan kayıpsız akış kopyalama (stream copy / remux) yapılır.
  - "MP4 olarak kaydet" seçildiğinde, gerekiyorsa kullanıcıya açık uyarı gösterilir.
- **Modüler Platform Tanıyıcı**:
  - YouTube, TikTok, Instagram, X (Twitter) bağlantılarını tanır; TikTok ve Instagram'ın yerel birleşik H.264+AAC akışlarını doğrudan kopyalar.
- **Güvenli ve Sağlam Mimari**:
  - yt-dlp doğrudan Python kütüphane API'siyle çağrılır (komut satırı enjeksiyonu riski yoktur).
  - Dosya adları Windows dosya sistemi yasak karakterlerine (`< > : " / \ | ? *`) ve `MAX_PATH` sınırına karşı temizlenir.
  - Windows adlandırılmış Mutex ile tekil örnek garantisi (aynı anda çift kopya açılmaz).

---

## Son Kullanıcı Kurulumu

1. [GitHub Releases](https://github.com/haluk/ff-dlp-viewer/releases) sayfasından en son `ff-dlp-viewer-Setup-v1.0.0.exe` dosyasını indirin.
2. Kurulumu çalıştırın.
   - Kurulum `%LOCALAPPDATA%\Programs\ff-dlp-viewer` dizinine yapılır; **yönetici izni (UAC) gerektirmez**.
   - Python, pip veya ffmpeg kurmanıza gerek yoktur; tüm bileşenler paketin içinde hazır gelir.
   - İsteğe bağlı olarak "Windows açıldığında sistem tepsisinde otomatik başlat" seçeneğini işaretleyebilirsiniz.

### Windows SmartScreen Uyarısı Hakkında
> [!NOTE]
> Kurulumu çalıştırdığınızda Windows SmartScreen *"Windows kişisel bilgisayarınızı korudu"* uyarısı verebilir.
> **Nedeni**: Yıllık yüzlerce dolar maliyeti olan kurumsal dijital kod imzalama sertifikası (Code Signing Certificate) bulunmayan tüm bağımsız açık kaynaklı yazılımlar, Windows tarafından "bilinmeyen yayımcı" olarak sınıflandırılır.
> **Çözüm**: *"Ek bilgi"* linkine tıklayıp *"Yine de çalıştır"* butonuna basarak güvenle kurabilirsiniz. Kaynak kodlar burada tamamen açıktır ve her sürümün SHA-256 sağlama özeti GitHub Releases sayfasında yayınlanır.

---

## Geliştirici Kılavuzu

### 1. Ortam Kurulumu
Depoyu klonlayıp geliştirici ortamını tek komutla kurabilirsiniz:
```cmd
git clone https://github.com/haluk/ff-dlp-viewer.git
cd ff-dlp-viewer
dev_setup.bat
```
*(veya manuel olarak:)*
```cmd
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```

### 2. Testleri Çalıştırma
```cmd
python -m pytest tests/
```
*42 adet birim testi platform tespiti, sanitization, saf kodek seçimi, tray ve arayüz başlatmasını doğrular.*

### 3. Uygulamayı Kaynaktan Başlatma
```cmd
python main.py
```

### 4. Tek Komutla Paketleme ve Installer Üretimi
```cmd
build.bat
```
veya
```cmd
python installer/build.py
```
*Bu komut PyInstaller ile bağımsız uygulamayı derler, `bin/ffmpeg.exe` dosyasını hazırlar, Inno Setup ile `dist/ff-dlp-viewer-Setup-v1.0.0.exe` dosyasını ve `.sha256` özetini üretir.*

---

## Proje Klasör Yapısı

```
ff-dlp-viewer/
├── core/                       # Çekirdek iş mantığı
│   ├── detector.py             # URL platform tespiti
│   ├── codec_selector.py       # Saf fonksiyonel kodek/konteyner seçici
│   ├── downloader.py           # yt-dlp Python API indirme motoru
│   ├── sanitization.py         # Dosya adı ve yol güvenlik filtreleri
│   └── updater.py              # GitHub Releases API ve SHA-256 doğrulayıcı
├── ui/                         # Piksel kullanıcı arayüzü
│   ├── app_window.py           # Ana Tkinter penceresi
│   ├── theme.py                # Pastel renk paleti ve tipografi
│   ├── widgets.py              # Keskin piksel buton, kart ve ilerleme çubuğu
│   ├── dialogs.py              # Hata ve onay pencereleri
│   └── strings.py              # Merkezi Türkçe metinler
├── tray/                       # Sistem tepsisi yönetimi
│   └── tray_manager.py         # pystray entegrasyonu ve piksel tepsi ikonu
├── tests/                      # Birim testleri ve mock format JSON'ları
├── installer/                  # Inno Setup (.iss) ve derleme scriptleri
├── main.py                     # Uygulama başlangıç noktası (Mutex & Döngü)
├── PRODUCT.md                  # Ürün şartnamesi
├── DESIGN.md                   # Tasarım şartnamesi
├── ARCHITECTURE.md             # Mimari şartnamesi
├── LICENSE                     # MIT ve üçüncü taraf lisansları
└── requirements.txt            # Sabit bağımlılık listesi
```

---

## Lisans

Bu proje [MIT Lisansı](LICENSE) ile lisanslanmıştır. Dahili kullanılan `yt-dlp` (The Unlicense) ve `FFmpeg` (LGPL v2.1+) kütüphanelerinin lisans şartlarına saygı duyulur.
