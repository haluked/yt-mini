#include "ui.h"
#include "engine.h"
#include "detector.h"
#include "codec_selector.h"
#include "sanitization.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#define IDC_GRP_URL           1000
#define IDC_EDIT_URL          1001
#define IDC_BTN_PASTE         1002
#define IDC_BTN_FETCH         1003
#define IDC_GRP_MEDIA         1004
#define IDC_STATIC_TITLE      1005
#define IDC_STATIC_META       1006
#define IDC_RADIO_VIDEO       1007
#define IDC_RADIO_AUDIO       1008
#define IDC_COMBO_RES         1009
#define IDC_CHK_FORCE_MP4     1010
#define IDC_COMBO_AUDIO       1011
#define IDC_STATIC_REASON     1012
#define IDC_GRP_DOWNLOAD      1013
#define IDC_PROGRESS          1014
#define IDC_STATIC_STATUS     1015
#define IDC_BTN_DOWNLOAD      1016
#define IDC_BTN_CANCEL        1017
#define IDC_BTN_OPEN_LOCATION 1018
#define IDC_BTN_CHOOSE_FOLDER 1019
#define IDC_STATIC_FOLDER     1020

#define ID_TRAY_RESTORE       2001
#define ID_TRAY_EXIT          2002

#define BASE_CLIENT_W         646
#define BASE_CLIENT_H         485
#define BASE_MIN_CLIENT_W     620

#define APP_WINDOW_STYLE      (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX)
#define APP_WINDOW_EX_STYLE   (WS_EX_APPWINDOW | WS_EX_CONTROLPARENT)

static HWND g_hWnd = NULL;
static HWND g_hGrpUrl = NULL;
static HWND g_hGrpMedia = NULL;
static HWND g_hGrpDownload = NULL;
static HWND g_hEditUrl = NULL;
static HWND g_hBtnPaste = NULL;
static HWND g_hBtnFetch = NULL;
static HWND g_hStaticTitle = NULL;
static HWND g_hStaticMeta = NULL;
static HWND g_hRadioVideo = NULL;
static HWND g_hRadioAudio = NULL;
static HWND g_hComboRes = NULL;
static HWND g_hChkForceMp4 = NULL;
static HWND g_hComboAudio = NULL;
static HWND g_hStaticReason = NULL;
static HWND g_hProgressBar = NULL;
static HWND g_hStaticStatus = NULL;
static HWND g_hStaticFolder = NULL;
static HWND g_hBtnDownload = NULL;
static HWND g_hBtnCancel = NULL;
static HWND g_hBtnOpenLocation = NULL;
static HWND g_hBtnChooseFolder = NULL;

static HFONT g_fontTitle = NULL;
static HFONT g_fontBold = NULL;
static HFONT g_fontNormal = NULL;
static HFONT g_fontSmall = NULL;

static NOTIFYICONDATAW g_nid;
static wchar_t g_download_dir[MAX_PATH];
static wchar_t g_last_downloaded_file[MAX_PATH] = L"";
static MediaMetadata g_current_meta;
static int g_has_meta = 0;
static int g_is_fetching = 0;
static int g_is_downloading = 0;
static int g_auto_download_after_fetch = 0;
static int g_selection_valid = 0;
static wchar_t g_last_fetched_url[1024] = L"";
static wchar_t g_initial_url[1024] = L"";
static SelectionResult g_current_selection;

static UINT g_dpi = 96;
static int g_fixed_total_h = 0;
static int g_min_total_w = 0;
static UINT g_taskbar_created_msg = 0;
static int g_last_tray_percent = -1;

static WNDPROC g_oldEditProc = NULL;

static int file_exists(const wchar_t *path)
{
    DWORD attr = GetFileAttributesW(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

typedef struct {
    int success;
    MediaMetadata meta;
    wchar_t error_msg[256];
} FetchResultMsg;

typedef struct {
    wchar_t status[32];
    double percent;
    double speed;
    int eta;
    long long downloaded;
    long long total;
    wchar_t filename[MAX_PATH];
    wchar_t error_msg[256];
} DownloadTickMsg;

static void update_format_selection(void);
static void trigger_fetch(void);
static void trigger_download(void);
static void layout_controls(void);
static void create_fonts(void);
static void apply_control_fonts(void);
static void update_folder_label(void);
static void save_download_dir(void);

static int S(int value)
{
    return MulDiv(value, (int)g_dpi, 96);
}

static UINT query_window_dpi(HWND hWnd)
{
    typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
    static GetDpiForWindowFn fn = NULL;
    static int loaded = 0;

    if (!loaded) {
        fn = (GetDpiForWindowFn)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        loaded = 1;
    }
    if (fn) {
        UINT dpi = fn(hWnd);
        if (dpi > 0) return dpi;
    }

    HDC dc = GetDC(NULL);
    UINT dpi = dc ? (UINT)GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(NULL, dc);
    return dpi ? dpi : 96;
}

static UINT query_system_dpi(void)
{
    HDC dc = GetDC(NULL);
    UINT dpi = dc ? (UINT)GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(NULL, dc);
    return dpi ? dpi : 96;
}

static void adjust_window_rect_for_dpi(RECT *rc, DWORD style, BOOL menu, DWORD ex_style, UINT dpi)
{
    typedef BOOL (WINAPI *AdjustForDpiFn)(LPRECT, DWORD, BOOL, DWORD, UINT);
    static AdjustForDpiFn fn = NULL;
    static int loaded = 0;

    if (!loaded) {
        fn = (AdjustForDpiFn)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi");
        loaded = 1;
    }
    if (fn) {
        fn(rc, style, menu, ex_style, dpi);
    } else {
        AdjustWindowRectEx(rc, style, menu, ex_style);
    }
}

static void update_fixed_metrics(void)
{
    RECT rc = { 0, 0, S(BASE_CLIENT_W), S(BASE_CLIENT_H) };
    adjust_window_rect_for_dpi(&rc, APP_WINDOW_STYLE, FALSE, APP_WINDOW_EX_STYLE, g_dpi);
    g_fixed_total_h = rc.bottom - rc.top;

    RECT min_rc = { 0, 0, S(BASE_MIN_CLIENT_W), S(BASE_CLIENT_H) };
    adjust_window_rect_for_dpi(&min_rc, APP_WINDOW_STYLE, FALSE, APP_WINDOW_EX_STYLE, g_dpi);
    g_min_total_w = min_rc.right - min_rc.left;
}

static void choose_folder_com(HWND hWnd)
{
    IFileOpenDialog *pfd = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void**)&pfd);
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        if (SUCCEEDED(pfd->lpVtbl->GetOptions(pfd, &dwOptions))) {
            pfd->lpVtbl->SetOptions(pfd, dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }
        pfd->lpVtbl->SetTitle(pfd, L"İndirme Klasörünü Seçin");

        if (SUCCEEDED(pfd->lpVtbl->Show(pfd, hWnd))) {
            IShellItem *psi = NULL;
            if (SUCCEEDED(pfd->lpVtbl->GetResult(pfd, &psi))) {
                PWSTR pszPath = NULL;
                if (SUCCEEDED(psi->lpVtbl->GetDisplayName(psi, SIGDN_FILESYSPATH, &pszPath))) {
                    wcsncpy(g_download_dir, pszPath, MAX_PATH - 1);
                    g_download_dir[MAX_PATH - 1] = L'\0';
                    CoTaskMemFree(pszPath);
                    SHCreateDirectoryExW(NULL, g_download_dir, NULL);
                    save_download_dir();
                    update_folder_label();
                }
                psi->lpVtbl->Release(psi);
            }
        }
        pfd->lpVtbl->Release(pfd);
    }
}

