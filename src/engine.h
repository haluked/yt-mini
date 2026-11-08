#ifndef ENGINE_H
#define ENGINE_H

#include <windows.h>
#include "codec_selector.h"
#include "detector.h"

typedef enum {
    ENGINE_STATE_IDLE = 0,
    ENGINE_STATE_FETCHING,
    ENGINE_STATE_DOWNLOADING
} EngineState;

typedef struct {
    wchar_t title[256];
    int duration;
    wchar_t duration_str[32];
    wchar_t thumbnail_url[512];
    PlatformInfo platform;
    StreamFormat formats[MAX_FORMATS];
    int format_count;
    int available_resolutions[MAX_RESOLUTIONS];
    int num_resolutions;
} MediaMetadata;

typedef void (*EngineProgressCallback)(
    const wchar_t *status,           /* L"downloading", L"processing", L"finished", L"cancelled", L"error" */
    double percent,                  /* 0.0 - 100.0 */
    double speed_bytes_sec,
    int eta_seconds,
    long long downloaded_bytes,
    long long total_bytes,
    const wchar_t *filename,
    const wchar_t *error_message,
    void *user_data
);

typedef void (*EngineFetchCallback)(
    int success,
    const MediaMetadata *meta,
    const wchar_t *error_message,
    void *user_data
);

int engine_init(void);
void engine_cleanup(void);

int engine_find_ytdlp(wchar_t *out_path, size_t max_len);
int engine_find_ffmpeg(wchar_t *out_path, size_t max_len);
int engine_ffmpeg_can_encode_mp3(void);

int engine_fetch_info_async(
    const wchar_t *url,
    EngineFetchCallback callback,
    void *user_data
);

int engine_download_async(
    const wchar_t *url,
    const SelectionResult *selection,
    const wchar_t *dest_dir,
    EngineProgressCallback callback,
    void *user_data
);

void engine_cancel(void);
EngineState engine_get_state(void);
int engine_is_busy(void);

/* Shared constant so engine.c and ui.c use the same text:
   when this message arrives, the UI shows a calm status line instead of an error dialog. */
#define ENGINE_MSG_CANCELLED L"The operation was cancelled by the user."

void format_duration_str(int seconds, wchar_t *out_str, size_t max_len);
void format_bytes_str(long long bytes, wchar_t *out_str, size_t max_len);

#endif /* ENGINE_H */
