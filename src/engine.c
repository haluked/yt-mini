#include "engine.h"
#include "cjson.h"
#include "sanitization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <shlobj.h>

static HANDLE g_current_process = NULL;
static HANDLE g_current_job = NULL;
static volatile int g_cancel_requested = 0;
static CRITICAL_SECTION g_engine_cs;
static EngineState g_engine_state = ENGINE_STATE_IDLE;
static volatile LONG g_active_workers = 0;
static HANDLE g_workers_done = NULL;

int engine_init(void)
{
    InitializeCriticalSection(&g_engine_cs);
    g_engine_state = ENGINE_STATE_IDLE;
    g_cancel_requested = 0;
    g_active_workers = 0;
    g_workers_done = CreateEventW(NULL, TRUE, TRUE, NULL);
    return 1;
}

void engine_cleanup(void)
{
    engine_cancel();

    /* Deleting running threads before they leave the critical section
       caused use-after-free. Wait for them to finish first. */
    if (g_workers_done) {
        WaitForSingleObject(g_workers_done, 5000);
    }

    /* If a worker is still running, close neither the critical section nor the event:
       worker_finished() could call SetEvent on a closed handle. */
    int idle = (InterlockedCompareExchange(&g_active_workers, 0, 0) == 0);
    if (idle) {
        DeleteCriticalSection(&g_engine_cs);
        if (g_workers_done) {
            CloseHandle(g_workers_done);
            g_workers_done = NULL;
        }
    }
}

static void worker_started(void)
{
    InterlockedIncrement(&g_active_workers);
    if (g_workers_done) ResetEvent(g_workers_done);
}

static void worker_finished(void)
{
    if (InterlockedDecrement(&g_active_workers) == 0 && g_workers_done) {
        SetEvent(g_workers_done);
    }
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
    if (g_current_job) {
        /* The job object also terminates the ffmpeg child process along with yt-dlp. */
        TerminateJobObject(g_current_job, 1);
    } else if (g_current_process && g_current_process != INVALID_HANDLE_VALUE) {
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

static HANDLE create_kill_job(void)
{
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (!job) return NULL;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    memset(&info, 0, sizeof(info));
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(job);
        return NULL;
    }
    return job;
}

/* Values placed in quotes on the command line must not contain quotes, backslashes
   or control characters; otherwise the argument boundary could be escaped. */
static int safe_command_arg(const wchar_t *s)
{
    if (!s || !*s) return 0;
    for (const wchar_t *p = s; *p; p++) {
        if (*p == L'"' || *p == L'\\' || iswcntrl(*p)) return 0;
    }
    return 1;
}

static int utf8_to_wide_bounded(const char *src, int src_len, wchar_t *dst, size_t max_len)
{
    if (!dst || max_len == 0) return 0;
    if (!src || src_len <= 0) {
        dst[0] = L'\0';
        return 0;
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, src, src_len, dst, (int)(max_len - 1));
    if (n <= 0) {
        dst[0] = L'\0';
        return 0;
    }
    dst[n] = L'\0';
    return 1;
}

/* Fits UTF-8 text into a fixed-width field. MultiByteToWideChar writes
   NOTHING if the target buffer is too small (returns 0); so long titles left
   the field empty. We align the byte limit to a character boundary and truncate. */
static void utf8_to_wide_field(const char *src, wchar_t *dst, size_t wide_cap)
{
    if (!dst || wide_cap < 2) {
        if (dst && wide_cap) dst[0] = L'\0';
        return;
    }
    dst[0] = L'\0';
    if (!src) return;

    int bytes = (int)strlen(src);
    if (bytes <= 0) return;

    int limit = (int)(wide_cap - 1);
    if (limit > bytes) limit = bytes;
    /* Step back so a multi-byte character is not cut in half. */
    while (limit > 0 && ((unsigned char)src[limit] & 0xC0) == 0x80) limit--;
    if (limit <= 0) return;

    int n = MultiByteToWideChar(CP_UTF8, 0, src, limit, dst, (int)(wide_cap - 1));
    if (n <= 0) {
        dst[0] = L'\0';
        return;
    }
    dst[n] = L'\0';
}

static void extract_last_error(const char *buf, wchar_t *out, size_t max_len)
{
    if (!out || max_len == 0) return;
    out[0] = L'\0';
    if (!buf) return;

    const char *last = NULL;
    const char *p = buf;
    while ((p = strstr(p, "ERROR:")) != NULL) {
        last = p;
        p += 6;
    }
    if (!last) return;

    last += 6;
    while (*last == ' ' || *last == '\t') last++;
    const char *end = last;
    while (*end && *end != '\r' && *end != '\n') end++;
    int len = (int)(end - last);
    if (len > 500) len = 500;
    utf8_to_wide_bounded(last, len, out, max_len);
}

static int ensure_directory(const wchar_t *dir)
{
    if (!dir || !*dir) return 0;
    int rc = SHCreateDirectoryExW(NULL, dir, NULL);
    return rc == ERROR_SUCCESS || rc == ERROR_ALREADY_EXISTS || rc == ERROR_FILE_EXISTS;
}

/* Reads a pipe until EOF and writes the FIRST `cap - 1` bytes into buf.
   It keeps reading after the buffer is full so the child process does not
   block while writing to stderr (otherwise the two-pipe setup would deadlock). */
static void drain_pipe_blocking(HANDLE pipe, char *buf, size_t cap, size_t *out_len)
{
    char chunk[1024];
    DWORD read = 0;
    size_t len = 0;

    if (buf && cap > 0) buf[0] = '\0';
    while (ReadFile(pipe, chunk, sizeof(chunk), &read, NULL) && read > 0) {
        if (!buf || cap == 0) continue;
        size_t copy = read;
        if (len + copy + 1 > cap) {
            copy = (cap > len + 1) ? (cap - len - 1) : 0;
        }
        if (copy > 0) {
            memcpy(buf + len, chunk, copy);
            len += copy;
            buf[len] = '\0';
        }
    }
    if (out_len) *out_len = len;
}

/* yt-dlp downloads separate video/audio streams individually, so a single "final
   destination" is not enough: a merge failure can leave multiple intermediate files.
   Keep the last 4 destinations (normally there are at most 2). */
#define MAX_TRACKED_DESTS 4
#define DEST_BUF_LEN (MAX_PATH * 2)

static void add_destination(char (*dests)[DEST_BUF_LEN], int *count, const char *src, size_t len)
{
    if (!dests || !count || !src) return;

    if (*count >= MAX_TRACKED_DESTS) {
        for (int i = 1; i < MAX_TRACKED_DESTS; i++) {
            memcpy(dests[i - 1], dests[i], DEST_BUF_LEN);
        }
        (*count)--;
    }
    if (len == (size_t)-1) len = strlen(src);
    if (len >= DEST_BUF_LEN) len = DEST_BUF_LEN - 1;
    memcpy(dests[*count], src, len);
    dests[*count][len] = '\0';
    (*count)++;
}

static int path_is_fresh_file(const wchar_t *path, const FILETIME *since)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!path || !path[0]) return 0;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return 0;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 0;
    return CompareFileTime(&fad.ftLastWriteTime, since) >= 0;
}