static void fallback_download_dir(void)
{
    if (SHGetFolderPathW(NULL, CSIDL_MYVIDEO, NULL, 0, g_download_dir) != S_OK) {
        if (SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, g_download_dir) == S_OK) {
            size_t len = wcslen(g_download_dir);
            if (len + 11 < MAX_PATH) {
                wcscat(g_download_dir, L"\\Downloads");
            }
        } else {
            wcscpy(g_download_dir, L".");
        }
    }
    SHCreateDirectoryExW(NULL, g_download_dir, NULL);
}

static void load_settings(void)
{
    g_download_dir[0] = L'\0';

    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ff-dlp-viewer", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD len = sizeof(g_download_dir);
        if (RegQueryValueExW(key, L"DownloadDir", NULL, &type, (LPBYTE)g_download_dir, &len) == ERROR_SUCCESS &&
            type == REG_SZ) {
            g_download_dir[MAX_PATH - 1] = L'\0';
            DWORD attr = GetFileAttributesW(g_download_dir);
            if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                g_download_dir[0] = L'\0';
            }
        } else {
            g_download_dir[0] = L'\0';
        }
        RegCloseKey(key);
    }

    if (g_download_dir[0] == L'\0') {
        fallback_download_dir();
    }
    SHCreateDirectoryExW(NULL, g_download_dir, NULL);
}

