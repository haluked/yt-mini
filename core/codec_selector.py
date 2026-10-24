"""Saf fonksiyonel kodek ve format seçim motoru.

Bu modül hiçbir yan etki (side-effect) barındırmaz:
- Girdi: yt-dlp info_dict (veya formats listesi) ve kullanıcı tercihleri
- Çıktı: Seçilen format id(ler)i, beklenen konteyner, yeniden kodlama durumu ve gerekçe metni.
"""

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple


@dataclass(frozen=True)
class SelectionResult:
    format_id: str                      # yt-dlp format dizesi (örn: "399+251" veya "18")
    video_format_id: Optional[str]      # Sadece video akış id'si
    audio_format_id: Optional[str]      # Sadece ses akış id'si
    expected_container: str             # Hedef konteyner (mp4, webm, mkv, opus, mp3, m4a)
    video_codec: str                    # Seçilen video kodeği (örn: av01, vp9, h264)
    audio_codec: str                    # Seçilen ses kodeği (örn: opus, aac, mp3)
    resolution: str                     # Çözünürlük etiketi (örn: 1080p, 4K)
    is_composite: bool                  # Ses ve video tek akışta birleşik mi?
    needs_reencode: bool                # Yeniden kodlama gerekiyor mu?
    reason: str                         # Seçim gerekçesi (açıklayıcı Türkçe)
    warning: Optional[str] = None       # Uyumsuzluk/dönüştürme uyarısı


def _normalize_video_codec(vcodec: Optional[str]) -> Tuple[str, int]:
    """Video kodeğini standartlaştırır ve öncelik puanı döner.

    Öncelik: AV1 (4) > VP9 (3) > H.265 (2) > H.264 (1) > Diğer (0)
    """
    if not vcodec or vcodec == "none":
        return "none", -1

    vc = vcodec.lower()
    if vc.startswith("av01") or "av1" in vc:
        return "av1", 4
    if vc.startswith("vp09") or vc.startswith("vp9") or "vp9" in vc:
        return "vp9", 3
    if vc.startswith("hvc1") or vc.startswith("hev1") or "hevc" in vc or "h265" in vc:
        return "hevc", 2
    if vc.startswith("avc1") or vc.startswith("avc") or "h264" in vc:
        return "h264", 1
    return vc.split(".")[0], 0


def _normalize_audio_codec(acodec: Optional[str]) -> Tuple[str, int]:
    """Ses kodeğini standartlaştırır ve öncelik puanı döner.

    Öncelik: Opus (3) > AAC (2) > MP3 (1) > Diğer (0)
    """
    if not acodec or acodec == "none":
        return "none", -1

    ac = acodec.lower()
    if "opus" in ac:
        return "opus", 3
    if ac.startswith("mp4a") or "aac" in ac:
        return "aac", 2
    if "mp3" in ac:
        return "mp3", 1
    return ac.split(".")[0], 0


def get_available_resolutions(formats: List[Dict[str, Any]]) -> List[int]:
    """Format listesinden mevcut video çözünürlüklerini (yükseklik/height) büyükten küçüğe sıralar."""
    heights = set()
    for f in formats:
        h = f.get("height")
        vcodec = f.get("vcodec")
        if h and isinstance(h, int) and h > 0 and vcodec and vcodec != "none":
            heights.add(h)
    return sorted(list(heights), reverse=True)


def select_best_format(
    formats: List[Dict[str, Any]],
    mode: str = "video",                    # "video" veya "audio"
    target_res: Optional[int | str] = "best", # "best" veya sayısal yükseklik örn: 1080
    force_mp4: bool = False,                # Kullanıcı MP4 zorladı mı?
    audio_format_choice: str = "opus",      # "opus", "aac", "mp3"
) -> SelectionResult:
    """Verilen format listesinden en uygun akışları deterministik olarak seçer."""
    if not formats:
        raise ValueError("Format listesi boş, seçim yapılamaz.")

    # 1. SES MODU
    if mode == "audio":
        return _select_audio_mode(formats, audio_format_choice)

    # 2. VİDEO MODU
    return _select_video_mode(formats, target_res, force_mp4)


