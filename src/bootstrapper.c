#define INITGUID
#include "bootstrapper.h"
#include "engine.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <ole2.h>
#include <urlmon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YTDLP_LATEST_URL L"https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe"
#define FFMPEG_URL       L"https://github.com/haluked/yt-mini/releases/download/v2.0.0-beta/ffmpeg.exe"

#define IDC_BS_TITLE       101
#define IDC_BS_DESC        102
#define IDC_BS_PROGRESS    103
#define IDC_BS_STATUS      104
#define IDC_BS_CANCEL      105

#define WM_USER_BOOTSTRAP_DONE (WM_USER + 301)

typedef struct {
    HWND hwnd_dlg;
    HWND hwnd_title;
    HWND hwnd_desc;
    HWND hwnd_progress;
    HWND hwnd_status;
    HWND hwnd_cancel;
    HFONT font_bold;
    HFONT font_normal;
    HFONT font_small;
    int cancel_requested;
    int result;
    int is_update_only;
    const wchar_t *current_item_name;
    DWORD last_update_tick;
    int last_percent;
    wchar_t fail_msg[256];   /* Which step failed and why (shown to the user) */
} BootstrapState;

/* IBindStatusCallback implementasyonu */
typedef struct {
    IBindStatusCallback callback;
    LONG ref_count;
    BootstrapState *state;
} DownloadCallback;

