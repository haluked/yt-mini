# Ürün: ff-dlp-viewer (Hafif Medya İndirici)

<!-- impeccable:product-schema 1 -->

## Platform

windows

## Stack

Saf C11 (Win32 API: user32/gdi32/comctl32/shell32/ole32), bağımsız yt-dlp ve ffmpeg
ikilileri, özel çizilmiş GDI arayüzü (gb_ui.c), sistem tepsisi. Harici GUI çatısı,
Python veya Electron yok.

## Kullanıcılar

- Birincil kullanıcı: Haluk ve terminal ya da teknik komutları bilmeyen aile üyeleri /
  günlük bilgisayar kullanıcıları.
- Durum: Web üzerinde izlediği veya saklamak istediği bir video ya da ses bağlantısını
  tek tıkla, en yüksek kalitede ve zahmetsizce bilgisayarına indirmek isteyen
  kullanıcılar.
- Amaç: Arka planda sessizce bekleyen, kaynak tüketmeyen ve kullanıcıyı karmaşık
  ffmpeg/yt-dlp parametreleriyle uğraştırmayan hızlı bir masaüstü aracı.

## Ürün Amacı

YouTube, Instagram, TikTok ve X (Twitter) gibi platformlardan video ve sesleri;
kaynağın sunduğu en uygun kodek/konteyner uyumuyla, gereksiz yeniden kodlama
yapmadan, sistem kaynaklarını (RAM/CPU) asgari düzeyde harcayarak ve terminal
penceresi açmadan indirmek.

## Konumlandırma

Gereksiz "AI dashboard" süslemeleri, yüzlerce megabaytlık Electron veya ağır Qt
bağımlılıkları, reklam ve telemetri içeren hantal indiricilerin aksine; 20 MB altında
boşta RAM tüketen, Steam/WhatsApp tarzı sessiz bir tepsi yaşam döngüsüne, deterministik
kodek seçimine ve Game Boy DMG paletli 8-bit arayüze sahip bağımsız bir Windows aracı.

## Çalışma Bağlamı

- Windows 10/11 masaüstü ortamı.
- Kullanıcı bir URL kopyalar, uygulama penceresini açar (veya panodan yapıştırır) ve
  tek tuşla video bilgilerini ve mevcut formatları görür.
- İndirme başlar; pencere arka planda çalışırken kapatılsa bile uygulama tepsi
  simgesine küçülür ve çalışmaya devam eder.
- İndirme bittiğinde kullanıcıya bildirim verilir ve dosya konumu tek tıkla açılabilir.

## Yetenekler ve Kısıtlar

- **Modüler Platform Ayrıştırıcı**: YouTube, Instagram, X/Twitter ve TikTok URL
  biçimlerini tanır ve ilgili platform kurallarını uygular.
- **Deterministik Kodek ve Format Seçimi**: Saf matematiksel öncelik (Video:
  AV1 > VP9 > H.265 > H.264; Ses: Opus > AAC > MP3). Mümkün olduğunda stream copy
  (remux) kullanılır; CPU yoran gereksiz yeniden kodlama yapılmaz.
- **Konteyner Şeffaflığı**: Hedef konteyner (.webm, .mp4, .mkv, .opus, .m4a, .mp3)
  indirmeden önce açıkça bildirilir.
- **MP4 Uyumluluk Koruması**: Kullanıcı MP4'ü zorladığında ve kodekler uyumsuzsa
  (ör. VP9/Opus), açık bir uyarı ile dönüştürme/remux seçeneği sunulur.
- **Gerçek Windows Kurulumu**: Inno Setup ile üretilen tek bir `Setup.exe`,
  `%LOCALAPPDATA%` altına yönetici izni gerektirmeden kurulur, Başlat Menüsü/Masaüstü
  kısayolları oluşturur ve temiz bir kaldırıcı içerir.
- **Araç Bootstrapper'ı ve yt-dlp Güncelleyicisi**: İlk çalıştırmada yt-dlp ve ffmpeg
  uygulamanın kendi `bin/` klasörüne indirilir (sistem `PATH` değişkenine asla
  dokunulmaz); tray menüsünden yt-dlp tek tıkla güncellenebilir.

## Marka Taahhütleri

- **Stil**: Game Boy DMG 8-bit estetiği: yalnızca 4 yeşil ton, piksel basamaklı
  (chamfer) köşeler, 2 px sert çerçeve/gölge, sistem `Consolas` fontu (anti-aliasing
  kapalı). Harici görsel/font dosyası yok.
- **Yasak kalıplar**: Gradient, parlama, bulanık gölge, anti-aliased (bulanık) köşe,
  işletim sisteminin çizdiği kontroller (combobox/radio/checkbox), emoji ikonlar ve
  yapay zekâ jeneriği kartlar kesinlikle kullanılmaz.
- **Dil**: Tamamen İngilizce ve yalın terminoloji.

## Ürün İlkeleri

1. **Görünmez ve Hafif**: Kullanılmadığında bellekte neredeyse yoktur (~15–20 MB RAM),
   CPU'yu asla rehin almaz.
2. **Kaynağa Saygı (Gereksiz Yeniden Kodlama Yok)**: Kaynak zaten kaliteli bir akış
   sunuyorsa tekrar kodlanmaz, doğrudan kopyalanır.
3. **Deterministik ve Test Edilebilir**: Aynı URL ve format listesi verildiğinde kodek
   motoru her zaman aynı çıktıyı üretir.
4. **Sıfırdan Başlayan Biri İçin Yalın**: Aile üyeleri yalnızca bir URL yapıştırıp
   "DOWNLOAD" düğmesine basarak çalışan bir dosya elde edebilmelidir.
