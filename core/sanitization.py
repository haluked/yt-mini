"""Dosya adı ve yol güvenliği (sanitization) modülü.

Windows dosya sistemi kısıtlamaları:
- Yasak karakterler: < > : " / \\ | ? * ve kontrol karakterleri (0-31)
- Ayrılmış dosya adları: CON, PRN, AUX, NUL, COM1..9, LPT1..9
- Sondaki nokta ve boşlukların temizlenmesi
- Path traversal (..) saldırılarını engelleme
- MAX_PATH (260 karakter) sınırına karşı güvenli kısaltma
"""

from __future__ import annotations
import os
import re
import unicodedata
from pathlib import Path


_FORBIDDEN_CHARS_RE = re.compile(r'[<>:"/\\|?*\x00-\x1f]')
_RESERVED_NAMES = {
    "CON", "PRN", "AUX", "NUL",
    *(f"COM{i}" for i in range(1, 10)),
    *(f"LPT{i}" for i in range(1, 10)),
}


def sanitize_filename(
    name: str,
    default_name: str = "indirilen_medya",
    max_length: int = 200,
) -> str:
    """Windows dosya sistemine uygun, güvenli bir dosya adı üretir."""
    if not name or not isinstance(name, str):
        return default_name

    # Unicode normalizasyonu (NFC)
    normalized = unicodedata.normalize("NFC", name.strip())

    # Path traversal ve dizin ayrıştırıcıları engelle
    normalized = normalized.replace("..", "").replace("/", "_").replace("\\", "_")

    # Yasak karakterleri alt çizgi ile değiştir
    clean = _FORBIDDEN_CHARS_RE.sub("_", normalized)

    # Çoklu alt çizgileri ve boşlukları sadeleştir
    clean = re.sub(r"\s+", " ", clean)
    clean = re.sub(r"_+", "_", clean).strip(" ._")

    # Boş kaldıysa varsayılan adı ata
    if not clean:
        clean = default_name

    # Ayrılmış Windows adlarını kontrol et (uzantısız gövdeye bakarak)
    stem, ext = os.path.splitext(clean)
    if stem.upper() in _RESERVED_NAMES:
        stem = f"_{stem}"
        clean = f"{stem}{ext}"

    # Uzunluk kısıtlaması (uzantıyı koruyarak kısalt)
    if len(clean) > max_length:
        stem_max = max(10, max_length - len(ext))
        clean = f"{stem[:stem_max].rstrip(' ._')}{ext}"

    return clean


def safe_join_path(base_dir: str | Path, filename: str) -> Path:
    """Temizlenmiş dosya adını ana dizinle güvenli bir şekilde birleştirir.

    Path traversal girişimlerini engeller.
    """
    base = Path(base_dir).resolve()
    clean_name = sanitize_filename(filename)
    target = (base / clean_name).resolve()

    # Dizin dışına çıkma kontrolü
    if not str(target).startswith(str(base)):
        raise ValueError(f"Geçersiz dosya yolu saptandı: {filename}")

    return target