static HRESULT STDMETHODCALLTYPE cb_QueryInterface(IBindStatusCallback *This, REFIID riid, void **ppvObject)
{
    if (!ppvObject) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IBindStatusCallback)) {
        *ppvObject = This;
        This->lpVtbl->AddRef(This);
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE cb_AddRef(IBindStatusCallback *This)
{
    DownloadCallback *self = (DownloadCallback*)This;
    return InterlockedIncrement(&self->ref_count);
}

static ULONG STDMETHODCALLTYPE cb_Release(IBindStatusCallback *This)
{
    DownloadCallback *self = (DownloadCallback*)This;
    LONG r = InterlockedDecrement(&self->ref_count);
    return r;
}

static HRESULT STDMETHODCALLTYPE cb_OnStartBinding(IBindStatusCallback *This, DWORD dwReserved, IBinding *pib)
{
    (void)This; (void)dwReserved; (void)pib;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE cb_GetPriority(IBindStatusCallback *This, LONG *pnPriority)
{
    (void)This; (void)pnPriority;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE cb_OnLowResource(IBindStatusCallback *This, DWORD reserved)
{
    (void)This; (void)reserved;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE cb_OnProgress(
    IBindStatusCallback *This,
    ULONG ulProgress,
    ULONG ulProgressMax,
    ULONG ulStatusCode,
    LPCWSTR szStatusText
)
{
    DownloadCallback *self = (DownloadCallback*)This;
    (void)ulStatusCode;
    (void)szStatusText;

    if (!self->state || self->state->cancel_requested) {
        return E_ABORT;
    }

    DWORD now = GetTickCount();
    int pct = (ulProgressMax > 0) ? (int)((double)ulProgress * 100.0 / (double)ulProgressMax) : 0;
    if (pct > 100) pct = 100;

    /* Avoid over-updating the UI: redraw when the percentage changes or at least every 100 ms */
    if (pct != self->state->last_percent || (now - self->state->last_update_tick) > 100) {
        self->state->last_percent = pct;
        self->state->last_update_tick = now;

        if (self->state->hwnd_progress) {
            SendMessageW(self->state->hwnd_progress, PBM_SETPOS, (WPARAM)pct, 0);
        }

        if (self->state->hwnd_status) {
            wchar_t line[256];
            double cur_mb = (double)ulProgress / (1024.0 * 1024.0);
            if (ulProgressMax > 0) {
                double tot_mb = (double)ulProgressMax / (1024.0 * 1024.0);
                _snwprintf(line, 256, L"Downloading: %ls (%.1f MB / %.1f MB) - %%%d",
                    self->state->current_item_name ? self->state->current_item_name : L"",
                    cur_mb, tot_mb, pct);
            } else {
                _snwprintf(line, 256, L"Downloading: %ls (%.1f MB)...",
                    self->state->current_item_name ? self->state->current_item_name : L"",
                    cur_mb);
            }
            line[255] = L'\0';
            SetWindowTextW(self->state->hwnd_status, line);
        }
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE cb_OnStopBinding(IBindStatusCallback *This, HRESULT hresult, LPCWSTR szError)
{
    (void)This; (void)hresult; (void)szError;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE cb_GetBindInfo(IBindStatusCallback *This, DWORD *grfBINDF, BINDINFO *pbindinfo)
{
    (void)This;
    if (!grfBINDF || !pbindinfo) return E_POINTER;
    *grfBINDF = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_PULLDATA | BINDF_GETNEWESTVERSION | BINDF_NOWRITECACHE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE cb_OnDataAvailable(IBindStatusCallback *This, DWORD grfBSCF, DWORD dwSize, FORMATETC *pformatetc, STGMEDIUM *pstgmed)
{
    (void)This; (void)grfBSCF; (void)dwSize; (void)pformatetc; (void)pstgmed;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE cb_OnObjectAvailable(IBindStatusCallback *This, REFIID riid, IUnknown *punk)
{
    (void)This; (void)riid; (void)punk;
    return S_OK;
}

static IBindStatusCallbackVtbl g_cb_vtbl = {
    cb_QueryInterface,
    cb_AddRef,
    cb_Release,
    cb_OnStartBinding,
    cb_GetPriority,
    cb_OnLowResource,
    cb_OnProgress,
    cb_OnStopBinding,
    cb_GetBindInfo,
    cb_OnDataAvailable,
    cb_OnObjectAvailable
};

static void init_cb(DownloadCallback *cb, BootstrapState *state)
{
    cb->callback.lpVtbl = &g_cb_vtbl;
    cb->ref_count = 1;
    cb->state = state;
}

static int get_localappdata_bin_dir(wchar_t *out_bin, size_t max_len)
{
    wchar_t local_app_data[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_app_data) != S_OK) {
        return 0;
    }
    _snwprintf(out_bin, max_len, L"%ls\\ff-dlp-viewer\\bin", local_app_data);
    out_bin[max_len - 1] = L'\0';
    SHCreateDirectoryExW(NULL, out_bin, NULL);
    return 1;
}

static DWORD WINAPI bootstrap_worker_thread(LPVOID lpParam)
{
    BootstrapState *state = (BootstrapState*)lpParam;
    if (!state) return 0;

    wchar_t bin_dir[MAX_PATH];

    if (state->is_update_only) {
        /* The update must target the copy the APP ACTUALLY USES.
           find_binary order: app dir -> app\bin -> ..\bin -> LOCALAPPDATA -> PATH.
           The old code always wrote to LOCALAPPDATA; in a portable install the copy
           in app\bin takes priority, so the update silently had no effect. */
        wchar_t used[MAX_PATH];
        if (engine_find_ytdlp(used, MAX_PATH)) {
            wchar_t *slash = wcsrchr(used, L'\\');
            if (slash) {
                *slash = L'\0';
                wcsncpy(bin_dir, used, MAX_PATH - 1);
                bin_dir[MAX_PATH - 1] = L'\0';
            } else if (!get_localappdata_bin_dir(bin_dir, MAX_PATH)) {
                PostMessageW(state->hwnd_dlg, WM_USER_BOOTSTRAP_DONE, 0, 0);
                return 0;
            }
        } else if (!get_localappdata_bin_dir(bin_dir, MAX_PATH)) {
            PostMessageW(state->hwnd_dlg, WM_USER_BOOTSTRAP_DONE, 0, 0);
            return 0;
        }
    } else if (!get_localappdata_bin_dir(bin_dir, MAX_PATH)) {
        PostMessageW(state->hwnd_dlg, WM_USER_BOOTSTRAP_DONE, 0, 0);
        return 0;
    }

    wchar_t ytdlp_target[MAX_PATH];
    wchar_t ffmpeg_target[MAX_PATH];
    _snwprintf(ytdlp_target, MAX_PATH, L"%ls\\yt-dlp.exe", bin_dir);
    _snwprintf(ffmpeg_target, MAX_PATH, L"%ls\\ffmpeg.exe", bin_dir);

    /* If not in update mode, determine which tools are missing */
    int need_ytdlp = 0;
    int need_ffmpeg = 0;

    if (state->is_update_only) {
        need_ytdlp = 1;
    } else {
        wchar_t dummy[MAX_PATH];
        if (!engine_find_ytdlp(dummy, MAX_PATH)) need_ytdlp = 1;
        if (!engine_find_ffmpeg(dummy, MAX_PATH)) need_ffmpeg = 1;
    }

    DownloadCallback cb;
    init_cb(&cb, state);

    /* 1. yt-dlp.exe indirme */
    if (need_ytdlp && !state->cancel_requested) {
        state->current_item_name = L"yt-dlp.exe";
        state->last_percent = -1;
        state->last_update_tick = 0;
        SendMessageW(state->hwnd_progress, PBM_SETPOS, 0, 0);
        SetWindowTextW(state->hwnd_status, L"Connecting to the yt-dlp server...");

        wchar_t tmp_path[MAX_PATH];
        _snwprintf(tmp_path, MAX_PATH, L"%ls\\yt-dlp.exe.tmp", bin_dir);
        DeleteFileW(tmp_path);

        HRESULT hr = URLDownloadToFileW(NULL, YTDLP_LATEST_URL, tmp_path, 0, &cb.callback);
        if (hr == S_OK && !state->cancel_requested) {
            /* The move may fail (file in use, AV lock, etc.);
               not checking the result and claiming "updated" was misleading. */
            if (!MoveFileExW(tmp_path, ytdlp_target, MOVEFILE_REPLACE_EXISTING)) {
                DeleteFileW(tmp_path);
                _snwprintf(state->fail_msg, 255,
                    L"Could not copy yt-dlp (the file may be in use).\nTarget: %ls", ytdlp_target);
                state->fail_msg[255] = L'\0';
                PostMessageW(state->hwnd_dlg, WM_USER_BOOTSTRAP_DONE, 0, 0);
                return 0;
            }
        } else {
            DeleteFileW(tmp_path);
            if (!state->cancel_requested) {
                wcsncpy(state->fail_msg, L"Could not download yt-dlp! Check your internet connection.", 255);
                state->fail_msg[255] = L'\0';
                SetWindowTextW(state->hwnd_status, L"Could not download yt-dlp!");
            }
            PostMessageW(state->hwnd_dlg, WM_USER_BOOTSTRAP_DONE, 0, 0);
            return 0;
        }
    }

    /* 2. Downloading ffmpeg.exe (first run only, and only if missing) */
    if (need_ffmpeg && !state->cancel_requested) {
        state->current_item_name = L"ffmpeg.exe";
        state->last_percent = -1;
        state->last_update_tick = 0;
        SendMessageW(state->hwnd_progress, PBM_SETPOS, 0, 0);
        SetWindowTextW(state->hwnd_status, L"Connecting to the ffmpeg server...");

        wchar_t tmp_path[MAX_PATH];
        _snwprintf(tmp_path, MAX_PATH, L"%ls\\ffmpeg.exe.tmp", bin_dir);
        DeleteFileW(tmp_path);

        HRESULT hr = URLDownloadToFileW(NULL, FFMPEG_URL, tmp_path, 0, &cb.callback);
        if (hr == S_OK && !state->cancel_requested) {
            if (!MoveFileExW(tmp_path, ffmpeg_target, MOVEFILE_REPLACE_EXISTING)) {
                DeleteFileW(tmp_path);
                _snwprintf(state->fail_msg, 255,
                    L"Could not copy ffmpeg (the file may be in use).\nTarget: %ls", ffmpeg_target);
                state->fail_msg[255] = L'\0';
                PostMessageW(state->hwnd_dlg, WM_USER_BOOTSTRAP_DONE, 0, 0);
                return 0;
            }
        } else {
            DeleteFileW(tmp_path);
            if (!state->cancel_requested) {
                wcsncpy(state->fail_msg, L"Could not download ffmpeg! Check your internet connection.", 255);
                state->fail_msg[255] = L'\0';
                SetWindowTextW(state->hwnd_status, L"Could not download ffmpeg!");
            }
            PostMessageW(state->hwnd_dlg, WM_USER_BOOTSTRAP_DONE, 0, 0);
            return 0;
        }
    }

    if (state->cancel_requested) {
        PostMessageW(state->hwnd_dlg, WM_USER_BOOTSTRAP_DONE, 0, 0);
    } else {
        SetWindowTextW(state->hwnd_status, L"Setup complete!");
        SendMessageW(state->hwnd_progress, PBM_SETPOS, 100, 0);
        Sleep(350);
        PostMessageW(state->hwnd_dlg, WM_USER_BOOTSTRAP_DONE, 1, 0);
    }
    return 0;
}

static LRESULT CALLBACK BootstrapDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BootstrapState *state = (BootstrapState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW *cs = (CREATESTRUCTW*)lParam;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }

        case WM_CREATE: {
            state = (BootstrapState*)((CREATESTRUCTW*)lParam)->lpCreateParams;
            if (!state) return 0;
            state->hwnd_dlg = hWnd;

            /* Fonts */
            state->font_bold = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            state->font_normal = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            state->font_small = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            HINSTANCE hInst = ((CREATESTRUCTW*)lParam)->hInstance;

            const wchar_t *title_text = state->is_update_only ? L"yt-dlp Update" : L"First-Run Setup";
            const wchar_t *desc_text = state->is_update_only
                ? L"Downloading and updating the latest yt-dlp release..."
                : L"Downloading the required tools into %LOCALAPPDATA%\\ff-dlp-viewer\\bin...";

            state->hwnd_title = CreateWindowExW(0, L"STATIC", title_text,
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                24, 16, 420, 24, hWnd, (HMENU)IDC_BS_TITLE, hInst, NULL);
            SendMessageW(state->hwnd_title, WM_SETFONT, (WPARAM)state->font_bold, TRUE);

            state->hwnd_desc = CreateWindowExW(0, L"STATIC", desc_text,
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                24, 44, 420, 36, hWnd, (HMENU)IDC_BS_DESC, hInst, NULL);
            SendMessageW(state->hwnd_desc, WM_SETFONT, (WPARAM)state->font_normal, TRUE);

            state->hwnd_progress = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                24, 88, 420, 20, hWnd, (HMENU)IDC_BS_PROGRESS, hInst, NULL);

            state->hwnd_status = CreateWindowExW(0, L"STATIC", L"Starting...",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                24, 114, 320, 20, hWnd, (HMENU)IDC_BS_STATUS, hInst, NULL);
            SendMessageW(state->hwnd_status, WM_SETFONT, (WPARAM)state->font_small, TRUE);

            state->hwnd_cancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                354, 112, 90, 28, hWnd, (HMENU)IDC_BS_CANCEL, hInst, NULL);
            SendMessageW(state->hwnd_cancel, WM_SETFONT, (WPARAM)state->font_normal, TRUE);

            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_BS_CANCEL || wmId == IDCANCEL) {
                if (state) {
                    state->cancel_requested = 1;
                    if (state->hwnd_status) {
                        SetWindowTextW(state->hwnd_status, L"Cancelling...");
                    }
                    if (state->hwnd_cancel) {
                        EnableWindow(state->hwnd_cancel, FALSE);
                    }
                }
            }
            return 0;
        }

        case WM_USER_BOOTSTRAP_DONE: {
            if (state) {
                state->result = (int)wParam;
            }
            DestroyWindow(hWnd);
            return 0;
        }

        case WM_CLOSE: {
            if (state && !state->cancel_requested) {
                state->cancel_requested = 1;
                if (state->hwnd_status) {
                    SetWindowTextW(state->hwnd_status, L"Cancelling...");
                }
                if (state->hwnd_cancel) {
                    EnableWindow(state->hwnd_cancel, FALSE);
                }
                return 0;
            }
            DestroyWindow(hWnd);
            return 0;
        }

        case WM_DESTROY: {
            if (state) {
                if (state->font_bold) { DeleteObject(state->font_bold); state->font_bold = NULL; }
                if (state->font_normal) { DeleteObject(state->font_normal); state->font_normal = NULL; }
                if (state->font_small) { DeleteObject(state->font_small); state->font_small = NULL; }
            }
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static int run_bootstrap_dialog(HWND hParent, int is_update_only, wchar_t *fail_out, size_t fail_len)
{
    /* Allocate on the heap: if WaitForSingleObject times out, the worker thread
       may still be using this struct; the stack would die at that moment (use-after-free).
       On timeout it is intentionally leaked (safe); when the thread finishes it is
       freed on the other path. */
    BootstrapState *state = (BootstrapState*)calloc(1, sizeof(BootstrapState));
    if (!state) return 0;
    state->is_update_only = is_update_only;
    state->result = 0;

    HINSTANCE hInst = GetModuleHandleW(NULL);
    const wchar_t *className = L"FfDlpBootstrapperWindow";

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = BootstrapDlgProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = className;
    RegisterClassExW(&wc);

    /* Window size and screen centering */
    int dlg_w = 484;
    int dlg_h = 190;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int pos_x = (screen_w - dlg_w) / 2;
    int pos_y = (screen_h - dlg_h) / 2;

    HWND hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        className,
        is_update_only ? L"ff-dlp-viewer - Update" : L"ff-dlp-viewer - First Setup",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        pos_x, pos_y, dlg_w, dlg_h,
        hParent, NULL, hInst, state
    );

    if (!hWnd) {
        free(state);
        return 0;
    }

    if (hParent) EnableWindow(hParent, FALSE);

    /* Start the background download thread */
    HANDLE hThread = CreateThread(NULL, 0, bootstrap_worker_thread, state, 0, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    int result = state->result;

    if (hThread) {
        DWORD wait_res = WaitForSingleObject(hThread, 5000);
        CloseHandle(hThread);
        if (wait_res == WAIT_OBJECT_0) {
            if (fail_out && fail_len > 0) {
                wcsncpy(fail_out, state->fail_msg, fail_len - 1);
                fail_out[fail_len - 1] = L'\0';
            }
            free(state);
        }
        /* WAIT_TIMEOUT: the worker may still be using state; do not touch it (leak is intentional). */
    } else {
        if (fail_out && fail_len > 0) {
            wcsncpy(fail_out, state->fail_msg, fail_len - 1);
            fail_out[fail_len - 1] = L'\0';
        }
        free(state);
    }

    if (hParent) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    return result;
}

int bootstrapper_ensure_tools(HWND hParent)
{
    wchar_t dummy[MAX_PATH];
    int has_ytdlp = engine_find_ytdlp(dummy, MAX_PATH);
    int has_ffmpeg = engine_find_ffmpeg(dummy, MAX_PATH);

    /* If already present, continue without showing any dialog */
    if (has_ytdlp && has_ffmpeg) {
        return 1;
    }

    wchar_t fail_msg[256] = L"";
    int res = run_bootstrap_dialog(hParent, 0, fail_msg, 256);
    if (!res) {
        wchar_t msg[512];
        if (fail_msg[0]) {
            _snwprintf(msg, 512, L"%ls\n\nRestart the app with an internet connection so it can continue.", fail_msg);
        } else {
            wcscpy(msg,
                L"Required tools (yt-dlp or ffmpeg) could not be downloaded or the operation was cancelled.\n"
                L"Restart the app with an internet connection so it can continue.");
        }
        msg[511] = L'\0';
        MessageBoxW(hParent, msg, L"Setup Could Not Complete", MB_OK | MB_ICONWARNING);
    }
    return res;
}

int bootstrapper_update_ytdlp(HWND hParent)
{
    wchar_t fail_msg[256] = L"";
    int res = run_bootstrap_dialog(hParent, 1, fail_msg, 256);
    if (res) {
        MessageBoxW(hParent,
            L"yt-dlp was updated to the latest version.",
            L"Update Successful",
            MB_OK | MB_ICONINFORMATION);
    } else {
        wchar_t msg[512];
        if (fail_msg[0]) {
            _snwprintf(msg, 512, L"%ls", fail_msg);
        } else {
            wcscpy(msg, L"An error occurred while updating yt-dlp, or the operation was cancelled.");
        }
        msg[511] = L'\0';
        MessageBoxW(hParent, msg, L"Update Failed", MB_OK | MB_ICONWARNING);
    }
    return res;
}
