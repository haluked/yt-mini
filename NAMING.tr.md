# Adlandırma Notu (NAMING.md)

**Türkçe** · [English](NAMING.md)

## Durum

| Yer | İsim | Açıklama |
|---|---|---|
| GitHub deposu | **`haluked/yt-mini`** | Projenin tek evi. Güncel sürüm `v2.4.0`. |
| Ürün | **`ff-dlp-viewer`** | Pencerede, kurulumda ve belgelerde görünen isim. |
| Yerel geliştirme klasörü | **`ff-dlp-viewer`** | Mevcut yollar ve alışkanlıklar bozulmasın diye klasör adı korunuyor. |
| İkili dosya adı | `ff-dlp-viewer.exe` | `build.bat` çıktısı. |

## Tek aktif sürüm

Yerel C11/Win32 derlemesi, bakımı yapılan tek sürümdür. Eski Python/PyInstaller
prototipleri emekliye ayrıldı; yalnızca git geçmişinde duruyorlar. Güncel ağaçta
derlenmiyor, paketlenmiyor ve belgelenmiyorlar.

Sürüm numarası `v2.4.0`'den ileri gider. Tek aktif sürüm çizgisi vardır.

## Neden iki isim var?

Proje `ff-dlp-viewer` adıyla başladı; depo daha sonra `yt-mini` olarak açıldı. Yerel
klasörü yeniden adlandırmak yolları ve alışkanlıkları bozacağı için yalnızca depo
`yt-mini` adını taşıyor. Ürünün adı her zaman `ff-dlp-viewer`.

## Kurallar

1. **Yerel klasör adı hiçbir zaman kodda referans verilmez** — kod her zaman
   çalıştırılabilirin kendi konumuna göre çözer (`GetModuleFileNameW` + `bin\` araması,
   bkz. `engine.c: find_binary`). Klasör güvenle yeniden adlandırılabilir.
2. Koddaki kimlik dizeleri (mutex, registry anahtarı, pencere sınıfı, başlıklar) hep
   **`ff-dlp-viewer`** kullanır. Tüm kod tabanında tutarlı kalmalıdır.
3. `bootstrapper.c` içindeki `FFMPEG_URL`, en son GitHub sürümünün `ffmpeg.exe`
   asset'ini gösterir. Her sürüm bu asset'i içermelidir (bkz. `RELEASE_CHECKLIST.md`).
