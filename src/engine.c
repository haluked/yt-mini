#include "engine.h"
#include "cjson.h"
#include "sanitization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <shlobj.h>

static HANDLE g_current_process = NULL;
static volatile int g_cancel_requested = 0;
static CRITICAL_SECTION g_engine_cs;
static EngineState g_engine_state = ENGINE_STATE_IDLE;

int engine_init(void)
{
    InitializeCriticalSection(&g_engine_cs);
    g_engine_state = ENGINE_STATE_IDLE;
    return 1;
}

void engine_cleanup(void)
{
    engine_cancel();
    DeleteCriticalSection(&g_engine_cs);
}

EngineState engine_get_state(void)
{
    EnterCriticalSection(&g_engine_cs);
    EngineState s = g_engine_state;
    LeaveCriticalSection(&g_engine_cs);
    return s;
}

int engine_is_busy(void)
{
    return engine_get_state() != ENGINE_STATE_IDLE;
}

void engine_cancel(void)
{
    EnterCriticalSection(&g_engine_cs);
    g_cancel_requested = 1;
    if (g_current_process && g_current_process != INVALID_HANDLE_VALUE) {
        TerminateProcess(g_current_process, 0);
    }
    LeaveCriticalSection(&g_engine_cs);
}

static int file_exists(const wchar_t *path)
{
    DWORD attr = GetFileAttributesW(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

static int find_binary(const wchar_t *name, wchar_t *out_path, size_t max_len)
{
    wchar_t app_path[MAX_PATH];
    GetModuleFileNameW(NULL, app_path, MAX_PATH);
    wchar_t *last_slash = wcsrchr(app_path, L'\\');
    if (last_slash) *last_slash = L'\0';

    /* 1. App Dir */
    wchar_t candidate[MAX_PATH];
    _snwprintf(candidate, MAX_PATH, L"%ls\\%ls.exe", app_path, name);
    if (file_exists(candidate)) {
        wcsncpy(out_path, candidate, max_len - 1);
        out_path[max_len - 1] = L'\0';
        return 1;
    }

    /* 2. App Dir / bin */
    _snwprintf(candidate, MAX_PATH, L"%ls\\bin\\%ls.exe", app_path, name);
    if (file_exists(candidate)) {
        wcsncpy(out_path, candidate, max_len - 1);
        out_path[max_len - 1] = L'\0';
        return 1;
    }

    /* 3. Parent / bin (in development) */
    _snwprintf(candidate, MAX_PATH, L"%ls\\..\\bin\\%ls.exe", app_path, name);
    if (file_exists(candidate)) {
        wcsncpy(out_path, candidate, max_len - 1);
        out_path[max_len - 1] = L'\0';
        return 1;
    }

    /* 4. %LOCALAPPDATA%\ff-dlp-viewer\bin */
    wchar_t local_app_data[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_app_data) == S_OK) {
        _snwprintf(candidate, MAX_PATH, L"%ls\\ff-dlp-viewer\\bin\\%ls.exe", local_app_data, name);
        if (file_exists(candidate)) {
            wcsncpy(out_path, candidate, max_len - 1);
            out_path[max_len - 1] = L'\0';
            return 1;
        }
    }

    /* 5. System PATH */
    if (SearchPathW(NULL, name, L".exe", (DWORD)max_len, out_path, NULL) > 0) {
        return 1;
    }

    return 0;
}

int engine_find_ytdlp(wchar_t *out_path, size_t max_len)
{
    return find_binary(L"yt-dlp", out_path, max_len);
}

int engine_find_ffmpeg(wchar_t *out_path, size_t max_len)
{
    return find_binary(L"ffmpeg", out_path, max_len);
}

void format_duration_str(int seconds, wchar_t *out_str, size_t max_len)
{
    if (seconds <= 0) {
        wcsncpy(out_str, L"Bilinmiyor", max_len - 1);
        out_str[max_len - 1] = L'\0';
        return;
    }
    int m = seconds / 60;
    int s = seconds % 60;
    int h = m / 60;
    m = m % 60;

    if (h > 0) {
        _snwprintf(out_str, max_len, L"%d:%02d:%02d", h, m, s);
    } else {
        _snwprintf(out_str, max_len, L"%02d:%02d", m, s);
    }
}

void format_bytes_str(long long bytes, wchar_t *out_str, size_t max_len)
{
    if (bytes <= 0) {
        wcsncpy(out_str, L"0.0 MB", max_len - 1);
        out_str[max_len - 1] = L'\0';
        return;
    }
    double mb = (double)bytes / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        _snwprintf(out_str, max_len, L"%.2f GB", mb / 1024.0);
    } else {
        _snwprintf(out_str, max_len, L"%.1f MB", mb);
    }
}