typedef struct {
    HANDLE pipe;
    char buf[4096];
    size_t len;
} StderrDrainParams;

static DWORD WINAPI stderr_drain_thread(LPVOID param)
{
    StderrDrainParams *d = (StderrDrainParams*)param;
    drain_pipe_blocking(d->pipe, d->buf, sizeof(d->buf), &d->len);
    return 0;
}

/* yt-dlp's --print-to-file output is UTF-8; independent of language or code page,
   so we read the final file path from there. It returns the file's last
   non-empty line. */
static int read_last_utf8_line(const wchar_t *path, wchar_t *out, size_t max_len)
{
    if (!out || max_len == 0) return 0;
    out[0] = L'\0';
    if (!path || !*path) return 0;

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;

    char buf[8192];
    LARGE_INTEGER fsize;
    if (GetFileSizeEx(h, &fsize)) {
        LARGE_INTEGER pos;
        pos.QuadPart = fsize.QuadPart > (LONGLONG)(sizeof(buf) - 1)
                     ? fsize.QuadPart - (LONGLONG)(sizeof(buf) - 1) : 0;
        SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    }

    DWORD total = 0, got = 0;
    while (total < sizeof(buf) - 1) {
        DWORD want = (DWORD)(sizeof(buf) - 1 - total);
        if (!ReadFile(h, buf + total, want, &got, NULL) || got == 0) break;
        total += got;
    }
    CloseHandle(h);
    buf[total] = '\0';

    int end = (int)total;
    while (end > 0 && (buf[end - 1] == '\n' || buf[end - 1] == '\r')) end--;
    if (end <= 0) return 0;
    int start = end;
    while (start > 0 && buf[start - 1] != '\n' && buf[start - 1] != '\r') start--;
    int len = end - start;
    if (len <= 0) return 0;

    int n = MultiByteToWideChar(CP_UTF8, 0, buf + start, len, out, (int)(max_len - 1));
    if (n <= 0) { out[0] = L'\0'; return 0; }
    out[n] = L'\0';
    return 1;
}

/* stdout lines may not be UTF-8 (yt-dlp can emit cp1254). Try UTF-8 first,
   then fall back to the system ANSI code page. */
static void decode_path_bytes(const char *src, wchar_t *dst, size_t max_len)
{
    if (!dst || max_len == 0) return;
    dst[0] = L'\0';
    if (!src || !*src) return;
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, dst, (int)(max_len - 1));
    if (n <= 0) n = MultiByteToWideChar(CP_ACP, 0, src, -1, dst, (int)(max_len - 1));
    if (n <= 0) dst[0] = L'\0';
    else dst[n] = L'\0';
}

/* yt-dlp is a Python program. PYTHONUTF8=1 forces Python's own file I/O
   layer to UTF-8; however yt-dlp.exe's CONSOLE output can still arrive in the
   local code page (cp1254 on Turkish Windows). Therefore:
     - JSON output is ASCII-escaped (\u00f6) -> always safe,
     - the final file path is read from a UTF-8 file via --print-to-file. */
static wchar_t *build_child_environment(void)
{
    LPCWSTR src = GetEnvironmentStringsW();
    if (!src) return NULL;

    size_t total = 0;
    for (LPCWSTR p = src; *p; p += wcslen(p) + 1) {
        total += wcslen(p) + 1;
    }

    static const wchar_t extra[] = L"PYTHONUTF8=1";
    size_t extra_len = wcslen(extra) + 1;             /* dize + NUL */
    wchar_t *env = (wchar_t*)malloc((total + extra_len + 1) * sizeof(wchar_t));
    if (!env) {
        FreeEnvironmentStringsW((LPWSTR)src);
        return NULL;
    }

    memcpy(env, src, total * sizeof(wchar_t));
    memcpy(env + total, extra, extra_len * sizeof(wchar_t));
    env[total + extra_len] = L'\0';                   /* second NUL terminating the block */

    FreeEnvironmentStringsW((LPWSTR)src);
    return env;
}

