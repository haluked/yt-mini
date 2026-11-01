# yt-mini (v2.0)

> Windows için hafif, sistem tepsisinde (tray) yaşayan, yt-dlp ve ffmpeg destekli video ve ses indirme aracı.

[![Python](https://img.shields.io/badge/Python-3.11+-4B8BBE?style=flat-square&logo=python&logoColor=white)](https://www.python.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-8CBE9C?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows_10_|_11-93BCD2?style=flat-square&logo=windows)](https://microsoft.com/windows)
[![Tests](https://img.shields.io/badge/Tests-44_Passed-success?style=flat-square)](tests/)

---

## Ekran Görüntüsü

"Kırtasiye defteri" düzeni: sıcak kağıt zemin, solda defterin kırmızı kenar çizgisi,
fosforlu kalem sarısı yalnızca **seçili** olanı işaretler.

```
+--------------------------------------------------------------+
| [yt]  yt-mini                                    [ YOUTUBE ] |
|       Hafif Video & Ses İndirici (v2.0)                      |
+--------------------------------------------------------------+
|  BAĞLANTI                                                    |
|  -----------                                                 |
|  [ Bağlantıyı buraya yapıştırın...   ] [Yapıştır] [Bilgi Getir]
|  YouTube · Instagram · TikTok · X bağlantıları desteklenir    |
|  ----------------------------------------------------------- |
|  MEDYA                                                       |
|  -----------                                                 |
|  +------------+  Test YouTube 4K Video                       |
|  | önizleme   |  YouTube · 03:35 · 5 kalite                   |
|  +------------+  AV1 + OPUS -> .webm · kopyalama              |
|  [Video] [Sadece Ses]                                        |
|  Kalite  [En İyi] [2160p] [1080p] [720p] [480p]              |
|  [x] MP4 olarak kaydet                                       |
|  i  Akış olduğu gibi kopyalanır, yeniden kodlanmaz.           |
|  ----------------------------------------------------------- |
|  [|||||||||||||||||||||||||||                 ]               |
|  %64 · 12.4 MB/s                         45.0 MB / 70.0 MB   |
|  [ İndir ]  [ İptal ]                       [ Klasörü Aç ]    |
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
  - PyPI ve GitHub API entegrasyonuyla yt-dlp motorunun ve uygulamanın güncelliğini sistem tepsisinden tek tıkla denetleme.
  - Dosya adları Windows dosya sistemi yasak karakterlerine (`< > : " / \ | ? *`) ve `MAX_PATH` sınırına karşı temizlenir.
  - Windows adlandırılmış Mutex ile tekil örnek garantisi (aynı anda çift kopya açılmaz).

---

## Son Kullanıcı Kurulumu

1. [GitHub Releases](https://github.com/haluked/yt-mini/releases) sayfasından en son `yt-mini-Setup-v2.0.0.exe` dosyasını indirin.
2. Kurulumu çalıştırın.
   - Kurulum `%LOCALAPPDATA%\Programs\yt-mini` dizinine yapılır; **yönetici izni (UAC) gerektirmez**.
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
git clone https://github.com/haluked/yt-mini.git
cd yt-mini
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
*44 adet birim testi platform tespiti, sanitization, saf kodek seçimi, yt-dlp & tray güncelleyici ve arayüz başlatmasını doğrular.*

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
*Bu komut PyInstaller ile bağımsız uygulamayı derler, `bin/ffmpeg.exe` dosyasını hazırlar, Inno Setup ile `dist/yt-mini-Setup-v2.0.0.exe` dosyasını ve `.sha256` özetini üretir.*

---

## Proje Klasör Yapısı

```
yt-mini/
├── core/                       # Çekirdek iş mantığı
│   ├── detector.py             # URL platform tespiti
│   ├── codec_selector.py       # Saf fonksiyonel kodek/konteyner seçici
│   ├── downloader.py           # yt-dlp Python API indirme motoru
│   ├── sanitization.py         # Dosya adı ve yol güvenlik filtreleri
│   └── updater.py              # GitHub Releases ve PyPI yt-dlp API güncelleyici & SHA-256 doğrulayıcı
├── ui/                         # Piksel kullanıcı arayüzü
│   ├── app_window.py           # Ana Tkinter penceresi
│   ├── theme.py                # Kağıt/mürekkep renk paleti ve tipografi
│   ├── widgets.py              # Buton, giriş, çip, onay kutusu, ilerleme çubuğu
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
