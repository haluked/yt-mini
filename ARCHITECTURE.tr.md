# Sistem Mimarisi Şartnamesi (ARCHITECTURE.tr.md)

## 1. Genel Mimari Bakış

ff-dlp-viewer, C11 ile yazılmış yerel bir Win32 uygulamasıdır. Windows sistem
DLL'lerinin dışında çalışma zamanı bağımlılığı yoktur: Python, .NET, Electron veya
harici bir GUI çatısı kullanılmaz. Çekirdek mantık (URL tespiti, kodek seçimi, dosya
adı temizliği, JSON ayrıştırma) arayüzden bağımsız saf C'dir ve birim testleriyle
doğrulanır.

```
                 ┌──────────────────────────────────────────────┐
                 │                 main.c (WinMain)             │
                 │  CLI ayrıştırma · tekil örnek · bootstrapper │
                 └───────────────┬──────────────────────────────┘
                                 │ pencereyi kurar, UI'yi başlatır
                                 ▼
                 ┌──────────────────────────────────────────────┐
                 │              ui.c (UI iş parçacığı)          │
                 │  pencere · özel çizim kontroller · tray      │
                 │  durum makinesi · DPI · sürükle & bırak      │
                 └───────┬───────────────────────────▲──────────┘
                         │ engine_*_async()          │ WM_USER_* mesajları
                         ▼                           │ (PostMessage)
                 ┌──────────────────────────────────────────────┐
                 │            engine.c (worker thread)          │
                 │  CreateThread · CreatePipe · ReadFile        │
                 │  ilerleme ayrıştırma · iptal · Job Object    │
                 └───────┬───────────────────────────▲──────────┘
                         │ CreateProcessW (gizli)    │ stdout / stderr
                         ▼                           │
                 ┌──────────────────────────────────────────────┐
                 │        yt-dlp.exe  →  ffmpeg.exe             │
                 │        (bin\ altında, asla PATH'te değil)    │
                 └──────────────────────────────────────────────┘
```

---

## 2. Modül Yapısı ve Sorumluluklar

```
ff-dlp-viewer/
├── src/
│   ├── main.c              # Giriş noktası: CLI, mutex, bootstrapper, ui_run()
│   ├── ui.c                # Ana pencere, yerleşim, durum makinesi, tray, toast, sürükle & bırak
│   ├── gb_ui.c             # Game Boy çizim ilkelleri, özel çizim kontroller, açılır liste, mesaj kutusu
│   ├── engine.c            # Worker thread, gizli süreç boruları, metadata/indirme, ilerleme, iptal
│   ├── bootstrapper.c      # İlk açılışta yt-dlp/ffmpeg indirme ve yt-dlp güncelleme penceresi
│   ├── codec_selector.c    # Saf deterministik kodek/konteyner seçim motoru
│   ├── detector.c          # URL doğrulama ve platform tespiti
│   ├── sanitization.c      # Windows'a uygun dosya adı ve yol işleme
│   ├── cjson.c             # Tek dosyalık JSON ayrıştırıcı (metadata ve format ağacı)
│   ├── *.h                 # Genel arayüzler ve paylaşılan sabitler
├── res/                    # app.ico, app.manifest, res.rc (sürüm bilgisi)
├── tests/test_core.c       # detector, sanitization, codec selector ve cJSON birim testleri
├── installer/setup.iss     # Inno Setup kurulum scripti
├── build.bat               # windres + gcc derlemesi ve boyut doğrulaması
├── DESIGN.md               # Game Boy 8-bit tasarım şartnamesi
├── PRODUCT.md              # Ürün şartnamesi
├── RETRO_ARCHITECTURE.md   # Yerel yeniden yazımın boyut/çalışma zamanı gerekçesi
├── NAMING.md               # Depo/ikili dosya adlandırma kararları
├── TODO.md                 # Yol haritası
└── README.md               # Kullanıcı ve geliştirici belgeleri
```

---

## 3. Veri Akışı

1. **URL girişi ve doğrulama**
   - `detector.c` URL'yi doğrular (http/https, uzunluk, tırnak/ters bölü/kontrol
     karakteri yok) ve platformu tespit eder (`youtube`, `tiktok`, `instagram`,
     `twitter`, `generic`).