/* The bundled ffmpeg does not always include an MP3 encoder; to avoid showing
   a pointless "MP3" option and failing downloads halfway, we check once
   and cache the result. */
int engine_ffmpeg_can_encode_mp3(void)
{
    static int cached = -1;
    if (cached >= 0) return cached;

    wchar_t ffmpeg_path[MAX_PATH];
    if (!engine_find_ffmpeg(ffmpeg_path, MAX_PATH)) return 0;

    wchar_t cmd_line[MAX_PATH + 64];
    _snwprintf(cmd_line, MAX_PATH + 64, L"\"%ls\" -hide_banner -encoders", ffmpeg_path);
    cmd_line[MAX_PATH + 63] = L'\0';

    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return 0;
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

    wchar_t *child_env = build_child_environment();
    BOOL ok = CreateProcessW(NULL, cmd_line, NULL, NULL, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, child_env, NULL, &si, &pi);
    free(child_env);
    CloseHandle(hWritePipe);
    if (!ok) {
        CloseHandle(hReadPipe);
        return 0;
    }

    /* The output may be large; keep reading even after the buffer is full.
       The total time is limited so a stuck ffmpeg cannot freeze startup. */
    char out[32768];
    size_t len = 0;
    DWORD got = 0;
    char chunk[4096];
    DWORD start_tick = GetTickCount();
    int finished = 0;

    for (;;) {
        DWORD avail = 0;
        if (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            DWORD want = avail > (DWORD)sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
            if (!ReadFile(hReadPipe, chunk, want, &got, NULL) || got == 0) break;
            if (len + got < sizeof(out)) {
                memcpy(out + len, chunk, got);
                len += got;
            }
            continue;
        }
        if (WaitForSingleObject(pi.hProcess, 20) == WAIT_OBJECT_0) {
            /* Process finished: drain the remaining bytes one last time. */
            while (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
                DWORD want = avail > (DWORD)sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
                if (!ReadFile(hReadPipe, chunk, want, &got, NULL) || got == 0) break;
                if (len + got < sizeof(out)) {
                    memcpy(out + len, chunk, got);
                    len += got;
                }
            }
            finished = 1;
            break;
        }
        if (GetTickCount() - start_tick > 5000) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }
    out[len] = '\0';
    WaitForSingleObject(pi.hProcess, 2000);
    CloseHandle(hReadPipe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    /* Cache the result only if ffmpeg actually ran and produced output;
       otherwise a temporary error (AV lock, etc.) would disable MP3 for the whole session. */
    if (finished) {
        cached = (strstr(out, "libmp3lame") || strstr(out, "libshine") ||
                  strstr(out, "mp3_mf")) ? 1 : 0;
        return cached;
    }
    return 0;
}

void format_duration_str(int seconds, wchar_t *out_str, size_t max_len)
{
    if (seconds <= 0) {
        wcsncpy(out_str, L"Unknown", max_len - 1);
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
        utf8_to_wide_field(title_item->valuestring, meta->title,
                           sizeof(meta->title) / sizeof(wchar_t));
    }

    cJSON *dur_item = cJSON_GetObjectItem(root, "duration");
    if (cJSON_IsNumber(dur_item)) {
        meta->duration = dur_item->valueint;
    }
    format_duration_str(meta->duration, meta->duration_str, sizeof(meta->duration_str)/sizeof(wchar_t));

    cJSON *thumb_item = cJSON_GetObjectItem(root, "thumbnail");
    if (cJSON_IsString(thumb_item) && thumb_item->valuestring) {
        utf8_to_wide_field(thumb_item->valuestring, meta->thumbnail_url,
                           sizeof(meta->thumbnail_url) / sizeof(wchar_t));
    }

    cJSON *formats_array = cJSON_GetObjectItem(root, "formats");
    if (cJSON_IsArray(formats_array)) {
        int arr_size = cJSON_GetArraySize(formats_array);
        /* yt-dlp lists formats from worst to best. If the limit is reached, take
           from the end so the BEST formats stay in the list. */
        int start = (arr_size > MAX_FORMATS) ? (arr_size - MAX_FORMATS) : 0;
        int f_idx = 0;
        for (int i = start; i < arr_size && f_idx < MAX_FORMATS; i++) {
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

    /* If the 'formats' array is missing or empty, check the single format info at root level */
    if (meta->format_count == 0) {
        cJSON *id = cJSON_GetObjectItem(root, "format_id");
        cJSON *ext = cJSON_GetObjectItem(root, "ext");
        cJSON *url = cJSON_GetObjectItem(root, "url");
        if ((id && cJSON_IsString(id)) || (ext && cJSON_IsString(ext)) || (url && cJSON_IsString(url))) {
            StreamFormat *sf = &meta->formats[0];
            memset(sf, 0, sizeof(StreamFormat));
            if (id && cJSON_IsString(id) && id->valuestring) strncpy(sf->format_id, id->valuestring, 63);
            else if (ext && cJSON_IsString(ext) && ext->valuestring) strncpy(sf->format_id, ext->valuestring, 63);
            else strcpy(sf->format_id, "direct");

            if (ext && cJSON_IsString(ext) && ext->valuestring) strncpy(sf->ext, ext->valuestring, 15);
            else strcpy(sf->ext, "mp4");

            cJSON *vc = cJSON_GetObjectItem(root, "vcodec");
            if (cJSON_IsString(vc) && vc->valuestring) strncpy(sf->vcodec, vc->valuestring, 63);
            else strcpy(sf->vcodec, "none");

            cJSON *ac = cJSON_GetObjectItem(root, "acodec");
            if (cJSON_IsString(ac) && ac->valuestring) strncpy(sf->acodec, ac->valuestring, 63);
            else strcpy(sf->acodec, "none");

            cJSON *h = cJSON_GetObjectItem(root, "height");
            if (cJSON_IsNumber(h)) sf->height = h->valueint;

            cJSON *w = cJSON_GetObjectItem(root, "width");
            if (cJSON_IsNumber(w)) sf->width = w->valueint;

            cJSON *fps = cJSON_GetObjectItem(root, "fps");
            if (cJSON_IsNumber(fps)) sf->fps = fps->valueint;

            meta->format_count = 1;
            meta->num_resolutions = get_available_resolutions(meta->formats, 1, meta->available_resolutions, MAX_RESOLUTIONS);
        }
    }

    cJSON_Delete(root);
}

static DWORD WINAPI fetch_thread_main(LPVOID param)
{
    FetchThreadParams *p = (FetchThreadParams*)param;
    wchar_t ytdlp_path[MAX_PATH];
    HANDLE job = NULL;

    if (!engine_find_ytdlp(ytdlp_path, MAX_PATH)) {
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);

        if (p->callback) {
            p->callback(0, NULL, L"yt-dlp.exe not found! Please place it in the bin/ folder.", p->user_data);
        }
        free(p);
        return 0;
    }

    if (!safe_command_arg(p->url)) {
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);

        if (p->callback) p->callback(0, NULL, L"Invalid link address.", p->user_data);
        free(p);
        return 0;
    }

    wchar_t cmd_line[2048];
    _snwprintf(cmd_line, 2048, L"\"%ls\" --dump-json --no-playlist --no-warnings \"%ls\"", ytdlp_path, p->url);

    /* stdout: JSON only; stderr: warning/error lines.
       If they shared a pipe, even a single line before the JSON broke parsing. */
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    HANDLE hErrReadPipe = NULL, hErrWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0) ||
        !CreatePipe(&hErrReadPipe, &hErrWritePipe, &sa, 64 * 1024)) {
        if (hReadPipe) CloseHandle(hReadPipe);
        if (hWritePipe) CloseHandle(hWritePipe);
        if (hErrReadPipe) CloseHandle(hErrReadPipe);
        if (hErrWritePipe) CloseHandle(hErrWritePipe);
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);

        if (p->callback) p->callback(0, NULL, L"Could not create the pipe.", p->user_data);
        free(p);
        return 0;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hErrReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = INVALID_HANDLE_VALUE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hErrWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    job = create_kill_job();
    wchar_t *child_env = build_child_environment();

    EnterCriticalSection(&g_engine_cs);
    BOOL ok = CreateProcessW(NULL, cmd_line, NULL, NULL, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
        child_env, NULL, &si, &pi);
    free(child_env);

    if (!ok) {
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
        CloseHandle(hErrWritePipe);
        CloseHandle(hErrReadPipe);
        if (job) CloseHandle(job);
        if (p->callback) p->callback(0, NULL, L"Could not start the yt-dlp process.", p->user_data);
        free(p);
        return 0;
    }

    if (job && !AssignProcessToJobObject(job, pi.hProcess)) {
        CloseHandle(job);
        job = NULL;
    }
    ResumeThread(pi.hThread);

    g_current_process = pi.hProcess;
    g_current_job = job;
    int cancel_now = g_cancel_requested;
    LeaveCriticalSection(&g_engine_cs);

    if (cancel_now) {
        if (g_current_job) TerminateJobObject(g_current_job, 1);
        else TerminateProcess(pi.hProcess, 0);
    }

    /* Close write ends immediately so ReadFile sees EOF when child terminates */
    CloseHandle(hWritePipe);
    CloseHandle(hErrWritePipe);

    /* Read JSON stdout */
    size_t buf_capacity = 1024 * 1024;
    size_t buf_len = 0;
    char *json_buf = (char*)malloc(buf_capacity);
    if (!json_buf) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(hReadPipe);
        CloseHandle(hErrReadPipe);

        EnterCriticalSection(&g_engine_cs);
        HANDLE hProc = g_current_process;
        HANDLE hJob = g_current_job;
        g_current_process = NULL;
        g_current_job = NULL;
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);

        if (hJob) CloseHandle(hJob);
        if (hProc) CloseHandle(hProc);
        if (pi.hThread) CloseHandle(pi.hThread);
        if (p->callback) p->callback(0, NULL, L"Out of memory.", p->user_data);
        free(p);
        return 0;
    }

    DWORD bytes_read = 0;
    char chunk[8192];
    int truncated = 0;

    /* stderr is drained continuously in a separate thread. Otherwise, if the child
       writes more than 64 KB of errors/warnings, the stderr pipe fills, the child
       blocks and the app deadlocks waiting for EOF on stdout. Parameters live on
       the heap so a join timeout cannot leave a thread writing to a dead stack. */
    char err_buf[4096];
    err_buf[0] = '\0';
    StderrDrainParams *drain = (StderrDrainParams*)calloc(1, sizeof(StderrDrainParams));
    HANDLE hDrainThread = NULL;
    if (drain) {
        drain->pipe = hErrReadPipe;
        hDrainThread = CreateThread(NULL, 0, stderr_drain_thread, drain, 0, NULL);
    }

    while (ReadFile(hReadPipe, chunk, sizeof(chunk), &bytes_read, NULL) && bytes_read > 0) {
        if (truncated) continue;   /* already overflowed: just drain so the pipe does not clog */

        if (buf_len + bytes_read + 1 >= buf_capacity) {
            /* Grow the capacity ONLY if realloc succeeds. Otherwise the old
               buffer stays small, the check passes and memcpy overflows. */
            size_t new_cap = (buf_capacity * 2) + bytes_read + 1;
            char *new_buf = (char*)realloc(json_buf, new_cap);
            if (!new_buf) {
                truncated = 1;
                continue;
            }
            json_buf = new_buf;
            buf_capacity = new_cap;
        }
        memcpy(json_buf + buf_len, chunk, bytes_read);
        buf_len += bytes_read;
    }
    json_buf[buf_len] = '\0';

    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);

    /* Process finished: the stderr thread returns after EOF. If the thread
       could not be created (very rare), do the same work here. */
    if (hDrainThread) {
        DWORD join_res = WaitForSingleObject(hDrainThread, 10000);
        CloseHandle(hDrainThread);
        if (join_res == WAIT_OBJECT_0) {
            memcpy(err_buf, drain->buf, sizeof(err_buf));
            err_buf[sizeof(err_buf) - 1] = '\0';
            free(drain);
            CloseHandle(hErrReadPipe);
        } else {
            /* The thread may still be running: intentionally leak the handles and
               buffer (cleaned up at process exit) so it never writes to dead memory. */
            hErrReadPipe = NULL;
        }
    } else {
        if (drain) free(drain);
        drain_pipe_blocking(hErrReadPipe, err_buf, sizeof(err_buf), NULL);
        CloseHandle(hErrReadPipe);
    }

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    EnterCriticalSection(&g_engine_cs);
    HANDLE hProc = g_current_process;
    HANDLE hJob = g_current_job;
    g_current_process = NULL;
    g_current_job = NULL;
    g_engine_state = ENGINE_STATE_IDLE;
    int was_cancelled = g_cancel_requested;
    LeaveCriticalSection(&g_engine_cs);

    if (hJob) CloseHandle(hJob);
    if (hProc) CloseHandle(hProc);
    if (pi.hThread) CloseHandle(pi.hThread);

    if (was_cancelled) {
        if (p->callback) p->callback(0, NULL, ENGINE_MSG_CANCELLED, p->user_data);
    } else if (truncated) {
        if (p->callback) p->callback(0, NULL, L"yt-dlp response too large; media info could not be read.", p->user_data);
    } else if (exit_code != 0 || buf_len == 0) {
        wchar_t err_msg[512] = L"Could not fetch video info. Check the link.";
        if (strstr(err_buf, "Private video")) wcscpy(err_msg, L"This video is private or unavailable.");
        else if (strstr(err_buf, "Sign in to confirm")) wcscpy(err_msg, L"This video requires signing in or age verification.");
        else if (strstr(err_buf, "Video unavailable")) wcscpy(err_msg, L"This video is unavailable or has been removed.");
        else {
            wchar_t detail[512];
            extract_last_error(err_buf, detail, 512);
            if (!detail[0]) extract_last_error(json_buf, detail, 512);
            if (detail[0]) {
                _snwprintf(err_msg, 512, L"yt-dlp: %ls", detail);
            }
        }

        if (p->callback) p->callback(0, NULL, err_msg, p->user_data);
    } else {
        MediaMetadata *meta = (MediaMetadata*)malloc(sizeof(MediaMetadata));
        if (meta) {
            parse_json_metadata(json_buf, meta);
            meta->platform = detect_platform(p->url);
            if (p->callback) p->callback(1, meta, NULL, p->user_data);
            free(meta);
        } else if (p->callback) {
            p->callback(0, NULL, L"Out of memory.", p->user_data);
        }
    }

    free(json_buf);
    free(p);
    return 0;
}

