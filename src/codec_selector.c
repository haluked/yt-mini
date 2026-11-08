#include "codec_selector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static void ascii_to_wide(const char *src, wchar_t *dst, size_t max_len)
{
    if (!dst || max_len == 0) return;
    size_t i = 0;
    while (src && src[i] && i + 1 < max_len) {
        dst[i] = (wchar_t)(unsigned char)src[i];
        i++;
    }
    dst[i] = L'\0';
}

static void str_tolower(const char *src, char *dst, size_t max_len)
{
    size_t i = 0;
    while (src && src[i] && i + 1 < max_len) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

void normalize_video_codec(const char *vcodec, char *out_norm, int *out_rank)
{
    if (!vcodec || vcodec[0] == '\0' || strcmp(vcodec, "none") == 0) {
        strcpy(out_norm, "none");
        *out_rank = -1;
        return;
    }

    char vc[128];
    str_tolower(vcodec, vc, sizeof(vc));

    if (strncmp(vc, "av01", 4) == 0 || strstr(vc, "av1") != NULL) {
        strcpy(out_norm, "av1");
        *out_rank = 4;
        return;
    }
    if (strncmp(vc, "vp09", 4) == 0 || strncmp(vc, "vp9", 3) == 0 || strstr(vc, "vp9") != NULL) {
        strcpy(out_norm, "vp9");
        *out_rank = 3;
        return;
    }
    if (strncmp(vc, "hvc1", 4) == 0 || strncmp(vc, "hev1", 4) == 0 ||
        strstr(vc, "hevc") != NULL || strstr(vc, "h265") != NULL) {
        strcpy(out_norm, "hevc");
        *out_rank = 2;
        return;
    }
    if (strncmp(vc, "avc1", 4) == 0 || strncmp(vc, "avc", 3) == 0 || strstr(vc, "h264") != NULL) {
        strcpy(out_norm, "h264");
        *out_rank = 1;
        return;
    }

    char *dot = strchr(vc, '.');
    if (dot) *dot = '\0';
    strncpy(out_norm, vc, 31);
    out_norm[31] = '\0';
    *out_rank = 0;
}

void normalize_audio_codec(const char *acodec, char *out_norm, int *out_rank)
{
    if (!acodec || acodec[0] == '\0' || strcmp(acodec, "none") == 0) {
        strcpy(out_norm, "none");
        *out_rank = -1;
        return;
    }

    char ac[128];
    str_tolower(acodec, ac, sizeof(ac));

    if (strstr(ac, "opus") != NULL) {
        strcpy(out_norm, "opus");
        *out_rank = 3;
        return;
    }
    if (strncmp(ac, "mp4a", 4) == 0 || strstr(ac, "aac") != NULL) {
        strcpy(out_norm, "aac");
        *out_rank = 2;
        return;
    }
    if (strstr(ac, "mp3") != NULL) {
        strcpy(out_norm, "mp3");
        *out_rank = 1;
        return;
    }

    char *dot = strchr(ac, '.');
    if (dot) *dot = '\0';
    strncpy(out_norm, ac, 31);
    out_norm[31] = '\0';
    *out_rank = 0;
}

static int compare_ints_desc(const void *a, const void *b)
{
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return ib - ia;
}

int get_available_resolutions(const StreamFormat *formats, int count, int *out_resolutions, int max_res)
{
    int num_res = 0;
    for (int i = 0; i < count; i++) {
        int h = formats[i].height;
        if (h <= 0 || strcmp(formats[i].vcodec, "none") == 0 || formats[i].vcodec[0] == '\0') {
            continue;
        }

        int exists = 0;
        for (int j = 0; j < num_res; j++) {
            if (out_resolutions[j] == h) {
                exists = 1;
                break;
            }
        }
        if (!exists && num_res < max_res) {
            out_resolutions[num_res++] = h;
        }
    }

    if (num_res > 1) {
        qsort(out_resolutions, num_res, sizeof(int), compare_ints_desc);
    }
    return num_res;
}

static int stream_has_video(const StreamFormat *f)
{
    return f && f->vcodec[0] != '\0' && strcmp(f->vcodec, "none") != 0;
}

static int stream_has_audio(const StreamFormat *f)
{
    return f && f->acodec[0] != '\0' && strcmp(f->acodec, "none") != 0;
}

/* yt-dlp'in generic extractor'ı doğrudan dosyalarda kodek bilgisi vermez:
   hem vcodec hem acodec "none" olur. Bu tür akışlar seçilemezse indirme boş
   format kimliğiyle başlıyordu. */
static int stream_is_direct(const StreamFormat *f)
{
    return f && f->format_id[0] != '\0' && f->ext[0] != '\0' &&
           !stream_has_video(f) && !stream_has_audio(f);
}

static int ext_is_video(const char *ext)
{
    static const char *video_exts[] = {
        "mp4", "webm", "mkv", "mov", "m4v", "avi", "flv", "ts", "mpg", "mpeg", "3gp", NULL
    };
    for (int i = 0; video_exts[i]; i++) {
        if (_stricmp(ext, video_exts[i]) == 0) return 1;
    }
    return 0;
}

static int ext_is_audio(const char *ext)
{
    static const char *audio_exts[] = {
        "mp3", "m4a", "aac", "opus", "ogg", "oga", "flac", "wav", "wma", "mka", NULL
    };
    for (int i = 0; audio_exts[i]; i++) {
        if (_stricmp(ext, audio_exts[i]) == 0) return 1;
    }
    return 0;
}

static int find_direct_format(const StreamFormat *formats, int count, int want_video)
{
    int best = -1;
    double best_score = -1.0;

    for (int i = 0; i < count; i++) {
        if (!stream_is_direct(&formats[i])) continue;
        if (want_video == 1 && !ext_is_video(formats[i].ext)) continue;

        double score = formats[i].tbr;
        if (want_video == 1 && _stricmp(formats[i].ext, "mp4") == 0) score += 1000.0;
        if (want_video == 0) {
            if (ext_is_audio(formats[i].ext)) score += 1000.0;
            else if (!ext_is_video(formats[i].ext)) score -= 1000.0;
        }
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

static int fill_direct_video(const StreamFormat *f, int force_mp4, SelectionResult *result)
{
    memset(result, 0, sizeof(SelectionResult));
    ascii_to_wide(f->format_id, result->format_id, 127);
    ascii_to_wide(f->format_id, result->video_format_id, 63);
    wcscpy(result->video_codec, L"unknown");
    wcscpy(result->audio_codec, L"unknown");
    ascii_to_wide(f->ext, result->expected_container, 15);
    result->is_composite = 1;
    result->is_direct = 1;

    if (f->height > 0) {
        _snwprintf(result->resolution, 31, L"%dp", f->height);
    } else {
        wcscpy(result->resolution, L"Direct");
    }

    if (force_mp4 && _wcsicmp(result->expected_container, L"mp4") != 0) {
        result->needs_reencode = 1;
        wcscpy(result->expected_container, L"mp4");
        wcscpy(result->warning, L"Doğrudan medya dosyası MP4 ile uyumlu değil; ffmpeg ile dönüştürülecek.");
        wcscpy(result->reason, L"Doğrudan dosya H.264/AAC MP4 biçimine dönüştürülüyor.");
    } else {
        result->needs_reencode = 0;
        _snwprintf(result->reason, 255,
            L"Doğrudan medya dosyası (kodek bilgisi yok) .%ls olarak olduğu gibi kaydediliyor.",
            result->expected_container);
    }
    return 1;
}

static int fill_direct_audio(const StreamFormat *f, const char *pref, SelectionResult *result)
{
    memset(result, 0, sizeof(SelectionResult));
    ascii_to_wide(f->format_id, result->format_id, 127);
    ascii_to_wide(f->format_id, result->audio_format_id, 63);
    wcscpy(result->video_codec, L"none");
    wcscpy(result->audio_codec, L"unknown");
    wcscpy(result->resolution, L"Audio Only");
    result->is_composite = 1;
    result->is_direct = 1;
    result->needs_reencode = 1;

    if (strcmp(pref, "aac") == 0) {
        wcscpy(result->expected_container, L"m4a");
    } else if (strcmp(pref, "mp3") == 0) {
        wcscpy(result->expected_container, L"mp3");
    } else {
        wcscpy(result->expected_container, L"opus");
    }

    wcscpy(result->warning, L"Doğrudan dosyanın kodek bilgisi yok; ses çıkarma/dönüştürme için ffmpeg gerekir.");
    _snwprintf(result->reason, 255,
        L"Doğrudan dosyadan ses çıkarılıp .%ls biçiminde kaydedilecek.",
        result->expected_container);
    return 1;
}

static int select_audio_mode(
    const StreamFormat *formats,
    int count,
    const wchar_t *preferred_audio,
    SelectionResult *result
)
{
    memset(result, 0, sizeof(SelectionResult));
    char pref[32] = "opus";
    if (preferred_audio) {
        if (_wcsicmp(preferred_audio, L"aac") == 0) strcpy(pref, "aac");
        else if (_wcsicmp(preferred_audio, L"mp3") == 0) strcpy(pref, "mp3");
        else strcpy(pref, "opus");
    }

    int best_idx = -1;
    double best_score = -1000.0;
    char best_norm_ac[64] = "none";

    for (int i = 0; i < count; i++) {
        if (formats[i].acodec[0] == '\0' || strcmp(formats[i].acodec, "none") == 0) {
            continue;
        }

        char norm_ac[64];
        int rank = 0;
        normalize_audio_codec(formats[i].acodec, norm_ac, &rank);

        int match_score = (strcmp(norm_ac, pref) == 0) ? 10 : rank;
        double bitrate = (formats[i].abr > 0) ? formats[i].abr : formats[i].tbr;
        double score = (double)(match_score * 100000) + bitrate;

        int is_audio_only = (formats[i].vcodec[0] == '\0' || strcmp(formats[i].vcodec, "none") == 0);
        if (is_audio_only) score += 50000.0;

        if (score > best_score) {
            best_score = score;
            best_idx = i;
            strcpy(best_norm_ac, norm_ac);
        }
    }

    if (best_idx < 0) {
        int direct_idx = find_direct_format(formats, count, 0);
        if (direct_idx >= 0) {
            return fill_direct_audio(&formats[direct_idx], pref, result);
        }
        wcscpy(result->reason, L"Bu bağlantıda ses akışı bulunamadı.");
        return 0;
    }

    const StreamFormat *best = &formats[best_idx];
    int is_comp = (best->vcodec[0] != '\0' && strcmp(best->vcodec, "none") != 0);

    memset(result, 0, sizeof(SelectionResult));
    ascii_to_wide(best->format_id, result->format_id, 127);
    result->video_format_id[0] = L'\0';
    ascii_to_wide(best->format_id, result->audio_format_id, 63);
    ascii_to_wide(best_norm_ac, result->audio_codec, 31);
    wcscpy(result->video_codec, L"none");
    wcscpy(result->resolution, L"Audio Only");
    result->is_composite = is_comp;

    if (strcmp(pref, "opus") == 0) {
        wcscpy(result->expected_container, L"opus");
        if (strcmp(best_norm_ac, "opus") == 0) {
            result->needs_reencode = 0;
            wcscpy(result->reason, L"Kaynağın sunduğu en yüksek kaliteli ham Opus akışı doğrudan kaydediliyor.");
        } else {
            result->needs_reencode = 1;
            _snwprintf(result->warning, 255, L"Kaynakta saf Opus yok (%hs mevcut). Opus'a dönüştürülecek.", best_norm_ac);
            _snwprintf(result->reason, 255, L"Mevcut %hs akışı Opus'a dönüştürülüyor.", best_norm_ac);
        }
    } else if (strcmp(pref, "aac") == 0) {
        wcscpy(result->expected_container, L"m4a");
        if (strcmp(best_norm_ac, "aac") == 0) {
            result->needs_reencode = 0;
            wcscpy(result->reason, L"Kaynağın yerel AAC akışı m4a konteynerinde remux edilerek kaydediliyor.");
        } else {
            result->needs_reencode = 1;
            _snwprintf(result->warning, 255, L"Kaynakta saf AAC yok (%hs mevcut). AAC'ye dönüştürülecek.", best_norm_ac);
            _snwprintf(result->reason, 255, L"Mevcut %hs akışı AAC'ye dönüştürülüyor.", best_norm_ac);
        }
    } else { /* mp3 */
        wcscpy(result->expected_container, L"mp3");
        if (strcmp(best_norm_ac, "mp3") == 0) {
            result->needs_reencode = 0;
            wcscpy(result->reason, L"Kaynağın ham MP3 akışı kopyalanıyor.");
        } else {
            result->needs_reencode = 1;
            _snwprintf(result->reason, 255, L"%hs akışı evrensel MP3 formatına dönüştürülüyor.", best_norm_ac);
        }
    }

    return 1;
}

static int select_video_mode(
    const StreamFormat *formats,
    int count,
    int target_res,
    int force_mp4,
    SelectionResult *result
)
{
    memset(result, 0, sizeof(SelectionResult));
    int available_res[MAX_RESOLUTIONS];
    int num_res = get_available_resolutions(formats, count, available_res, MAX_RESOLUTIONS);

    int resolved_target = 0;
    if (num_res > 0) {
        if (target_res <= 0) {
            resolved_target = available_res[0];
        } else {
            resolved_target = available_res[0];
            int min_diff = abs(available_res[0] - target_res);
            for (int i = 0; i < num_res; i++) {
                int diff = abs(available_res[i] - target_res);
                if (diff < min_diff) {
                    min_diff = diff;
                    resolved_target = available_res[i];
                }
            }
        }
    }

    int best_vid_idx = -1;
    double best_vid_score = -1000.0;
    char best_norm_vc[64] = "none";

    for (int i = 0; i < count; i++) {
        if (formats[i].vcodec[0] == '\0' || strcmp(formats[i].vcodec, "none") == 0) {
            continue;
        }

        if (resolved_target > 0 && formats[i].height != resolved_target) {
            continue;
        }

        char norm_vc[64];
        int rank = 0;
        normalize_video_codec(formats[i].vcodec, norm_vc, &rank);

        int codec_prio = 0;
        if (force_mp4) {
            if (strcmp(norm_vc, "h264") == 0) codec_prio = 10;
            else if (strcmp(norm_vc, "av1") == 0) codec_prio = 8;
            else if (strcmp(norm_vc, "hevc") == 0) codec_prio = 6;
            else codec_prio = 1;
        } else {
            codec_prio = rank;
        }

        int fps = (formats[i].fps > 0) ? formats[i].fps : 30;
        double vbr = (formats[i].vbr > 0) ? formats[i].vbr : formats[i].tbr;
        double score = (double)(codec_prio * 1000000) + (fps * 1000) + vbr;

        if (score > best_vid_score) {
            best_vid_score = score;
            best_vid_idx = i;
            strcpy(best_norm_vc, norm_vc);
        }
    }

    if (best_vid_idx < 0) {
        for (int i = 0; i < count; i++) {
            if (formats[i].vcodec[0] == '\0' || strcmp(formats[i].vcodec, "none") == 0) continue;
            char norm_vc[64];
            int rank = 0;
            normalize_video_codec(formats[i].vcodec, norm_vc, &rank);
            int codec_prio = force_mp4 ? ((strcmp(norm_vc, "h264") == 0) ? 10 : rank) : rank;
            double score = (double)(codec_prio * 1000000) + (formats[i].height * 100) + formats[i].tbr;
            if (score > best_vid_score) {
                best_vid_score = score;
                best_vid_idx = i;
                strcpy(best_norm_vc, norm_vc);
            }
        }
    }

    if (best_vid_idx < 0) {
        int direct_idx = find_direct_format(formats, count, 1);
        if (direct_idx >= 0) {
            return fill_direct_video(&formats[direct_idx], force_mp4, result);
        }

        int has_audio_candidate = 0;
        for (int i = 0; i < count; i++) {
            if (stream_has_audio(&formats[i]) ||
                (stream_is_direct(&formats[i]) && ext_is_audio(formats[i].ext))) {
                has_audio_candidate = 1;
                break;
            }
        }
        if (has_audio_candidate) {
            wcscpy(result->reason,
                L"Bu bağlantı yalnızca ses içeriyor. 'Yalnızca Ses (Müzik)' seçeneğini kullanın.");
        } else {
            wcscpy(result->reason, L"Bu bağlantıda indirilebilecek bir video akışı bulunamadı.");
        }
        return 0;
    }

    const StreamFormat *best_vid = &formats[best_vid_idx];
    int is_comp = (best_vid->acodec[0] != '\0' && strcmp(best_vid->acodec, "none") != 0);

    memset(result, 0, sizeof(SelectionResult));
    ascii_to_wide(best_vid->format_id, result->video_format_id, 63);
    ascii_to_wide(best_norm_vc, result->video_codec, 31);
    result->is_composite = is_comp;

    if (best_vid->height > 0) {
        _snwprintf(result->resolution, 31, L"%dp", best_vid->height);
    } else {
        wcscpy(result->resolution, L"Auto");
    }

    if (is_comp) {
        char norm_ac[64];
        int arank = 0;
        normalize_audio_codec(best_vid->acodec, norm_ac, &arank);
        ascii_to_wide(norm_ac, result->audio_codec, 31);
        ascii_to_wide(best_vid->format_id, result->format_id, 127);

        if (force_mp4 && strcmp(best_vid->ext, "mp4") != 0) {
            wcscpy(result->expected_container, L"mp4");
            if ((strcmp(best_norm_vc, "h264") == 0 || strcmp(best_norm_vc, "hevc") == 0) && strcmp(norm_ac, "aac") == 0) {
                result->needs_reencode = 0;
                wcscpy(result->reason, L"Birleşik akış remux (stream copy) ile MP4 konteynerine aktarılıyor.");
            } else {
                result->needs_reencode = 1;
                _snwprintf(result->warning, 255, L"Kaynak akış (%hs/%hs) MP4 uyumluluğu için dönüştürülecektir.", best_norm_vc, norm_ac);
                wcscpy(result->reason, L"Birleşik akış MP4 gereksinimi nedeniyle dönüştürülüyor.");
            }
        } else {
            ascii_to_wide(best_vid->ext[0] ? best_vid->ext : "mp4", result->expected_container, 15);
            _snwprintf(result->reason, 255, L"Platform tek birleşik akış sundu (%hs + %hs). Doğrudan stream copy yapılıyor.", best_norm_vc, norm_ac);
        }
        return 1;
    }

    /* Separate streams: find best audio stream */
    int best_aud_idx = -1;
    double best_aud_score = -1000.0;
    char best_norm_ac[64] = "none";

    for (int i = 0; i < count; i++) {
        if (formats[i].acodec[0] == '\0' || strcmp(formats[i].acodec, "none") == 0) continue;
        int is_aud_only = (formats[i].vcodec[0] == '\0' || strcmp(formats[i].vcodec, "none") == 0);

        char norm_ac[64];
        int arank = 0;
        normalize_audio_codec(formats[i].acodec, norm_ac, &arank);

        int prio = 0;
        if (force_mp4) {
            prio = (strcmp(norm_ac, "aac") == 0) ? 10 : arank;
        } else {
            prio = arank;
        }

        double abr = (formats[i].abr > 0) ? formats[i].abr : formats[i].tbr;
        double score = (double)(prio * 100000) + abr;
        if (is_aud_only) score += 50000.0;

        if (score > best_aud_score) {
            best_aud_score = score;
            best_aud_idx = i;
            strcpy(best_norm_ac, norm_ac);
        }
    }

    if (best_aud_idx >= 0) {
        const StreamFormat *best_aud = &formats[best_aud_idx];
        ascii_to_wide(best_aud->format_id, result->audio_format_id, 63);
        ascii_to_wide(best_norm_ac, result->audio_codec, 31);
        _snwprintf(result->format_id, 127, L"%hs+%hs", best_vid->format_id, best_aud->format_id);
    } else {
        ascii_to_wide(best_vid->format_id, result->format_id, 127);
        wcscpy(result->audio_codec, L"none");
    }

    if (force_mp4) {
        wcscpy(result->expected_container, L"mp4");
        if (strcmp(best_norm_vc, "vp9") == 0) {
            result->needs_reencode = 1;
            wcscpy(result->warning, L"Kaynakta bu çözünürlükte VP9 kodeği var. MP4 için H.264 dönüştürmesi yapılacaktır.");
            wcscpy(result->reason, L"Kullanıcı MP4 seçti; VP9 akışı H.264'e dönüştürülüp AAC ses ile birleştirilecek.");
        } else if (strcmp(best_norm_ac, "opus") == 0) {
            result->needs_reencode = 1;
            result->audio_only_reencode = 1;
            wcscpy(result->warning, L"Opus ses akışı MP4 ile uyumlu olmadığından AAC'ye dönüştürülecektir (video akışı kopyalanır).");
            _snwprintf(result->reason, 255, L"%hs video kopyalanıyor, Opus ses AAC'ye dönüştürülüp MP4 yapılıyor.", best_norm_vc);
        } else {
            result->needs_reencode = 0;
            _snwprintf(result->reason, 255, L"%hs video ve %hs ses MP4 içinde remux (stream copy) ediliyor.", best_norm_vc, best_norm_ac);
        }
    } else {
        if ((strcmp(best_norm_vc, "vp9") == 0 || strcmp(best_norm_vc, "av1") == 0) && strcmp(best_norm_ac, "opus") == 0) {
            wcscpy(result->expected_container, L"webm");
            _snwprintf(result->reason, 255, L"Kaynağın doğal akışları (%hs + OPUS) webm içinde sıfır re-encode ile birleştiriliyor.", best_norm_vc);
        } else if ((strcmp(best_norm_vc, "h264") == 0 || strcmp(best_norm_vc, "hevc") == 0) && strcmp(best_norm_ac, "aac") == 0) {
            wcscpy(result->expected_container, L"mp4");
            _snwprintf(result->reason, 255, L"Kaynağın doğal akışları (%hs + AAC) mp4 içinde sıfır re-encode ile birleştiriliyor.", best_norm_vc);
        } else {
            wcscpy(result->expected_container, L"mkv");
            _snwprintf(result->reason, 255, L"Farklı kodekler (%hs + %hs) en uyumlu mkv konteynerinde kayıpsız remux ediliyor.", best_norm_vc, best_norm_ac);
        }
    }

    return 1;
}

int select_best_format(
    const StreamFormat *formats,
    int count,
    const wchar_t *mode,
    int target_res,
    int force_mp4,
    const wchar_t *audio_format_choice,
    SelectionResult *result
)
{
    if (!formats || count <= 0 || !result) return 0;

    if (mode && _wcsicmp(mode, L"audio") == 0) {
        return select_audio_mode(formats, count, audio_format_choice, result);
    }
    return select_video_mode(formats, count, target_res, force_mp4, result);
}
