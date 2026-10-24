"""İndirme motoru (downloader) birim testleri."""

import threading
import pytest
from unittest.mock import MagicMock, patch

from core.downloader import (
    DownloadEngine,
    DownloadCancelledException,
    ProgressState,
    format_duration,
    format_bytes,
)
from core.codec_selector import SelectionResult


def test_format_duration():
    assert format_duration(65) == "01:05"
    assert format_duration(3665) == "1:01:05"
    assert format_duration(0) == "Bilinmiyor"
    assert format_duration(None) == "Bilinmiyor"


def test_format_bytes():
    assert format_bytes(1024 * 1024 * 50) == "50.0 MB"
    assert format_bytes(1024 * 1024 * 1024 * 2) == "2.00 GB"
    assert format_bytes(0) == "0.0 MB"


def test_translate_error():
    engine = DownloadEngine()

    err_private = Exception("This is a private video")
    assert "gizli veya giriş gerektiriyor" in engine._translate_error(err_private)

    err_network = Exception("unable to download webpage: connection refused")
    assert "İnternet bağlantısı kurulamadı" in engine._translate_error(err_network)

    err_ffmpeg = Exception("ffmpeg is not installed on your system")
    assert "ffmpeg" in engine._translate_error(err_ffmpeg)


@patch("core.downloader.yt_dlp.YoutubeDL")
def test_fetch_info_success(mock_ydl_cls):
    mock_instance = MagicMock()
    mock_ydl_cls.return_value.__enter__.return_value = mock_instance
    mock_instance.extract_info.return_value = {
        "title": "Harika Video",
        "duration": 120,
        "thumbnail": "https://example.com/thumb.jpg",
        "formats": [
            {"format_id": "137", "vcodec": "h264", "acodec": "none", "height": 1080},
            {"format_id": "140", "vcodec": "none", "acodec": "aac", "abr": 128},
        ],
    }

    engine = DownloadEngine()
    meta = engine.fetch_info("https://www.youtube.com/watch?v=12345678901")

    assert meta.title == "Harika Video"
    assert meta.duration == 120
    assert meta.platform_key == "youtube"
    assert 1080 in meta.available_resolutions


def test_cancellation_during_hook():
    engine = DownloadEngine()
    cancel_evt = threading.Event()
    cancel_evt.set()  # Önceden iptal edildi

    selection = SelectionResult(
        format_id="137+140",
        video_format_id="137",
        audio_format_id="140",
        expected_container="mp4",
        video_codec="h264",
        audio_codec="aac",
        resolution="1080p",
        is_composite=False,
        needs_reencode=False,
        reason="test",
    )

    with patch("core.downloader.yt_dlp.YoutubeDL") as mock_ydl:
        def fake_download(urls):
            # hook'u çağır
            hook = mock_ydl.call_args[0][0]["progress_hooks"][0]
            hook({"status": "downloading", "total_bytes": 1000, "downloaded_bytes": 100})

        mock_ydl.return_value.__enter__.return_value.download.side_effect = fake_download

        with pytest.raises(DownloadCancelledException):
            engine.download("https://example.com/v", selection, "C:/dummy", cancel_event=cancel_evt)
