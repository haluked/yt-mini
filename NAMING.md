# Adlandırma Notu (NAMING.md)

## Durum

| Yer | İsim | Açıklama |
|---|---|---|
| GitHub deposu (hedef) | **`yt-mini-lite`** | C sürümünün yeni, kalıcı evi. Sürümleme `v1.0.0`'dan temiz başlar. |
| GitHub deposu (eski) | `haluked/yt-mini` | Python sürümlerinin arşivi (v1.0 → v5.0 + pixel-UI v2.0). C sürümü buradaki `beta` dalından ayrılıyor. |
| Yerel geliştirme klasörü | **`ff-dlp-viewer`** | Kodun doğduğu, alışkanlık haline geldiği isim. **Bilinçli olarak değiştirilmiyor** (klasör adına bağlı alışkanlıklar/yollar bozulmasın diye). |
| İkili dosya adı | `ff-dlp-viewer.exe` | `build.bat` çıktısı. |

## Neden iki isim var?

Proje, kodun içinde önce `ff-dlp-viewer` adıyla geliştirildi; daha sonra ürün
kimliği `yt-mini` ailesine bağlandı. Klasörü yeniden adlandırmak, alışkanlıkları
ve olası yol referanslarını bozar — bu yüzden **klasör adı yerelde kalır**,
yalnızca depo ve ürün kimliği `yt-mini-lite` olur.

## Kural

1. **Yerel klasör adı hiçbir zaman kodda referans verilmez** — kod her zaman
   çalıştırılabilirin kendi konumuna göre göreli çözümleme yapar
   (`GetModuleFileNameW` + `bin\` araması, bkz. `engine.c: find_binary`).
   Yani klasörün adı önemsizdir; istenirse güvenle yeniden adlandırılabilir.
2. Kod içindeki kimlik dizeleri (mutex, registry, pencere sınıfı, başlıklar)
   `yt-mini-lite` geçişinde **tek commit'te, tamamı birden** güncellenmelidir
   (bkz. TODO.md #6).
3. `bootstrapper.c` içindeki `FFMPEG_URL` eski `yt-mini` deposunun
   `v2.0.0-beta` release'ine bağlıdır; yt-mini-lite'ta ilk release yayınlandığı
   anda yeni URL'e taşınır (bkz. TODO.md #4).