static DWORD WINAPI fetch_thread_proc(LPVOID param)
{
    DWORD rc = fetch_thread_main(param);
    worker_finished();
    return rc;
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
    g_cancel_requested = 0;
    LeaveCriticalSection(&g_engine_cs);

    FetchThreadParams *p = (FetchThreadParams*)malloc(sizeof(FetchThreadParams));
    if (!p) {
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);
        return 0;
    }
    wcsncpy(p->url, url, 1023);
    p->url[1023] = L'\0';
    p->callback = callback;
    p->user_data = user_data;

    worker_started();
    HANDLE hThread = CreateThread(NULL, 0, fetch_thread_proc, p, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        return 1;
    }
    worker_finished();

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
    if (strstr(spd_str, "GiB/s")) spd *= 1024.0 * 1024.0 * 1024.0;
    else if (strstr(spd_str, "MiB/s") || strstr(spd_str, "MB/s")) spd *= 1024.0 * 1024.0;
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

    /* Showing a percentage makes no sense when the total size is unknown. With
       multiple streams yt-dlp can send "100% / 0 bytes", which made the gauge
       fill before the work was done. */
    if (*out_total <= 0) *out_percent = 0;
}

static void download_fail(DownloadThreadParams *p, const wchar_t *message)
{
    EnterCriticalSection(&g_engine_cs);
    g_engine_state = ENGINE_STATE_IDLE;
    LeaveCriticalSection(&g_engine_cs);
    if (p->callback) {
        p->callback(L"error", 0, 0, 0, 0, 0, L"", message, p->user_data);
    }
    free(p);
}

