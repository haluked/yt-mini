# Tasarım Şartnamesi (DESIGN.md) — Game Boy 8-bit

<!-- impeccable:design-schema 1 -->

## 1. Görsel kimlik

Uygulama, **Game Boy DMG** ekranı gibi davranır: yalnızca dört yeşil ton, keskin
pikseller, sert çerçeveler ve sistem fontu. Amaç nostaljik bir oyun konsolu hissi
vermek; süslemek değil. Her öğe ya bir işlevi yerine getirir ya da yer kaplamaz.

**Fiziksel sahne:** Akşam evde, mutfak masasında, tek elle bağlantı yapıştıran bir
aile üyesi. Bu yüzden yazılar büyük ve net, hedef alanlar rahat tıklanır, pencere
sabit ve öngörülebilirdir.

### Temel kurallar
1. **Dört ton:** Ara renk yok. Gerektiğinde 1 px dama deseni (dither) kullanılır.
2. **Keskin geometri:** Köşe yuvarlatma yok. Çerçeve ve gölge 2 px, sert.
3. **Gölge = 2 px kayma:** Bulanık gölge, gradient, parlama yok.
4. **Özel çizim:** Buton, chip, checkbox, açılır liste, ilerleme çubuğu, bildirim,
   onay penceresi ve başlık çubuğu `BS_OWNERDRAW`/GDI ile çizilir. Yerel
   `COMBOBOX`, `BUTTON` (radio/checkbox) ve `PROGRESS_CLASS` kullanılmaz.
   Tek istisna: URL giriş kutusu (`EDIT`), çünkü IME, pano ve metin seçimi
   güvenilirliği için yerel denetim gerekir.
5. **Harici varlık yok:** Görsel, ikon dosyası veya font taşınmaz. Tray ikonu
   dışında her şey kodla çizilir.
6. **Sabit yerleşim:** Pencere 646×485 tasarım birimidir; içerik değişince
   zıplamaz. Yalnızca DPI ile ölçeklenir.

---

## 2. Palet

| Token | Hex | Rol |
| :--- | :--- | :--- |
| `GB_DARKEST` | `#0F380F` | Metin, çerçeve, seçili yüzey, dolu ilerleme |
| `GB_DARK` | `#306230` | Buton yüzü, hover, ayırıcı, ikincil yüzey |
| `GB_LIGHT` | `#8BAC0F` | Sayfa zemini, çukur alan, pasif dama |
| `GB_LIGHTEST` | `#9BBC0F` | Panel ve giriş yüzeyi, açık metin |

Kontrast: `GB_DARKEST` / `GB_LIGHTEST` ≈ **6.0:1** (gövde metni için yeterli).
`GB_DARK` üzerine `GB_LIGHTEST` ≈ **3.7:1**; bu yüzden küçük metin asla
`GB_DARK` zemin üzerine yazılmaz. Buton hover'ında metin `GB_LIGHTEST` olsa da
buton yazısı 12-14 px kalın olduğu için kabul edilir; kritik metin değildir.

---

## 3. Tipografi

Sistemde kurulu **Consolas** kullanılır; `NONANTIALIASED_QUALITY` ile çizilir.
Böylece hem 8-bit keskinliği elde edilir hem Türkçe karakterler (ğ, ş, ı, İ, ç,
ö, ü) garanti olur ve paket boyutu artmaz.

| Token | Tanım | Kullanım |
| :--- | :--- | :--- |
| `FONT_TITLE` | Consolas 20 bold | Uygulama adı |
| `FONT_BOLD` | Consolas 14 bold | Bölüm başlığı, chip, önemli buton |
| `FONT_BODY` | Consolas 14 | Gövde, medya başlığı |
| `FONT_SMALL` | Consolas 12 | Teknik veri, küçük butonlar, not |

Hiyerarşi: **uygulama adı → bölüm şeridi → medya başlığı → gövde → teknik veri.**

---

## 4. Ölçü ve yerleşim

- **Pencere:** 646×485 istemci alanı; sabit boyut, kendi başlık çubuğu.
- **Başlık çubuğu:** 44 px. Sürüklenebilir (`HTCAPTION`), sağda küçült/kapat
  düğmeleri (26×24). Platform rozeti düğmelerin solunda.
- **Konsol çerçevesi:** 6 px `GB_DARK` dış çerçeve + 1 px `GB_DARKEST` iç çizgi.
- **8 px ızgara:** dış boşluk 16-22 px, bölüm arası 14 px, iç boşluk 8 px.
- **Bölüm panelleri:** `GB_LIGHTEST` zemin, 2 px `GB_DARKEST` çerçeve; sol üstte
  22 px yüksekliğinde koyu başlık şeridi (`1 BAĞLANTI`, `2 MEDYA VE FORMAT`,
  `3 İNDİRME DURUMU`). Şerit, panelin üst çizgisine yapışıktır.
- **Sabit medya yüksekliği:** 212 px; boş durum, künye, seçenekler ve not bu
  alanı paylaşır. Pencere içerikle birlikte büyümez.

---

## 5. Bileşen dili

