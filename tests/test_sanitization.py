"""Dosya adı sanitization testleri."""

import pytest
from pathlib import Path
from core.sanitization import sanitize_filename, safe_join_path


def test_sanitize_forbidden_chars():
    dirty = 'Video: "Kedi & Köpek" <Önemli> | 1080p? *Test*'
    clean = sanitize_filename(dirty)
    for char in '<>:"/\\|?*':
        assert char not in clean
    assert "Kedi & Köpek" in clean


def test_sanitize_windows_reserved_names():
    clean = sanitize_filename("CON.mp4")
    assert clean == "_CON.mp4"

    clean_aux = sanitize_filename("aux.webm")
    assert clean_aux == "_aux.webm"


def test_sanitize_path_traversal():
    clean = sanitize_filename("../../etc/passwd.mp4")
    assert ".." not in clean
    assert "/" not in clean


def test_sanitize_max_length():
    very_long = "a" * 300 + ".mp4"
    clean = sanitize_filename(very_long, max_length=100)
    assert len(clean) <= 100
    assert clean.endswith(".mp4")


def test_safe_join_path(tmp_path):
    safe_path = safe_join_path(tmp_path, "normal_video.mp4")
    assert safe_path.parent == tmp_path
    assert safe_path.name == "normal_video.mp4"
