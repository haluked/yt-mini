"""Güncelleme denetleyicisi ve bütünlük (SHA-256) doğrulayıcı modülü.

- ff-dlp-viewer GitHub Releases API üzerinden güncellemeleri denetler.
- yt-dlp sürüm kontrolünü gerçekleştirir.
- Yeni sürüm varsa indirmeden önce ve sonra SHA-256 doğrulamasını yapar.
"""

from __future__ import annotations
import hashlib
import json
import os
import threading
from dataclasses import dataclass
from typing import Callable, Optional
from urllib.request import Request, urlopen

CURRENT_APP_VERSION = "1.0.0"
GITHUB_REPO = "haluk/ff-dlp-viewer"  # GitHub proje yolu


@dataclass(frozen=True)
class UpdateInfo:
    has_update: bool
    current_version: str
    latest_version: str
    release_notes: str
    download_url: Optional[str] = None
    expected_sha256: Optional[str] = None
    asset_name: Optional[str] = None


def get_current_versions() -> dict[str, str]:
    """Uygulama ve bağlı bileşenlerin mevcut sürümlerini döner."""
    try:
        import yt_dlp
        yt_version = getattr(yt_dlp.version, "__version__", "Bilinmiyor")
    except Exception:
        yt_version = "Bilinmiyor"

    return {
        "app": CURRENT_APP_VERSION,
        "yt_dlp": yt_version,
    }


def verify_file_sha256(filepath: str, expected_hash: str) -> bool:
    """Verilen dosyanın SHA-256 özet değerini doğrular."""
    if not os.path.isfile(filepath) or not expected_hash:
        return False

    sha256 = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            sha256.update(chunk)

    computed = sha256.hexdigest().lower().strip()
    return computed == expected_hash.lower().strip()


def check_for_updates(repo: str = GITHUB_REPO) -> UpdateInfo:
    """GitHub API üzerinden en son sürümü denetler."""
    api_url = f"https://api.github.com/repos/{repo}/releases/latest"
    req = Request(
        api_url,
        headers={
            "User-Agent": f"ff-dlp-viewer/{CURRENT_APP_VERSION}",
            "Accept": "application/vnd.github.v3+json",
        },
    )

    try:
        with urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))

        tag_name = data.get("tag_name", "").lstrip("v")
        body = data.get("body", "Yenilikler belirtilmedi.")
        assets = data.get("assets", [])

        # Setup.exe asset'ini ara
        setup_asset = None
        sha_asset = None

        for asset in assets:
            name = asset.get("name", "").lower()
            if name.endswith("setup.exe") or name.endswith(".exe"):
                setup_asset = asset
            elif name.endswith(".sha256") or name.endswith("checksums.txt"):
                sha_asset = asset

        has_update = _is_newer_version(tag_name, CURRENT_APP_VERSION)
        download_url = setup_asset.get("browser_download_url") if setup_asset else None
        asset_name = setup_asset.get("name") if setup_asset else None

        # Eğer checksum dosyası varsa SHA256'yı çek
        expected_sha = None
        if sha_asset and sha_asset.get("browser_download_url"):
            try:
                with urlopen(sha_asset["browser_download_url"], timeout=5) as sha_resp:
                    sha_content = sha_resp.read().decode("utf-8")
                    # Dosya adını içeren satırı bul
                    for line in sha_content.splitlines():
                        if asset_name and asset_name in line:
                            expected_sha = line.split()[0]
                            break
                        elif len(line.strip().split()[0]) == 64:
                            expected_sha = line.strip().split()[0]
            except Exception:
                pass

        return UpdateInfo(
            has_update=has_update,
            current_version=CURRENT_APP_VERSION,
            latest_version=tag_name,
            release_notes=body,
            download_url=download_url,
            expected_sha256=expected_sha,
            asset_name=asset_name,
        )

    except Exception:
        # Ağ hatası veya henüz release olmaması durumunda
        return UpdateInfo(
            has_update=False,
            current_version=CURRENT_APP_VERSION,
            latest_version=CURRENT_APP_VERSION,
            release_notes="Güncelleme sunucusuna erişilemedi veya henüz yayınlanan yeni bir sürüm yok.",
        )


def check_updates_async(
    on_result: Callable[[UpdateInfo], None],
    repo: str = GITHUB_REPO,
) -> threading.Thread:
    """Güncelleme denetimini UI'yı dondurmadan arka planda çalıştırır."""
    def _worker():
        info = check_for_updates(repo)
        on_result(info)

    t = threading.Thread(target=_worker, daemon=True)
    t.start()
    return t


def _is_newer_version(latest: str, current: str) -> bool:
    """SemVer sürüm karşılaştırması yapar."""
    try:
        lat_parts = [int(p) for p in latest.split(".") if p.isdigit()]
        cur_parts = [int(p) for p in current.split(".") if p.isdigit()]
        return lat_parts > cur_parts
    except Exception:
        return False
