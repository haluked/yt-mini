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

    /* Çalışan thread'ler kritik bölümü kullanmayı bırakmadan silmek
       use-after-free'e yol açıyordu. Önce bitmelerini bekle. */
    if (g_workers_done) {
        WaitForSingleObject(g_workers_done, 5000);
    }
    if (InterlockedCompareExchange(&g_active_workers, 0, 0) == 0) {
        DeleteCriticalSection(&g_engine_cs);
    }
    if (g_workers_done) {
        CloseHandle(g_workers_done);
        g_workers_done = NULL;
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
        /* Job object yt-dlp ile birlikte ffmpeg alt sürecini de sonlandırır. */
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

/* Komut satırına tırnak içinde giren değerlerde tırnak/ters bölü/kontrol
   karakteri olmamalı; aksi halde argüman sınırı aşılabilir. */
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

/* Bir pipe'ı EOF'a kadar okuyup son `cap - 1` baytı buf'a yazar.
   Tampon dolduktan sonra da okumaya devam eder: çocuk süreç stderr'e yazarken
   bloke olmasın (aksi halde iki pipe'lı kurulumda kilitlenme olur). */
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

typedef struct {
    HANDLE pipe;
    char *buf;
    size_t cap;
    size_t len;
} StderrDrainParams;

/* Aynı anda tek fetch çalıştığı için (engine durumu) bu tamponlar statiktir;
   thread join zaman aşımına uğrasa bile ölü stack'e yazma riski olmaz. */
static char g_drain_buf[4096];
static StderrDrainParams g_drain_params;

/* yt-dlp'nin --print-to-file çıktısı UTF-8'dir; dil/kod sayfasından bağımsız
   olduğu için nihai dosya yolunu buradan okuyoruz. Dosyanın son boş olmayan
   satırını döndürür. */
static int read_last_utf8_line(const wchar_t *path, wchar_t *out, size_t max_len)
{
    if (!out || max_len == 0) return 0;
    out[0] = L'\0';
    if (!path || !*path) return 0;

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;

    char buf[8192];
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

/* stdout satırları UTF-8 olmayabilir (yt-dlp cp1254 yazabiliyor). Önce UTF-8
   dene, olmazsa sistem ANSI kod sayfasına düş. */
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

static DWORD WINAPI stderr_drain_thread(LPVOID param)
{
    StderrDrainParams *d = (StderrDrainParams*)param;
    drain_pipe_blocking(d->pipe, d->buf, d->cap, &d->len);
    return 0;
}

/* yt-dlp bir Python programıdır. PYTHONUTF8=1 Python'un kendi dosya/G/Ç
   katmanını UTF-8'e zorlar; ancak yt-dlp.exe'nin KONSOL çıktısı yine de yerel
   kod sayfasında (Türkçe Windows'ta cp1254) gelebiliyor. Bu yüzden:
     - JSON çıktısı ASCII kaçışlıdır (\u00f6) -> her zaman güvenli,
     - nihai dosya yolu --print-to-file ile UTF-8 dosyadan okunur. */
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
    env[total + extra_len] = L'\0';                   /* blok sonlandıran ikinci NUL */

    FreeEnvironmentStringsW((LPWSTR)src);
    return env;
}

/* Paketlenen ffmpeg her zaman MP3 kodlayıcısı içermiyor; UI'da boş yere
   "MP3" seçeneği göstermemek ve indirmeyi yarıda bırakmamak için bir kez
   kontrol edip önbelleğe alıyoruz. */
int engine_ffmpeg_can_encode_mp3(void)
{
    static int cached = -1;
    if (cached >= 0) return cached;

    wchar_t ffmpeg_path[MAX_PATH];
    cached = 0;
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

    /* Çıktı büyük olabilir; tamponu doldursak bile okumaya devam et. */
    char out[32768];
    size_t len = 0;
    DWORD got = 0;
    char chunk[4096];
    while (ReadFile(hReadPipe, chunk, sizeof(chunk), &got, NULL) && got > 0) {
        if (len + got < sizeof(out)) {
            memcpy(out + len, chunk, got);
            len += got;
        }
    }
    out[len] = '\0';
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (strstr(out, "libmp3lame") || strstr(out, "libshine") || strstr(out, "mp3_mf"))
        cached = 1;
    return cached;
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
        meta->title[255] = L'\0';
    }

    cJSON *dur_item = cJSON_GetObjectItem(root, "duration");
    if (cJSON_IsNumber(dur_item)) {
        meta->duration = dur_item->valueint;
    }
    format_duration_str(meta->duration, meta->duration_str, sizeof(meta->duration_str)/sizeof(wchar_t));

    cJSON *thumb_item = cJSON_GetObjectItem(root, "thumbnail");
    if (cJSON_IsString(thumb_item) && thumb_item->valuestring) {
        MultiByteToWideChar(CP_UTF8, 0, thumb_item->valuestring, -1, meta->thumbnail_url, 511);
        meta->thumbnail_url[511] = L'\0';
    }

    cJSON *formats_array = cJSON_GetObjectItem(root, "formats");
    if (cJSON_IsArray(formats_array)) {
        int arr_size = cJSON_GetArraySize(formats_array);
        /* yt-dlp formatları kötüden iyiye sıralar. Sınır dolarsa baştan değil
           sondan alarak EN İYİ formatların listede kalmasını garanti ederiz. */
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

    /* Eğer 'formats' dizisi yoksa veya boşsa, root düzeyindeki tekil format bilgilerini kontrol et */
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
            p->callback(0, NULL, L"yt-dlp.exe bulunamadı! Lütfen bin/ klasörüne yerleştirin.", p->user_data);
        }
        free(p);
        return 0;
    }

    if (!safe_command_arg(p->url)) {
        EnterCriticalSection(&g_engine_cs);
        g_engine_state = ENGINE_STATE_IDLE;
        LeaveCriticalSection(&g_engine_cs);

        if (p->callback) p->callback(0, NULL, L"Geçersiz bağlantı adresi.", p->user_data);
        free(p);
        return 0;
    }

    wchar_t cmd_line[2048];
    _snwprintf(cmd_line, 2048, L"\"%ls\" --dump-json --no-playlist --no-warnings \"%ls\"", ytdlp_path, p->url);

    /* stdout: yalnızca JSON; stderr: uyarı/hata satırları.
       Aynı pipe'a karışsalar JSON'dan önce gelen tek bir satır bile ayrıştırmayı bozuyordu. */
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

        if (p->callback) p->callback(0, NULL, L"Boru hattı oluşturulamadı.", p->user_data);
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
        if (p->callback) p->callback(0, NULL, L"yt-dlp süreci başlatılamadı.", p->user_data);
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
        if (p->callback) p->callback(0, NULL, L"Bellek yetersiz.", p->user_data);
        free(p);
        return 0;
    }

    DWORD bytes_read = 0;
    char chunk[8192];
    int truncated = 0;

    /* stderr ayrı bir thread'de sürekli boşaltılır. Aksi halde çocuk 64 KB'tan
       fazla hata/uyarı yazarsa stderr pipe'ı dolar, çocuk bloke olur ve biz
       stdout'ta EOF beklerken uygulama kilitlenirdi. */
    char err_buf[4096];
    err_buf[0] = '\0';
    g_drain_buf[0] = '\0';
    g_drain_params.pipe = hErrReadPipe;
    g_drain_params.buf = g_drain_buf;
    g_drain_params.cap = sizeof(g_drain_buf);
    g_drain_params.len = 0;
    HANDLE hDrainThread = CreateThread(NULL, 0, stderr_drain_thread, &g_drain_params, 0, NULL);

    while (ReadFile(hReadPipe, chunk, sizeof(chunk), &bytes_read, NULL) && bytes_read > 0) {
        if (buf_len + bytes_read + 1 >= buf_capacity) {
            buf_capacity = (buf_capacity * 2) + bytes_read + 1;
            char *new_buf = (char*)realloc(json_buf, buf_capacity);
            if (!new_buf) {
                /* Boru tıkanmasın diye okumaya devam et, ama veriyi at. */
                truncated = 1;
                continue;
            }
            json_buf = new_buf;
        }
        memcpy(json_buf + buf_len, chunk, bytes_read);
        buf_len += bytes_read;
    }
    json_buf[buf_len] = '\0';

    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);

    /* Süreç bitti: stderr yazan thread EOF alıp döner. Thread açılamadıysa
       (çok nadir) aynı işi burada yap. */
    if (hDrainThread) {
        WaitForSingleObject(hDrainThread, 10000);
        CloseHandle(hDrainThread);
        memcpy(err_buf, g_drain_buf, sizeof(err_buf));
        err_buf[sizeof(err_buf) - 1] = '\0';
    } else {
        drain_pipe_blocking(hErrReadPipe, err_buf, sizeof(err_buf), NULL);
    }
    CloseHandle(hErrReadPipe);

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
        if (p->callback) p->callback(0, NULL, L"yt-dlp yanıtı çok büyük; medya bilgisi okunamadı.", p->user_data);
    } else if (exit_code != 0 || buf_len == 0) {
        wchar_t err_msg[512] = L"Video bilgisi alınamadı. Bağlantıyı kontrol edin.";
        if (strstr(err_buf, "Private video")) wcscpy(err_msg, L"Bu video gizli veya erişilemez.");
        else if (strstr(err_buf, "Sign in to confirm")) wcscpy(err_msg, L"Bu video oturum açma veya yaş onayı gerektiriyor.");
        else if (strstr(err_buf, "Video unavailable")) wcscpy(err_msg, L"Bu video kullanılamıyor veya silinmiş.");
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
            p->callback(0, NULL, L"Bellek yetersiz.", p->user_data);
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

    /* Toplam boyut bilinmiyorsa yüzde göstermenin anlamı yok. yt-dlp çoklu
       akışlarda "%100 / 0 bayt" satırı gönderebiliyor; bu, göstergenin iş
       bitmeden dolmasına yol açıyordu. */
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
        download_fail(p, L"yt-dlp.exe bulunamadı! Lütfen bin/ klasörüne yerleştirin.");
        return 0;
    }

    if (!safe_command_arg(p->url) ||
        !safe_command_arg(p->selection.format_id) ||
        !safe_command_arg(p->selection.expected_container)) {
        download_fail(p, L"Geçersiz indirme parametresi.");
        return 0;
    }

    int audio_mode = (wcscmp(p->selection.resolution, L"Audio Only") == 0);
    int needs_ffmpeg = audio_mode || !p->selection.is_direct || p->selection.needs_reencode;
    if (needs_ffmpeg && !has_ffmpeg) {
        download_fail(p, L"ffmpeg.exe bulunamadı. Birleştirme veya dönüştürme için ffmpeg gerekli.");
        return 0;
    }

    /* Paketlenen ffmpeg'de MP3 kodlayıcısı olmayabilir. İndirmeyi başlatıp
       yarıda hata vermek yerine burada dur ve net söyle. */
    if (audio_mode && _wcsicmp(p->selection.expected_container, L"mp3") == 0 &&
        !engine_ffmpeg_can_encode_mp3()) {
        download_fail(p, L"Bu ffmpeg sürümünde MP3 kodlayıcısı yok. Lütfen AAC (m4a) veya Opus seçin.");
        return 0;
    }

    if (!ensure_directory(p->dest_dir)) {
        download_fail(p, L"İndirme klasörü oluşturulamadı. Lütfen başka bir klasör seçin.");
        return 0;
    }

    wchar_t ffmpeg_dir_arg[MAX_PATH + 32] = L"";
    if (has_ffmpeg) {
        wchar_t *last_slash = wcsrchr(ffmpeg_path, L'\\');
        if (last_slash) *last_slash = L'\0';
        _snwprintf(ffmpeg_dir_arg, sizeof(ffmpeg_dir_arg)/sizeof(wchar_t), L"--ffmpeg-location \"%ls\"", ffmpeg_path);
    }

    /* Uzun başlıklar MAX_PATH'i aşmasın ve aynı başlıklı videolar
       birbirinin üzerine yazılmasın. Klasör adındaki '%' yt-dlp şablon
       motorunda özel karakter olduğu için '%%' olarak kaçılır. */
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

    wchar_t out_template[MAX_PATH + 64];
    _snwprintf(out_template, sizeof(out_template)/sizeof(wchar_t),
        L"-o \"%ls\\%%(title).150B [%%(id)s].%%(ext)s\"", dir_escaped);

    /* Nihai dosya yolunu metinden tahmin etmek yerine yt-dlp'ye UTF-8 bir
       dosyaya yazdırıyoruz: stdout kod sayfası bozuk olabiliyor (ör. cp1254),
       bu yüzden Türkçe/emoji başlıklı dosyalarda "Dosya Konumunu Aç" yanlış
       yola gidiyordu. --print-to-file her zaman UTF-8 yazar. */
    wchar_t final_path_file[MAX_PATH] = L"";
    wchar_t final_path_arg[MAX_PATH * 2] = L"";
    {
        wchar_t tmp_dir[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tmp_dir) > 0) {
            _snwprintf(final_path_file, MAX_PATH, L"%lsffdlp_finalpath_%lu.txt",
                tmp_dir, (unsigned long)GetCurrentProcessId());
            final_path_file[MAX_PATH - 1] = L'\0';
            DeleteFileW(final_path_file);

            /* yt-dlp bunu output template olarak işler: '%' kaçışı şart. */
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
        /* Video akışı kopyalanır, yalnızca ses AAC'ye çevrilip MP4'e birleştirilir.
           (--recode-video kullansaydık kaynak video da yeniden kodlanacaktı.)
           yt-dlp sözdizimi "PP:ARGS" (PP+EXE:ARGS); '+' ayırıcısı burada YANLIŞ. */
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

    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        download_fail(p, L"Boru hattı oluşturulamadı.");
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
        if (p->callback) p->callback(L"error", 0, 0, 0, 0, 0, L"", L"İndirme süreci başlatılamadı.", p->user_data);
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
                    /* --recode-video sonrası gerçek dosya budur; öncesi silinir. */
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

    /* Postprocessing başarısız olduysa yt-dlp indirdiği ara dosyayı bırakır
       (ör. MP3 dönüşümü patladığında .webm). Bu çalıştırmada oluştuğunu
       zaman damgasından doğrulayıp temizle; kullanıcının eski dosyalarına
       dokunma. */
    if (!was_cancelled && exit_code != 0 && last_destination[0] != '\0') {
        wchar_t leftover[MAX_PATH];
        decode_path_bytes(last_destination, leftover, MAX_PATH);
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (leftover[0] && GetFileAttributesExW(leftover, GetFileExInfoStandard, &fad) &&
            CompareFileTime(&fad.ftLastWriteTime, &dl_start_time) >= 0) {
            DeleteFileW(leftover);
        }
    }

    /* Nihai yol: önce yt-dlp'nin UTF-8 raporu, olmazsa stdout'tan tahmin. */
    wchar_t wide_dest[MAX_PATH] = L"";
    if (!was_cancelled && exit_code == 0) {
        if (!read_last_utf8_line(final_path_file, wide_dest, MAX_PATH)) {
            decode_path_bytes(last_destination, wide_dest, MAX_PATH);
        }
        if (wide_dest[0] != L'\0' && GetFileAttributesW(wide_dest) == INVALID_FILE_ATTRIBUTES) {
            /* Rapor edilen yol diskte yoksa (silinmiş/yeniden adlandırılmış)
               yanlış bilgi vermeyelim. */
            wide_dest[0] = L'\0';
        }
    }
    if (final_path_file[0] != L'\0') DeleteFileW(final_path_file);

    if (p->callback) {
        if (was_cancelled) {
            p->callback(L"cancelled", 0, 0, 0, 0, 0, L"", L"İndirme kullanıcı tarafından iptal edildi.", p->user_data);
        } else if (exit_code == 0) {
            p->callback(L"finished", 100.0, 0, 0, last_total, last_total, wide_dest, NULL, p->user_data);
        } else {
            wchar_t detail[512];
            wchar_t message[768] = L"İndirme sırasında bir hata oluştu.";
            extract_last_error(last_error, detail, 512);
            if (detail[0]) {
                if (wcsstr(detail, L"Requested format is not available")) {
                    wcscpy(message, L"Seçilen format bu bağlantıda mevcut değil. Farklı bir kalite deneyin.");
                } else if (wcsstr(detail, L"ffmpeg") || wcsstr(detail, L"ffprobe")) {
                    wcscpy(message, L"ffmpeg bulunamadı veya çalıştırılamadı. Birleştirme/dönüştürme yapılamıyor.");
                } else if (wcsstr(detail, L"HTTP Error 403")) {
                    wcscpy(message, L"Sunucu indirmeye izin vermedi (HTTP 403). Bağlantıyı tarayıcıda açıp tekrar deneyin.");
                } else if (wcsstr(detail, L"Unable to download webpage")) {
                    wcscpy(message, L"Bağlantıya erişilemedi. İnternet bağlantınızı kontrol edin.");
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
