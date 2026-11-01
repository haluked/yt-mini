"""Tray yöneticisi ve güncelleyici testleri."""

import io
import json
from unittest.mock import patch, MagicMock
import pytest
from core.updater import (
    CURRENT_APP_VERSION,
    _is_newer_version,
    verify_file_sha256,
    check_ytdlp_update,
)
from tray.tray_manager import create_pixel_tray_icon


def test_is_newer_version():
    assert _is_newer_version("1.0.1", "1.0.0") is True
    assert _is_newer_version("1.1.0", "1.0.5") is True
    assert _is_newer_version("2.0.0", "1.9.9") is True
    assert _is_newer_version("1.0.0", "1.0.0") is False
    assert _is_newer_version("0.9.9", "1.0.0") is False
    # CalVer (yt-dlp) karşılaştırma testleri
    assert _is_newer_version("2026.10.26", "2026.8.19") is True
    assert _is_newer_version("2026.8.19", "2026.10.26") is False
    assert _is_newer_version("2026.10.26", "2026.10.26") is False


def test_check_ytdlp_update_mock():
    mock_response = io.BytesIO(json.dumps({"info": {"version": "2026.10.26"}}).encode("utf-8"))
    with patch("urllib.request.urlopen", return_value=mock_response):
        has_update, cur, latest = check_ytdlp_update()
        assert latest == "2026.10.26"
        assert cur != ""


def test_check_ytdlp_update_network_error():
    with patch("urllib.request.urlopen", side_effect=Exception("Network unreachable")):
        has_update, cur, latest = check_ytdlp_update()
        assert has_update is False


def test_verify_file_sha256(tmp_path):
    test_file = tmp_path / "sample.txt"
    test_file.write_text("ff-dlp-viewer-test-sha256", encoding="utf-8")

    # echo -n "ff-dlp-viewer-test-sha256" | sha256sum
    import hashlib
    expected_hash = hashlib.sha256(b"ff-dlp-viewer-test-sha256").hexdigest()

    assert verify_file_sha256(str(test_file), expected_hash) is True
    assert verify_file_sha256(str(test_file), "wrong_hash") is False


def test_create_pixel_tray_icon():
    img = create_pixel_tray_icon(is_active=False)
    assert img.size == (32, 32)
    assert img.mode == "RGBA"

    img_active = create_pixel_tray_icon(is_active=True)
    assert img_active.size == (32, 32)
