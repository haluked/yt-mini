# Gelecek Planları ve Yol Haritası (TODO)

## 1. Pano Ön-Çekim Belleği ve Zaman Aşımı Yönetimi (Clipboard Fetch Memory Cache)
- **Açıklama:** Kullanıcı YouTube bağlantısını kopyaladığında arka planda çekilen video/medya bilgileri hemen silinmeyip birkaç dakika (örn. 5-10 dakika) boyunca bellekte güvenli şekilde önbelleğe alınacak.
- **Kullanım Senaryosu:**
  - Kullanıcı 4 saniyelik açılır bildirim (toast popup) çıktığında meşgulse veya bildirime yetişemezse, video bilgileri kaybolmayacak.
  - Kullanıcı daha sonra sistem tepsisindeki (tray) simgeye veya görev çubuğuna tıkladığında "Son Kopyalanan Medya: [Video Adı]" seçeneğini görecek ve tek tıkla indirmeye devam edebilecek.
  - Süre dolduğunda veya yeni bir bağlantı kopyalandığında eski önbellek temizlenerek bellek sızıntısı önlenecek.
