# Gelecek Planları ve Yol Haritası (TODO)

## 1. Pano Ön-Çekim Belleği ve Zaman Aşımı Yönetimi (Clipboard Fetch Memory Cache)
- **Açıklama:** Kullanıcı YouTube bağlantısını kopyaladığında arka planda çekilen video/medya bilgileri hemen silinmeyip birkaç dakika (örn. 5-10 dakika) boyunca bellekte güvenli şekilde önbelleğe alınacak.
- **Kullanım Senaryosu:**
  - Kullanıcı 4 saniyelik açılır bildirim (toast popup) çıktığında meşgulse veya bildirime yetişemezse, video bilgileri kaybolmayacak.
  - Kullanıcı daha sonra sistem tepsisindeki (tray) simgeye veya görev çubuğuna tıkladığında "Son Kopyalanan Medya: [Video Adı]" seçeneğini görecek ve tek tıkla indirmeye devam edebilecek.
  - Süre dolduğunda veya yeni bir bağlantı kopyalandığında eski önbellek temizlenerek bellek sızıntısı önlenecek.

## 2. Pano Otomatik-Çekimini Tüm Platformlara Genişletme ("Bu linki indirebilir miyiz?")
- **Açıklama:** Şu anda panoya kopyalanan **yalnızca YouTube** bağlantıları otomatik sorgulanıyor ve toast bildirimi gösteriyor. İleride herhangi bir bağlantı kopyalandığında yt-dlp'a "bu linki indirebilir miyiz?" diye sorulabilir (desteklediği 1000+ site).
- **Tasarım Notları:**
  - Kopyalanan URL'nin platformu tespit edilemiyorsa (generic/web) sessizce denemeli; sonuç başarısızsa hiçbir bildirim göstermemeli (kullanıcıyı gereksiz toast ile rahatsız etmemek için).
  - Toast rozeti şu an sabit "▶ YOUTUBE" yazıyor (`ui.c` WM_PAINT) — platform genişletilince rozet `meta.platform.badge_label`'dan dinamik alınmalı.
  - Bing/Google arama sayfası bağlantıları gibi indirilemeyecek URL'ler için kısa bir " Kara liste" (örn. google.com, bing.com) düşünülebilir.

## 3. Pasif Güncelleme Denetimi (RAM dostu model)
- **Açıklama:** Şu an tepsi menüsündeki "Güncellemeleri Denetle (yt-dlp)" **sürüm kontrolü yapmadan** her seferinde ~17-30 MB indiriyor. Hedef: uygulamanın açılışında + tray'de beklerken günde en fazla bir kez, tek HTTP isteğiyle (PyPI/GitHub API) sürüm karşılaştırması yapmak.
- **Davranış:** Güncelleme varsa tray baloncuğu "Güncelleme var, tıkla" der; kullanıcı tıklarsa indirme başlar. Arka plan servisi / scheduled task YOK (sıfır ek RAM).
- **Bağlı iş:** İndirilen yt-dlp/ffmpeg dosyalarının **SHA-256 doğrulaması** (GitHub Releases API `digest` alanı kullanılabilir) bu aşamada birlikte eklenmeli. Ayrıca güncelleme "uygulamanın kullandığı kopyayı" hedeflemeye devam etmeli (bkz. bootstrapper düzeltmesi).

## 4. ffmpeg Dağıtım URL'i
- **Açıklama:** `bootstrapper.c` içindeki FFMPEG_URL hâlâ **eski yt-mini deposunun** v2.0.0-beta release'ine bağlı. yt-mini-lite deposu açılıp ilk release yayınlandığında bu URL yeni depoya taşınmalı.
- **Alternatif:** ffmpeg kaynağı olarak gyan.dev veya BtbN resmi yapıları + sabit SHA-256 (LGPL/GPL lisans notuyla). Karar: dağıtım mimarisi tartışmasında.

## 5. Kurulum Stratejisi (Architecture — tartışılacak)
- **Karar (şimdilik):** yt-dlp + ffmpeg **her zaman uygulama klasörüne** (app-dir\bin veya %LOCALAPPDATA%\Programs\...\bin) kurulur; sistem PATH'ine DOKUNULMAZ. Böylece uygulama hep "portable mantığıyla" çalışır, kaldırma tertemiz olur.
- **İleride:** PATH'e ekleme, installer'da **opsiyonel bir onay kutusu** olarak düşünülebilir (gelişmiş kullanıcılar için). Windows registry/ortam değişkeni değişikliği geri alınabilir olmalı.

## 6. Kimlik/Marka Düzeltmesi (yt-mini-lite geçişi)
- Mutex (`Local\ff-dlp-viewer-light`), registry anahtarı (`HKCU\Software\ff-dlp-viewer`), pencere sınıfı ve başlıklar hâlâ **ff-dlp-viewer** adını taşıyor. yt-mini-lite deposuna geçişte tek seferde, hepsi birden güncellenmeli (yarım rebrand = iki isim karışır).
- Not: pencere sınıfı adı değişirse `main.c`'teki `FindWindowW(UI_WINDOW_CLASS, ...)` ikinci örnek aktarımı otomatik olarak yeni adı kullanır — iki tarafı aynı commit'te değiştirmek yeterli.

## 7. Format Listesi Sınırı
- MAX_FORMATS = 256 (bellek Challenge'ı gereği sabit). yt-dlp formatları kötüden iyiye sıraladığı için sınır aşılırsa artık **en iyiler korunarak** sondan alınıyor (bkz. engine.c parse_json_metadata). Nadir sitelerde 256'yı aşan listeler için ileride dinamik ayrım düşünülebilir; pratikte gerek yok.
