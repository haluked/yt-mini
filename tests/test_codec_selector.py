"""Kodek ve format seçici motor birim testleri."""

import pytest
from core.codec_selector import (
    select_best_format,
    get_available_resolutions,
    _normalize_video_codec,
    _normalize_audio_codec,
)
from tests.fixtures.sample_formats import (
    YOUTUBE_SAMPLE,
    TIKTOK_SAMPLE,
    INSTAGRAM_SAMPLE,
    TWITTER_SAMPLE,
)


def test_available_resolutions_youtube():
    res = get_available_resolutions(YOUTUBE_SAMPLE["formats"])
    assert res == [2160, 1080, 720, 360]


def test_youtube_default_best_selects_av1_and_opus():
    # 4K için AV1 (401) ve Opus (251) seçilmeli
    result = select_best_format(YOUTUBE_SAMPLE["formats"], mode="video", target_res="best", force_mp4=False)
    assert result.video_format_id == "401"  # AV1 4K
    assert result.audio_format_id == "251"  # Opus 160k
    assert result.video_codec == "av1"
    assert result.audio_codec == "opus"
    assert result.expected_container == "webm"
    assert not result.needs_reencode


def test_youtube_1080p_prefers_av1():
    result = select_best_format(YOUTUBE_SAMPLE["formats"], mode="video", target_res=1080, force_mp4=False)
    assert result.video_format_id == "399"  # 1080p AV1
    assert result.audio_format_id == "251"  # Opus
    assert result.video_codec == "av1"
    assert result.expected_container == "webm"
    assert not result.needs_reencode


def test_youtube_1080p_force_mp4_selects_h264_and_aac():
    # MP4 zorlandığında H.264 (137) ve AAC (140) seçilerek sıfır re-encode ile MP4 oluşturulmalı
    result = select_best_format(YOUTUBE_SAMPLE["formats"], mode="video", target_res=1080, force_mp4=True)
    assert result.video_format_id == "137"  # H.264 1080p
    assert result.audio_format_id == "140"  # AAC 128k
    assert result.video_codec == "h264"
    assert result.audio_codec == "aac"
    assert result.expected_container == "mp4"
    assert not result.needs_reencode


def test_youtube_audio_only_opus():
    result = select_best_format(YOUTUBE_SAMPLE["formats"], mode="audio", audio_format_choice="opus")
    assert result.audio_format_id == "251"
    assert result.expected_container == "opus"
    assert result.audio_codec == "opus"
    assert not result.needs_reencode


def test_youtube_audio_only_aac():
    result = select_best_format(YOUTUBE_SAMPLE["formats"], mode="audio", audio_format_choice="aac")
    assert result.audio_format_id == "140"
    assert result.expected_container == "m4a"
    assert result.audio_codec == "aac"
    assert not result.needs_reencode


def test_youtube_audio_only_mp3():
    result = select_best_format(YOUTUBE_SAMPLE["formats"], mode="audio", audio_format_choice="mp3")
    # Kaynakta ham mp3 olmadığından en yüksek kaliteli akış alınıp mp3'e dönüştürülmeli
    assert result.expected_container == "mp3"
    assert result.needs_reencode is True


def test_tiktok_single_composite_stream():
    result = select_best_format(TIKTOK_SAMPLE["formats"], mode="video", target_res="best")
    assert result.is_composite is True
    assert result.video_codec == "h264"
    assert result.audio_codec == "aac"
    assert result.expected_container == "mp4"
    assert not result.needs_reencode


def test_instagram_single_composite_stream():
    result = select_best_format(INSTAGRAM_SAMPLE["formats"], mode="video", target_res="best")
    assert result.is_composite is True
    assert result.video_codec == "h264"
    assert result.audio_codec == "aac"
    assert result.expected_container == "mp4"
    assert not result.needs_reencode


def test_twitter_selects_highest_res():
    result = select_best_format(TWITTER_SAMPLE["formats"], mode="video", target_res="best")
    assert result.video_format_id == "http-1080"
    assert result.resolution == "1080p"
    assert result.expected_container == "mp4"
