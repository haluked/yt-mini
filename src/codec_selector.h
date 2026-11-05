#ifndef CODEC_SELECTOR_H
#define CODEC_SELECTOR_H

#include <wchar.h>

#define MAX_FORMATS 256
#define MAX_RESOLUTIONS 32

typedef struct {
    char format_id[64];
    char ext[16];
    char vcodec[64];
    char acodec[64];
    int width;
    int height;
    int fps;
    double vbr;
    double abr;
    double tbr;
} StreamFormat;

typedef struct {
    wchar_t format_id[128];
    wchar_t video_format_id[64];
    wchar_t audio_format_id[64];
    wchar_t expected_container[16];
    wchar_t video_codec[32];
    wchar_t audio_codec[32];
    wchar_t resolution[32];
    int is_composite;
    int needs_reencode;
    wchar_t reason[256];
    wchar_t warning[256];
} SelectionResult;

void normalize_video_codec(const char *vcodec, char *out_norm, int *out_rank);
void normalize_audio_codec(const char *acodec, char *out_norm, int *out_rank);

int get_available_resolutions(const StreamFormat *formats, int count, int *out_resolutions, int max_res);

int select_best_format(
    const StreamFormat *formats,
    int count,
    const wchar_t *mode,              /* L"video" or L"audio" */
    int target_res,                   /* 0 for "best", or specific height like 1080 */
    int force_mp4,                    /* 1 to force MP4 container */
    const wchar_t *audio_format_choice, /* L"opus", L"aac", L"mp3" */
    SelectionResult *result
);

#endif /* CODEC_SELECTOR_H */