static void save_download_dir(void)
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ff-dlp-viewer", 0, NULL, 0,
                        KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(key, L"DownloadDir", 0, REG_SZ, (const BYTE*)g_download_dir,
                       (DWORD)((wcslen(g_download_dir) + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
}

static void update_folder_label(void)
{
    if (!g_hStaticFolder) return;
    wchar_t label[MAX_PATH + 32];
    _snwprintf(label, MAX_PATH + 31, L"📁 %ls", g_download_dir);
    label[MAX_PATH + 31] = L'\0';
    SetWindowTextW(g_hStaticFolder, label);
}

static void destroy_fonts(void)
{
    if (g_fontTitle) { DeleteObject(g_fontTitle); g_fontTitle = NULL; }
    if (g_fontBold) { DeleteObject(g_fontBold); g_fontBold = NULL; }
    if (g_fontNormal) { DeleteObject(g_fontNormal); g_fontNormal = NULL; }
    if (g_fontSmall) { DeleteObject(g_fontSmall); g_fontSmall = NULL; }
}

static void create_fonts(void)
{
    destroy_fonts();

    g_fontTitle = CreateFontW(S(20), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fontBold = CreateFontW(S(14), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fontNormal = CreateFontW(S(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_fontSmall = CreateFontW(S(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

static void apply_control_fonts(void)
{
    if (!g_hWnd) return;

    SendMessageW(g_hGrpUrl, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hGrpMedia, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hGrpDownload, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hEditUrl, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hBtnPaste, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hBtnFetch, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hStaticTitle, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
    SendMessageW(g_hStaticMeta, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
    SendMessageW(g_hRadioVideo, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hRadioAudio, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hComboRes, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hChkForceMp4, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hComboAudio, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hStaticReason, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
    SendMessageW(g_hStaticStatus, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hStaticFolder, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
    SendMessageW(g_hBtnDownload, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
    SendMessageW(g_hBtnCancel, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hBtnOpenLocation, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    SendMessageW(g_hBtnChooseFolder, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
}

static void layout_controls(void)
{
    if (!g_hWnd || !g_hGrpUrl || !g_hBtnChooseFolder) return;

    RECT rc;
    GetClientRect(g_hWnd, &rc);
    int cw = rc.right;

    int x20 = S(20);
    int x32 = S(32);
    int inner_w = cw - S(40);
    int content_w = cw - S(64);
    if (content_w < S(200)) content_w = S(200);

    /* Grup 1: bağlantı */
    SetWindowPos(g_hGrpUrl, NULL, x20, S(45), inner_w, S(65), SWP_NOZORDER);

    int fetch_w = S(90);
    int paste_w = S(80);
    int gap = S(8);
    int fetch_x = cw - x32 - fetch_w;
    int paste_x = fetch_x - gap - paste_w;
    int edit_w = paste_x - gap - x32;
    if (edit_w < S(150)) edit_w = S(150);

    SetWindowPos(g_hEditUrl, NULL, x32, S(68), edit_w, S(25), SWP_NOZORDER);
    SetWindowPos(g_hBtnPaste, NULL, paste_x, S(67), paste_w, S(27), SWP_NOZORDER);
    SetWindowPos(g_hBtnFetch, NULL, fetch_x, S(67), fetch_w, S(27), SWP_NOZORDER);

    /* Grup 2: medya ve format */
    SetWindowPos(g_hGrpMedia, NULL, x20, S(118), inner_w, S(212), SWP_NOZORDER);
    SetWindowPos(g_hStaticTitle, NULL, x32, S(140), content_w, S(20), SWP_NOZORDER);
    SetWindowPos(g_hStaticMeta, NULL, x32, S(162), content_w, S(18), SWP_NOZORDER);
    SetWindowPos(g_hRadioVideo, NULL, x32, S(188), S(150), S(22), SWP_NOZORDER);
    SetWindowPos(g_hRadioAudio, NULL, S(195), S(188), S(160), S(22), SWP_NOZORDER);
    SetWindowPos(g_hComboRes, NULL, x32, S(216), S(220), S(200), SWP_NOZORDER);
    SetWindowPos(g_hChkForceMp4, NULL, S(265), S(217), S(180), S(22), SWP_NOZORDER);
    SetWindowPos(g_hComboAudio, NULL, x32, S(216), S(220), S(200), SWP_NOZORDER);
    SetWindowPos(g_hStaticReason, NULL, x32, S(248), content_w, S(34), SWP_NOZORDER);

    int choose_btn_w = S(110);
    int choose_btn_h = S(26);
    int folder_x = x32 + choose_btn_w + S(10);
    int folder_w = content_w - choose_btn_w - S(10);
    if (folder_w < S(100)) folder_w = S(100);

    SetWindowPos(g_hBtnChooseFolder, NULL, x32, S(288), choose_btn_w, choose_btn_h, SWP_NOZORDER);
    SetWindowPos(g_hStaticFolder, NULL, folder_x, S(292), folder_w, S(20), SWP_NOZORDER);

    /* Grup 3: indirme durumu */
    SetWindowPos(g_hGrpDownload, NULL, x20, S(338), inner_w, S(130), SWP_NOZORDER);
    SetWindowPos(g_hProgressBar, NULL, x32, S(360), content_w, S(20), SWP_NOZORDER);
    SetWindowPos(g_hStaticStatus, NULL, x32, S(386), content_w, S(20), SWP_NOZORDER);

    int btn_y = S(416);
    int btn_h = S(36);
    int open_w = S(170);
    int open_x = cw - x32 - open_w;

    SetWindowPos(g_hBtnDownload, NULL, x32, btn_y, S(120), btn_h, SWP_NOZORDER);
    SetWindowPos(g_hBtnCancel, NULL, S(160), btn_y, S(90), btn_h, SWP_NOZORDER);
    SetWindowPos(g_hBtnOpenLocation, NULL, open_x, btn_y, open_w, btn_h, SWP_NOZORDER);

    InvalidateRect(g_hWnd, NULL, TRUE);
}

static void on_fetch_callback(int success, const MediaMetadata *meta, const wchar_t *err, void *user_data)
{
    FetchResultMsg *msg = (FetchResultMsg*)calloc(1, sizeof(FetchResultMsg));
    if (!msg) return;

    msg->success = success;
    if (meta) msg->meta = *meta;
    if (err) {
        wcsncpy(msg->error_msg, err, 255);
        msg->error_msg[255] = L'\0';
    }

    if (g_hWnd && IsWindow(g_hWnd)) {
        PostMessageW(g_hWnd, WM_USER_FETCH_DONE, 0, (LPARAM)msg);
    } else {
        free(msg);
    }
}

static void on_download_tick_callback(
    const wchar_t *status,
    double percent,
    double speed,
    int eta,
    long long downloaded,
    long long total,
    const wchar_t *filename,
    const wchar_t *err,
    void *user_data
)
{
    DownloadTickMsg *msg = (DownloadTickMsg*)calloc(1, sizeof(DownloadTickMsg));
    if (!msg) return;

    if (status) {
        wcsncpy(msg->status, status, 31);
        msg->status[31] = L'\0';
    }
    msg->percent = percent;
    msg->speed = speed;
    msg->eta = eta;
    msg->downloaded = downloaded;
    msg->total = total;
    if (filename) {
        wcsncpy(msg->filename, filename, MAX_PATH - 1);
        msg->filename[MAX_PATH - 1] = L'\0';
    }
    if (err) {
        wcsncpy(msg->error_msg, err, 255);
        msg->error_msg[255] = L'\0';
    }

    if (g_hWnd && IsWindow(g_hWnd)) {
        PostMessageW(g_hWnd, WM_USER_DOWNLOAD_TICK, 0, (LPARAM)msg);
    } else {
        free(msg);
    }
}

static LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_PASTE) {
        LRESULT res = CallWindowProcW(g_oldEditProc, hWnd, msg, wParam, lParam);
        wchar_t url[1024];
        GetWindowTextW(hWnd, url, 1023);
        if (is_valid_url(url)) {
            trigger_fetch();
        }
        return res;
    } else if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        trigger_download();
        return 0;
    }
    return CallWindowProcW(g_oldEditProc, hWnd, msg, wParam, lParam);
}

static void trigger_fetch(void)
{
    wchar_t url[1024];
    GetWindowTextW(g_hEditUrl, url, 1023);
    if (!is_valid_url(url)) {
        MessageBoxW(g_hWnd, L"Lütfen geçerli bir video veya ses bağlantısı girin.", L"Geçersiz Bağlantı", MB_OK | MB_ICONWARNING);
        return;
    }

    /* Zaten aynı URL çekiliyorsa mükerrer süreç başlatma */
    if (g_is_fetching) {
        return;
    }

    /* Zaten bu URL çekilmiş ve hazırsa tekrar gereksiz ağ trafiği yapma */
    if (g_has_meta && wcscmp(url, g_last_fetched_url) == 0) {
        return;
    }

    g_is_fetching = 1;
    g_has_meta = 0;
    g_selection_valid = 0;
    if (g_hBtnOpenLocation) ShowWindow(g_hBtnOpenLocation, SW_HIDE);
    wcsncpy(g_last_fetched_url, url, 1023);
    g_last_fetched_url[1023] = L'\0';

    SetWindowTextW(g_hBtnFetch, L"Çekiliyor...");
    EnableWindow(g_hBtnFetch, FALSE);
    EnableWindow(g_hBtnCancel, TRUE);
    SetWindowTextW(g_hStaticTitle, L"Medya bilgileri sorgulanıyor, lütfen bekleyin...");
    SetWindowTextW(g_hStaticMeta, L"");
    SetWindowTextW(g_hStaticReason, L"");
    SetWindowTextW(g_hStaticStatus, L"Bilgi getiriliyor...");

    if (!engine_fetch_info_async(url, on_fetch_callback, NULL)) {
        g_is_fetching = 0;
        EnableWindow(g_hBtnFetch, TRUE);
        SetWindowTextW(g_hBtnFetch, L"Bilgi Getir");
        SetWindowTextW(g_hStaticStatus, L"Motor şu an meşgul, lütfen bekleyin.");
    }
}

static void update_format_selection(void)
{
    if (!g_has_meta) {
        g_selection_valid = 0;
        EnableWindow(g_hBtnDownload, FALSE);
        return;
    }

    memset(&g_current_selection, 0, sizeof(g_current_selection));

    int is_video = (SendMessageW(g_hRadioVideo, BM_GETCHECK, 0, 0) == BST_CHECKED);
    int target_res = 0;
    int ok = 0;

    if (is_video) {
        ShowWindow(g_hComboRes, SW_SHOW);
        ShowWindow(g_hChkForceMp4, SW_SHOW);
        ShowWindow(g_hComboAudio, SW_HIDE);

        int cur_sel = (int)SendMessageW(g_hComboRes, CB_GETCURSEL, 0, 0);
        if (cur_sel > 0 && cur_sel - 1 < g_current_meta.num_resolutions) {
            target_res = g_current_meta.available_resolutions[cur_sel - 1];
        }

        int force_mp4 = (SendMessageW(g_hChkForceMp4, BM_GETCHECK, 0, 0) == BST_CHECKED);
        ok = select_best_format(g_current_meta.formats, g_current_meta.format_count,
                                L"video", target_res, force_mp4, L"opus", &g_current_selection);
    } else {
        ShowWindow(g_hComboRes, SW_HIDE);
        ShowWindow(g_hChkForceMp4, SW_HIDE);
        ShowWindow(g_hComboAudio, SW_SHOW);

        int aud_sel = (int)SendMessageW(g_hComboAudio, CB_GETCURSEL, 0, 0);
        const wchar_t *pref = (aud_sel == 1) ? L"aac" : ((aud_sel == 2) ? L"mp3" : L"opus");
        ok = select_best_format(g_current_meta.formats, g_current_meta.format_count,
                                L"audio", 0, 0, pref, &g_current_selection);
    }

    if (!ok) {
        g_selection_valid = 0;
        EnableWindow(g_hBtnDownload, FALSE);

        wchar_t msg[512];
        if (g_current_selection.reason[0] != L'\0') {
            _snwprintf(msg, 512, L"[!] %ls", g_current_selection.reason);
        } else {
            wcscpy(msg, L"[!] Bu bağlantı için uygun bir format bulunamadı.");
        }
        msg[511] = L'\0';
        SetWindowTextW(g_hStaticReason, msg);
        return;
    }

    g_selection_valid = 1;
    if (!g_is_downloading && !g_is_fetching) {
        EnableWindow(g_hBtnDownload, TRUE);
    }

    wchar_t reason_text[512];
    if (g_current_selection.warning[0] != L'\0') {
        _snwprintf(reason_text, 512, L"[!] %ls\n%ls", g_current_selection.warning, g_current_selection.reason);
    } else {
        _snwprintf(reason_text, 512, L">> Hedef: .%ls | %ls", g_current_selection.expected_container, g_current_selection.reason);
    }
    reason_text[511] = L'\0';
    SetWindowTextW(g_hStaticReason, reason_text);
}

static void trigger_download(void)
{
    wchar_t url[1024];
    GetWindowTextW(g_hEditUrl, url, 1023);
    if (!is_valid_url(url)) {
        MessageBoxW(g_hWnd, L"Lütfen geçerli bir video veya ses bağlantısı girin.", L"Geçersiz Bağlantı", MB_OK | MB_ICONWARNING);
        return;
    }

    /* Zaten indirme sürüyorsa tekrar başlatma */
    if (g_is_downloading) {
        return;
    }

    /* Durum 1: Bilgi çekme işlemi zaten devam ediyorsa; indirme emrini kuyruğa al */
    if (g_is_fetching) {
        g_auto_download_after_fetch = 1;
        EnableWindow(g_hBtnDownload, FALSE);
        EnableWindow(g_hBtnCancel, TRUE);
        SetWindowTextW(g_hStaticStatus, L"Bilgiler alınıyor, biter bitmez indirme otomatik başlayacak...");
        return;
    }

    /* Durum 2: Henüz bilgi çekilmemişse; tek tıkla otomatik çekip indireceğiz */
    if (!g_has_meta || wcscmp(url, g_last_fetched_url) != 0) {
        g_auto_download_after_fetch = 1;
        trigger_fetch();
        return;
    }

    /* Durum 3: Bilgiler hazır, doğrudan indirmeyi başlat */
    if (!g_selection_valid ||
        g_current_selection.format_id[0] == L'\0' ||
        g_current_selection.expected_container[0] == L'\0') {
        update_format_selection();
        if (!g_selection_valid) {
            const wchar_t *reason = g_current_selection.reason[0]
                ? g_current_selection.reason
                : L"Bu bağlantıdan indirilebilecek uygun bir akış bulunamadı.";
            MessageBoxW(g_hWnd, reason, L"İndirilemiyor", MB_OK | MB_ICONWARNING);
            return;
        }
    }

    g_is_downloading = 1;
    if (g_hBtnOpenLocation) ShowWindow(g_hBtnOpenLocation, SW_HIDE);
    g_last_downloaded_file[0] = L'\0';
    EnableWindow(g_hBtnDownload, FALSE);
    EnableWindow(g_hBtnCancel, TRUE);
    SendMessageW(g_hProgressBar, PBM_SETPOS, 0, 0);
    SetWindowTextW(g_hStaticStatus, L"İndirme başlatılıyor...");

    if (!engine_download_async(url, &g_current_selection, g_download_dir, on_download_tick_callback, NULL)) {
        g_is_downloading = 0;
        EnableWindow(g_hBtnDownload, TRUE);
        EnableWindow(g_hBtnCancel, FALSE);
        SetWindowTextW(g_hStaticStatus, L"Motor şu an meşgul, lütfen bekleyin.");
    }
}

static void init_tray(HWND hWnd, HINSTANCE hInstance)
{
    memset(&g_nid, 0, sizeof(NOTIFYICONDATAW));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_USER_TRAY_ICON;
    g_nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(101));
    if (!g_nid.hIcon) g_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wcsncpy(g_nid.szTip, L"ff-dlp-viewer", sizeof(g_nid.szTip)/sizeof(wchar_t) - 1);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void tray_set_tip(const wchar_t *tip)
{
    g_nid.uFlags = NIF_TIP;
    wcsncpy(g_nid.szTip, tip, sizeof(g_nid.szTip)/sizeof(wchar_t) - 1);
    g_nid.szTip[sizeof(g_nid.szTip)/sizeof(wchar_t) - 1] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

static void tray_balloon(const wchar_t *title, const wchar_t *text)
{
    g_nid.uFlags = NIF_INFO;
    wcsncpy(g_nid.szInfoTitle, title, sizeof(g_nid.szInfoTitle)/sizeof(wchar_t) - 1);
    wcsncpy(g_nid.szInfo, text, sizeof(g_nid.szInfo)/sizeof(wchar_t) - 1);
    g_nid.szInfoTitle[sizeof(g_nid.szInfoTitle)/sizeof(wchar_t) - 1] = L'\0';
    g_nid.szInfo[sizeof(g_nid.szInfo)/sizeof(wchar_t) - 1] = L'\0';
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

static void remove_tray(void)
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static void show_tray_menu(HWND hWnd)
{
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESTORE, L"Göster");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Çıkış");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_taskbar_created_msg != 0 && msg == g_taskbar_created_msg) {
        /* Explorer yeniden başlarsa tepsi simgesini geri koy. */
        init_tray(hWnd, (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE));
        return 0;
    }

    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;
            DragAcceptFiles(hWnd, TRUE);
            g_dpi = query_window_dpi(hWnd);
            update_fixed_metrics();
            create_fonts();
            load_settings();

            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

            /* =========================================================
               GROUP 1: URL & BAĞLANTI
               ========================================================= */
            g_hGrpUrl = CreateWindowExW(0, L"BUTTON", L" Bağlantı ",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                20, 45, 590, 65, hWnd, (HMENU)IDC_GRP_URL, hInst, NULL);
            SendMessageW(g_hGrpUrl, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hEditUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                32, 68, 390, 25, hWnd, (HMENU)IDC_EDIT_URL, hInst, NULL);
            SendMessageW(g_hEditUrl, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            /* Subclass edit box for auto-fetch on paste and Enter */
            g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_hEditUrl, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            g_hBtnPaste = CreateWindowExW(0, L"BUTTON", L"Yapıştır",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                430, 67, 80, 27, hWnd, (HMENU)IDC_BTN_PASTE, hInst, NULL);
            SendMessageW(g_hBtnPaste, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnFetch = CreateWindowExW(0, L"BUTTON", L"Bilgi Getir",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                516, 67, 82, 27, hWnd, (HMENU)IDC_BTN_FETCH, hInst, NULL);
            SendMessageW(g_hBtnFetch, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            /* =========================================================
               GROUP 2: MEDYA VE FORMAT AYARLARI
               ========================================================= */
            g_hGrpMedia = CreateWindowExW(0, L"BUTTON", L" Medya ve Format Seçimi ",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                20, 118, 590, 205, hWnd, (HMENU)IDC_GRP_MEDIA, hInst, NULL);
            SendMessageW(g_hGrpMedia, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hStaticTitle = CreateWindowExW(0, L"STATIC", L"Bağlantıyı girin veya yapıştırın.",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX,
                32, 142, 565, 20, hWnd, (HMENU)IDC_STATIC_TITLE, hInst, NULL);
            SendMessageW(g_hStaticTitle, WM_SETFONT, (WPARAM)g_fontBold, TRUE);

            g_hStaticMeta = CreateWindowExW(0, L"STATIC", L"Desteklenenler: YouTube, TikTok, Instagram, X (Twitter), Web",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                32, 164, 565, 18, hWnd, (HMENU)IDC_STATIC_META, hInst, NULL);
            SendMessageW(g_hStaticMeta, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            g_hRadioVideo = CreateWindowExW(0, L"BUTTON", L"Video Olarak İndir",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
                32, 192, 150, 22, hWnd, (HMENU)IDC_RADIO_VIDEO, hInst, NULL);
            SendMessageW(g_hRadioVideo, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            SendMessageW(g_hRadioVideo, BM_SETCHECK, BST_CHECKED, 0);

            g_hRadioAudio = CreateWindowExW(0, L"BUTTON", L"Yalnızca Ses (Müzik)",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
                195, 192, 160, 22, hWnd, (HMENU)IDC_RADIO_AUDIO, hInst, NULL);
            SendMessageW(g_hRadioAudio, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hComboRes = CreateWindowExW(0, L"COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                32, 222, 220, 200, hWnd, (HMENU)IDC_COMBO_RES, hInst, NULL);
            SendMessageW(g_hComboRes, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            SendMessageW(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"En Yüksek Kalite (Varsayılan)");
            SendMessageW(g_hComboRes, CB_SETCURSEL, 0, 0);

            g_hChkForceMp4 = CreateWindowExW(0, L"BUTTON", L"MP4 Olarak Zorla",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                265, 223, 180, 22, hWnd, (HMENU)IDC_CHK_FORCE_MP4, hInst, NULL);
            SendMessageW(g_hChkForceMp4, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hComboAudio = CreateWindowExW(0, L"COMBOBOX", NULL,
                WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                32, 222, 220, 200, hWnd, (HMENU)IDC_COMBO_AUDIO, hInst, NULL);
            SendMessageW(g_hComboAudio, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            SendMessageW(g_hComboAudio, CB_ADDSTRING, 0, (LPARAM)L"Opus (Ham En Yüksek Kalite)");
            SendMessageW(g_hComboAudio, CB_ADDSTRING, 0, (LPARAM)L"AAC (M4A - Apple Uyumlu)");
            SendMessageW(g_hComboAudio, CB_ADDSTRING, 0, (LPARAM)L"MP3 (Evrensel Format)");
            SendMessageW(g_hComboAudio, CB_SETCURSEL, 0, 0);

            g_hStaticReason = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                32, 248, 565, 34, hWnd, (HMENU)IDC_STATIC_REASON, hInst, NULL);
            SendMessageW(g_hStaticReason, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            g_hBtnChooseFolder = CreateWindowExW(0, L"BUTTON", L"Konum Seç",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                32, 288, 110, 26, hWnd, (HMENU)IDC_BTN_CHOOSE_FOLDER, hInst, NULL);
            SendMessageW(g_hBtnChooseFolder, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hStaticFolder = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS | SS_NOPREFIX | SS_NOTIFY,
                150, 292, 440, 20, hWnd, (HMENU)IDC_STATIC_FOLDER, hInst, NULL);
            SendMessageW(g_hStaticFolder, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            /* =========================================================
               GROUP 3: İNDİRME DURUMU & EYLEMLER
               ========================================================= */
            g_hGrpDownload = CreateWindowExW(0, L"BUTTON", L" İndirme Durumu ",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                20, 338, 590, 130, hWnd, (HMENU)IDC_GRP_DOWNLOAD, hInst, NULL);
            SendMessageW(g_hGrpDownload, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                32, 360, 565, 20, hWnd, (HMENU)IDC_PROGRESS, hInst, NULL);

            g_hStaticStatus = CreateWindowExW(0, L"STATIC", L"Hazır",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                32, 386, 565, 20, hWnd, (HMENU)IDC_STATIC_STATUS, hInst, NULL);
            SendMessageW(g_hStaticStatus, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnDownload = CreateWindowExW(0, L"BUTTON", L"İNDİR",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                32, 416, 120, 36, hWnd, (HMENU)IDC_BTN_DOWNLOAD, hInst, NULL);
            SendMessageW(g_hBtnDownload, WM_SETFONT, (WPARAM)g_fontBold, TRUE);

            g_hBtnCancel = CreateWindowExW(0, L"BUTTON", L"İptal",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | WS_DISABLED,
                160, 416, 90, 36, hWnd, (HMENU)IDC_BTN_CANCEL, hInst, NULL);
            SendMessageW(g_hBtnCancel, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnOpenLocation = CreateWindowExW(0, L"BUTTON", L"Dosya Konumunu Aç",
                WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                438, 416, 170, 36, hWnd, (HMENU)IDC_BTN_OPEN_LOCATION, hInst, NULL);
            SendMessageW(g_hBtnOpenLocation, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            ShowWindow(g_hBtnOpenLocation, SW_HIDE);

            update_folder_label();
            apply_control_fonts();
            layout_controls();
            init_tray(hWnd, hInst);

            if (g_initial_url[0] != L'\0' && is_valid_url(g_initial_url)) {
                SetWindowTextW(g_hEditUrl, g_initial_url);
                trigger_fetch();
            }
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            wchar_t dropped_file[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, dropped_file, MAX_PATH) > 0) {
                FILE *f = _wfopen(dropped_file, L"r, ccs=UTF-8");
                if (f) {
                    wchar_t line[1024];
                    if (fgetws(line, 1023, f)) {
                        wchar_t *nl = wcschr(line, L'\n');
                        if (nl) *nl = L'\0';
                        nl = wcschr(line, L'\r');
                        if (nl) *nl = L'\0';
                        SetWindowTextW(g_hEditUrl, line);
                        if (is_valid_url(line)) trigger_fetch();
                    }
                    fclose(f);
                } else {
                    SetWindowTextW(g_hEditUrl, dropped_file);
                    if (is_valid_url(dropped_file)) trigger_fetch();
                }
            }
            DragFinish(hDrop);
            return 0;
        }

        case WM_SETCURSOR: {
            HWND hCursorWnd = (HWND)wParam;
            if (hCursorWnd == g_hStaticFolder) {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            SetBkMode(hdc, TRANSPARENT);
            if (hCtl == g_hStaticFolder) {
                SetTextColor(hdc, RGB(0, 102, 204));
                return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
            }
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            SelectObject(hdc, g_fontTitle);
            TextOutW(hdc, S(22), S(14), L"ff-dlp-viewer", 13);

            SelectObject(hdc, g_fontBold);
            const wchar_t *badge = g_has_meta ? g_current_meta.platform.badge_label : L"WEB";
            RECT badge_rc = { rc.right - S(102), S(14), rc.right - S(22), S(36) };
            DrawEdge(hdc, &badge_rc, EDGE_ETCHED, BF_RECT);
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            DrawTextW(hdc, badge, -1, &badge_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_BTN_PASTE) {
                if (OpenClipboard(hWnd)) {
                    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                    if (hData) {
                        wchar_t *pText = (wchar_t*)GlobalLock(hData);
                        if (pText) {
                            SetWindowTextW(g_hEditUrl, pText);
                            GlobalUnlock(hData);
                            if (is_valid_url(pText)) {
                                trigger_fetch();
                            }
                        }
                    }
                    CloseClipboard();
                }
            } else if (wmId == IDC_BTN_FETCH) {
                trigger_fetch();
            } else if (wmId == IDC_BTN_DOWNLOAD) {
                trigger_download();
            } else if (wmId == IDC_BTN_CANCEL) {
                engine_cancel();
                g_auto_download_after_fetch = 0;
                SetWindowTextW(g_hStaticStatus, L"İptal ediliyor...");
            } else if (wmId == IDC_BTN_OPEN_LOCATION) {
                if (g_last_downloaded_file[0] != L'\0' && file_exists(g_last_downloaded_file)) {
                    wchar_t arg[MAX_PATH + 32];
                    _snwprintf(arg, MAX_PATH + 31, L"/select,\"%ls\"", g_last_downloaded_file);
                    arg[MAX_PATH + 31] = L'\0';
                    ShellExecuteW(NULL, L"open", L"explorer.exe", arg, NULL, SW_SHOWNORMAL);
                } else {
                    ShellExecuteW(NULL, L"open", g_download_dir, NULL, NULL, SW_SHOWNORMAL);
                }
            } else if (wmId == IDC_BTN_CHOOSE_FOLDER) {
                choose_folder_com(hWnd);
            } else if (wmId == IDC_STATIC_FOLDER && (wmEvent == STN_CLICKED || wmEvent == STN_DBLCLK)) {
                ShellExecuteW(NULL, L"open", g_download_dir, NULL, NULL, SW_SHOWNORMAL);
            } else if (wmId == IDC_RADIO_VIDEO || wmId == IDC_RADIO_AUDIO ||
                       wmId == IDC_CHK_FORCE_MP4 ||
                       (wmId == IDC_COMBO_RES && wmEvent == CBN_SELCHANGE) ||
                       (wmId == IDC_COMBO_AUDIO && wmEvent == CBN_SELCHANGE)) {
                update_format_selection();
            } else if (wmId == ID_TRAY_RESTORE) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            } else if (wmId == ID_TRAY_EXIT) {
                engine_cancel();
                DestroyWindow(hWnd);
            } else if (wmId == IDOK) {
                /* Enter: odak nerede olursa olsun indirmeyi başlat. */
                trigger_download();
            } else if (wmId == IDCANCEL) {
                /* Esc: süren işlemi iptal et, yoksa pencereyi kapat. */
                if (engine_is_busy()) {
                    engine_cancel();
                    g_auto_download_after_fetch = 0;
                    SetWindowTextW(g_hStaticStatus, L"İptal ediliyor...");
                } else {
                    PostMessageW(hWnd, WM_CLOSE, 0, 0);
                }
            }
            break;
        }

        case WM_USER_FETCH_DONE: {
            FetchResultMsg *msg = (FetchResultMsg*)lParam;
            if (!msg) break;

            g_is_fetching = 0;
            EnableWindow(g_hBtnFetch, TRUE);
            SetWindowTextW(g_hBtnFetch, L"Bilgi Getir");

            if (msg->success) {
                g_current_meta = msg->meta;
                g_has_meta = 1;

                SetWindowTextW(g_hStaticTitle, g_current_meta.title);

                wchar_t meta_info[256];
                _snwprintf(meta_info, 256, L"Süre: %ls | Platform: %ls | Mevcut Akış: %d adet",
                    g_current_meta.duration_str,
                    g_current_meta.platform.display_name,
                    g_current_meta.format_count);
                SetWindowTextW(g_hStaticMeta, meta_info);

                /* Fill Resolution dropdown */
                SendMessageW(g_hComboRes, CB_RESETCONTENT, 0, 0);
                SendMessageW(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"En Yüksek Kalite (Varsayılan)");
                for (int i = 0; i < g_current_meta.num_resolutions; i++) {
                    wchar_t res_str[64];
                    int h = g_current_meta.available_resolutions[i];
                    if (h >= 2160) _snwprintf(res_str, 64, L"%dp (4K Ultra HD)", h);
                    else if (h >= 1440) _snwprintf(res_str, 64, L"%dp (2K QHD)", h);
                    else if (h >= 1080) _snwprintf(res_str, 64, L"%dp (Full HD)", h);
                    else if (h >= 720) _snwprintf(res_str, 64, L"%dp (HD)", h);
                    else _snwprintf(res_str, 64, L"%dp", h);
                    SendMessageW(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)res_str);
                }
                SendMessageW(g_hComboRes, CB_SETCURSEL, 0, 0);

                update_format_selection();
                InvalidateRect(hWnd, NULL, TRUE);

                /* Eğer kullanıcı indir butonuna basmış ve bilgi bekleniyorsa, şimdi indir */
                if (g_auto_download_after_fetch) {
                    g_auto_download_after_fetch = 0;
                    trigger_download();
                } else {
                    SetWindowTextW(g_hStaticStatus, L"Medya hazır. İndirmek için 'İNDİR' butonuna basın.");
                    EnableWindow(g_hBtnDownload, g_selection_valid ? TRUE : FALSE);
                    EnableWindow(g_hBtnCancel, FALSE);
                }
            } else {
                g_has_meta = 0;
                g_selection_valid = 0;
                g_auto_download_after_fetch = 0;
                EnableWindow(g_hBtnDownload, FALSE);
                EnableWindow(g_hBtnCancel, FALSE);
                SetWindowTextW(g_hStaticTitle, L"Bilgi alınamadı.");
                SetWindowTextW(g_hStaticMeta, msg->error_msg);
                SetWindowTextW(g_hStaticStatus, L"Hata oluştu.");
                MessageBoxW(hWnd, msg->error_msg[0] ? msg->error_msg : L"Video bilgisi alınamadı.", L"Hata", MB_OK | MB_ICONERROR);
            }

            free(msg);
            return 0;
        }

        case WM_USER_DOWNLOAD_TICK: {
            DownloadTickMsg *msg = (DownloadTickMsg*)lParam;
            if (!msg) break;

            if (wcscmp(msg->status, L"downloading") == 0) {
                SendMessageW(g_hProgressBar, PBM_SETPOS, (WPARAM)(int)msg->percent, 0);

                wchar_t speed_str[64];
                format_bytes_str((long long)msg->speed, speed_str, 64);

                wchar_t dl_str[64];
                format_bytes_str(msg->downloaded, dl_str, 64);

                wchar_t tot_str[64];
                format_bytes_str(msg->total, tot_str, 64);

                wchar_t status_line[256];
                _snwprintf(status_line, 256, L"İndiriliyor: %.1f%% | Hız: %ls/s | Kalan: %02d:%02d | %ls / %ls",
                    msg->percent, speed_str, msg->eta / 60, msg->eta % 60, dl_str, tot_str);
                SetWindowTextW(g_hStaticStatus, status_line);

                int pct = (int)msg->percent;
                if (pct != g_last_tray_percent && pct >= 0 && pct <= 100) {
                    g_last_tray_percent = pct;
                    wchar_t tip[128];
                    _snwprintf(tip, 128, L"ff-dlp-viewer - İndiriliyor: %d%%", pct);
                    tray_set_tip(tip);
                }
            } else if (wcscmp(msg->status, L"processing") == 0) {
                SendMessageW(g_hProgressBar, PBM_SETPOS, 100, 0);
                SetWindowTextW(g_hStaticStatus, L"Akışlar birleştiriliyor ve konteyner tamamlanıyor...");
                tray_set_tip(L"ff-dlp-viewer - Birleştiriliyor...");
            } else if (wcscmp(msg->status, L"finished") == 0) {
                g_is_downloading = 0;
                g_last_tray_percent = -1;
                SendMessageW(g_hProgressBar, PBM_SETPOS, 100, 0);
                SetWindowTextW(g_hStaticStatus, L"[Tamamlandı] Dosya başarıyla kaydedildi.");
                EnableWindow(g_hBtnDownload, TRUE);
                EnableWindow(g_hBtnCancel, FALSE);
                if (msg->filename[0] != L'\0') {
                    wcsncpy(g_last_downloaded_file, msg->filename, MAX_PATH - 1);
                    g_last_downloaded_file[MAX_PATH - 1] = L'\0';
                }
                if (g_hBtnOpenLocation) {
                    ShowWindow(g_hBtnOpenLocation, SW_SHOW);
                    EnableWindow(g_hBtnOpenLocation, TRUE);
                }
                tray_set_tip(L"ff-dlp-viewer - Hazır");
                tray_balloon(L"İndirme tamamlandı", L"Dosya seçilen klasöre kaydedildi.");
            } else if (wcscmp(msg->status, L"cancelled") == 0) {
                g_is_downloading = 0;
                g_last_tray_percent = -1;
                SetWindowTextW(g_hStaticStatus, L"İndirme iptal edildi.");
                EnableWindow(g_hBtnDownload, TRUE);
                EnableWindow(g_hBtnCancel, FALSE);
                if (g_hBtnOpenLocation) ShowWindow(g_hBtnOpenLocation, SW_HIDE);
                tray_set_tip(L"ff-dlp-viewer - Hazır");
            } else if (wcscmp(msg->status, L"error") == 0) {
                g_is_downloading = 0;
                g_last_tray_percent = -1;
                SetWindowTextW(g_hStaticStatus, msg->error_msg[0] ? msg->error_msg : L"İndirme hatası.");
                EnableWindow(g_hBtnDownload, TRUE);
                EnableWindow(g_hBtnCancel, FALSE);
                if (g_hBtnOpenLocation) ShowWindow(g_hBtnOpenLocation, SW_HIDE);
                tray_set_tip(L"ff-dlp-viewer - Hata");
                MessageBoxW(hWnd, msg->error_msg[0] ? msg->error_msg : L"İndirme sırasında bir hata oluştu.", L"İndirme Hatası", MB_OK | MB_ICONERROR);
            }

            free(msg);
            return 0;
        }

        case WM_USER_TRAY_ICON: {
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                show_tray_menu(hWnd);
            } else if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO*)lParam;
            if (g_min_total_w > 0) {
                mmi->ptMinTrackSize.x = g_min_total_w;
            }
            if (g_fixed_total_h > 0) {
                mmi->ptMinTrackSize.y = g_fixed_total_h;
                mmi->ptMaxTrackSize.y = g_fixed_total_h;
            }
            return 0;
        }

        case WM_SIZING: {
            if (g_fixed_total_h <= 0) break;
            RECT *rc = (RECT*)lParam;
            int h = rc->bottom - rc->top;
            if (h != g_fixed_total_h) {
                if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT) {
                    rc->top = rc->bottom - g_fixed_total_h;
                } else {
                    rc->bottom = rc->top + g_fixed_total_h;
                }
            }
            if (g_min_total_w > 0 && (rc->right - rc->left) < g_min_total_w) {
                if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT) {
                    rc->left = rc->right - g_min_total_w;
                } else {
                    rc->right = rc->left + g_min_total_w;
                }
            }
            return TRUE;
        }

        case WM_DPICHANGED: {
            g_dpi = HIWORD(wParam);
            if (g_dpi == 0) g_dpi = query_window_dpi(hWnd);
            update_fixed_metrics();
            create_fonts();
            apply_control_fonts();

            RECT *suggested = (RECT*)lParam;
            if (suggested && g_fixed_total_h > 0) {
                SetWindowPos(hWnd, NULL,
                    suggested->left, suggested->top,
                    suggested->right - suggested->left, g_fixed_total_h,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            layout_controls();
            InvalidateRect(hWnd, NULL, TRUE);
            return 0;
        }

        case WM_COPYDATA: {
            const COPYDATASTRUCT *cds = (const COPYDATASTRUCT*)lParam;
            if (!cds || cds->dwData != 1 || !cds->lpData || cds->cbData < sizeof(wchar_t)) break;

            const wchar_t *url = (const wchar_t*)cds->lpData;
            size_t max_chars = cds->cbData / sizeof(wchar_t);
            if (url[max_chars - 1] != L'\0') break;
            if (!is_valid_url(url)) break;

            ShowWindow(hWnd, SW_RESTORE);
            SetForegroundWindow(hWnd);
            SetWindowTextW(g_hEditUrl, url);
            g_has_meta = 0;
            g_selection_valid = 0;
            SetWindowTextW(g_hStaticTitle, L"Bağlantı aktarıldı, bilgiler alınıyor...");
            trigger_fetch();
            return TRUE;
        }

        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) {
                if (engine_is_busy()) {
                    ShowWindow(hWnd, SW_HIDE);
                }
            } else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
                layout_controls();
            }
            return 0;
        }

        case WM_CLOSE: {
            if (engine_is_busy()) {
                int res = MessageBoxW(hWnd,
                    L"İndirme işlemi devam ediyor.\nArka planda devam etmesi için sistem tepsisine küçültülsün mü?",
                    L"İndirme Sürüyor", MB_YESNOCANCEL | MB_ICONQUESTION);
                if (res == IDYES) {
                    ShowWindow(hWnd, SW_HIDE);
                    return 0;
                } else if (res == IDCANCEL) {
                    return 0;
                }
                engine_cancel();
            }
            DestroyWindow(hWnd);
            return 0;
        }

        case WM_DESTROY: {
            g_hWnd = NULL;
            remove_tray();
            destroy_fonts();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int ui_run(HINSTANCE hInstance, int nCmdShow, const wchar_t *initial_url)
{
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    g_taskbar_created_msg = RegisterWindowMessageW(L"TaskbarCreated");

    if (initial_url) {
        wcsncpy(g_initial_url, initial_url, 1023);
        g_initial_url[1023] = L'\0';
    }

    g_dpi = query_system_dpi();
    update_fixed_metrics();

    RECT rc = { 0, 0, S(BASE_CLIENT_W), S(BASE_CLIENT_H) };
    adjust_window_rect_for_dpi(&rc, APP_WINDOW_STYLE, FALSE, APP_WINDOW_EX_STYLE, g_dpi);
    int init_w = rc.right - rc.left;
    int init_h = g_fixed_total_h > 0 ? g_fixed_total_h : (rc.bottom - rc.top);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(101));
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = UI_WINDOW_CLASS;

    if (!RegisterClassExW(&wc)) {
        CoUninitialize();
        return 0;
    }

    HWND hWnd = CreateWindowExW(
        APP_WINDOW_EX_STYLE,
        UI_WINDOW_CLASS,
        L"ff-dlp-viewer",
        APP_WINDOW_STYLE,
        CW_USEDEFAULT, CW_USEDEFAULT, init_w, init_h,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        CoUninitialize();
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    SetFocus(g_hEditUrl);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    CoUninitialize();
    return (int)msg.wParam;
}
