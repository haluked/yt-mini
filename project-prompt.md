# Görev: Windows için hafif, tray'de yaşayan yt-dlp tabanlı video/ses indirme uygulaması

> **Tarihsel not:** Bu dosya projenin ilk Python prototipi dönemindeki görev tanımıdır.
> Güncel uygulama yerel C11/Win32 sürümüdür; bkz. [README.md](README.md).

## Genel hedef
yt-dlp ve ffmpeg'i arka planda kullanan, terminal bilgisi gerektirmeyen, sade bir Windows masaüstü uygulaması geliştir. Ana öncelikler: çok düşük RAM/CPU kullanımı, doğru kodek/konteyner seçimi ve temiz, "AI yapımı" hissi vermeyen bir arayüz. Daha önce prototipleri yapıldı ve beğenilmedi (kırılgan mimari, sürekli küçük hatalar). Bu yüzden sıfırdan, sağlam ve test edilebilir bir mimariyle yaz.

## Aşama 0: Tasarım ve plan (koddan önce)
- Arayüz kodu yazmadan önce "impeccable" tasarım skill'ini çalıştır. Kararları DESIGN.md dosyasına yaz (renk paleti, font, boşluk sistemi, bileşenler).
- Mimariyi ARCHITECTURE.md dosyasına yaz (modüller, veri akışı, hangi modül neyden sorumlu).
- Stil: sade, piksel (pixel-art) estetiği, pastel tonlar. Gradient, glow, gereksiz gölge, emoji ve jenerik "AI dashboard" görünümünden kaçın.
- Planı ve tasarımı bana sun, onayımdan sonra koda geç.

