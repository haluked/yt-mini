# Product: ff-dlp-viewer (Hafif Medya İndirici)

<!-- impeccable:product-schema 1 -->

## Platform

windows

## Stack

Saf C11 (Win32 API: user32/gdi32/comctl32/shell32/ole32), yt-dlp ve ffmpeg bağımsız ikilileri, özel çizilmiş GDI arayüzü (gb_ui.c), sistem tepsisi. Harici GUI çatısı, Python veya Electron yok.

## Users

- Birincil kullanıcı: Haluk ve terminal/teknik komutları bilmeyen aile üyeleri/günlük bilgisayar kullanıcıları.
- Durum: Web üzerinde izlenen veya kaydedilmek istenen bir video ya da ses bağlantısını tek tıkla, en yüksek kalitede ve zahmetsizce bilgisayarına indirmek isteyen kullanıcılar.
- Amaç: Karmaşık ffmpeg/yt-dlp parametreleriyle uğraşmadan, arka planda sessizce bekleyen, kaynak tüketmeyen ve hızlı çalışan masaüstü aracı.

## Product Purpose

YouTube, Instagram, TikTok, X (Twitter) gibi platformlardan video ve sesleri; kaynağın sunduğu en uygun kodek/konteyner uyumuyla, gereksiz re-encode yapmadan, sistem kaynaklarını (RAM/CPU) asgari düzeyde harcayarak ve terminal penceresi açmadan indirmek.

## Positioning

Gereksiz "AI dashboard" süslemeleri, yüzlerce megabaytlık Electron veya ağır Qt bağımlılıkları ve reklam/telemetri içeren hantal indiricilerin aksine; 20 MB altında boşta RAM tüketen, Steam/WhatsApp benzeri sessiz tray (sistem tepsisi) yaşam döngüsüne sahip, deterministik kodek seçimi yapan ve Game Boy DMG paletli 8-bit arayüze sahip bağımsız Windows aracı.

## Operating Context

- Windows 10/11 masaüstü ortamı.
- Kullanıcı URL kopyalar, uygulama penceresini açar (veya panodan yapıştırır), tek tuşla video bilgilerini ve mevcut formatları görür.
- İndirme başlatılır; indirme arka planda çalışırken pencere kapatılsa bile tray ikonuna küçülür ve çalışmaya devam eder.
- İndirme bittiğinde bildirim verir, kullanıcı istediğinde tek tıkla dosya konumunu açabilir.

## Capabilities and Constraints

- **Modüler Platform Ayrıştırıcı**: YouTube, Instagram, X/Twitter, TikTok URL formatlarını tespit edip ilgili platform kurallarını uygular.
- **Deterministik Kodek & Format Seçimi**: Saf matematiksel öncelik (Video: AV1 > VP9 > H.265 > H.264; Ses: Opus > AAC). Mümkün olduğunda stream copy (remux) kullanılır, CPU yoran gereksiz re-encode yapılmaz.
- **Konteyner Şeffaflığı**: İndirme öncesinde hedef konteyner (.webm, .mp4, .mkv, .opus, .m4a, .mp3) kullanıcıya açıkça bildirilir.
- **MP4 Uyum Koruması**: Kullanıcı "MP4 olarak indir" seçtiğinde, kodek uyumsuzluğu varsa (ör. VP9/Opus) açık uyarı ve dönüştürme/remux seçeneği sunulur.
- **Gelişmiş Mod**: İleri düzey kullanıcılar için manuel kodek/konteyner seçimi (varsayılan gizli).
- **Gerçek Windows Kurulumu**: Inno Setup ile tek parça `Setup.exe`, `%LOCALAPPDATA%` altına yönetici izni gerektirmeden kurulum, Başlat/Masaüstü kısayolları ve temiz Uninstaller.
- **Sistem İçi Güncelleyici**: GitHub Releases API (yt-mini uygulama güncellemeleri, SHA-256 doğrulama) ve PyPI API (yt-dlp indirme motoru sürüm kontrolü) ile bileşenlerin her zaman güncel kalmasını sağlar.

## Brand Commitments

- **Stil**: Game Boy DMG 8-bit estetiği: yalnızca 4 yeşil ton, piksel basamaklı (chamfer) köşeler, 2 px sert çerçeve/gölge, sistem `Consolas` fontu (anti-aliasing kapalı). Harici görsel/font dosyası yok.
- **Anti-pattern Yasakları**: Gradient, parlama (glow), bulanık gölge, anti-aliased (bulanık) köşe, işletim sisteminin çizdiği kontrol (combobox/radio/checkbox), emoji ikonlar ve yapay zeka jeneriği kartlar kesinlikle yer almaz.
- **Dil**: Tamamen Türkçe ve yalın terminoloji.

## Product Principles

1. **Görünmez ve Hafif**: Kullanılmadığı zaman bellekte yok denecek kadar az yer tutar (~15-20 MB RAM), CPU'yu asla rehin almaz.
2. **Kaynağa Saygı (No Needless Re-encode)**: Kaynak zaten kaliteli bir akış veriyorsa tekrar kodlama yapılmaz, doğrudan kopyalanır.
3. **Deterministik ve Test Edilebilir**: Aynı URL ve format listesi verildiğinde kodek motoru her zaman aynı çıktıyı üretir.
4. **Sıfırdan Başlayan Biri İçin Yalın**: Aile fertleri sadece URL yapıştırıp "İndir"e basarak sorunsuz dosya alabilmelidir.
