#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>

#include "../src/detector.h"
#include "../src/sanitization.h"
#include "../src/codec_selector.h"

static void test_detector(void)
{
    printf("[TEST] Detector testing...\n");

    assert(!is_valid_url(NULL));
    assert(!is_valid_url(L""));
    assert(!is_valid_url(L"ftp://example.com"));
    assert(is_valid_url(L"https://youtube.com/watch?v=1234"));
    assert(is_valid_url(L"https://example.com/a?b=1&c=2"));
    assert(!is_valid_url(L"https://example.com/\" --exec \"calc\""));
    assert(!is_valid_url(L"https://example.com/a\nb"));

    PlatformInfo p1 = detect_platform(L"https://www.youtube.com/watch?v=dQw4w9WgXcQ");
    assert(p1.type == PLATFORM_YOUTUBE);
    assert(wcscmp(p1.badge_label, L"YOUTUBE") == 0);

    PlatformInfo p2 = detect_platform(L"https://tiktok.com/@user/video/12345");
    assert(p2.type == PLATFORM_TIKTOK);

    PlatformInfo p3 = detect_platform(L"https://instagram.com/reel/abcdef");
    assert(p3.type == PLATFORM_INSTAGRAM);

    PlatformInfo p4 = detect_platform(L"https://x.com/user/status/12345");
    assert(p4.type == PLATFORM_TWITTER);

    PlatformInfo p5 = detect_platform(L"https://vimeo.com/123456");
    assert(p5.type == PLATFORM_GENERIC);

    printf("  -> Detector tests PASSED!\n");
}

static void test_sanitization(void)
{
    printf("[TEST] Sanitization testing...\n");

    wchar_t out[256];
    sanitize_filename(L"Video: \"Kedi & Köpek\" <Önemli> | 1080p? *Test*", out, 256, L"default");
    assert(wcschr(out, L'<') == NULL);
    assert(wcschr(out, L'>') == NULL);
    assert(wcschr(out, L':') == NULL);
    assert(wcschr(out, L'\"') == NULL);
    assert(wcschr(out, L'|') == NULL);
    assert(wcschr(out, L'?') == NULL);
    assert(wcschr(out, L'*') == NULL);
    assert(wcsstr(out, L"Kedi & Köpek") != NULL);

    sanitize_filename(L"CON.mp4", out, 256, L"default");
    assert(_wcsicmp(out, L"_CON.mp4") == 0);

    sanitize_filename(L"aux.webm", out, 256, L"default");
    assert(_wcsicmp(out, L"_aux.webm") == 0);

    sanitize_filename(L"../../etc/passwd.mp4", out, 256, L"default");
    assert(wcsstr(out, L"..") == NULL);
    assert(wcschr(out, L'/') == NULL);

    printf("  -> Sanitization tests PASSED!\n");
}

