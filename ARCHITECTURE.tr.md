# Sistem Mimarisi Şartnamesi (ARCHITECTURE.md)

## 1. Genel Mimari Bakış

ff-dlp-viewer, yüksek modülerliğe sahip, katmanlı ve olay güdümlü (event-driven) bir mimari ile tasarlanmıştır. Çekirdek iş mantığı (indirme, kodek analizi, platform tespiti) kullanıcı arayüzünden tamamen bağımsızdır ve saf Python fonksiyonları/sınıfları olarak birim testleriyle doğrulanabilir.

```
                    ┌─────────────────────────┐
                    │    Kullanıcı Arayüzü    │
                    │   (Tkinter Pixel UI)    │
                    └───────────┬─────────────┘
                                │ Olaylar / Kuyruk (Thread-Safe Queue)
                                ▼
 ┌──────────────────────────────────────────────────────────────────┐
 │                         Uygulama Kontrolcüsü                     │
 │                        (AppCoordinator / State)                  │
 └──────┬───────────────────────┬───────────────────────────┬───────┘
        │                       │                           │
        ▼                       ▼                           ▼
┌───────────────┐       ┌───────────────┐           ┌───────────────┐
│ Platform &    │       │ Saf Kodek     │           │ İndirme       │
│ URL Tanıyıcı  │       │ Seçici        │           │ Yöneticisi    │
│ (extractor)   │       │ (codec_logic) │           │ (engine)      │
└───────┬───────┘       └───────┬───────┘           └───────┬───────┘
        │                       │                           │
        │                       │                           ▼
        │                       │                   ┌───────────────┐
        │                       │                   │ yt-dlp Python │
        │                       │                   │ Kütüphane API │
        │                       │                   └───────┬───────┘
        │                       │                           │
        │                       │                           ▼
        │                       │                   ┌───────────────┐
        │                       │                   │ ffmpeg        │
        │                       │                   │ (Gömülü CLI)  │
        └───────────────────────┴───────────────────┴───────────────┘
                                │
                                ▼
                    ┌───────────────────────┐
                    │   Windows Tray &      │
                    │   Yaşam Döngüsü       │
                    │   (pystray + WinAPI)  │
                    └───────────────────────┘
```

---

## 2. Modül Yapısı ve Sorumluluklar

Proje dizini aşağıdaki gibi modüler paket yapısında kurgulanmıştır:

```
ff-dlp-viewer/
│
├── core/                       # Çekirdek mantık (UI bağımsız)
│   ├── __init__.py
│   ├── detector.py             # URL platform tespiti (YouTube, TikTok, IG, X, Genel)
│   ├── codec_selector.py       # Saf fonksiyonel kodek/konteyner seçici & kural motoru
│   ├── downloader.py           # yt-dlp Python API sarmalayıcısı (Worker thread & callback)
│   ├── sanitization.py         # Dosya adı temizleme (Windows yasak karakterler, traversal)
│   └── updater.py              # GitHub Releases ve PyPI yt-dlp API güncelleyici & SHA-256 doğrulayıcı
│
├── ui/                         # Piksel kullanıcı arayüzü
│   ├── __init__.py
│   ├── app_window.py           # Ana Tkinter penceresi ve yaşam döngüsü
│   ├── theme.py                # Pastel renk paleti, font tanımları, metrikler
│   ├── widgets.py              # Özel piksel buton, ilerleme çubuğu, sekme bileşenleri
│   ├── dialogs.py              # Hata, onay ve güncelleme pencereleri
│   └── strings.py              # Tüm Türkçe arayüz metinleri (i18n hazır)
│
├── tray/                       # Sistem tepsisi yönetimi
│   ├── __init__.py
│   └── tray_manager.py         # pystray entegrasyonu, minimize-to-tray, RAM serbest bırakma
│
├── tests/                      # Birim ve entegrasyon testleri
│   ├── __init__.py
│   ├── test_detector.py        # Platform tanıma testleri
│   ├── test_codec_selector.py  # Gerçek JSON format listeleriyle deterministik seçim testleri
│   ├── test_sanitization.py    # Dosya yolu güvenlik testleri
│   └── fixtures/               # YouTube, TikTok, IG, X örnek info_dict JSON'ları
│
├── installer/                  # Kurulum ve paketleme
│   ├── setup.iss               # Inno Setup derleme scripti (Türkçe arayüz, %LOCALAPPDATA%)
│   ├── build.py                # PyInstaller + Inno Setup tek komutla derleme aracı
│   └── dev_setup.bat           # Geliştirici ortamı kurucu (pip venv)
│
├── main.py                     # Uygulama başlangıç noktası (Tekil örnek kontrolü / Mutex)
├── DESIGN.md                   # Tasarım şartnamesi
├── PRODUCT.md                  # Ürün şartnamesi
├── ARCHITECTURE.md             # Mimari şartnamesi
├── README.md                   # Proje tanıtımı ve belgeler
├── LICENSE                     # Lisans dosyası
└── requirements.txt            # Python bağımlılıkları (sabit sürümlerle)
```