typedef struct {
    wchar_t url[1024];
    EngineFetchCallback callback;
    void *user_data;
} FetchThreadParams;

static void parse_json_metadata(const char *json_str, MediaMetadata *meta)
{
    memset(meta, 0, sizeof(MediaMetadata));
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return;

    cJSON *title_item = cJSON_GetObjectItem(root, "title");
    if (cJSON_IsString(title_item) && title_item->valuestring) {
        MultiByteToWideChar(CP_UTF8, 0, title_item->valuestring, -1, meta->title, 255);
    }

    cJSON *dur_item = cJSON_GetObjectItem(root, "duration");
    if (cJSON_IsNumber(dur_item)) {
        meta->duration = dur_item->valueint;
        format_duration_str(meta->duration, meta->duration_str, sizeof(meta->duration_str)/sizeof(wchar_t));
    }

    cJSON *thumb_item = cJSON_GetObjectItem(root, "thumbnail");
    if (cJSON_IsString(thumb_item) && thumb_item->valuestring) {
        MultiByteToWideChar(CP_UTF8, 0, thumb_item->valuestring, -1, meta->thumbnail_url, 511);
    }

    cJSON *formats_array = cJSON_GetObjectItem(root, "formats");
    if (cJSON_IsArray(formats_array)) {
        int arr_size = cJSON_GetArraySize(formats_array);
        int f_idx = 0;
        for (int i = 0; i < arr_size && f_idx < MAX_FORMATS; i++) {
            cJSON *f = cJSON_GetArrayItem(formats_array, i);
            if (!cJSON_IsObject(f)) continue;

            StreamFormat *sf = &meta->formats[f_idx];
            memset(sf, 0, sizeof(StreamFormat));

            cJSON *id = cJSON_GetObjectItem(f, "format_id");
            if (cJSON_IsString(id) && id->valuestring) strncpy(sf->format_id, id->valuestring, 63);

            cJSON *ext = cJSON_GetObjectItem(f, "ext");
            if (cJSON_IsString(ext) && ext->valuestring) strncpy(sf->ext, ext->valuestring, 15);

            cJSON *vc = cJSON_GetObjectItem(f, "vcodec");
            if (cJSON_IsString(vc) && vc->valuestring) strncpy(sf->vcodec, vc->valuestring, 63);
            else strcpy(sf->vcodec, "none");

            cJSON *ac = cJSON_GetObjectItem(f, "acodec");
            if (cJSON_IsString(ac) && ac->valuestring) strncpy(sf->acodec, ac->valuestring, 63);
            else strcpy(sf->acodec, "none");

            cJSON *h = cJSON_GetObjectItem(f, "height");
            if (cJSON_IsNumber(h)) sf->height = h->valueint;

            cJSON *w = cJSON_GetObjectItem(f, "width");
            if (cJSON_IsNumber(w)) sf->width = w->valueint;

            cJSON *fps = cJSON_GetObjectItem(f, "fps");
            if (cJSON_IsNumber(fps)) sf->fps = fps->valueint;

            cJSON *vbr = cJSON_GetObjectItem(f, "vbr");
            if (cJSON_IsNumber(vbr)) sf->vbr = vbr->valuedouble;

            cJSON *abr = cJSON_GetObjectItem(f, "abr");
            if (cJSON_IsNumber(abr)) sf->abr = abr->valuedouble;

            cJSON *tbr = cJSON_GetObjectItem(f, "tbr");
            if (cJSON_IsNumber(tbr)) sf->tbr = tbr->valuedouble;

            f_idx++;
        }
        meta->format_count = f_idx;
        meta->num_resolutions = get_available_resolutions(meta->formats, meta->format_count, meta->available_resolutions, MAX_RESOLUTIONS);
    }

    cJSON_Delete(root);
}