static void test_codec_selector(void)
{
    printf("[TEST] Codec Selector testing...\n");

    /* YouTube Sample */
    StreamFormat yt_formats[] = {
        /* 0: Audio 249 opus */ {"249", "webm", "none", "opus", 0, 0, 0, 0, 50, 50},
        /* 1: Audio 250 opus */ {"250", "webm", "none", "opus", 0, 0, 0, 0, 70, 70},
        /* 2: Audio 251 opus */ {"251", "webm", "none", "opus", 0, 0, 0, 0, 160, 160},
        /* 3: Audio 139 aac  */ {"139", "m4a", "none", "mp4a.40.5", 0, 0, 0, 0, 48, 48},
        /* 4: Audio 140 aac  */ {"140", "m4a", "none", "mp4a.40.2", 0, 0, 0, 0, 128, 128},
        /* 5: Video 137 h264 */ {"137", "mp4", "avc1.640028", "none", 1920, 1080, 30, 3000, 0, 3000},
        /* 6: Video 248 vp9  */ {"248", "webm", "vp9", "none", 1920, 1080, 30, 2500, 0, 2500},
        /* 7: Video 399 av1  */ {"399", "mp4", "av01.0.08m.08", "none", 1920, 1080, 30, 2000, 0, 2000},
        /* 8: Video 313 vp9  */ {"313", "webm", "vp09.00.51.08.01.01.01.01.00", "none", 3840, 2160, 60, 18000, 0, 18000},
        /* 9: Video 401 av1  */ {"401", "mp4", "av01.0.12m.08", "none", 3840, 2160, 60, 14000, 0, 14000},
        /* 10: Video 136 h264*/ {"136", "mp4", "avc1.4d401f", "none", 1280, 720, 30, 1500, 0, 1500},
        /* 11: Video 247 vp9 */ {"247", "webm", "vp9", "none", 1280, 720, 30, 1200, 0, 1200},
        /* 12: Comp 18 h264  */ {"18",  "mp4", "avc1.42001E", "mp4a.40.2", 640, 360, 30, 0, 0, 600},
    };
    int yt_count = sizeof(yt_formats) / sizeof(yt_formats[0]);

    int res_list[16];
    int res_count = get_available_resolutions(yt_formats, yt_count, res_list, 16);
    assert(res_count == 4);
    assert(res_list[0] == 2160);
    assert(res_list[1] == 1080);
    assert(res_list[2] == 720);
    assert(res_list[3] == 360);

    /* Test 1: YouTube Best -> 4K AV1 (401) + Opus (251) -> webm */
    SelectionResult r1;
    assert(select_best_format(yt_formats, yt_count, L"video", 0, 0, L"opus", &r1));
    assert(wcscmp(r1.video_format_id, L"401") == 0);
    assert(wcscmp(r1.audio_format_id, L"251") == 0);
    assert(wcscmp(r1.video_codec, L"av1") == 0);
    
    
    assert(r1.needs_reencode == 0);

    /* Test 2: YouTube 1080p -> 1080p AV1 (399) + Opus (251) -> webm */
    SelectionResult r2;
    assert(select_best_format(yt_formats, yt_count, L"video", 1080, 0, L"opus", &r2));
    assert(wcscmp(r2.video_format_id, L"399") == 0);
    assert(wcscmp(r2.audio_format_id, L"251") == 0);
    assert(wcscmp(r2.video_codec, L"av1") == 0);
    assert(wcscmp(r2.expected_container, L"webm") == 0);
    assert(r2.needs_reencode == 0);

    /* Test 3: YouTube 1080p Force MP4 -> H.264 (137) + AAC (140) -> mp4 */
    SelectionResult r3;
    assert(select_best_format(yt_formats, yt_count, L"video", 1080, 1, L"opus", &r3));
    assert(wcscmp(r3.video_format_id, L"137") == 0);
    assert(wcscmp(r3.audio_format_id, L"140") == 0);
    assert(wcscmp(r3.video_codec, L"h264") == 0);
    assert(wcscmp(r3.audio_codec, L"aac") == 0);
    assert(wcscmp(r3.expected_container, L"mp4") == 0);
    assert(r3.needs_reencode == 0);

    /* Test 4: YouTube Audio Only Opus */
    SelectionResult r4;
    assert(select_best_format(yt_formats, yt_count, L"audio", 0, 0, L"opus", &r4));
    assert(wcscmp(r4.audio_format_id, L"251") == 0);
    assert(wcscmp(r4.expected_container, L"opus") == 0);
    assert(wcscmp(r4.audio_codec, L"opus") == 0);
    assert(r4.needs_reencode == 0);

    /* Test 5: YouTube Audio Only AAC */
    SelectionResult r5;
    assert(select_best_format(yt_formats, yt_count, L"audio", 0, 0, L"aac", &r5));
    assert(wcscmp(r5.audio_format_id, L"140") == 0);
    assert(wcscmp(r5.expected_container, L"m4a") == 0);
    assert(wcscmp(r5.audio_codec, L"aac") == 0);
    assert(r5.needs_reencode == 0);

    /* Test 6: YouTube Audio Only MP3 */
    SelectionResult r6;
    assert(select_best_format(yt_formats, yt_count, L"audio", 0, 0, L"mp3", &r6));
    assert(wcscmp(r6.expected_container, L"mp3") == 0);
    assert(r6.needs_reencode == 1);

    /* Test 7: TikTok Single Composite Stream */
    StreamFormat tiktok_formats[] = {
        {"download_addr-0", "mp4", "h264", "aac", 1920, 1080, 30, 0, 0, 2400}
    };
    SelectionResult r7;
    assert(select_best_format(tiktok_formats, 1, L"video", 0, 0, L"opus", &r7));
    assert(r7.is_composite == 1);
    assert(wcscmp(r7.video_codec, L"h264") == 0);
    assert(wcscmp(r7.audio_codec, L"aac") == 0);
    assert(wcscmp(r7.expected_container, L"mp4") == 0);
    assert(r7.needs_reencode == 0);

    /* Test 8: Twitter Selects Highest Res */
    StreamFormat twitter_formats[] = {
        {"http-320", "mp4", "h264", "aac", 0, 320, 30, 0, 0, 400},
        {"http-720", "mp4", "h264", "aac", 0, 720, 30, 0, 0, 1200},
        {"http-1080", "mp4", "h264", "aac", 0, 1080, 30, 0, 0, 2500},
    };
    SelectionResult r8;
    assert(select_best_format(twitter_formats, 3, L"video", 0, 0, L"opus", &r8));
    assert(wcscmp(r8.video_format_id, L"http-1080") == 0);
    assert(wcscmp(r8.resolution, L"1080p") == 0);
    assert(wcscmp(r8.expected_container, L"mp4") == 0);

    /* Test 9: Direct media (yt-dlp reports vcodec/acodec as null) */
    StreamFormat direct_mp4[] = {
        {"mp4", "mp4", "none", "none", 0, 0, 0, 0, 0, 0}
    };
    SelectionResult r9;
    assert(select_best_format(direct_mp4, 1, L"video", 0, 0, L"opus", &r9));
    assert(r9.is_direct == 1);
    assert(wcscmp(r9.format_id, L"mp4") == 0);
    assert(wcscmp(r9.expected_container, L"mp4") == 0);
    assert(r9.needs_reencode == 0);

    /* Test 10: Direct webm forced to MP4 needs re-encode */
    StreamFormat direct_webm[] = {
        {"webm", "webm", "none", "none", 0, 0, 0, 0, 0, 0}
    };
    SelectionResult r10;
    assert(select_best_format(direct_webm, 1, L"video", 0, 1, L"opus", &r10));
    assert(r10.is_direct == 1);
    assert(wcscmp(r10.expected_container, L"mp4") == 0);
    assert(r10.needs_reencode == 1);

    /* Test 11: Direct file in audio mode is extracted/converted */
    SelectionResult r11;
    assert(select_best_format(direct_mp4, 1, L"audio", 0, 0, L"mp3", &r11));
    assert(r11.is_direct == 1);
    assert(wcscmp(r11.expected_container, L"mp3") == 0);
    assert(r11.needs_reencode == 1);

    /* Test 12: Audio-only source in video mode fails with a clear reason */
    StreamFormat audio_only[] = {
        {"251", "webm", "none", "opus", 0, 0, 0, 0, 160, 160}
    };
    SelectionResult r12;
    assert(!select_best_format(audio_only, 1, L"video", 0, 0, L"opus", &r12));
    assert(wcsstr(r12.reason, L"Yalnızca Ses") != NULL);

    /* Test 13: No audio source in audio mode fails with a clear reason */
    StreamFormat video_only[] = {
        {"137", "mp4", "avc1.640028", "none", 1920, 1080, 30, 3000, 0, 3000}
    };
    SelectionResult r13;
    assert(!select_best_format(video_only, 1, L"audio", 0, 0, L"opus", &r13));
    assert(r13.reason[0] != L'\0');

    printf("  -> Codec Selector tests PASSED!\n");
}

int main(void)
{
    printf("==========================================\n");
    printf("RUNNING FF-DLP-VIEWER LIGHT C UNIT TESTS\n");
    printf("==========================================\n");

    test_detector();
    test_sanitization();
    test_codec_selector();

    printf("\nALL CORE C TESTS PASSED WITH 100%% SUCCESS!\n");
    return 0;
}