---

## 3. Veri Akışı ve Olay Yönetimi

1. **URL Girişi**:
   - Kullanıcı URL girdiğinde `detector.py` platformu regex ile tanır (`youtube`, `instagram`, `tiktok`, `twitter`, `generic`).
2. **Metadata Çekimi (Asenkron Worker)**:
   - UI donmaması için arka plan iş parçacığında `yt-dlp.YoutubeDL({'extract_flat': False})` ile sadece metadata (`extract_info(..., download=False)`) çekilir.
   - Çekilen veri UI iş parçacığına `queue.Queue` üzerinden iletilir.
3. **Format & Kodek Çözümlemesi (Saf Fonksiyon)**:
   - `codec_selector.select_best_format(info_dict, mode="video", target_res="best", force_mp4=False)` saf fonksiyonu çalışır.
   - Bu fonksiyon hiçbir yan etki (side-effect) içermez.
   - Çıktı: Seçilen video format id, ses format id, beklenen konteyner uzantısı, dönüştürme gerekip gerekmediği ve gerekçesi.
4. **İndirme ve İlerleme Raporu**:
   - İndirme başlatıldığında `downloader.py` ayrı bir iş parçacığında çalışır.
   - yt-dlp `progress_hooks` mekanizması indirme hızını, yüzdesini ve kalan süreyi thread-safe kuyruk ile UI'ya iletir.
   - Arayüz 100ms aralıklarla bu kuyruğu tüketerek piksel ilerleme çubuğunu günceller.
5. **İptal ve Tamamlanma**:
   - İptal isteği bayrak (`threading.Event`) ile güvenli şekilde yt-dlp'ye iletilir.
   - İndirme tamamlandığında Windows Toast bildirimi / arayüz bildirimi verilir.

---

## 4. Kodek ve Konteyner Seçim Motoru (Deterministik Kural Motoru)

### Video Öncelik Sıralaması:
$$\text{AV1} > \text{VP9} > \text{H.265 (HEVC)} > \text{H.264 (AVC)}$$

### Ses Öncelik Sıralaması:
$$\text{Opus} > \text{AAC} > \text{MP3}$$

### Kurallar:
1. **Stream Copy (Remux) Önceliği**:
   Eğer seçilen video akışı ve ses akışı doğal olarak bir MP4 veya MKV/WebM konteynerinde re-encode olmadan paketlenebiliyorsa, ffmpeg `-c copy` (stream copy) kullanılır. CPU ve zaman harcanmaz.
2. **"MP4 Olarak Kaydet" Mantığı**:
   - Kullanıcı "MP4 Olarak Kaydet" seçtiğinde:
     - Kaynakta H.264 + AAC akışları varsa doğrudan bunlar seçilir ve remux edilir (sıfır kalite kaybı, sıfır işlemci yükü).
     - Kaynakta sadece VP9 veya AV1 varsa, kullanıcının seçimine göre ya MKV/WebM olarak remux edilir ya da ffmpeg ile H.264 dönüştürmesi açık uyarı verilerek yapılır.
3. **TikTok ve Instagram Durumu**:
   - Bu platformlar neredeyse her zaman tek bir birleşik H.264 + AAC MP4 akışı sunar. Fonksiyon bunu tespit edip gereksiz hiçbir ayırma/birleştirme yapmadan doğrudan ham akışı indirir.

---

## 5. Tray (Sistem Tepsisi) Yaşam Döngüsü ve Bellek Optimizasyonu