static DWORD WINAPI fetch_thread_proc(LPVOID param)
{
    FetchThreadParams *p = (FetchThreadParams*)param;
    wchar_t ytdlp_path[MAX_PATH];

    if (!engine_find_ytdlp(ytdlp_path, MAX_PATH)) {
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);

        if (p->callback) {
            p->callback(0, NULL, L"yt-dlp.exe bulunamadı! Lütfen bin/ klasörüne yerleştirin.", p->user_data);
        }
        free(p);
        return 0;
    }

    wchar_t cmd_line[2048];
    _snwprintf(cmd_line, 2048, L"\"%ls\" --no-interactive --dump-json --no-playlist --no-warnings \"%ls\"", ytdlp_path, p->url);

    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);

        if (p->callback) p->callback(0, NULL, L"Boru hattı oluşturulamadı.", p->user_data);
        free(p);
        return 0;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = INVALID_HANDLE_VALUE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    EnterCriticalSection(&g_engine_cs);
    g_cancel_requested = 0;
    BOOL ok = CreateProcessW(NULL, cmd_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!ok) {
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
        if (p->callback) p->callback(0, NULL, L"yt-dlp süreci başlatılamadı.", p->user_data);
        free(p);
        return 0;
    }

    g_current_process = pi.hProcess;
    LeaveCriticalSection(&g_engine_cs);

    /* Close write end immediately so ReadFile sees EOF when child terminates */
    CloseHandle(hWritePipe);

    /* Read JSON stdout */
    size_t buf_capacity = 256 * 1024;
    size_t buf_len = 0;
    char *json_buf = (char*)malloc(buf_capacity);
    DWORD bytes_read = 0;
    char chunk[4096];

    while (ReadFile(hReadPipe, chunk, sizeof(chunk) - 1, &bytes_read, NULL) && bytes_read > 0) {
        if (buf_len + bytes_read >= buf_capacity) {
            buf_capacity *= 2;
            json_buf = (char*)realloc(json_buf, buf_capacity);
        }
        memcpy(json_buf + buf_len, chunk, bytes_read);
        buf_len += bytes_read;
    }
    json_buf[buf_len] = '\0';

    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, 30000);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    EnterCriticalSection(&g_engine_cs);
    HANDLE hProc = g_current_process;
    g_current_process = NULL;
    g_engine_state = ENGINE_STATE_IDLE;
    int was_cancelled = g_cancel_requested;
    LeaveCriticalSection(&g_engine_cs);

    if (hProc) CloseHandle(hProc);
    if (pi.hThread) CloseHandle(pi.hThread);

    if (was_cancelled) {
        if (p->callback) p->callback(0, NULL, L"İşlem kullanıcı tarafından iptal edildi.", p->user_data);
    } else if (exit_code != 0 || buf_len == 0) {
        wchar_t err_msg[512] = L"Video bilgisi alınamadı. Bağlantıyı kontrol edin.";
        if (strstr(json_buf, "Private video")) wcscpy(err_msg, L"Bu video gizli veya erişilemez.");
        else if (strstr(json_buf, "Sign in to confirm")) wcscpy(err_msg, L"Bu video oturum açma veya yaş onayı gerektiriyor.");
        else if (strstr(json_buf, "Video unavailable")) wcscpy(err_msg, L"Bu video kullanılamıyor veya silinmiş.");

        if (p->callback) p->callback(0, NULL, err_msg, p->user_data);
    } else {
        MediaMetadata *meta = (MediaMetadata*)malloc(sizeof(MediaMetadata));
        parse_json_metadata(json_buf, meta);
        meta->platform = detect_platform(p->url);

        if (p->callback) p->callback(1, meta, NULL, p->user_data);
        free(meta);
    }

    free(json_buf);
    free(p);
    return 0;
}

int engine_fetch_info_async(
    const wchar_t *url,
    EngineFetchCallback callback,
    void *user_data
)
{
    EnterCriticalSection(&g_engine_cs);
    if (g_engine_state != ENGINE_STATE_IDLE) {
        LeaveCriticalSection(&g_engine_cs);
        return 0; /* Busy with another task */
    }
    g_engine_state = ENGINE_STATE_FETCHING;
    LeaveCriticalSection(&g_engine_cs);

    FetchThreadParams *p = (FetchThreadParams*)malloc(sizeof(FetchThreadParams));
    wcsncpy(p->url, url, 1023);
    p->url[1023] = L'\0';
    p->callback = callback;
    p->user_data = user_data;

    HANDLE hThread = CreateThread(NULL, 0, fetch_thread_proc, p, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        return 1;
    }

    EnterCriticalSection(&g_engine_cs);
    g_engine_state = ENGINE_STATE_IDLE;
    LeaveCriticalSection(&g_engine_cs);
    free(p);
    return 0;
}

typedef struct {
    wchar_t url[1024];
    SelectionResult selection;
    wchar_t dest_dir[MAX_PATH];
    EngineProgressCallback callback;
    void *user_data;
} DownloadThreadParams;