2. **Metadata çekimi (asenkron)**
   - `ui.c`, `engine_fetch_async()` çağırır. `engine.c` bir worker thread başlatır
     (`CreateThread`) ve gizli borularla (`CreateProcessW` + `CreatePipe`)
     `yt-dlp.exe --dump-json --no-playlist --no-warnings "<url>"` komutunu çalıştırır.
   - stdout yalnızca JSON taşır; stderr ayrı bir iş parçacığında sürekli boşaltılır,
     böylece alt süreç dolu boru yüzünden asla kilitlenmez.
   - JSON `cjson.c` ile ayrıştırılır; format girdileri sabit `MAX_FORMATS` (256)
     dizisinde tutulur. Sınıra ulaşıldığında yt-dlp'nin kötüden iyiye sıralı
     listesinin sonundan alınarak **en iyi** formatlar korunur.
3. **Kodek çözümlemesi (saf fonksiyon)**
   - `codec_selector.c` içindeki `select_best_format()` yan etkisizdir. Format listesi,
     mod (`video`/`audio`), hedef çözünürlük, MP4 zorlama bayrağı ve ses tercihi
     verildiğinde seçilen format kimliğini, beklenen konteyneri, yeniden kodlama
     gerekip gerekmediğini, gerekçe metnini ve isteğe bağlı uyarıyı döndürür.
4. **İndirme ve ilerleme**
   - `engine_download_async()` aynı worker modelinde çalışır. yt-dlp,
     `percent|speed|eta|downloaded|total` satırları yazdıran bir `--progress-template`
     ile başlatılır; her satır ayrıştırılıp arayüze `WM_USER_DOWNLOAD_TICK` olarak
     gönderilir.
   - Nihai dosya yolu, konsol kod sayfasından tahmin edilmek yerine yt-dlp tarafından
     UTF-8 bir geçici dosyaya yazılır (`--print-to-file after_move:filepath`); böylece
     Türkçe/emoji başlıklar doğru yola çözülür.
5. **İptal ve tamamlanma**
   - İptal isteği, tüm süreç ağacını (yt-dlp ve varsa ffmpeg alt süreci) tutan Job
     Object'i sonlandırır; yetim süreç kalmaz.
   - Hata durumunda yalnızca bu çalıştırmada oluşan ara dosyalar zaman damgasına göre
     temizlenir; nihai/işlenmiş çıktı asla silinmez.
   - Arayüz `finished`, `cancelled` veya `error` alır; durum satırını, tray ipucunu ve
     pencereleri günceller.

---

## 4. Kodek ve Konteyner Seçim Motoru

### Video önceliği
$$\text{AV1} > \text{VP9} > \text{H.265 (HEVC)} > \text{H.264 (AVC)}$$

### Ses önceliği
$$\text{Opus} > \text{AAC} > \text{MP3}$$

### Kurallar
1. **Önce stream copy**: seçilen akışlar yeniden kodlanmadan MP4, WebM veya MKV içine
   paketlenebiliyorsa yt-dlp bunları `-c copy` (`--merge-output-format`) ile birleştirir.
   CPU zamanı harcanmaz.
2. **MP4 zorla**:
   - H.264 + AAC kaynaklar doğrudan kopyalanır (sıfır kayıp, sıfır CPU).
   - VP9/AV1 video + Opus ses: yalnızca ses AAC'ye çevrilir, video kopyalanır
     (`--ppa "Merger:-c:v copy -c:a aac"`).
   - H.264'e dönüştürülmesi gereken VP9 video, indirme başlamadan önce arayüzde açık
     bir uyarı gösterilerek yeniden kodlanır.
3. **TikTok / Instagram**: bu platformlar genellikle tek parça H.264+AAC MP4 akışı
   sunar; ayırma/birleştirme adımı olmadan doğrudan indirilir.
4. **Doğrudan medya dosyaları** (`.mp4`, `.webm`, ...): yt-dlp iki akış için de kodek
   bilgisi bildirmediğinde dosya doğrudan kaydedilir; ses modunda ffmpeg ile çıkarılır
   veya dönüştürülür.

---

## 5. İş Parçacıkları, Arayüz ve Çizim

- **UI iş parçacığı**: tüm pencerelerin ve kontrollerin sahibidir. Bütün çizim UI iş
  parçacığında GDI ile yapılır.
- **Worker iş parçacıkları**: aynı anda tek metadata/indirme worker'ı çalışır.
  `engine_is_busy()` durumu bildirir; arayüz asla üst üste iş başlatmaz.
- **Mesaj aktarımı**: worker hiçbir UI nesnesine dokunmaz; `PostMessage(WM_USER_*)`
  çağırır ve sonucu UI iş parçacığı okur.
- **Özel çizim arayüzü**: butonlar, seçim çipleri, onay kutusu, açılır liste, ilerleme
  çubuğu, başlık çubuğu ve modal pencereler `gb_ui.c` ile çizilir. Tek yerel kontrol,
  IME/pano/seçim güvenilirliği için URL `EDIT` kutusudur.