static DWORD WINAPI download_thread_main(LPVOID param)
{
    DownloadThreadParams *p = (DownloadThreadParams*)param;
    wchar_t ytdlp_path[MAX_PATH];
    wchar_t ffmpeg_path[MAX_PATH];
    int has_ffmpeg = engine_find_ffmpeg(ffmpeg_path, MAX_PATH);
    HANDLE job = NULL;

    if (!engine_find_ytdlp(ytdlp_path, MAX_PATH)) {
        download_fail(p, L"yt-dlp.exe not found! Please place it in the bin/ folder.");
        return 0;
    }

    if (!safe_command_arg(p->url) ||
        !safe_command_arg(p->selection.format_id) ||
        !safe_command_arg(p->selection.expected_container)) {
        download_fail(p, L"Invalid download parameter.");
        return 0;
    }

    int audio_mode = (wcscmp(p->selection.resolution, L"Audio Only") == 0);
    int needs_ffmpeg = audio_mode || !p->selection.is_direct || p->selection.needs_reencode;
    if (needs_ffmpeg && !has_ffmpeg) {
        download_fail(p, L"ffmpeg.exe not found. ffmpeg is required for merging or converting.");
        return 0;
    }

    /* The bundled ffmpeg may lack an MP3 encoder. Instead of starting the
       download and failing halfway, stop here and say so clearly. */
    if (audio_mode && _wcsicmp(p->selection.expected_container, L"mp3") == 0 &&
        !engine_ffmpeg_can_encode_mp3()) {
        download_fail(p, L"This ffmpeg build has no MP3 encoder. Please choose AAC (m4a) or Opus.");
        return 0;
    }

    if (!ensure_directory(p->dest_dir)) {
        download_fail(p, L"Could not create the download folder. Please choose another folder.");
        return 0;
    }

    wchar_t ffmpeg_dir_arg[MAX_PATH + 32] = L"";
    if (has_ffmpeg) {
        wchar_t *last_slash = wcsrchr(ffmpeg_path, L'\\');
        if (last_slash) *last_slash = L'\0';
        _snwprintf(ffmpeg_dir_arg, sizeof(ffmpeg_dir_arg)/sizeof(wchar_t), L"--ffmpeg-location \"%ls\"", ffmpeg_path);
    }

    /* Keep long titles from exceeding MAX_PATH and stop videos with the same
       title from overwriting each other. A '%' in the folder name is escaped
       as '%%' because it is special in the yt-dlp template engine. */
    wchar_t dir_escaped[MAX_PATH * 2];
    {
        const wchar_t *src = p->dest_dir;
        wchar_t *dst = dir_escaped;
        wchar_t *dst_end = dir_escaped + (sizeof(dir_escaped) / sizeof(wchar_t)) - 1;
        while (*src && dst + 2 <= dst_end) {
            if (*src == L'%') { *dst++ = L'%'; *dst++ = L'%'; }
            else { *dst++ = *src; }
            src++;
        }
        *dst = L'\0';
    }

    /* If the folder name contains '%', the escape doubles (<= 2*MAX_PATH).
       Size the buffer accordingly and always guarantee a NUL. */
    wchar_t out_template[MAX_PATH * 2 + 128];
    _snwprintf(out_template, sizeof(out_template)/sizeof(wchar_t),
        L"-o \"%ls\\%%(title).150B [%%(id)s].%%(ext)s\"", dir_escaped);
    out_template[(sizeof(out_template)/sizeof(wchar_t)) - 1] = L'\0';

    /* Instead of guessing the final path from text, we have yt-dlp write it to
       a UTF-8 file: the stdout code page can be broken (e.g. cp1254), so
       with Turkish/emoji titles "Open File Location" went to the wrong
       path. --print-to-file always writes UTF-8. */
    wchar_t final_path_file[MAX_PATH] = L"";
    wchar_t final_path_arg[MAX_PATH * 2] = L"";
    {
        wchar_t tmp_dir[MAX_PATH];
        tmp_dir[0] = L'\0';
        /* GetTempPathW returns the required size, NOT 0, when the buffer is small. */
        DWORD tmp_len = GetTempPathW(MAX_PATH, tmp_dir);
        if (tmp_len > 0 && tmp_len < MAX_PATH) {
            _snwprintf(final_path_file, MAX_PATH, L"%lsffdlp_finalpath_%lu.txt",
                tmp_dir, (unsigned long)GetCurrentProcessId());
            final_path_file[MAX_PATH - 1] = L'\0';
            DeleteFileW(final_path_file);

            /* yt-dlp treats this as an output template: the '%' escape is required. */
            const wchar_t *src = final_path_file;
            wchar_t *dst = final_path_arg;
            wchar_t *dst_end = final_path_arg + (sizeof(final_path_arg) / sizeof(wchar_t)) - 1;
            while (*src && dst + 2 <= dst_end) {
                if (*src == L'%') { *dst++ = L'%'; *dst++ = L'%'; }
                else { *dst++ = *src; }
                src++;
            }
            *dst = L'\0';
        }
    }

    wchar_t extra_opts[512] = L"";
    if (audio_mode) {
        if (_wcsicmp(p->selection.expected_container, L"mp3") == 0) {
            wcscpy(extra_opts, L"-x --audio-format mp3");
        } else if (_wcsicmp(p->selection.expected_container, L"m4a") == 0) {
            wcscpy(extra_opts, L"-x --audio-format m4a");
        } else {
            wcscpy(extra_opts, L"-x --audio-format opus");
        }
    } else if (p->selection.audio_only_reencode) {
        /* The video stream is copied; only the audio is converted to AAC and merged into MP4.
           (using --recode-video would re-encode the source video as well.)
           yt-dlp syntax is "PP:ARGS" (PP+EXE:ARGS); the '+' separator is WRONG here. */
        _snwprintf(extra_opts, 511,
            L"--merge-output-format %ls --ppa \"Merger:-c:v copy -c:a aac\"",
            p->selection.expected_container);
    } else if (p->selection.needs_reencode) {
        _snwprintf(extra_opts, 511, L"--recode-video %ls", p->selection.expected_container);
    } else if (!p->selection.is_direct) {
        _snwprintf(extra_opts, 511, L"--merge-output-format %ls", p->selection.expected_container);
    }

    wchar_t path_report_arg[MAX_PATH * 2 + 80] = L"";
    if (final_path_file[0] != L'\0') {
        _snwprintf(path_report_arg, sizeof(path_report_arg)/sizeof(wchar_t),
            L"--print-to-file \"after_move:filepath\" \"%ls\"", final_path_arg);
        path_report_arg[(sizeof(path_report_arg)/sizeof(wchar_t)) - 1] = L'\0';
    }

    wchar_t cmd_line[4096];
    _snwprintf(cmd_line, 4096,
        L"\"%ls\" -f \"%ls\" %ls %ls %ls %ls --newline --progress-template \"%%(progress._percent_str)s|%%(progress._speed_str)s|%%(progress._eta_str)s|%%(progress.downloaded_bytes)s|%%(progress.total_bytes)s\" --no-playlist \"%ls\"",
        ytdlp_path,
        p->selection.format_id,
        extra_opts,
        ffmpeg_dir_arg,
        out_template,
        path_report_arg,
        p->url
    );
    cmd_line[4095] = L'\0';

    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        download_fail(p, L"Could not create the pipe.");
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

    job = create_kill_job();
    wchar_t *child_env = build_child_environment();

    FILETIME dl_start_time;
    GetSystemTimeAsFileTime(&dl_start_time);

    EnterCriticalSection(&g_engine_cs);
    BOOL ok = CreateProcessW(NULL, cmd_line, NULL, NULL, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
        child_env, NULL, &si, &pi);
    free(child_env);

    if (!ok) {
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
        if (job) CloseHandle(job);
        if (p->callback) p->callback(L"error", 0, 0, 0, 0, 0, L"", L"Could not start the download process.", p->user_data);
        free(p);
        return 0;
    }

    if (job && !AssignProcessToJobObject(job, pi.hProcess)) {
        CloseHandle(job);
        job = NULL;
    }
    ResumeThread(pi.hThread);

    g_current_process = pi.hProcess;
    g_current_job = job;
    int cancel_now = g_cancel_requested;
    LeaveCriticalSection(&g_engine_cs);

    if (cancel_now) {
        if (g_current_job) TerminateJobObject(g_current_job, 1);
        else TerminateProcess(pi.hProcess, 0);
    }

    /* Close write end immediately so ReadFile sees EOF when process finishes */
    CloseHandle(hWritePipe);

    /* Read output line by line */
    char line_buf[1024];
    size_t line_len = 0;
    char ch;
    DWORD bytes_read = 0;
    char last_error[768] = "";
    char last_destination[MAX_PATH * 2] = "";
    /* DOWNLOADED source files coming only from [download] lines.
       These are the candidates to delete after postprocessing failures; last_destination
       can be overwritten by the PP output (e.g. X.mp3), so it is not used for cleanup. */
    char download_destinations[MAX_TRACKED_DESTS][DEST_BUF_LEN];
    int download_dest_count = 0;
    memset(download_destinations, 0, sizeof(download_destinations));

    double last_percent = 0.0;
    double last_speed = 0.0;
    int last_eta = 0;
    long long last_downloaded = 0;
    long long last_total = 0;

    while (ReadFile(hReadPipe, &ch, 1, &bytes_read, NULL) && bytes_read > 0) {
        if (ch == '\r' || ch == '\n') {
            if (line_len > 0) {
                line_buf[line_len] = '\0';
                const char *trimmed = line_buf;
                while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

                if (strncmp(trimmed, "ERROR:", 6) == 0) {
                    strncpy(last_error, trimmed, sizeof(last_error) - 1);
                    last_error[sizeof(last_error) - 1] = '\0';
                } else if (strncmp(trimmed, "[download] Destination: ", 24) == 0) {
                    strncpy(last_destination, trimmed + 24, sizeof(last_destination) - 1);
                    last_destination[sizeof(last_destination) - 1] = '\0';
                    add_destination(download_destinations, &download_dest_count,
                                    trimmed + 24, (size_t)-1);
                } else if (strstr(trimmed, "Merging formats into \"") != NULL) {
                    const char *p_merge = strstr(trimmed, "Merging formats into \"") + 22;
                    strncpy(last_destination, p_merge, sizeof(last_destination) - 1);
                    last_destination[sizeof(last_destination) - 1] = '\0';
                    char *q = strrchr(last_destination, '"');
                    if (q) *q = '\0';
                } else if (strncmp(trimmed, "[ExtractAudio] Destination: ", 28) == 0) {
                    strncpy(last_destination, trimmed + 28, sizeof(last_destination) - 1);
                    last_destination[sizeof(last_destination) - 1] = '\0';
                } else if (strstr(trimmed, "[VideoConvertor]") != NULL &&
                           strstr(trimmed, " Destination: ") != NULL) {
                    /* This is the real file after --recode-video; the previous one is deleted. */
                    const char *p_dest = strstr(trimmed, " Destination: ") + 14;
                    strncpy(last_destination, p_dest, sizeof(last_destination) - 1);
                    last_destination[sizeof(last_destination) - 1] = '\0';
                } else if (strstr(trimmed, "has already been downloaded") != NULL &&
                           strncmp(trimmed, "[download] ", 11) == 0) {
                    const char *p_start = trimmed + 11;
                    const char *p_end = strstr(p_start, "has already been downloaded");
                    int plen = (int)(p_end - p_start);
                    while (plen > 0 && p_start[plen - 1] == ' ') plen--;
                    if (plen > 0 && plen < (int)sizeof(last_destination)) {
                        strncpy(last_destination, p_start, plen);
                        last_destination[plen] = '\0';
                        add_destination(download_destinations, &download_dest_count,
                                        p_start, (size_t)plen);
                    }
                } else if (strchr(line_buf, '|')) {
                    parse_progress_line(line_buf, &last_percent, &last_speed, &last_eta, &last_downloaded, &last_total);
                    if (p->callback) {
                        p->callback(L"downloading", last_percent, last_speed, last_eta, last_downloaded, last_total, L"", NULL, p->user_data);
                    }
                } else if (strstr(line_buf, "[Merger]") ||
                           strstr(line_buf, "[ExtractAudio]") ||
                           strstr(line_buf, "[VideoConvertor]") ||
                           strstr(line_buf, "[Fixup")) {
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
    HANDLE hJob = g_current_job;
    g_current_process = NULL;
    g_current_job = NULL;
    g_engine_state = ENGINE_STATE_IDLE;
    int was_cancelled = g_cancel_requested;
    LeaveCriticalSection(&g_engine_cs);

    if (hJob) CloseHandle(hJob);
    if (hProc) CloseHandle(hProc);
    if (pi.hThread) CloseHandle(pi.hThread);

    /* If postprocessing fails, yt-dlp leaves the intermediate files it downloaded
       (e.g. the video/audio streams when merging fails). Delete only the SOURCE
       files that came from [download] lines, after verifying from their timestamp
       that they were created in this run. The final/PP output is never deleted. */
    if (!was_cancelled && exit_code != 0) {
        for (int i = 0; i < download_dest_count; i++) {
            wchar_t leftover[MAX_PATH];
            decode_path_bytes(download_destinations[i], leftover, MAX_PATH);
            if (path_is_fresh_file(leftover, &dl_start_time)) {
                DeleteFileW(leftover);
            }
        }
    }

    /* Final path: first yt-dlp's UTF-8 report, otherwise guess from stdout. */
    wchar_t wide_dest[MAX_PATH] = L"";
    if (!was_cancelled && exit_code == 0) {
        if (!read_last_utf8_line(final_path_file, wide_dest, MAX_PATH)) {
            decode_path_bytes(last_destination, wide_dest, MAX_PATH);
        }
        if (wide_dest[0] != L'\0' && GetFileAttributesW(wide_dest) == INVALID_FILE_ATTRIBUTES) {
            /* If the reported path is missing on disk (deleted/renamed),
               do not report the wrong information. */
            wide_dest[0] = L'\0';
        }
    }
    if (final_path_file[0] != L'\0') DeleteFileW(final_path_file);

    if (p->callback) {
        if (was_cancelled) {
            p->callback(L"cancelled", 0, 0, 0, 0, 0, L"", L"Download cancelled by the user.", p->user_data);
        } else if (exit_code == 0) {
            p->callback(L"finished", 100.0, 0, 0, last_total, last_total, wide_dest, NULL, p->user_data);
        } else {
            wchar_t detail[512];
            wchar_t message[768] = L"An error occurred during the download.";
            extract_last_error(last_error, detail, 512);
            if (detail[0]) {
                if (wcsstr(detail, L"Requested format is not available")) {
                    wcscpy(message, L"The selected format is not available for this link. Try another quality.");
                } else if (wcsstr(detail, L"ffmpeg") || wcsstr(detail, L"ffprobe")) {
                    wcscpy(message, L"ffmpeg was not found or could not run. Merging/converting is unavailable.");
                } else if (wcsstr(detail, L"HTTP Error 403")) {
                    wcscpy(message, L"The server refused the download (HTTP 403). Open the link in a browser and try again.");
                } else if (wcsstr(detail, L"Unable to download webpage")) {
                    wcscpy(message, L"Could not reach the link. Check your internet connection.");
                } else {
                    _snwprintf(message, 768, L"yt-dlp: %ls", detail);
                }
            }
            p->callback(L"error", 0, 0, 0, 0, 0, L"", message, p->user_data);
        }
    }

    free(p);
    return 0;
}

static DWORD WINAPI download_thread_proc(LPVOID param)
{
    DWORD rc = download_thread_main(param);
    worker_finished();
    return rc;
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
    g_cancel_requested = 0;
    LeaveCriticalSection(&g_engine_cs);

    DownloadThreadParams *p = (DownloadThreadParams*)malloc(sizeof(DownloadThreadParams));
    if (!p) {
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);
        return 0;
    }
    wcsncpy(p->url, url, 1023);
    p->url[1023] = L'\0';
    memcpy(&p->selection, selection, sizeof(SelectionResult));
    wcsncpy(p->dest_dir, dest_dir, MAX_PATH - 1);
    p->dest_dir[MAX_PATH - 1] = L'\0';
    p->callback = callback;
    p->user_data = user_data;

    worker_started();
    HANDLE hThread = CreateThread(NULL, 0, download_thread_proc, p, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        return 1;
    }
    worker_finished();

    EnterCriticalSection(&g_engine_cs);
    g_engine_state = ENGINE_STATE_IDLE;
    LeaveCriticalSection(&g_engine_cs);
    free(p);
    return 0;
}