static void parse_progress_line(
    const char *line,
    double *out_percent,
    double *out_speed,
    int *out_eta,
    long long *out_downloaded,
    long long *out_total
)
{
    /* Format: "%(progress._percent_str)s|%(progress._speed_str)s|%(progress._eta_str)s|%(progress.downloaded_bytes)s|%(progress.total_bytes)s" */
    char copy[512];
    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *pipe1 = strchr(copy, '|');
    if (!pipe1) return;
    *pipe1 = '\0';

    char *pipe2 = strchr(pipe1 + 1, '|');
    if (!pipe2) return;
    *pipe2 = '\0';

    char *pipe3 = strchr(pipe2 + 1, '|');
    if (!pipe3) return;
    *pipe3 = '\0';

    char *pipe4 = strchr(pipe3 + 1, '|');
    if (!pipe4) return;
    *pipe4 = '\0';

    /* 1. Percent */
    char *pct_str = copy;
    while (*pct_str == ' ') pct_str++;
    *out_percent = atof(pct_str);

    /* 2. Speed */
    char *spd_str = pipe1 + 1;
    while (*spd_str == ' ') spd_str++;
    double spd = atof(spd_str);
    if (strstr(spd_str, "MiB/s") || strstr(spd_str, "MB/s")) spd *= 1024.0 * 1024.0;
    else if (strstr(spd_str, "KiB/s") || strstr(spd_str, "kB/s")) spd *= 1024.0;
    *out_speed = spd;

    /* 3. ETA */
    char *eta_str = pipe2 + 1;
    while (*eta_str == ' ') eta_str++;
    int eta_sec = 0;
    int m = 0, s = 0, h = 0;
    if (sscanf(eta_str, "%d:%d:%d", &h, &m, &s) == 3) {
        eta_sec = h * 3600 + m * 60 + s;
    } else if (sscanf(eta_str, "%d:%d", &m, &s) == 2) {
        eta_sec = m * 60 + s;
    }
    *out_eta = eta_sec;

    /* 4. Downloaded bytes */
    *out_downloaded = atoll(pipe3 + 1);

    /* 5. Total bytes */
    *out_total = atoll(pipe4 + 1);
}