WhatsApp / Steam modelinde çalışır:
- **Pencere Kapatma Olayı (`WM_DELETE_WINDOW`)**:
  - `window.destroy()` veya `window.withdraw()` çağrılır.
  - Önbellekteki küçük resimler (thumbnail) ve geçici veri nesneleri `None` yapılarak Python Garbage Collector (`gc.collect()`) tetiklenir.
  - RAM tüketimi 10-15 MB aralığına iner.
  - Arka planda yalnızca hafif `pystray` döngüsü ve eğer varsa indirme iş parçacığı kalır.
- **Tray İkonuna Sol Tık**:
  - Pencere anında yeniden görünür yapılır (`window.deiconify()`), öne odaklanır (`lift()`).
- **Tamamen Çıkış (`WM_QUIT`)**:
  - Eğer aktif bir indirme sürüyorsa: Tray veya diyalog üzerinden "Devam eden indirme var. İptal edilip çıkılsın mı?" onay penceresi açılır.
  - Kullanıcı onaylarsa indirme iş parçacığı güvenle durdurulur ve uygulama kapatılır.
- **Tekil Örnek (Single Instance)**:
  - Windows Adlandırılmış Mutex (`CreateMutexW`) kullanılarak uygulamanın birden fazla kopyasının aynı anda çalışması engellenir. İkinci kopya açıldığında mevcut pencere öne getirilir.

---

## 6. Güvenlik ve Hata Yönetimi

1. **Komut Enjeksiyonu Önleme**:
   - yt-dlp doğrudan Python modülü olarak import edilir (`import yt_dlp`). Asla `subprocess.Popen("yt-dlp " + url)` şeklinde komut dizesi birleştirilmez.
   - ffmpeg çağrılarında daima argüman listesi (`args=['ffmpeg', '-i', ...]`, `shell=False`) kullanılır.
2. **Yol Aşımı (Path Traversal) ve Dosya Adı Temizliği**:
   - `re.sub(r'[<>:"/\\|?*]', '_', title)` ve Unicode normalizasyonu uygulanır.
   - Dosya yolu işletim sistemi sınırları (MAX_PATH) ve `..` güvenlik açığına karşı sanitize edilir.
3. **Açıklayıcı Hata Mesajları**:
   - Özel/silinmiş video: *"Bu video gizli veya platformdan kaldırılmış."*
   - Ağ hatası: *"İnternet bağlantısı kesildi. Lütfen bağlantınızı kontrol edin."*
   - ffmpeg eksikliği: *"Gerekli medya dönüştürücü (ffmpeg) bulunamadı. Lütfen kurulumu onarın."*

---

## 7. Paketleme, Kurulum ve Güncelleme

### A. Kurulum (Inno Setup)
- Tek bir `Setup.exe` üretilir.
- Kurulum dizini: `%LOCALAPPDATA%\Programs\ff-dlp-viewer` (Yönetici yetkisi gerektirmez, UAC penceresiyle kullanıcıyı korkutmaz).
- Başlat Menüsü kısayolu ve Masaüstü simgesi oluşturulur.
- İsteğe bağlı `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run` ile "Windows ile tray'de başlat" seçeneği.
- Temiz kaldırma (Uninstall.exe) desteği.

### B. ff-dlp Bağımsız Paketleme
- PyInstaller ile `--onedir` veya optimize `--onefile` derlenir.
- Gömülü Python çalışma ortamı pakete dahildir.
- ffmpeg ikilisi uygulamanın kendi `bin/` klasöründe yer alır; sistem `PATH` ortam değişkenine dokunulmaz.

### C. Güncelleme Mekanizması
- **Uygulama Güncellemesi**: GitHub API'den son sürüm etiketi kontrol edilir (`/repos/{user}/{repo}/releases/latest`). Yeni sürüm varsa değişiklik notları ve indirme bağlantısı gösterilir; indirilen yeni `Setup.exe` için `SHA-256` doğrulaması yapılır.
- **İndirme Motoru (yt-dlp) Güncellemesi**: PyPI JSON API (`https://pypi.org/pypi/yt-dlp/json`) üzerinden yt-dlp'nin en güncel kararlı sürümü sorgulanır ve kurulu sürümle CalVer karşılaştırması yapılır. Platform algoritmalarındaki güncellemeler kullanıcıya sistem tepsisinden tek tıkla şeffafça bildirilir.
- Onay verildiğinde eski uygulama kapatılır, yeni setup güvenle çalıştırılır.