def _select_audio_mode(
    formats: List[Dict[str, Any]],
    preferred_audio: str = "opus",
) -> SelectionResult:
    """Yalnızca ses indirme modu için seçim yapar."""
    audio_only = [
        f for f in formats
        if (f.get("acodec") and f.get("acodec") != "none")
        and (not f.get("vcodec") or f.get("vcodec") == "none")
    ]

    # Eğer sadece ses akışı yoksa, ses içeren birleşik akışları da değerlendir
    candidates = audio_only if audio_only else [
        f for f in formats if f.get("acodec") and f.get("acodec") != "none"
    ]

    if not candidates:
        raise ValueError("Bu medyada geçerli ses akışı bulunamadı.")

    # İstenen kodeğe ve kaliteye göre puanla
    def audio_score(fmt: Dict[str, Any]) -> Tuple[int, float]:
        norm_ac, rank = _normalize_audio_codec(fmt.get("acodec"))
        match_score = 10 if norm_ac == preferred_audio.lower() else rank
        bitrate = fmt.get("abr") or fmt.get("tbr") or 0.0
        return (match_score, float(bitrate))

    best_audio = max(candidates, key=audio_score)
    norm_ac, _ = _normalize_audio_codec(best_audio.get("acodec"))

    # İstenen format ile akışın formatı örtüşüyor mu?
    needs_reencode = False
    warning = None
    expected_ext = preferred_audio.lower()

    if preferred_audio.lower() == "opus":
        if norm_ac == "opus":
            expected_ext = "opus"
            reason = "Kaynağın sunduğu en yüksek kaliteli ham Opus akışı doğrudan kaydediliyor."
        else:
            expected_ext = "opus"
            needs_reencode = True
            warning = f"Kaynakta saf Opus yok ({norm_ac.upper()} mevcut). Opus'a dönüştürülecek."
            reason = f"Mevcut {norm_ac.upper()} akışı Opus'a dönüştürülüyor."
    elif preferred_audio.lower() == "aac":
        if norm_ac == "aac":
            expected_ext = "m4a"
            reason = "Kaynağın yerel AAC akışı m4a konteynerinde remux edilerek kaydediliyor."
        else:
            expected_ext = "m4a"
            needs_reencode = True
            warning = f"Kaynakta saf AAC yok ({norm_ac.upper()} mevcut). AAC'ye dönüştürülecek."
            reason = f"Mevcut {norm_ac.upper()} akışı AAC'ye dönüştürülüyor."
    elif preferred_audio.lower() == "mp3":
        if norm_ac == "mp3":
            expected_ext = "mp3"
            reason = "Kaynağın ham MP3 akışı kopyalanıyor."
        else:
            expected_ext = "mp3"
            needs_reencode = True
            reason = f"{norm_ac.upper()} akışı evrensel MP3 formatına dönüştürülüyor."

    return SelectionResult(
        format_id=str(best_audio["format_id"]),
        video_format_id=None,
        audio_format_id=str(best_audio["format_id"]),
        expected_container=expected_ext,
        video_codec="none",
        audio_codec=norm_ac,
        resolution="Audio Only",
        is_composite=bool(best_audio.get("vcodec") and best_audio.get("vcodec") != "none"),
        needs_reencode=needs_reencode,
        reason=reason,
        warning=warning,
    )