## Teknoloji
- Dil: Python 3.11+
- İndirme: yt-dlp (Python kütüphanesi olarak, subprocess değil). Sürümü sabitle.
- Medya işleme: ffmpeg (uygulamanın kendi klasörüne yerleşir, sistem PATH'ine dokunulmaz)
- Arayüz: hafif kütüphane (Tkinter/customtkinter öncelikli). Qt gibi ağır çözümlerden kaçın, seçimi RAM gerekçesiyle DESIGN.md'de belirt.
- Tray: pystray veya eşdeğeri
- Paketleme: PyInstaller veya Nuitka ile bağımsız .exe, Inno Setup ile kurulum dosyası

## Kurulum ve paketleme (setup.bat YOK, sadece gerçek installer)
- Son kullanıcı için tek bir `Setup.exe` üret (Inno Setup). Kullanıcı Python, pip, yt-dlp veya ffmpeg'i kendisi kurmak zorunda kalmamalı.
- Python çalışma ortamı ve tüm kütüphaneler paketin içinde gelsin.
- yt-dlp ve ffmpeg ya installer'a gömülsün ya da kurulum sırasında resmi kaynaktan indirilsin. Hangisinin daha sağlam olduğunu değerlendirip gerekçesiyle seç.
- Installer: kurulum klasörü seçimi (varsayılan %LOCALAPPDATA% altı, yönetici izni gerektirmesin), Başlat menüsü kısayolu, isteğe bağlı masaüstü kısayolu, isteğe bağlı "Windows açılışında tray'de başlat", kaldırıcı (uninstaller).
- Installer ve uygulama Türkçe arayüzlü olsun.
- Geliştirici tarafı için ayrıca `dev_setup.bat` (veya `requirements.txt` + README adımı). Son kullanıcıya gösterilmez.
- Build tek komutla çalışsın (`build.bat` veya `build.py`): uygulamayı paketler, installer'ı üretir, çıktıyı `dist/` altına koyar.
- Güncelleme: "Güncellemeleri denetle" yeni sürüm bulursa GitHub Releases'ten yeni Setup.exe'yi indirip kullanıcı onayıyla çalıştırsın. Güncellemeyi çalıştırmadan önce bütünlüğünü doğrula (checksum).
- İmzasız .exe nedeniyle SmartScreen uyarısı çıkabilir. README'de nedenini kısaca açıkla.

## Tray davranışı (WhatsApp/Steam gibi)
- Pencere kapatılınca uygulama sonlanmaz, Windows sağ alt bildirim alanında tray ikonu olarak kalır.
- Tray ikonuna sol tık: pencereyi açar.
- Sağ tık menüsü: Aç, Güncellemeleri denetle, Tamamen çık.
- Pencere kapalıyken arayüz nesneleri bellekten serbest bırakılır. Boşta RAM kullanımı mümkün olduğunca düşük olmalı.
- Devam eden indirme varken "Tamamen çık" seçilirse onay iste.

## Arayüz akışı
1. En üstte URL yapıştırma alanı. URL yapıştırılıp onaylanınca kaynak otomatik tanınır (başlangıçta YouTube, Instagram, X/Twitter, TikTok). Kaynak desteği modüler olsun, yeni platform eklemek kolay olsun.
2. Onaydan sonra video bilgileri çekilir: başlık, küçük resim, süre, mevcut formatlar, kaynağın video kodeği, ses kodeği ve konteyneri. Kullanıcıya gösterilir.
3. İki sekme:
   - **Video:** Çözünürlük seçimi (En iyi, 4K, 1440p, 1080p, 720p ... 144p'ye kadar, yalnızca videoda gerçekten mevcut olanlar). Varsayılan her zaman "En iyi".
   - **Ses:** Sadece ses indirme. Varsayılan Opus. Alternatif AAC ve MP3.
4. İndirme ilerlemesi (yüzde, hız, kalan süre), iptal ve kuyruk desteği.

## Kodek ve konteyner mantığı (kritik)
- Varsayılan: kaynağın sunduğu en uygun kodeği otomatik seç. Video önceliği AV1 > VP9 > H.265 > H.264, ses önceliği Opus > AAC. Kaynak ne sunuyorsa (ör. YouTube'da webm/mkv) konteyner o kalsın. Gereksiz yeniden kodlama (re-encode) yapma, mümkünse stream copy / remux kullan.
- Konteyner, video bilgisi çekildiğinde arayüzde önceden gösterilir.
- "MP4 olarak indir" seçeneği her zaman mevcut olsun. Kodek MP4 ile uyumsuzsa (ör. VP9/Opus) kullanıcıya açıkça söyle; uyumlu kodeği seç ya da dönüştürmeyi onayla.
- Kaynak tek kodek sunuyorsa (ör. TikTok/Instagram genelde H.264/AAC) en iyi mevcut olanı kullan ve nedenini göster.
- "Gelişmiş" bölümü varsayılan olarak kapalı: video kodeği, ses kodeği ve konteyneri elle değiştirme ("deneysel" uyarısıyla).
- Kodek seçimi saf bir fonksiyon olsun: girdi yt-dlp format listesi, çıktı seçilen format ve konteyner. YouTube, TikTok, Instagram ve X için gerçek format JSON örnekleriyle birim testleri yaz.

## Performans gereksinimleri
- Boşta RAM kullanımını ölçülebilir şekilde düşük tut. Sonunda tray modunda ve pencere açıkken ayrı ayrı raporla.
- Ağır import'ları lazy yükle, gereksiz arka plan thread/polling kullanma.
- Arayüz indirme sırasında asla donmasın (indirme ayrı thread/process).

## Güvenlik
- Kullanıcıdan gelen URL'yi komut satırına birleştirme. yt-dlp'ye kütüphane API'siyle ver, ffmpeg'e argüman listesiyle çağır (shell=False).
- İndirilen dosya adlarını temizle (path traversal, Windows'ta yasak karakterler).

## Proje yönetimi
- Yerel klasörde git repo başlat. Mantıklı noktalarda anlamlı commit mesajlarıyla yedek al.
- GitHub'da yayınlanmaya uygun yapı: README.md (kurulum, kullanım, ekran görüntüsü alanı), LICENSE (yt-dlp Unlicense, ffmpeg LGPL/GPL, lisans uyumunu kontrol et), .gitignore, açık klasör yapısı.
- Yayın öncesi kontrol listesi hazırla. Ben onaylamadan GitHub'a push etme.

## Kalite beklentisi
- Temiz, okunabilir, modüler kod: UI, indirme motoru, kaynak tanıma, kodek seçimi, tray, güncelleme ayrı modüller.
- Hata durumlarında (geçersiz URL, özel/silinmiş video, ağ kesintisi, ffmpeg bulunamadı) anlaşılır Türkçe mesaj göster. Uygulama çökmesin.
- Arayüz Türkçe, ileride dil eklemeye uygun (metinler ayrı bir dosyada).

## Çalışma şekli
Önce planı, ARCHITECTURE.md ve DESIGN.md'yi sun, onayımı bekle. Sonra sırayla: çekirdek indirme motoru, kodek mantığı, arayüz, tray, kurulum/paketleme, güncelleme, git/GitHub hazırlığı. Her aşama bitince kısa bir özet ver ve bir sonrakine geçmeden onay iste.
