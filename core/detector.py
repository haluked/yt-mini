"""URL platform tespiti ve doğrulama modülü.

Desteklenen öncelikli platformlar:
- YouTube (youtube.com, youtu.be, shorts vb.)
- TikTok (tiktok.com)
- Instagram (instagram.com / reels / p / tv)
- X / Twitter (twitter.com, x.com)
- Genel (yt-dlp destekli tüm diğer siteler)
"""

from __future__ import annotations
import re
from dataclasses import dataclass
from typing import Optional
from urllib.parse import urlparse


@dataclass(frozen=True)
class PlatformInfo:
    key: str
    display_name: str
    badge_label: str
    is_specialized: bool


PLATFORMS = {
    "youtube": PlatformInfo("youtube", "YouTube", "YOUTUBE", True),
    "tiktok": PlatformInfo("tiktok", "TikTok", "TIKTOK", True),
    "instagram": PlatformInfo("instagram", "Instagram", "INSTA", True),
    "twitter": PlatformInfo("twitter", "X (Twitter)", "X", True),
    "generic": PlatformInfo("generic", "Web Medya", "WEB", False),
}

# Regex kalıpları
_YOUTUBE_RE = re.compile(
    r"^(https?://)?(www\.|m\.)?(youtube\.com|youtu\.be)/(watch\?|shorts/|embed/|v/|.+\?v=)?",
    re.IGNORECASE,
)
_TIKTOK_RE = re.compile(
    r"^(https?://)?(www\.|vm\.|vt\.)?tiktok\.com/",
    re.IGNORECASE,
)
_INSTAGRAM_RE = re.compile(
    r"^(https?://)?(www\.)?instagram\.com/(p|reel|tv|stories)/",
    re.IGNORECASE,
)
_TWITTER_RE = re.compile(
    r"^(https?://)?(www\.)?(twitter\.com|x\.com)/.+/status/",
    re.IGNORECASE,
)


def is_valid_url(url: str) -> bool:
    """URL'nin geçerli bir HTTP/HTTPS adresi olup olmadığını doğrular."""
    if not url or not isinstance(url, str):
        return False
    url = url.strip()
    try:
        parsed = urlparse(url)
        return parsed.scheme in ("http", "https") and bool(parsed.netloc)
    except Exception:
        return False


def detect_platform(url: str) -> PlatformInfo:
    """Verilen URL'nin hangi platforma ait olduğunu tespit eder."""
    if not is_valid_url(url):
        return PLATFORMS["generic"]

    url_clean = url.strip()

    if _YOUTUBE_RE.search(url_clean):
        return PLATFORMS["youtube"]
    if _TIKTOK_RE.search(url_clean):
        return PLATFORMS["tiktok"]
    if _INSTAGRAM_RE.search(url_clean):
        return PLATFORMS["instagram"]
    if _TWITTER_RE.search(url_clean):
        return PLATFORMS["twitter"]

    return PLATFORMS["generic"]
