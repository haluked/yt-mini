# Tasarım Şartnamesi (DESIGN.md)

<!-- impeccable:design-schema 1 -->

## 1. Görsel kimlik: "Kırtasiye Defteri"

Uygulama, üzerine fosforlu kalemle işaretler konmuş bir **defter sayfasıdır**. Kullanıcı
sayfayı yukarıdan aşağıya sıralı bir form gibi okur: bağlantı → medya → seçenek → indir.

Bu dünya bilinçli bir karşı çıkıştır: kart içinde kart, her öğeyi 2px siyah çerçeveye
alan "çizim programı taslağı" görünümü reddedilir. Derinlik gölgeyle değil, **tek piksel
çizgiyle** anlatılır.

**Fiziksel sahne:** Akşam evde, mutfak masasında, tek elle bağlantı yapıştıran bir aile
üyesi. Zemin bu yüzden sıcak kağıt; metin matbaa mürekkebi; vurgu gerçek bir fosforlu
kalemin bıraktığı iz.

### Temel kurallar
1. **Keskin geometri:** Köşe yuvarlatma yok (0 px). Bütün çerçeveler 1 px.
2. **Gölge yok:** Yükselti/çukur hissi kenarlık rengi ve zemin tonuyla verilir. Blur,
   gradient, parlama (glow) kullanılmaz.
3. **Tek vurgu:** Fosforlu sarı (`HILITE`) yalnızca *şu an seçili/aktif olan* anlamına
   gelir. Dekorasyon için kullanılmaz.
4. **Yerli bileşen yok:** `tk.Checkbutton`, `tk.Radiobutton`, `tk.Menubutton` gibi
   işletim sisteminin çizdiği kontroller kullanılmaz; hepsi `ui/widgets.py` içinde
   yeniden çizilir. Böylece tek bir görsel dil korunur.
5. **Ölü alan yok:** Medya bölümünün yüksekliği sabittir; boş durum bu alanı öğretici
   metinle doldurur, pencere içerik değişince zıplamaz.

---

## 2. Neden Tkinter (mimari gerekçe)

| Kriter | PyQt6 / PySide6 | CustomTkinter | Saf Tkinter (özel piksel tema) |
| :--- | :--- | :--- | :--- |
| Boşta RAM (pencere açık) | 65–85 MB | 32–48 MB | **14–22 MB** |
| Boşta RAM (tray modu) | 40–55 MB | 25–35 MB | **8–14 MB** |
| Başlangıç | ~1.2 sn | ~0.6 sn | **~0.15 sn** |
| Paket boyutu | +45–60 MB | +10–15 MB | **+0 MB** |

Tkinter standart kütüphanededir, ek DLL taşımaz. Tray'e küçültülünce pencere
`withdraw()` edilir, görseller bellekten atılır (`gc.collect()`), RAM ~10–15 MB'a iner.
Piksel estetiği zaten keskin kenarlık ve düz renk bloklarından oluştuğu için Tkinter'ın
çizim modeliyle doğal olarak uyuşur.

---

## 3. Renk paleti

| Token | Hex | Rol |
| :--- | :--- | :--- |
| `PAPER` | `#FAF5E9` | Ana zemin: sıcak defter kağıdı |
| `PAPER_DEEP` | `#F1E9D7` | Çukur alanlar, boş durum kutusu, pasif zemin |
| `PAPER_LINE` | `#E4D8BE` | 1 px kural çizgileri, pasif kenarlık |
| `WHITE` | `#FFFDF6` | Giriş kutusu yüzeyi (kirli beyaz) |
| `INK` | `#28271F` | Ana metin ve çerçeve mürekkebi |
| `INK_SOFT` | `#635C4E` | İkincil metin (kağıt üzerinde 6.1:1) |
| `INK_FAINT` | `#9A9182` | Sadece pasif/devre dışı metin ve dekor |
| `MARGIN_RED` | `#D98E7C` | Defterin sol kenar çizgisi |
| `HILITE` | `#F6DE8A` | Fosforlu kalem: seçili durum, monogram, rozet |
| `SAGE` / `SAGE_DEEP` | `#8CBE9C` / `#6FA37F` | Birincil eylem (`İndir`), ilerleme dolgusu |
| `SKY` / `SKY_DEEP` | `#93BCD2` / `#6E9CB5` | Bilgi eylemi (`Bilgi Getir`), odak çerçevesi |
| `AMBER` | `#EFC787` | Uyarı şeridi (dönüştürme gereken durumlar) |
| `CORAL` / `CORAL_DEEP` | `#E2937F` / `#C97A66` | `İptal`, hata |