### A. Buton (`gb_draw_control`, rol `PUSH` / `PRIMARY` / `DANGER`)
Yüz `GB_LIGHTEST` (birincil/tehlikeli eylemde `GB_DARKEST`), 2 px `GB_DARKEST`
çerçeve, sağ-alt 2 px `GB_DARK` sert gölge. Basılınca yüz `GB_DARK` olur ve içerik
1 px aşağı kayar. Pasifken zemin `GB_LIGHT` dama deseniyle doldurulur.
Hover'da yüz `GB_DARK`, yazı `GB_LIGHTEST`. Odakta 1 px iç çerçeve.

### B. Seçim çipi (`CHIP`)
Video / Ses seçimi. Seçili: `GB_DARKEST` zemin + `GB_LIGHTEST` yazı. Seçili
değil: `GB_LIGHTEST` zemin + `GB_DARK` çerçeve. Hover: `GB_LIGHT`.

### C. Onay kutusu (`CHECK`)
14×14 px kare; `GB_LIGHTEST` zemin, 2 px `GB_DARKEST` çerçeve. İşaretliyken içi
`GB_DARKEST` olur ve 2 px'lik piksel onay işareti `GB_LIGHTEST` ile çizilir.

### D. Açılır liste (`DROP` + `gb_dropdown_open`)
Kapalı hâlde buton gibi görünür; sağda piksel aşağı oku vardır. Tıklanınca
`WS_POPUP` bir liste açılır: `GB_LIGHTEST` zemin, 2 px çerçeve, 24 px satır.
Seçili satır `GB_DARKEST`/`GB_LIGHTEST`, hover `GB_DARK`/`GB_LIGHTEST`, seçili
satırın solunda 2×4 px işaret. `↑/↓`, `Enter`, `Esc` ve dışına tıklama desteklenir.

### E. İlerleme çubuğu
`GB_LIGHT` kanal, 2 px `GB_DARKEST` çerçeve. Dolgu, 7 px blok + 2 px boşluk
ritmiyle `GB_DARKEST` bloklardan oluşur. Animasyon yok; gerçek veriyle güncellenir.

### F. URL girişi
Yerel `EDIT` denetimi; çevresinde 2 px `GB_DARKEST` çerçeve. Zemin `GB_LIGHTEST`,
metin `GB_DARKEST`, yatay iç boşluk 8 px. Odaklandığında imleç `GB_DARKEST`.

### G. Bildirim (toast)
370×96 px, `GB_LIGHTEST` zemin, 2 px çerçeve. Üstte 22 px `GB_DARKEST` şerit:
platform rozeti (`GB_DARK`), süre, sağda `X`. Altında başlık, "İNDİRMEYE HAZIR -
4 SN SONRA KAPANIR" ve sağda `İNDİR` butonu. 4 saniye sonra kapanır; fare üzerine
gelince süre sıfırlanır.

### H. Modal mesaj kutusu (`gb_message_box`)
Sahibinin ortasında açılır. `GB_LIGHTEST` zemin, 2 px çerçeve, 26 px
`GB_DARKEST` başlık şeridi, solda 30×30 ikon karesi (`X`, `!`, `?`). Altta sağa
yaslı butonlar: `TAMAM`, `EVET`, `HAYIR`, `İPTAL`. `Enter` varsayılanı, `Esc`
iptali tetikler. Sahip pencere modal sırasında devre dışıdır.

---

## 6. Durumlar

| Durum | Görünüm |
| :--- | :--- |
| Boş | Medya panelinde "Bağlantıyı girin veya yapıştırın." + desteklenen platformlar |
| Getiriliyor | `Bilgi Getir` → `Çekiliyor...`, buton pasif, durum "Bilgi getiriliyor..." |
| Hazır | Başlık + süre/platform/akış sayısı + hedef konteyner notu; `İNDİR` etkin |
| İndiriliyor | Bloklar dolar; `%`, hız, kalan süre ve `MB / MB` durum satırında |
| İşleniyor | Çubuk %100, "Akışlar birleştiriliyor ve konteyner tamamlanıyor..." |
| Tamamlandı | "Dosya başarıyla kaydedildi.", `Dosya Konumunu Aç` görünür |
| Hata | `X` ikonlu modal mesaj kutusu; durum satırında hata özeti |
| Pasif | Dama desenli zemin, `GB_DARK` çerçeve/yazı, tıklama yok |

---

## 7. Yasaklar

- Dört ton dışında renk; gradient, blur, parlama, yumuşak gölge.
- Yuvarlatılmış köşe; kart içinde kart.
- Yerel `COMBOBOX`, radio/checkbox, `PROGRESS_CLASS` (URL `EDIT` istisnadır).
- Emoji veya unicode simgeyle ikon taklidi.
- Dekoratif animasyon, açılışta sıralı giriş efekti.
- Harici görsel/font dosyası eklemek (boyut ve determinizm).
- Küçük metni `GB_DARK` zemin üzerine yazmak (kontrast).

---

## 8. Uygulama notları

- Palet ve çizim ilkelleri: `src/gb_ui.h`, `src/gb_ui.c`.
- Ana pencere, yerleşim ve durum yönetimi: `src/ui.c`.
- Ölçekleme: `gb_scale()` / `S()` — 96 DPI taban, Per-Monitor V2.
- Boyut bütçesi: **< 1 MB zorunlu**; mevcut derleme ~337 KB.
- Yeni bileşen eklerken: rengi paletten seç, 2 px çerçeve kullan, beş durumu
  (normal, hover, basılı, pasif, odak) çiz, metni Türkçe tut.
