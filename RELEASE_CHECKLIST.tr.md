# Yayın Öncesi Kontrol Listesi

ff-dlp-viewer GitHub'da yayınlanmadan veya son kullanıcılara teslim edilmeden önce
doğrulanması gereken liste:

- [x] **Tasarım ve Arayüz Denetimi (Game Boy 8-bit)**:
  - Dört tonlu DMG paleti (`#0F380F`, `#306230`, `#8BAC0F`, `#9BBC0F`) uygulandı; palet dışında renk yok.
  - Piksel basamaklı köşeler, 2 px sert çerçeve/gölge; gradient, blur, parlama veya anti-aliased köşe yok.
  - Her kontrol özel çizilir; tek yerel kontrol URL `EDIT` kutusudur.

- [x] **Kod Kalitesi ve Birim Testleri**:
  - Tüm C çekirdek testleri `tests\test_core.exe` ile geçiyor (detector, sanitization, codec selector, cJSON).
  - MP4 zorlama davranışı kapsanıyor (doğrudan kopyalama, yalnızca ses dönüştürme, tam yeniden kodlama).
  - Dosya adı temizliği (Windows yasak karakterler, yol aşımı, ayrılmış adlar, MAX_PATH) doğrulandı.

- [x] **Boyut ve Performans**:
  - `build.bat` çalıştırılabilir dosyanın 1 MB altında kaldığını doğruluyor (mevcut: ~343 KB).
  - Boşta RAM ~10–15 MB ölçüldü.
  - Harici çalışma zamanı yok: yalnızca yerel Windows sistem DLL'lerine bağlanıyor.

- [x] **Arka Plan ve Tray Davranışı**:
  - Pencere kapatıldığında tepsiye gizleniyor ve uygulama çalışmaya devam ediyor.
  - İlk kapatmada bir kerelik "arka planda çalışıyor" bildirimi gösteriliyor.
  - İndirme sürerken çıkış istendiğinde onay isteniyor.
  - İkinci başlatma yeni kopya açmak yerine URL'yi mevcut pencereye (`WM_COPYDATA`) aktarıyor.

- [x] **Güvenlik ve İzolasyon**:
  - Süreçler `CreateProcessW` ile başlatılıyor; kabuk komut dizesi birleştirilmiyor.
  - URL'ler tırnak, ters bölü ve kontrol karakterlerini reddediyor.
  - yt-dlp/ffmpeg uygulamanın kendi `bin/` klasörüne kuruluyor; sistem `PATH` değiştirilmiyor.
  - Windows adlandırılmış mutex tek örneği garanti ediyor.
  - İptal durumunda Job Object tüm süreç ağacını sonlandırıyor.

- [x] **Kurulum ve Paketleme**:
  - `installer/setup.iss` İngilizce Inno Setup kurulumu sağlıyor (kullanıcı bazlı `%LOCALAPPDATA%`, yönetici izni yok).
  - `build.bat` yerel çalıştırılabilir dosyayı boyut doğrulamasıyla derliyor.
  - İlk çalıştırma bootstrapper'ı eksikse yt-dlp/ffmpeg indiriyor.

- [x] **Lisans Uyumluluğu**:
  - `LICENSE`, MIT Lisansı ile birlikte bağımsız ikili dosyalar olarak yt-dlp (The Unlicense) ve FFmpeg (LGPL v2.1+) kullanım şartlarını içeriyor.

- [ ] **Kullanıcı Onayı ve GitHub Push**:
  - Haluk onaylayana kadar GitHub'a hiçbir push yapılmaz.
