"""Uygulama genelinde kullanılan Türkçe metinler (i18n uyumlu).

Yazım kuralları:
- Butonlar eylemi adlandırır ("İndir", "Bilgi Getir", "Klasörü Aç").
- Teknik terimler (container, codec) sadeleştirilir: "konteyner", "kodek".
- Aile fertlerinin anlayacağı yalın dil; kısaltma ve jargon yok.
"""

APP_NAME = "yt-mini"
APP_SUBTITLE = "Hafif Video & Ses İndirici (v2.0)"

# URL Alanı
URL_PLACEHOLDER = "Bağlantıyı buraya yapıştırın..."
BTN_PASTE = "Yapıştır"
BTN_FETCH = "Bilgi Getir"
BTN_FETCH_BUSY = "Getiriliyor"
FETCHING_INFO = "Medya bilgileri çözümleniyor..."
HINT_SUPPORTED = "YouTube · Instagram · TikTok · X bağlantıları desteklenir"

# Boş durum (henüz bağlantı yok)
EMPTY_TITLE = "Henüz bir bağlantı yok"
EMPTY_STEPS = [
    "Bağlantıyı üstteki kutuya yapıştır",
    "“Bilgi Getir”e bas, video bilgileri gelsin",
    "Kaliteyi seç, “İndir”e bas",
]

# Sekmeler
TAB_VIDEO = "Video"
TAB_AUDIO = "Sadece Ses"

# Video Sekmesi
LABEL_RESOLUTION = "Kalite"
RES_BEST = "En İyi (Otomatik)"
CHECK_FORCE_MP4 = "MP4 olarak kaydet"
MP4_WARNING_VP9 = "Kaynakta bu çözünürlükte VP9 var. MP4 için H.264 dönüştürmesi yapılacaktır."
MP4_WARNING_OPUS = "Opus ses akışı MP4 uyumu için AAC'ye dönüştürülecektir."

# Ses Sekmesi
LABEL_AUDIO_FORMAT = "Format"
AUDIO_OPUS = "Opus (En Yüksek / Orijinal)"
AUDIO_AAC = "AAC (Geniş Uyum / m4a)"
AUDIO_MP3 = "MP3 (Evrensel Uyum)"

# Ses formatı çipleri: (değer, etiket). Etiket "sonuç" dilinde: aile dili.
AUDIO_CHOICES = [
    ("opus", "Orijinal (Opus)"),
    ("aac", "Uyumlu (M4A)"),
    ("mp3", "MP3"),
]

# Not şeridi
NOTE_STREAM_COPY = "Akış olduğu gibi kopyalanır, yeniden kodlanmaz."
NOTE_AUDIO_COPY = "Ses olduğu gibi kopyalanır, yeniden kodlanmaz."
NOTE_AUDIO_ENCODE_MP3 = "MP3 için ses yeniden kodlanacak (küçük kalite kaybı)."
NOTE_AUDIO_ENCODE_AAC = "AAC için ses yeniden kodlanacak."

# Gelişmiş
ADVANCED_TITLE = "Gelişmiş Seçenekler (Deneysel)"
ADVANCED_CONTAINER = "Zorunlu Konteyner:"
CONTAINER_AUTO = "Otomatik (En Uyumlu)"

# İndirme ve Durum
BTN_DOWNLOAD = "İndir"
BTN_CANCEL = "İptal"
BTN_OPEN_FOLDER = "Klasörü Aç"
STATUS_READY = "Bağlantı bekleniyor."
STATUS_READY_MEDIA = "Hazır. Kaliteyi seçip “İndir”e basın."
STATUS_DOWNLOADING = "İndiriliyor..."
STATUS_PROCESSING = "Akışlar birleştiriliyor / işleniyor..."
STATUS_FINISHED = "İndirme tamamlandı."
STATUS_CANCELLED = "İndirme iptal edildi."

# Hata ve Bilgilendirme
ERR_EMPTY_URL = "Lütfen geçerli bir video veya ses bağlantısı girin."
ERR_INVALID_URL = "Girdiğiniz bağlantı geçerli bir web adresi değil."
CONFIRM_EXIT_DOWNLOADING = "Şu anda devam eden bir indirme işlemi var.\n\nUygulamadan çıkarsanız indirme iptal edilecektir. Yine de çıkmak istiyor musunuz?"
CONFIRM_TITLE = "Çıkış Onayı"

# Sistem Tepsisi (Tray)
TRAY_OPEN = "yt-mini'yi Aç"
TRAY_CHECK_UPDATES = "Güncellemeleri Denetle..."
TRAY_EXIT = "Tamamen Çık"
TRAY_DOWNLOADING_TOOLTIP = "yt-mini (İndiriliyor...)"
TRAY_IDLE_TOOLTIP = "yt-mini (Boşta)"
