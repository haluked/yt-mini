"""Kırtasiye defteri tasarım sistemi: pastel kağıt, mürekkep ve fosforlu kalem.

Renk mantığı:
- Zemin sıcak kağıt (PAPER), çukur alanlar biraz daha koyu kağıt (PAPER_DEEP).
- Tüm metin ve çizgiler mürekkep tonlarında (INK / INK_SOFT / INK_FAINT).
- Vurgu rengi tek: fosforlu kalem sarısı (HILITE) -> sadece "seçili/aktif" durumlar.
- Eylem renkleri (SAGE / SKY / CORAL) sadece butonlar ve durum bilgisi için.
"""

# ------------------------------------------------------------------ Kağıt / mürekkep
PAPER = "#FAF5E9"          # Ana zemin: sıcak defter kağıdı
PAPER_DEEP = "#F1E9D7"     # Çukur alanlar, pasif zeminler
PAPER_LINE = "#E4D8BE"     # İnce kural çizgileri (defter çizgisi)
WHITE = "#FFFDF6"          # Giriş kutusu yüzeyi (kirli beyaz)

INK = "#28271F"            # Ana metin, çerçeve mürekkebi
INK_SOFT = "#635C4E"       # İkincil metin (6.1:1 kontrast)
INK_FAINT = "#9A9182"      # Sadece pasif/devre dışı metin

MARGIN_RED = "#D98E7C"     # Defterin sol kenar çizgisi
HILITE = "#F6DE8A"         # Fosforlu kalem sarısı (seçili durum)
FOCUS = "#3D6B85"          # Klavye odağı halkası (kağıt üzerinde 5.2:1)

# ------------------------------------------------------------------ Eylem renkleri
SAGE = "#8CBE9C"           # Birincil eylem: İndir
SAGE_DEEP = "#6FA37F"      # Koyu adaçayı (ilerleme dolgusu, metin)
SKY = "#93BCD2"            # Bilgi eylemi: Bilgi Getir
SKY_DEEP = "#6E9CB5"       # Odak çerçevesi
AMBER = "#EFC787"          # Uyarı şeridi
CORAL = "#E2937F"          # İptal / hata
CORAL_DEEP = "#C97A66"

# ------------------------------------------------------------------ Tipografi
FONT_FAMILY = "Segoe UI"
FONT_MONO = "Consolas"

FONT_TITLE = (FONT_FAMILY, 13, "bold")     # Uygulama adı
FONT_SECTION = (FONT_FAMILY, 10, "bold")   # Bölüm başlıkları
FONT_BODY = (FONT_FAMILY, 9, "normal")     # Gövde metni
FONT_BODY_BOLD = (FONT_FAMILY, 9, "bold")  # Buton ve vurgu metni
FONT_CAPTION = (FONT_FAMILY, 8, "normal")  # Küçük açıklamalar
FONT_META = (FONT_MONO, 9, "normal")       # Teknik veri satırları
FONT_LABEL = (FONT_MONO, 9, "bold")        # Büyük harf etiketler
FONT_BADGE = (FONT_MONO, 8, "bold")        # Rozetler

# ------------------------------------------------------------------ Ölçüler (8px ızgara)
WINDOW_WIDTH = 520
WINDOW_HEIGHT = 529       # Yedek değer; gerçek yükseklik içerikten ölçülür

PAD_WINDOW = 16
PAD_GAP = 14
PAD_INTERNAL = 8

MARGIN_RAIL = 34           # Sol defter kenar boşluğu
CONTENT_HEIGHT = 196       # Medya bölümünün sabit yüksekliği (zıplama olmasın diye)

BORDER_THICK = 2
BORDER_THIN = 1