Renk stratejisi **Restrained**: nötr kağıt + tek vurgu (fosforlu sarı) + yalnızca
eylemlerde görünen üç durum rengi. Renk asla dekorasyon değildir.

Kontrast: gövde ve yer tutucu metin ≥ 4.5:1 (ölçüldü: `INK`/`PAPER` 13.9:1,
`INK_SOFT`/`PAPER` 6.1:1, `INK`/`HILITE` 11.3:1, `INK`/`SAGE` 7.1:1, `INK`/`SKY` 7.4:1).

---

## 4. Tipografi

Sistemde kurulu fontlar kullanılır; harici font dosyası yüklenmez (RAM ve paket boyutu).

| Token | Tanım | Kullanım |
| :--- | :--- | :--- |
| `FONT_TITLE` | Segoe UI 13 bold | Uygulama adı |
| `FONT_SECTION` | Segoe UI 10 bold | Diyalog başlıkları |
| `FONT_BODY` | Segoe UI 9 | Gövde, açıklama, medya başlığı (bold varyantı) |
| `FONT_CAPTION` | Segoe UI 8 | Küçük açıklamalar, not şeridi |
| `FONT_LABEL` | Consolas 9 bold | BÜYÜK HARF bölüm etiketleri |
| `FONT_META` | Consolas 8 | Teknik veri: süre, kodek, hız, boyut, çipler |
| `FONT_BADGE` | Consolas 8 bold | Platform rozeti |

Hiyerarşi sırası: **uygulama adı (13 bold) → bölüm etiketi (9 mono bold + sarı alt çizgi)
→ medya başlığı (9 bold) → gövde (9) → teknik veri (8 mono, soluk)**. Teknik veri her
zaman mono ve soluk; böylece göz önce başlığı, sonra ayrıntıyı görür.

---

## 5. Ölçü ve yerleşim

- **Pencere:** 520 px genişlik; yükseklik içerikten ölçülür (≈531 px). Yeniden
  boyutlandırma kapalı — yerleşim deterministik kalır.
- **8 px ızgara:** dış boşluk 16 px, bölüm arası 14 px, iç boşluk 8 px.
- **Sol cilt boşluğu:** 34 px boş kağıt + 1 px kırmızı kenar çizgisi; içerik buradan
  18 px sağda başlar (defterin yazı alanı).
- **Sabit medya yüksekliği:** `CONTENT_HEIGHT = 196 px`. Boş durum, medya künyesi,
  sekmeler ve seçenekler bu alanı paylaşır. Böylece bağlantı getirildiğinde pencere
  zıplamaz, altta ölü alan kalmaz.
- **Esnek boşluk:** Ses sekmesinde `media_spacer` kalan yüksekliği yutar; not şeridi
  bölümün dibine yaslanır.
- **Dikey ritim:** her bölümün üstünde 14 px boşluk, altında 1 px kural çizgisi.

---

## 6. Bileşen dili (`ui/widgets.py`)

### A. Buton (`PixelButton`)
1 px mürekkep çerçeve + düz dolgu. Basılınca dolgu koyulaşır, gölge/oyma yok.
Tonlar: `primary` (adaçayı), `info` (gök mavisi), `neutral` (kağıt), `danger` (mercan),
`quiet`. Her buton şu durumları bilir: normal, üzerine gelme, basılı, pasif, klavye
odağı (çerçeve `SKY_DEEP`), `Return`/`Space` ile tetikleme.

### B. Giriş kutusu (`PixelEntry`)
1 px `INK_FAINT` çerçeve, `WHITE` zemin, 9/7 px iç boşluk. Odaklandığında çerçeve
`SKY_DEEP` olur. Yer tutucu metin `INK_SOFT` ile yazılır (kontrast ≥ 4.5:1), kutu
boşken geri gelir. Seçim rengi `HILITE`.

### C. Seçim çipi (`PixelChip`)
Küçük, tek seçimlik etiket (çözünürlük, ses formatı, Video/Ses sekmesi).
Seçili: `HILITE` zemin + `INK` çerçeve + bold metin. Seçili değil: `PAPER` zemin +
`PAPER_LINE` çerçeve + `INK_SOFT` metin. Üzerine gelince `PAPER_DEEP`.