def _select_video_mode(
    formats: List[Dict[str, Any]],
    target_res: Optional[int | str],
    force_mp4: bool,
) -> SelectionResult:
    """Video modu için en uygun video ve ses akışlarını seçer."""
    # Video akışlarını ayır
    video_formats = [
        f for f in formats
        if f.get("vcodec") and f.get("vcodec") != "none"
    ]

    if not video_formats:
        raise ValueError("Bu medyada geçerli video akışı bulunamadı.")

    # 1. Çözünürlük hedefi belirle
    available_res = get_available_resolutions(formats)
    if not available_res:
        target_height = 0
    elif target_res == "best" or target_res is None:
        target_height = available_res[0]
    else:
        target_height = int(target_res)
        if target_height not in available_res:
            # En yakın uygun çözünürlüğü bul
            target_height = min(available_res, key=lambda r: abs(r - target_height))

    # Hedef çözünürlükteki video adayları
    res_candidates = [
        f for f in video_formats
        if (f.get("height") == target_height) or (target_height == 0)
    ]
    if not res_candidates:
        res_candidates = video_formats

    # MP4 zorlandıysa ve kaynakta doğrudan H.264 / AV1 varsa önceliklendir
    def video_rank(fmt: Dict[str, Any]) -> Tuple[int, int, float]:
        norm_vc, rank = _normalize_video_codec(fmt.get("vcodec"))
        if force_mp4:
            # MP4 uyumlu kodekler: H.264 (en uyumlu) ve AV1
            if norm_vc == "h264":
                codec_priority = 10
            elif norm_vc == "av1":
                codec_priority = 8
            elif norm_vc == "hevc":
                codec_priority = 6
            else:
                codec_priority = 1  # VP9 MP4 için dönüştürme veya remux gerektirir
        else:
            # Doğal öncelik: AV1 > VP9 > H.265 > H.264
            codec_priority = rank

        fps = int(fmt.get("fps") or 30)
        vbr = float(fmt.get("vbr") or fmt.get("tbr") or 0.0)
        return (codec_priority, fps, vbr)

    best_video = max(res_candidates, key=video_rank)
    norm_vc, vc_rank = _normalize_video_codec(best_video.get("vcodec"))
    res_label = f"{best_video.get('height', 'Bilinmeyen')}p"

    # Birleşik akış mı (hem ses hem video içeren tek format örn: TikTok/Instagram/Eski YouTube)?
    is_composite = bool(best_video.get("acodec") and best_video.get("acodec") != "none")

    if is_composite:
        norm_ac, _ = _normalize_audio_codec(best_video.get("acodec"))
        container = best_video.get("ext", "mp4")
        needs_reencode = False
        warning = None

        if force_mp4 and container != "mp4":
            if norm_vc in ("h264", "hevc") and norm_ac == "aac":
                container = "mp4"
                reason = "Birleşik akış remux (stream copy) ile MP4 konteynerine aktarılıyor."
            else:
                container = "mp4"
                needs_reencode = True
                warning = f"Kaynak akış ({norm_vc}/{norm_ac}) MP4 uyumluluğu için dönüştürülecektir."
                reason = "Birleşik akış MP4 gereksinimi nedeniyle dönüştürülüyor."
        else:
            reason = f"Platform tek birleşik akış sundu ({norm_vc.upper()} + {norm_ac.upper()}). Doğrudan stream copy yapılıyor."

        return SelectionResult(
            format_id=str(best_video["format_id"]),
            video_format_id=str(best_video["format_id"]),
            audio_format_id=None,
            expected_container=container,
            video_codec=norm_vc,
            audio_codec=norm_ac,
            resolution=res_label,
            is_composite=True,
            needs_reencode=needs_reencode,
            reason=reason,
            warning=warning,
        )

    # Ayrı akışlar (Adaptive DASH / YouTube modeli) -> En iyi ses akışını bul
    audio_formats = [
        f for f in formats
        if (f.get("acodec") and f.get("acodec") != "none")
        and (not f.get("vcodec") or f.get("vcodec") == "none")
    ]
    if not audio_formats:
        audio_formats = [f for f in formats if f.get("acodec") and f.get("acodec") != "none"]

    def audio_rank(fmt: Dict[str, Any]) -> Tuple[int, float]:
        norm_ac, rank = _normalize_audio_codec(fmt.get("acodec"))
        if force_mp4:
            # MP4 için AAC öncelikli (en yüksek uyumluluk)
            prio = 10 if norm_ac == "aac" else rank
        else:
            # Doğal öncelik: Opus > AAC > MP3
            prio = rank
        abr = float(fmt.get("abr") or fmt.get("tbr") or 0.0)
        return (prio, abr)

    best_audio = max(audio_formats, key=audio_rank) if audio_formats else None
    norm_ac, _ = _normalize_audio_codec(best_audio.get("acodec") if best_audio else "none")

    # Konteyner ve Remux kararı
    format_id_comb = f"{best_video['format_id']}+{best_audio['format_id']}" if best_audio else str(best_video["format_id"])
    needs_reencode = False
    warning = None

    if force_mp4:
        container = "mp4"
        if norm_vc == "vp9":
            needs_reencode = True
            warning = "Kaynakta bu çözünürlükte VP9 kodeği var. MP4 için H.264 dönüştürmesi yapılacaktır."
            reason = "Kullanıcı MP4 seçti; VP9 akışı H.264'e dönüştürülüp AAC ses ile birleştirilecek."
        elif norm_ac == "opus":
            # Video H.264/AV1 ama ses Opus ise, sadece sesi AAC'ye dönüştürmek yeterlidir, video kopyalanır!
            needs_reencode = True
            warning = "Opus ses akışı MP4 ile uyumlu olmadığından AAC'ye dönüştürülecektir (video akışı kopyalanır)."
            reason = f"{norm_vc.upper()} video kopyalanıyor, Opus ses AAC'ye dönüştürülüp MP4 yapılıyor."
        else:
            reason = f"{norm_vc.upper()} video ve {norm_ac.upper()} ses MP4 içinde remux (stream copy) ediliyor."
    else:
        # Doğal konteyner seçimi (re-encode yapmadan akışları birleştir)
        if norm_vc in ("vp9", "av1") and norm_ac == "opus":
            container = "webm"
            reason = f"Kaynağın doğal akışları ({norm_vc.upper()} + OPUS) webm içinde sıfır re-encode ile birleştiriliyor."
        elif norm_vc in ("h264", "hevc") and norm_ac == "aac":
            container = "mp4"
            reason = f"Kaynağın doğal akışları ({norm_vc.upper()} + AAC) mp4 içinde sıfır re-encode ile birleştiriliyor."
        else:
            container = "mkv"
            reason = f"Farklı kodekler ({norm_vc.upper()} + {norm_ac.upper()}) en uyumlu mkv konteynerinde kayıpsız remux ediliyor."

    return SelectionResult(
        format_id=format_id_comb,
        video_format_id=str(best_video["format_id"]),
        audio_format_id=str(best_audio["format_id"]) if best_audio else None,
        expected_container=container,
        video_codec=norm_vc,
        audio_codec=norm_ac,
        resolution=res_label,
        is_composite=False,
        needs_reencode=needs_reencode,
        reason=reason,
        warning=warning,
    )
