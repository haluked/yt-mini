"""Gerçek platformlardan alınmış format örnekleri ve mock veriler."""

YOUTUBE_SAMPLE = {
    "title": "Test YouTube 4K Video",
    "duration": 215,
    "thumbnail": "https://i.ytimg.com/vi/test/maxresdefault.jpg",
    "formats": [
        # Ses akışları
        {"format_id": "249", "vcodec": "none", "acodec": "opus", "abr": 50, "ext": "webm"},
        {"format_id": "250", "vcodec": "none", "acodec": "opus", "abr": 70, "ext": "webm"},
        {"format_id": "251", "vcodec": "none", "acodec": "opus", "abr": 160, "ext": "webm"},
        {"format_id": "139", "vcodec": "none", "acodec": "mp4a.40.5", "abr": 48, "ext": "m4a"},
        {"format_id": "140", "vcodec": "none", "acodec": "mp4a.40.2", "abr": 128, "ext": "m4a"},
        # 1080p Video akışları
        {"format_id": "137", "vcodec": "avc1.640028", "acodec": "none", "height": 1080, "fps": 30, "vbr": 3000, "ext": "mp4"},
        {"format_id": "248", "vcodec": "vp9", "acodec": "none", "height": 1080, "fps": 30, "vbr": 2500, "ext": "webm"},
        {"format_id": "399", "vcodec": "av01.0.08m.08", "acodec": "none", "height": 1080, "fps": 30, "vbr": 2000, "ext": "mp4"},
        # 4K Video akışları (2160p) - YouTube'da 4K H.264 genelde yoktur, VP9 ve AV1 vardır!
        {"format_id": "313", "vcodec": "vp09.00.51.08.01.01.01.01.00", "acodec": "none", "height": 2160, "fps": 60, "vbr": 18000, "ext": "webm"},
        {"format_id": "401", "vcodec": "av01.0.12m.08", "acodec": "none", "height": 2160, "fps": 60, "vbr": 14000, "ext": "mp4"},
        # 720p Video akışları
        {"format_id": "136", "vcodec": "avc1.4d401f", "acodec": "none", "height": 720, "fps": 30, "vbr": 1500, "ext": "mp4"},
        {"format_id": "247", "vcodec": "vp9", "acodec": "none", "height": 720, "fps": 30, "vbr": 1200, "ext": "webm"},
        # Birleşik legacy format
        {"format_id": "18", "vcodec": "avc1.42001E", "acodec": "mp4a.40.2", "height": 360, "fps": 30, "ext": "mp4"},
    ]
}

TIKTOK_SAMPLE = {
    "title": "Komik Kedi Videosu",
    "duration": 34,
    "thumbnail": "https://p16-sign.tiktokcdn-us.com/tos-useast5/test.jpeg",
    "formats": [
        {
            "format_id": "download_addr-0",
            "vcodec": "h264",
            "acodec": "aac",
            "height": 1080,
            "width": 1920,
            "fps": 30,
            "tbr": 2400,
            "ext": "mp4",
        }
    ]
}

INSTAGRAM_SAMPLE = {
    "title": "Doğa Manzarası Reel",
    "duration": 15,
    "thumbnail": "https://instagram.fist1-1.fna.fbcdn.net/test.jpg",
    "formats": [
        {
            "format_id": "dash-hd",
            "vcodec": "avc1.640028",
            "acodec": "mp4a.40.2",
            "height": 1080,
            "width": 1080,
            "fps": 30,
            "tbr": 3200,
            "ext": "mp4",
        }
    ]
}

TWITTER_SAMPLE = {
    "title": "Teknoloji Duyurusu Tweet",
    "duration": 45,
    "thumbnail": "https://pbs.twimg.com/media/test.jpg",
    "formats": [
        {"format_id": "http-320", "vcodec": "h264", "acodec": "aac", "height": 320, "tbr": 400, "ext": "mp4"},
        {"format_id": "http-720", "vcodec": "h264", "acodec": "aac", "height": 720, "tbr": 1200, "ext": "mp4"},
        {"format_id": "http-1080", "vcodec": "h264", "acodec": "aac", "height": 1080, "tbr": 2500, "ext": "mp4"},
    ]
}