### D. Onay kutusu (`PixelCheck`)
14×14 px kare. İşaretliyken `HILITE` zemin + `INK` onay işareti. Yerli `tk.Checkbutton`
kullanılmaz.

### E. İlerleme çubuğu (`PixelProgressBar`)
1 px çerçeve, `PAPER_DEEP` kanal, 6 px blok + 2 px boşluk ritmiyle dolan `SAGE_DEEP`
bloklar. Yüzde metni ayrı satırda mono yazılır. Animasyon yok; ilerleme gerçek veriyle
güncellenir.

### F. Bölüm başlığı (`SectionHeading`)
`FONT_LABEL` büyük harf etiket + altında 3 px `HILITE` çizgi. Bölüm sırası numaralanmaz;
sıra konumla anlatılır.

### G. Not şeridi
`PAPER_DEEP` zemin + ikon + `FONT_CAPTION` metin. Durum uyarısı olduğunda zemin `AMBER`
olur ve metin dönüştürme bilgisini verir. Yükseklik sabittir (36 px) — içerik değişince
sayfa kaymaz.

### H. Önizleme yuvası (`PixelThumb`)
118×66 px, kesikli 1 px çerçeve, ortada "önizleme yok" mono yazısı. Görsel gelince
çerçevesiz yerleştirilir.

### I. Rozet (`PixelBadge`)
Fosforlu sarı zeminli mono etiket: `WEB`, `YOUTUBE`, `TIKTOK`, `INSTAGRAM`, `X`.

### J. Diyalog (`PixelMessageBox`)
Tek kağıt yaprağı: 1 px mürekkep çerçeve, başlıkta durum renginde 9×9 px kare, gövde
`INK_SOFT`, altta 1 px kural ve sağa yaslı butonlar. Kart içinde kart yok.
`Escape` kapatır, `Return` onaylar.

---

## 7. Durumlar

| Durum | Görünüm |
| :--- | :--- |
| Boş | `PAPER_DEEP` kutu, ortada "Henüz bir bağlantı yok" + ne yapılacağını söyleyen 2 satır |
| Getiriliyor | `Bilgi Getir` → "Getiriliyor", buton pasif, durum satırı "Medya bilgileri alınıyor..." |
| Hazır | Künye + seçenekler + not şeridi; `İndir` etkin |
| İndiriliyor | Blok çubuk dolar, `%64 · 12.4 MB/s` solda, `45.0 MB / 70.0 MB` sağda, `İptal` etkin |
| İşleniyor | Çubuk %100, "Akışlar birleştiriliyor / işleniyor..." |
| Tamamlandı | "İndirme tamamlandı.", dosya adı sağda, `Klasörü Aç` etkin |
| Hata | `CORAL` başlıklı diyalog; durum satırı "Hata oluştu." |
| Pasif | `PAPER_DEEP` dolgu, `PAPER_LINE` çerçeve, `INK_FAINT` metin, tıklama yok |

---

## 8. Yasaklar

- Gradient, gölge, blur, parlama, yuvarlatılmış köşe.
- İşletim sisteminin çizdiği kontrol (checkbutton, radiobutton, menubutton, combobox).
- Kart içinde kart; her öğeyi kalın siyah çerçeveye alma.
- Emoji veya unicode simgeyle ikon taklidi.
- Dekoratif animasyon; sayfa açılışında sıralı giriş efekti.
- Fosforlu sarıyı dekorasyon olarak kullanmak (yalnızca seçili/aktif durum).
- Aynı ekranda ikiden fazla farklı buton tonu.

---

## 9. Sistem tepsisi ikonu

32×32 piksel-art indirme ikonu (`tray/tray_manager.py`): mürekkep çerçeve, kağıt tepsi,
adaçayı (boşta gök mavisi) ok. Programatik çizilir; harici görsel dosyası yoktur.
İndirme başlayınca ikon ve ipucu metni güncellenir.

---

## 10. Yeni bileşen eklerken

1. Rengi `ui/theme.py` içindeki tokenlardan seç; hex'i doğrudan yazma.
2. Derinliği 1 px kenarlıkla anlat; gölge ekleme.
3. Beş durumu da (normal, hover, basılı, pasif, odak) uygula.
4. Metni `ui/strings.py` içine ekle; arayüz kodunda gömülü Türkçe metin bırakma.
5. Yerleşimi sabit yükseklikli bir bölgeye koy; pencere boyutu içerikle değişmesin.
