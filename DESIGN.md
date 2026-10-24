# Tasarım Şartnamesi (DESIGN.md)

<!-- impeccable:design-schema 1 -->

## 1. Tasarım Felsefesi ve Görsel Kimlik

Bu uygulamanın görsel dili: **"Sade Piksel & Pastel Masaüstü" (Pixel-Art / Retro-Modern Utility)**.
Modern web ve yapay zeka arayüzlerinin klişeleşmiş öğeleri (yoğun gradient'ler, mor/mavi neon parlamalar, aşırı yuvarlatılmış hap butonlar, bulanık cam/blur efektleri, emojiler) tamamen reddedilmiştir.

### Temel Prensipler
1. **Keskin ve Dürüst Geometri**: Köşe yuvarlatma (border-radius) 0 pikseldir. Çerçeveler keskin, dürüst ve piksel seviyesinde belirlidir (1px veya 2px katı kenarlıklar).
2. **Yumuşak Pastel Palet**: Gözü yormayan, düşük doygunluklu ama kontrastı yüksek pastel tonlar.
3. **Maksimum Alan Verimliliği (Spacious yet Compact)**: Öğeler birbirine yapışık değildir, ritmik bir boşluk (8px ızgara) sistemiyle nefes alır; ancak gereksiz devasa başlıklarla ekranı işgal etmez.
4. **İşlevsel Tipografi**: Monospace veya nötr geometrik sans-serif fontlar ile okunabilir hiyerarşi.

---

## 2. GUI Kütüphanesi Seçimi ve Bellek (RAM) Analizi

Haluk'un talimatı doğrultusunda PyQt6, CustomTkinter ve Saf Tkinter/ttk karşılaştırılmıştır:

| Kriter | PyQt6 / PySide6 | CustomTkinter | Saf Tkinter / Ttk (Özel Piksel Tema) |
| :--- | :--- | :--- | :--- |
| **Boşta RAM (Pencere Açık)** | 65 - 85 MB | 32 - 48 MB | **14 - 22 MB** |
| **Boşta RAM (Tray Modu / Pencere Kapalı)** | 40 - 55 MB | 25 - 35 MB | **8 - 14 MB** |
| **Başlangıç Süresi** | ~1.2 sn | ~0.6 sn | **~0.15 sn** |
| **Piksel Estetiği Uyumu** | Zorlama stil gerekir | Köşeler varsayılan yuvarlaktır, anti-alias kenarlık verir | **Doğal uyumlu** (keskin kenarlıklar, piksel çerçeveler, katı renk blokları) |
| **Paket Boyutu (.exe)** | +45-60 MB | +10-15 MB | **+0 MB** (Python ile gömülü gelir) |

### Karar ve Mimari Gerekçe:
**Seçim: Tkinter tabanlı Özel Piksel Bileşen Mimarisi (`tkinter` + özel çizimli piksel widget'lar).**
- Tkinter, Python'ın kendi standart kütüphanesidir; fazladan yüzlerce DLL taşımaz, çalıştırılabilir dosya boyutunu ve açılış süresini asgari düzeyde tutar.
- Tray'e küçültüldüğünde Tkinter penceresi `withdraw()` edilip ağır görseller bellekten atıldığında (garbage collection çağrısı ile) çalışan uygulamanın RAM ayak izi **10-15 MB** seviyesine iner.
- Butonlar, sekmeler ve ilerleme çubuğu için piksel-art tarzında keskin kenarlı (1px/2px solid border, 0 radius) ve katı pastel dolgulu özel bileşenler kullanılacaktır.

---

## 3. Renk Paleti (Pastel Piksel Sistemi)

Renkler retro el konsolları ve sıcak kağıt tonlarından esinlenen pastel palet olarak belirlenmiştir:

| Token Adı | Hex Kodu | Kullanım Alanı | Anlam ve İşlev |
| :--- | :--- | :--- | :--- |
| `bg-primary` | `#F4F1EA` | Ana pencere arka planı | Sıcak, göz yormayan ham kağıt pastel krem |
| `bg-surface` | `#FFFFFF` | Giriş kutuları, kart ve liste yüzeyleri | Saf beyaz, net kontrast |
| `bg-sunken` | `#EAE5DC` | Çukur alanlar, pasif zeminler, sekme çubuğu | Basık/oyuk hissi veren pastel bej |
| `border-dark` | `#2D2B2A` | Dış çerçeveler, aktif buton sınırları (2px) | Keskin kömür karası piksel kenarlık |
| `border-muted`| `#B8B0A2` | İkincil sınırlar, ayırıcılar (1px) | Yumuşak gri-bej çizgi |
| `text-main` | `#1E1D1D` | Başlıklar, birincil metinler | Yüksek okunabilirlikli derin koyu gri |
| `text-muted` | `#6E685F` | İkincil metinler, ipuçları, etiketler | Düşük kontrastlı açıklayıcı gri |
| `accent-green`| `#7DBA84` | "İndir" butonu, tamamlandı durumu, video sekmesi | Pastel adaçayı yeşili (güvenli, eylem) |
| `accent-blue` | `#78A1BB` | Bilgi kartları, URL getirme, ses sekmesi | Pastel gök mavisi (analiz ve bilgi) |
| `accent-amber`| `#E5B869` | Uyarılar, MP4 kodek uyumsuzluk bildirimleri | Pastel bal sarısı (dikkat ve onay) |
| `accent-coral`| `#DE7A68` | İptal butonu, hata bildirimleri | Pastel mercan kırmızı (tehlike, durdur) |
| `accent-hover`| `#C2BCB0` | Buton üzerine gelme (hover) hali | Belirgin basılabilir geri bildirim |

---

## 4. Tipografi Sistemi

- **Windows Sistem Fontu Tercihi**: `Segoe UI` (modern netlik) veya `Consolas / Courier New` (kod ve teknik veriler için).
- Yazı tipleri işletim sisteminde kurulu olanlardan seçilir; harici büyük font dosyaları yüklenerek bellek ve başlangıç süresi şişirilmez.
- **Hiyerarşi**:
  - **Başlık (Title)**: 13pt Bold, `#1E1D1D` (Kompakt ve net)
  - **Bölüm Başlıkları (Section)**: 11pt Bold, `#1E1D1D`
  - **Gövde Metni (Body)**: 10pt Regular, `#1E1D1D`
  - **Teknik Veri / Etiket (Mono/Meta)**: 9pt Regular `Consolas`, `#6E685F` (Kodek, bitrate, çözünürlük etiketleri için)
  - **Küçük İpuçları (Caption)**: 8.5pt Regular, `#6E685F`

---

## 5. Boşluk ve Düzen Sistemi (Spacing & Layout)

Tüm arayüz **8px ızgara (8pt grid)** temeline oturur:
- **Pencere Boyutu**: `520px` genişlik, `580px` yükseklik (küçük, zarif, ekranı kaplamayan taşınabilir boyut).
- **Dış Kenar Boşlukları (Window Padding)**: `16px`
- **Bileşenler Arası Düşey Boşluk (Stack Gap)**: `12px`
- **Bileşen İçi Boşluklar (Internal Padding)**:
  - Butonlar: `8px` dikey, `16px` yatay
  - URL Input: `10px` dikey, `12px` yatay
  - Kart içi: `12px` dört yön

---

## 6. Bileşen Tasarımları (Pixel Affordances)

### A. URL Giriş Çubuğu (URL Input Bar)
- Arka plan `#FFFFFF`, 2px keskin kenarlık `#2D2B2A`.
- Sağ tarafında gömülü piksel "Yapıştır & Getir" butonu (`#EAE5DC` pastel bej, 1px iç sınır).
- Odaklandığında (Focus): 2px kenarlık rengi `#78A1BB` (Pastel Mavi) olur.

### B. Medya Bilgi Kartı (Media Info Card)
- URL çözümlendiğinde beliren kompakt panel.
- Sol tarafta: `120x68px` video küçük resmi (thumbnail), 1px `#2D2B2A` kenarlık içinde.
- Sağ tarafta:
  - Satır 1: Başlık (en fazla 2 satır, kesme ile).
  - Satır 2: Kaynak platform rozeti (ör. `[YOUTUBE]`, `[TIKTOK]`, `[INSTAGRAM]`, `[X]`) - küçük piksel rozet.
  - Satır 3: Süre, Kaynak Video Kodeği, Kaynak Ses Kodeği, Konteyner (Örn: `1080p | VP9 + Opus -> .webm`).

### C. İki Modlu Sekmeler (Tabs: Video & Ses)
- Üstte yan yana 2 keskin piksel sekme: `[ [Video İndir] ]` ve `[ [Sadece Ses] ]`.
- Aktif Sekme: Arka plan `#F4F1EA`, alt çizgi açık (içerikle bütünleşik), 2px `#2D2B2A` kenarlık.
- Pasif Sekme: Arka plan `#EAE5DC`, metin `#6E685F`.

### D. Çözünürlük ve Format Seçici (Format Selector)
- **Video Sekmesinde**:
  - Çözünürlük radyo butonları veya piksel dropdown (`En İyi (Varsayılan)`, `4K`, `1440p`, `1080p`, `720p`, `480p`, `360p`). Yalnızca videoda mevcut olanlar listelenir.
  - "MP4 Olarak Kaydet" onay kutusu (Checkbox): İşaretlendiğinde, eğer kaynak VP9/Opus ise sarı uyarı kutusu belirir: *"Kaynak VP9/Opus içeriyor. MP4 için H.264/AAC akışına dönüştürülecektir."*
- **Ses Sekmesinde**:
  - Format seçimi: `Opus (En Yüksek / Orijinal)`, `AAC (Geniş Uyum)`, `MP3 (Evrensel Uyum)`.

### E. İlerleme ve Durum Çubuğu (Pixel Progress Bar)
- 2px `#2D2B2A` keskin dış çerçeve.
- İç dolgu: `#7DBA84` (Adaçayı yeşili) bloklar halinde veya kesintisiz katı dolgu.
- Altında tek satır durum metni: `%64.2 | 12.4 MB/s | Kalan: 00:18 | 45.2 MB / 70.0 MB`

### F. Sistem Tepsisi (Tray) İkonu ve Menüsü
- Piksel-art tarzında zarif 16x16 / 32x32 indirme ikonu (küçük disk/ok ikonu).
- Sol tık: Pencereyi aç / öne getir.
- Sağ tık menüsü:
  - **ff-dlp-viewer'ı Aç** (Varsayılan kalın)
  - ─── (Ayırıcı)
  - **Güncellemeleri Denetle...**
  - ─── (Ayırıcı)
  - **Tamamen Çık** (Aktif indirme varsa onay diyalogu ile)