static DWORD WINAPI download_thread_proc(LPVOID param)
{
    DownloadThreadParams *p = (DownloadThreadParams*)param;
    wchar_t ytdlp_path[MAX_PATH];
    wchar_t ffmpeg_path[MAX_PATH];
    int has_ffmpeg = engine_find_ffmpeg(ffmpeg_path, MAX_PATH);

    if (!engine_find_ytdlp(ytdlp_path, MAX_PATH)) {
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);

        if (p->callback) {
            p->callback(L"error", 0, 0, 0, 0, 0, L"", L"yt-dlp.exe bulunamadı!", p->user_data);
        }
        free(p);
        return 0;
    }

    wchar_t ffmpeg_dir_arg[MAX_PATH + 32] = L"";
    if (has_ffmpeg) {
        wchar_t *last_slash = wcsrchr(ffmpeg_path, L'\\');
        if (last_slash) *last_slash = L'\0';
        _snwprintf(ffmpeg_dir_arg, sizeof(ffmpeg_dir_arg)/sizeof(wchar_t), L"--ffmpeg-location \"%ls\"", ffmpeg_path);
    }

    wchar_t out_template[MAX_PATH + 32];
    _snwprintf(out_template, sizeof(out_template)/sizeof(wchar_t), L"-o \"%ls\\%%(title)s.%%(ext)s\"", p->dest_dir);

    wchar_t extra_opts[256] = L"";
    if (wcscmp(p->selection.resolution, L"Audio Only") == 0) {
        if (_wcsicmp(p->selection.expected_container, L"mp3") == 0) {
            wcscpy(extra_opts, L"-x --audio-format mp3");
        } else if (_wcsicmp(p->selection.expected_container, L"m4a") == 0) {
            wcscpy(extra_opts, L"-x --audio-format m4a");
        } else {
            wcscpy(extra_opts, L"-x --audio-format opus");
        }
    } else {
        _snwprintf(extra_opts, 255, L"--merge-output-format %ls", p->selection.expected_container);
    }

    wchar_t cmd_line[4096];
    _snwprintf(cmd_line, 4096,
        L"\"%ls\" --no-interactive -f \"%ls\" %ls %ls %ls --newline --progress-template \"%%(progress._percent_str)s|%%(progress._speed_str)s|%%(progress._eta_str)s|%%(progress.downloaded_bytes)s|%%(progress.total_bytes)s\" --no-playlist \"%ls\"",
        ytdlp_path,
        p->selection.format_id,
        extra_opts,
        ffmpeg_dir_arg,
        out_template,
        p->url
    );

    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);

        if (p->callback) p->callback(L"error", 0, 0, 0, 0, 0, L"", L"Boru hattı oluşturulamadı.", p->user_data);
        free(p);
        return 0;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = INVALID_HANDLE_VALUE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    EnterCriticalSection(&g_engine_cs);
    g_cancel_requested = 0;
    BOOL ok = CreateProcessW(NULL, cmd_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!ok) {
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
        if (p->callback) p->callback(L"error", 0, 0, 0, 0, 0, L"", L"İndirme süreci başlatılamadı.", p->user_data);
        free(p);
        return 0;
    }

    g_current_process = pi.hProcess;
    LeaveCriticalSection(&g_engine_cs);

    /* Close write end immediately so ReadFile sees EOF when process finishes */
    CloseHandle(hWritePipe);

    /* Read output line by line */
    char line_buf[1024];
    size_t line_len = 0;
    char ch;
    DWORD bytes_read = 0;

    double last_percent = 0.0;
    double last_speed = 0.0;
    int last_eta = 0;
    long long last_downloaded = 0;
    long long last_total = 0;

    while (ReadFile(hReadPipe, &ch, 1, &bytes_read, NULL) && bytes_read > 0) {
        if (ch == '\r' || ch == '\n') {
            if (line_len > 0) {
                line_buf[line_len] = '\0';
                if (strchr(line_buf, '|')) {
                    parse_progress_line(line_buf, &last_percent, &last_speed, &last_eta, &last_downloaded, &last_total);
                    if (p->callback) {
                        p->callback(L"downloading", last_percent, last_speed, last_eta, last_downloaded, last_total, L"", NULL, p->user_data);
                    }
                } else if (strstr(line_buf, "[Merger]") || strstr(line_buf, "[ExtractAudio]")) {
                    if (p->callback) {
                        p->callback(L"processing", 100.0, 0, 0, last_total, last_total, L"", NULL, p->user_data);
                    }
                }
                line_len = 0;
            }
        } else if (line_len + 1 < sizeof(line_buf)) {
            line_buf[line_len++] = ch;
        }
    }

    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    EnterCriticalSection(&g_engine_cs);
    HANDLE hProc = g_current_process;
    g_current_process = NULL;
    g_engine_state = ENGINE_STATE_IDLE;
    int was_cancelled = g_cancel_requested;
    LeaveCriticalSection(&g_engine_cs);

    if (hProc) CloseHandle(hProc);
    if (pi.hThread) CloseHandle(pi.hThread);

    if (p->callback) {
        if (was_cancelled) {
            p->callback(L"cancelled", 0, 0, 0, 0, 0, L"", L"İndirme kullanıcı tarafından iptal edildi.", p->user_data);
        } else if (exit_code == 0) {
            p->callback(L"finished", 100.0, 0, 0, last_total, last_total, L"", NULL, p->user_data);
        } else {
            p->callback(L"error", 0, 0, 0, 0, 0, L"", L"İndirme sırasında bir hata oluştu.", p->user_data);
        }
    }

    free(p);
    return 0;
}

int engine_download_async(
    const wchar_t *url,
    const SelectionResult *selection,
    const wchar_t *dest_dir,
    EngineProgressCallback callback,
    void *user_data
)
{
    EnterCriticalSection(&g_engine_cs);
    if (g_engine_state != ENGINE_STATE_IDLE) {
        LeaveCriticalSection(&g_engine_cs);
        return 0; /* Busy with another task */
    }
    g_engine_state = ENGINE_STATE_DOWNLOADING;
    LeaveCriticalSection(&g_engine_cs);

    DownloadThreadParams *p = (DownloadThreadParams*)malloc(sizeof(DownloadThreadParams));
    wcsncpy(p->url, url, 1023);
    p->url[1023] = L'\0';
    memcpy(&p->selection, selection, sizeof(SelectionResult));
    wcsncpy(p->dest_dir, dest_dir, MAX_PATH - 1);
    p->dest_dir[MAX_PATH - 1] = L'\0';
    p->callback = callback;
    p->user_data = user_data;

    HANDLE hThread = CreateThread(NULL, 0, download_thread_proc, p, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        return 1;
    }

    EnterCriticalSection(&g_engine_cs);
    g_engine_state = ENGINE_STATE_IDLE;
    LeaveCriticalSection(&g_engine_cs);
    free(p);
    return 0;
}
