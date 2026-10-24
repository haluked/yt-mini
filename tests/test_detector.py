"""Platform dedektörü birim testleri."""

import pytest
from core.detector import detect_platform, is_valid_url


@pytest.mark.parametrize(
    "url, expected_key",
    [
        ("https://www.youtube.com/watch?v=dQw4w9WgXcQ", "youtube"),
        ("https://youtu.be/dQw4w9WgXcQ", "youtube"),
        ("https://youtube.com/shorts/51wXpMh2Y_M", "youtube"),
        ("https://www.tiktok.com/@user/video/7212345678901234567", "tiktok"),
        ("https://vm.tiktok.com/ZM8xABCDe/", "tiktok"),
        ("https://www.instagram.com/reel/C1234567890/", "instagram"),
        ("https://instagram.com/p/C_abcde1234/", "instagram"),
        ("https://twitter.com/user/status/1234567890123456789", "twitter"),
        ("https://x.com/user/status/1234567890123456789", "twitter"),
        ("https://vimeo.com/12345678", "generic"),
        ("https://example.com/video.mp4", "generic"),
    ],
)
def test_detect_platform(url, expected_key):
    info = detect_platform(url)
    assert info.key == expected_key


@pytest.mark.parametrize(
    "url, valid",
    [
        ("https://example.com", True),
        ("http://localhost:8000/test", True),
        ("ftp://invalid.com", False),
        ("invalid_url", False),
        ("", False),
        ("   ", False),
        (None, False),
    ],
)
def test_is_valid_url(url, valid):
    assert is_valid_url(url) == valid