- **Sabit yerleşim**: 646×485 tasarım birimi, Per-Monitor V2 DPI ile ölçeklenir
  (`gb_scale` / `S()`). Medya alanının yüksekliği sabittir; pencere zıplamaz.
- **Tray yaşam döngüsü**: kapatma pencereyi gizler (`SW_HIDE`) ve bir kerelik bildirim
  gösterir; tray menüsü geri yükler veya çıkar. Explorer yeniden başladığında ikon
  `TaskbarCreated` mesajıyla yeniden kaydedilir.
- **Toast penceresi**: ayrı bir `WS_POPUP` pencere çekilen medyayı 4 saniye gösterir;
  üzerine gelmek sayacı sıfırlar.

---

## 6. Tekil Örnek ve Komut Satırı

- `CreateMutexW(Local\ff-dlp-viewer)` tek örneği garanti eder; ikinci başlatma mevcut
  pencereyi (`FindWindowW(UI_WINDOW_CLASS)`) bulup öne getirir.
- `--url <URL>` / `--url=URL` bağlantıyı açık pencereye `WM_COPYDATA` ile gönderir.
- `--help` / `--version` mesaj penceresi gösterip çıkar.

---

## 7. Araç Bootstrapper'ı ve Güncellemeler

- Açılışta `bootstrapper_ensure_tools()` `yt-dlp.exe` ve `ffmpeg.exe` dosyalarını şu
  sırayla arar: uygulama dizini → app\bin → ..\bin →
  `%LOCALAPPDATA%\ff-dlp-viewer\bin` → sistem `PATH`.
- Eksik araçlar URLMon `IBindStatusCallback` API'siyle uygulamanın kendi `bin/`
  klasörüne indirilir. Dosyalar `.tmp` olarak yazılır ve yalnızca aktarım tamamlanınca
  yerine taşınır; başarısız taşıma "başarılı" gibi gösterilmez.
- Sistem `PATH` değişkeni asla değiştirilmez.
- Tray menüsündeki "Check for Updates (yt-dlp)" en son `yt-dlp.exe` sürümünü indirir ve
  uygulamanın gerçekten kullandığı kopyayı hedefler.

---

## 8. Hata Yönetimi ve Güvenlik

1. **Kabuk komutu birleştirme yok**: araçlar `CreateProcessW` ile ve doğrulanmış
   değerlerden kurulan bir argüman dizesiyle başlatılır; URL'lerde tırnak, ters bölü ve
   kontrol karakterleri reddedilir.
2. **Yol güvenliği**: `sanitization.c` Windows'a ayrılmış karakterleri temizler, yol
   aşımını (`..`) engeller, ayrılmış adları (CON, PRN, AUX, NUL, COM1–9, LPT1–9)
   işler, boşlukları toparlar ve uzunluk sınırlarını uygular.
3. **İnsan diliyle hatalar**: motor hataları sade mesajlara çevrilir (özel video,
   oturum/yaş gereksinimi, HTTP 403, ağa ulaşılamıyor, ffmpeg eksik, format mevcut
   değil) ve Game Boy mesaj kutusunda gösterilir.
4. **Sınırlı kaynaklar**: sabit boyutlu format tablosu (256), sınırlı ilerleme
   tamponları, sürekli boşaltılan stderr ve yalnızca bir iş parçacığının zaman aşımını
   aşabildiği yerlerde belgelenmiş bilinçli sızıntılar.

---

## 9. Paketleme ve Boyut Bütçesi

- `build.bat` `windres` çalıştırır, `gcc -O3 -s -flto -static -mwindows` ile derler ve
  1 MB üzerindeki boyutu hata olarak reddeder.
- Kurulum dosyası Inno Setup (`installer/setup.iss`) ile `%LOCALAPPDATA%` altına
  yönetici izni gerektirmeyen kullanıcı bazlı kurulum olarak üretilir.
- Mevcut çıktı: ~343 KB çalıştırılabilir dosya, ~10-15 MB boşta RAM.

---

## 10. Testler

`tests/test_core.c` şunları kapsar:
- platform tespiti ve ana makine eşleştirme,
- dosya adı temizliği (yasak karakterler, yol aşımı, ayrılmış adlar, uzunluk),
- YouTube/TikTok/Instagram/X tarzı format kümelerinde deterministik kodek seçimi,
- MP4 zorlama davranışı (kopyalama / yalnızca ses dönüştürme / tam yeniden kodlama),
- Unicode kaçışları, vekil çiftler, derinlik sınırları ve kesik girdi dahil cJSON
  ayrıştırma.
