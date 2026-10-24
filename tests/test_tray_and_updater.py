"""Tray yöneticisi ve güncelleyici testleri."""

import pytest
from core.updater import (
    CURRENT_APP_VERSION,
    _is_newer_version,
    verify_file_sha256,
)
from tray.tray_manager import create_pixel_tray_icon


def test_is_newer_version():
    assert _is_newer_version("1.0.1", "1.0.0") is True
    assert _is_newer_version("1.1.0", "1.0.5") is True
    assert _is_newer_version("2.0.0", "1.9.9") is True
    assert _is_newer_version("1.0.0", "1.0.0") is False
    assert _is_newer_version("0.9.9", "1.0.0") is False


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
