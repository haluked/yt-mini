#include "ui.h"
#include "engine.h"
#include "detector.h"
#include "codec_selector.h"
#include "sanitization.h"
#include "bootstrapper.h"
#include "gb_ui.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

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
#define IDC_BTN_MIN           1021
#define IDC_BTN_CLOSE         1022

#define ID_TRAY_RESTORE       2001
#define ID_TRAY_EXIT          2002
#define ID_TRAY_CHECK_UPDATE  2003

#define BASE_CLIENT_W         646
#define BASE_CLIENT_H         485
#define BASE_MIN_CLIENT_W     620

#define APP_WINDOW_STYLE      (WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU)
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
static HWND g_hStaticStatus = NULL;
static HWND g_hStaticFolder = NULL;
static HWND g_hBtnDownload = NULL;
static HWND g_hBtnCancel = NULL;
static HWND g_hBtnOpenLocation = NULL;
static HWND g_hBtnChooseFolder = NULL;
static HWND g_hBtnMin = NULL;
static HWND g_hBtnClose = NULL;

/* Game Boy arayüz durumu (yerel radio/checkbox/combobox yerine) */
static int g_mode_audio = 0;
static int g_force_mp4 = 0;
static int g_res_sel = 0;
static int g_aud_sel = 0;
static wchar_t g_res_items[32][96];
static int g_res_count = 0;
static wchar_t g_aud_items[4][96];
static int g_aud_count = 0;
static int g_progress = 0;

static RECT g_rc_panel_url;
static RECT g_rc_panel_media;
static RECT g_rc_panel_download;
static RECT g_rc_progress;
static RECT g_rc_edit;

/* Owner-draw butonların hover durumu */
#define GB_MAX_TRACKED 16
typedef struct {
    HWND hwnd;
    WNDPROC old_proc;
    int hover;
} GbTrackedButton;

static GbTrackedButton g_tracked[GB_MAX_TRACKED];
static int g_tracked_count = 0;

/* URL kutusunun özel imleci ve seçim rengi */
#define EDIT_CARET_TIMER 7
static int g_edit_focused = 0;
static int g_edit_caret_visible = 1;


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
static int g_mp3_available = 0;
static int g_tray_hint_shown = 0;
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
    int success;
    MediaMetadata meta;
    wchar_t url[1024];
    wchar_t error_msg[256];
} AutoFetchResultMsg;

typedef struct {
    MediaMetadata meta;
    wchar_t url[1024];
    HFONT font_badge;
    HFONT font_title;
    HFONT font_small;
    HFONT font_btn;
    RECT rc_btn;
    RECT rc_close;
    int btn_hover;
    int close_hover;
} ToastState;

static ToastState g_toast_state;
static HWND g_hToastWnd = NULL;
static wchar_t g_last_clipboard_url[1024] = L"";

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
static void refresh_dropdown_labels(void);
static void tray_balloon(const wchar_t *title, const wchar_t *text);
static void show_toast_popup(HWND hMainWnd, const MediaMetadata *meta, const wchar_t *url);
static void apply_toast_to_main(ToastState *state, int auto_start_download);
static void trigger_autofetch(const wchar_t *url);

static int S(int value)
{
    return MulDiv(value, (int)g_dpi, 96);
}

/* ------------------------------------------------------------------
   Owner-draw butonlarda fare üstünde olma durumu.
   GDI'nin DRAWITEMSTRUCT'ı hover bilmediği için kendi takibimizi yapıyoruz.
   ------------------------------------------------------------------ */
static GbTrackedButton *tracked_find(HWND hwnd)
{
    int i;
    for (i = 0; i < g_tracked_count; i++) {
        if (g_tracked[i].hwnd == hwnd) return &g_tracked[i];
    }
    return NULL;
}

static LRESULT CALLBACK TrackedButtonProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    GbTrackedButton *t = tracked_find(hwnd);
    WNDPROC old = t ? t->old_proc : NULL;

    if (msg == WM_MOUSEMOVE && t) {
        if (!t->hover) {
            t->hover = 1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        {
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
    } else if (msg == WM_MOUSELEAVE && t) {
        if (t->hover) {
            t->hover = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }

    return old ? CallWindowProcW(old, hwnd, msg, wParam, lParam)
               : DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void track_button(HWND hwnd)
{
    GbTrackedButton *t;
    if (!hwnd || g_tracked_count >= GB_MAX_TRACKED) return;
    t = &g_tracked[g_tracked_count++];
    t->hwnd = hwnd;
    t->hover = 0;
    t->old_proc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)TrackedButtonProc);
}

static int tracked_hover(HWND hwnd)
{
    GbTrackedButton *t = tracked_find(hwnd);
    return t ? t->hover : 0;
}

/* Panel zemin fırçası (static/edit denetimlerinin arka planı) */
static HBRUSH panel_brush(void)
{
    static HBRUSH brush = NULL;
    if (!brush) brush = CreateSolidBrush(GB_LIGHTEST);
    return brush;
}

/* ------------------------------------------------------------------
   Açılır listelerin içeriği
   ------------------------------------------------------------------ */
static void fill_res_items(void)
{
    int i;
    g_res_count = 0;
    wcsncpy(g_res_items[g_res_count], L"EN YÜKSEK KALİTE", 95);
    g_res_items[g_res_count][95] = L'\0';
    g_res_count++;

    for (i = 0; i < g_current_meta.num_resolutions && g_res_count < 32; i++) {
        wchar_t *dst = g_res_items[g_res_count];
        int h = g_current_meta.available_resolutions[i];
        if (h >= 2160) _snwprintf(dst, 95, L"%dp  4K ULTRA HD", h);
        else if (h >= 1440) _snwprintf(dst, 95, L"%dp  2K QHD", h);
        else if (h >= 1080) _snwprintf(dst, 95, L"%dp  FULL HD", h);
        else if (h >= 720) _snwprintf(dst, 95, L"%dp  HD", h);
        else _snwprintf(dst, 95, L"%dp", h);
        dst[95] = L'\0';
        g_res_count++;
    }
    if (g_res_sel >= g_res_count) g_res_sel = 0;
}

static void fill_aud_items(void)
{
    g_aud_count = 0;
    wcsncpy(g_aud_items[g_aud_count], L"OPUS  EN YÜKSEK KALİTE", 95);
    g_aud_items[g_aud_count][95] = L'\0';
    g_aud_count++;

    wcsncpy(g_aud_items[g_aud_count], L"AAC  M4A  APPLE UYUMLU", 95);
    g_aud_items[g_aud_count][95] = L'\0';
    g_aud_count++;

    if (g_mp3_available) {
        wcsncpy(g_aud_items[g_aud_count], L"MP3  EVRENSEL FORMAT", 95);
        g_aud_items[g_aud_count][95] = L'\0';
        g_aud_count++;
    }
    if (g_aud_sel >= g_aud_count) g_aud_sel = 0;
}

static void on_res_picked(int index, void *user)
{
    (void)user;
    g_res_sel = index;
    refresh_dropdown_labels();
    update_format_selection();
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void on_aud_picked(int index, void *user)
{
    (void)user;
    g_aud_sel = index;
    refresh_dropdown_labels();
    update_format_selection();
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void open_res_dropdown(void)
{
    const wchar_t *ptrs[32];
    RECT anchor;
    int i;
    if (g_res_count <= 0) fill_res_items();
    if (g_res_count <= 0) return;
    for (i = 0; i < g_res_count; i++) ptrs[i] = g_res_items[i];

    GetWindowRect(g_hComboRes, &anchor);
    MapWindowPoints(NULL, g_hWnd, (POINT*)&anchor, 2);
    gb_dropdown_open(g_hWnd, &anchor, ptrs, g_res_count, g_res_sel, on_res_picked, NULL);
}

static void open_aud_dropdown(void)
{
    const wchar_t *ptrs[4];
    RECT anchor;
    int i;
    if (g_aud_count <= 0) fill_aud_items();
    if (g_aud_count <= 0) return;
    for (i = 0; i < g_aud_count; i++) ptrs[i] = g_aud_items[i];

    GetWindowRect(g_hComboAudio, &anchor);
    MapWindowPoints(NULL, g_hWnd, (POINT*)&anchor, 2);
    gb_dropdown_open(g_hWnd, &anchor, ptrs, g_aud_count, g_aud_sel, on_aud_picked, NULL);
}

/* Açılır butonun üzerindeki metni günceller. */
static void refresh_dropdown_labels(void)
{
    if (g_res_count > 0 && g_res_sel >= 0 && g_res_sel < g_res_count) {
        SetWindowTextW(g_hComboRes, g_res_items[g_res_sel]);
    }
    if (g_aud_count > 0 && g_aud_sel >= 0 && g_aud_sel < g_aud_count) {
        SetWindowTextW(g_hComboAudio, g_aud_items[g_aud_sel]);
    }
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
    g_tray_hint_shown = 0;

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

        {
            DWORD hint = 0;
            DWORD hint_len = sizeof(hint);
            DWORD hint_type = 0;
            if (RegQueryValueExW(key, L"TrayHintShown", NULL, &hint_type,
                                 (LPBYTE)&hint, &hint_len) == ERROR_SUCCESS &&
                hint_type == REG_DWORD) {
                g_tray_hint_shown = hint ? 1 : 0;
            }
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

static void save_tray_hint_shown(void)
{
    HKEY key;
    DWORD value = 1;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ff-dlp-viewer", 0, NULL, 0,
                        KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(key, L"TrayHintShown", 0, REG_DWORD,
                       (const BYTE*)&value, sizeof(value));
        RegCloseKey(key);
    }
}

/* X'e basınca uygulama kapanmaz; Steam/WhatsApp gibi tepsiye gizlenir. */
static void hide_to_tray(HWND hWnd)
{
    ShowWindow(hWnd, SW_HIDE);

    if (!g_tray_hint_shown) {
        g_tray_hint_shown = 1;
        save_tray_hint_shown();
        tray_balloon(L"Arka planda çalışıyor",
                     L"Tamamen kapatmak için tepsi simgesine sağ tıklayıp "
                     L"\"Çıkış\" seçeneğini kullanın.");
    }
}

static void update_folder_label(void)
{
    if (!g_hStaticFolder) return;
    wchar_t label[MAX_PATH + 32];
    _snwprintf(label, MAX_PATH + 31, L"> %ls", g_download_dir);
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

    /* 8-bit hissi: Consolas + anti-aliasing kapalı. Türkçe karakterler
       Consolas'ta tamdır; harici font dosyası taşımaya gerek yok. */
    g_fontTitle = CreateFontW(-S(20), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    g_fontBold = CreateFontW(-S(14), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    g_fontNormal = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    g_fontSmall = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
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
    SetRect(&g_rc_panel_url, x20, S(45), x20 + inner_w, S(45) + S(65));

    int fetch_w = S(100);
    int paste_w = S(80);
    int gap = S(8);
    int fetch_x = cw - x32 - fetch_w;
    int paste_x = fetch_x - gap - paste_w;
    int edit_w = paste_x - gap - x32;
    if (edit_w < S(150)) edit_w = S(150);

    SetWindowPos(g_hEditUrl, NULL, x32, S(70), edit_w, S(22), SWP_NOZORDER);
    SetRect(&g_rc_edit, x32, S(70), x32 + edit_w, S(70) + S(22));
    SetWindowPos(g_hBtnPaste, NULL, paste_x, S(67), paste_w, S(27), SWP_NOZORDER);
    SetWindowPos(g_hBtnFetch, NULL, fetch_x, S(67), fetch_w, S(27), SWP_NOZORDER);

    /* Grup 2: medya ve format */
    SetWindowPos(g_hGrpMedia, NULL, x20, S(118), inner_w, S(212), SWP_NOZORDER);
    SetRect(&g_rc_panel_media, x20, S(118), x20 + inner_w, S(118) + S(212));
    SetWindowPos(g_hStaticTitle, NULL, x32, S(140), content_w, S(20), SWP_NOZORDER);
    SetWindowPos(g_hStaticMeta, NULL, x32, S(162), content_w, S(18), SWP_NOZORDER);
    SetWindowPos(g_hRadioVideo, NULL, x32, S(188), S(150), S(24), SWP_NOZORDER);
    SetWindowPos(g_hRadioAudio, NULL, S(195), S(188), S(160), S(24), SWP_NOZORDER);
    SetWindowPos(g_hComboRes, NULL, x32, S(216), S(220), S(26), SWP_NOZORDER);
    SetWindowPos(g_hChkForceMp4, NULL, S(265), S(217), S(180), S(24), SWP_NOZORDER);
    SetWindowPos(g_hComboAudio, NULL, x32, S(216), S(220), S(26), SWP_NOZORDER);
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
    SetRect(&g_rc_panel_download, x20, S(338), x20 + inner_w, S(338) + S(130));
    SetRect(&g_rc_progress, x32, S(360), x32 + content_w, S(360) + S(20));
    SetWindowPos(g_hStaticStatus, NULL, x32, S(386), content_w, S(20), SWP_NOZORDER);

    int btn_y = S(416);
    int btn_h = S(36);
    int open_w = S(170);
    int open_x = cw - x32 - open_w;

    SetWindowPos(g_hBtnDownload, NULL, x32, btn_y, S(120), btn_h, SWP_NOZORDER);
    SetWindowPos(g_hBtnCancel, NULL, S(160), btn_y, S(90), btn_h, SWP_NOZORDER);
    SetWindowPos(g_hBtnOpenLocation, NULL, open_x, btn_y, open_w, btn_h, SWP_NOZORDER);

    /* Başlık çubuğu düğmeleri */
    {
        int close_w = S(26);
        int close_h = S(24);
        int close_x = cw - S(14) - close_w;
        int min_x = close_x - S(6) - close_w;
        SetWindowPos(g_hBtnClose, NULL, close_x, S(9), close_w, close_h, SWP_NOZORDER);
        SetWindowPos(g_hBtnMin, NULL, min_x, S(9), close_w, close_h, SWP_NOZORDER);
    }

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

static void on_autofetch_callback(int success, const MediaMetadata *meta, const wchar_t *err, void *user_data)
{
    AutoFetchResultMsg *msg = (AutoFetchResultMsg*)calloc(1, sizeof(AutoFetchResultMsg));
    if (!msg) return;

    msg->success = success;
    if (meta) msg->meta = *meta;
    if (user_data) {
        wcsncpy(msg->url, (const wchar_t*)user_data, 1023);
        msg->url[1023] = L'\0';
        free(user_data);
    }
    if (err) {
        wcsncpy(msg->error_msg, err, 255);
        msg->error_msg[255] = L'\0';
    }

    if (g_hWnd && IsWindow(g_hWnd)) {
        PostMessageW(g_hWnd, WM_USER_AUTOFETCH_DONE, 0, (LPARAM)msg);
    } else {
        free(msg);
    }
}

static void trigger_autofetch(const wchar_t *url)
{
    if (!url || !url[0]) return;
    wchar_t *copy_url = _wcsdup(url);
    if (!engine_fetch_info_async(url, on_autofetch_callback, copy_url)) {
        if (copy_url) free(copy_url);
    }
}

/* ------------------------------------------------------------------
   URL kutusu: özel imleç ve GB renkli metin seçimi
   Yerel EDIT denetimi metin düzenlemeyi yapar; görsel katmanı biz çizeriz.
   ------------------------------------------------------------------ */
static int edit_char_x(HWND hWnd, int index, int *x)
{
    LRESULT pos;
    if (index < 0) return 0;
    pos = SendMessageW(hWnd, EM_POSFROMCHAR, (WPARAM)index, 0);
    if (pos == -1) return 0;
    *x = (int)(short)LOWORD(pos);
    return 1;
}

static void edit_start_blink(HWND hWnd)
{
    UINT ms = GetCaretBlinkTime();
    if (ms == (UINT)-1 || ms < 200 || ms > 2000) ms = 530;
    g_edit_caret_visible = 1;
    KillTimer(hWnd, EDIT_CARET_TIMER);
    SetTimer(hWnd, EDIT_CARET_TIMER, ms, NULL);
}

static void edit_after_edit(HWND hWnd)
{
    HideCaret(hWnd);
    edit_start_blink(hWnd);
    InvalidateRect(hWnd, NULL, FALSE);
}

static void edit_draw_overlay(HWND hWnd)
{
    HDC dc;
    RECT rc, sel;
    TEXTMETRICW tm;
    SIZE one;
    HFONT font;
    HGDIOBJ old_font;
    DWORD sel_start = 0, sel_end = 0;
    wchar_t text[2048];
    POINT caret;
    int len, x0 = 0, x1 = 0, has_sel, cx, caret_w;

    if (!g_edit_focused || !GetCaretPos(&caret)) return;
    if (!GetClientRect(hWnd, &rc)) return;

    dc = GetDC(hWnd);
    font = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
    if (!font) font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    old_font = SelectObject(dc, font);
    if (!GetTextMetricsW(dc, &tm)) {
        SelectObject(dc, old_font);
        ReleaseDC(hWnd, dc);
        return;
    }
    if (!GetTextExtentPoint32W(dc, L"0", 1, &one)) one.cx = tm.tmAveCharWidth;

    SendMessageW(hWnd, EM_GETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);
    len = GetWindowTextW(hWnd, text, 2047);
    if (len < 0) len = 0;
    if (sel_start > (DWORD)len) sel_start = (DWORD)len;
    if (sel_end > (DWORD)len) sel_end = (DWORD)len;
    has_sel = (sel_end > sel_start);

    if (has_sel) {
        if (!edit_char_x(hWnd, (int)sel_start, &x0)) x0 = rc.left;
        if ((int)sel_end < len) {
            if (!edit_char_x(hWnd, (int)sel_end, &x1)) x1 = x0;
        } else if (len > 0) {
            if (edit_char_x(hWnd, len - 1, &x1)) x1 += one.cx;
            else x1 = x0;
        } else {
            x1 = x0;
        }

        sel.left = x0;
        sel.right = x1;
        sel.top = caret.y;
        sel.bottom = caret.y + tm.tmHeight;
        if (sel.left < rc.left) sel.left = rc.left;
        if (sel.right > rc.right) sel.right = rc.right;
        if (sel.right > sel.left) {
            gb_fill(dc, &sel, GB_DARKEST);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, GB_LIGHTEST);
            TextOutW(dc, x0, caret.y, text + sel_start, (int)(sel_end - sel_start));
        }
    }

    if (g_edit_caret_visible) {
        RECT cr;
        caret_w = S(2);
        if (caret_w < 2) caret_w = 2;
        cx = caret.x;
        if (cx < rc.left) cx = rc.left;
        if (cx + caret_w > rc.right) cx = rc.right - caret_w;
        cr.left = cx;
        cr.top = caret.y;
        cr.right = cx + caret_w;
        cr.bottom = caret.y + tm.tmHeight;
        gb_fill(dc, &cr, has_sel ? GB_LIGHTEST : GB_DARKEST);
    }

    SelectObject(dc, old_font);
    ReleaseDC(hWnd, dc);
}

static LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT res;

    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        trigger_download();
        return 0;
    }

    if (msg == WM_TIMER && wParam == EDIT_CARET_TIMER) {
        g_edit_caret_visible = !g_edit_caret_visible;
        HideCaret(hWnd);
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    if (msg == WM_SETFOCUS) {
        res = CallWindowProcW(g_oldEditProc, hWnd, msg, wParam, lParam);
        g_edit_focused = 1;
        HideCaret(hWnd);
        edit_start_blink(hWnd);
        InvalidateRect(hWnd, NULL, FALSE);
        return res;
    }

    if (msg == WM_KILLFOCUS) {
        res = CallWindowProcW(g_oldEditProc, hWnd, msg, wParam, lParam);
        g_edit_focused = 0;
        KillTimer(hWnd, EDIT_CARET_TIMER);
        InvalidateRect(hWnd, NULL, FALSE);
        return res;
    }

    if (msg == WM_PAINT) {
        res = CallWindowProcW(g_oldEditProc, hWnd, msg, wParam, lParam);
        HideCaret(hWnd);
        edit_draw_overlay(hWnd);
        return res;
    }

    if (msg == WM_PASTE) {
        wchar_t url[1024];
        res = CallWindowProcW(g_oldEditProc, hWnd, msg, wParam, lParam);
        GetWindowTextW(hWnd, url, 1023);
        edit_after_edit(hWnd);
        if (is_valid_url(url)) {
            trigger_fetch();
        }
        return res;
    }

    if (msg == WM_CHAR || msg == WM_KEYDOWN || msg == WM_CUT || msg == WM_CLEAR ||
        msg == WM_UNDO || msg == WM_SETTEXT ||
        msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK ||
        (msg == WM_MOUSEMOVE && (wParam & MK_LBUTTON))) {
        res = CallWindowProcW(g_oldEditProc, hWnd, msg, wParam, lParam);
        edit_after_edit(hWnd);
        return res;
    }

    return CallWindowProcW(g_oldEditProc, hWnd, msg, wParam, lParam);
}

/* Kullanıcı URL'yi başında/sonunda boşlukla yapıştırırsa yt-dlp başarısız
   oluyordu; doğrulamadan ve motora vermeden önce temizle. */
static void trim_ws(wchar_t *s)
{
    if (!s) return;
    wchar_t *p = s;
    while (*p == L' ' || *p == L'\t' || *p == L'\r' || *p == L'\n') p++;
    if (p != s) memmove(s, p, (wcslen(p) + 1) * sizeof(wchar_t));

    size_t len = wcslen(s);
    while (len > 0) {
        wchar_t c = s[len - 1];
        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') s[--len] = L'\0';
        else break;
    }
}

static void trigger_fetch(void)
{
    wchar_t url[1024];
    GetWindowTextW(g_hEditUrl, url, 1023);
    trim_ws(url);
    if (!is_valid_url(url)) {
        gb_message_box(g_hWnd, L"GEÇERSİZ BAĞLANTI",
            L"Lütfen geçerli bir video veya ses bağlantısı girin.",
            MB_OK | MB_ICONWARNING);
        return;
    }

    /* Motor meşgulse (ön-çekim, indirme ya da toast fetch sürüyorsa) HİÇBİR
       durum değişmeden geri dön: mevcut medya bilgisi silinmesin, UI
       "sorgulanıyor" takılı kalmasın ve otomatik-indirme bayrağı asılı kalmasın. */
    if (g_is_fetching || engine_is_busy()) {
        g_auto_download_after_fetch = 0;
        SetWindowTextW(g_hStaticStatus, L"Motor şu an meşgul, lütfen bekleyin.");
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
        g_auto_download_after_fetch = 0;
        EnableWindow(g_hBtnFetch, TRUE);
        SetWindowTextW(g_hBtnFetch, L"Bilgi Getir");
        SetWindowTextW(g_hStaticStatus, L"Motor şu an meşgul, lütfen bekleyin.");
    }
}

static void update_format_selection(void)
{
    int is_video = !g_mode_audio;

    /* Görünürlüğü her durumda güncelle: medya yokken sekme değiştirilirse
       çözünürlük/ses kutuları yine doğru takas edilmeli. */
    if (is_video) {
        ShowWindow(g_hComboRes, SW_SHOW);
        ShowWindow(g_hChkForceMp4, SW_SHOW);
        ShowWindow(g_hComboAudio, SW_HIDE);
    } else {
        ShowWindow(g_hComboRes, SW_HIDE);
        ShowWindow(g_hChkForceMp4, SW_HIDE);
        ShowWindow(g_hComboAudio, SW_SHOW);
    }

    if (!g_has_meta) {
        g_selection_valid = 0;
        EnableWindow(g_hBtnDownload, FALSE);
        return;
    }

    memset(&g_current_selection, 0, sizeof(g_current_selection));

    int target_res = 0;
    int ok = 0;

    if (is_video) {
        int cur_sel = g_res_sel;
        if (cur_sel > 0 && cur_sel - 1 < g_current_meta.num_resolutions) {
            target_res = g_current_meta.available_resolutions[cur_sel - 1];
        }

        int force_mp4 = g_force_mp4;
        ok = select_best_format(g_current_meta.formats, g_current_meta.format_count,
                                L"video", target_res, force_mp4, L"opus", &g_current_selection);
    } else {
        int aud_sel = g_aud_sel;
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
    trim_ws(url);
    if (!is_valid_url(url)) {
        gb_message_box(g_hWnd, L"GEÇERSİZ BAĞLANTI",
            L"Lütfen geçerli bir video veya ses bağlantısı girin.",
            MB_OK | MB_ICONWARNING);
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
            gb_message_box(g_hWnd, L"İNDİRİLEMİYOR", reason,
                MB_OK | MB_ICONWARNING);
            return;
        }
    }

    g_is_downloading = 1;
    if (g_hBtnOpenLocation) ShowWindow(g_hBtnOpenLocation, SW_HIDE);
    g_last_downloaded_file[0] = L'\0';
    EnableWindow(g_hBtnDownload, FALSE);
    EnableWindow(g_hBtnCancel, TRUE);
    g_progress = 0;
    InvalidateRect(g_hWnd, &g_rc_progress, FALSE);
    SetWindowTextW(g_hStaticStatus, L"İndirme başlatılıyor...");

    if (!engine_download_async(url, &g_current_selection, g_download_dir, on_download_tick_callback, NULL)) {
        g_is_downloading = 0;
        EnableWindow(g_hBtnDownload, TRUE);
        EnableWindow(g_hBtnCancel, FALSE);
        SetWindowTextW(g_hStaticStatus, L"Motor şu an meşgul, lütfen bekleyin.");
    }
}

static void apply_toast_to_main(ToastState *state, int auto_start_download)
{
    if (!state || !g_hWnd) return;

    ShowWindow(g_hWnd, SW_SHOW);
    ShowWindow(g_hWnd, SW_RESTORE);

    DWORD curThread = GetCurrentThreadId();
    DWORD foreThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    if (curThread != foreThread) {
        AttachThreadInput(curThread, foreThread, TRUE);
        BringWindowToTop(g_hWnd);
        SetForegroundWindow(g_hWnd);
        AttachThreadInput(curThread, foreThread, FALSE);
    } else {
        BringWindowToTop(g_hWnd);
        SetForegroundWindow(g_hWnd);
    }

    SetWindowTextW(g_hEditUrl, state->url);
    wcsncpy(g_last_fetched_url, state->url, 1023);
    g_last_fetched_url[1023] = L'\0';

    g_current_meta = state->meta;
    g_has_meta = 1;

    SetWindowTextW(g_hStaticTitle, g_current_meta.title);

    wchar_t meta_info[256];
    _snwprintf(meta_info, 256, L"Süre: %ls | Platform: %ls | Mevcut Akış: %d adet",
        g_current_meta.duration_str,
        g_current_meta.platform.display_name,
        g_current_meta.format_count);
    SetWindowTextW(g_hStaticMeta, meta_info);

    g_res_sel = 0;
    fill_res_items();
    refresh_dropdown_labels();

    update_format_selection();
    InvalidateRect(g_hWnd, NULL, TRUE);

    if (auto_start_download) {
        trigger_download();
    } else {
        SetWindowTextW(g_hStaticStatus, L"Medya hazır. İndirmek için 'İNDİR' butonuna basın.");
        EnableWindow(g_hBtnDownload, g_selection_valid ? TRUE : FALSE);
    }
}

static LRESULT CALLBACK ToastWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CREATE: {
            g_toast_state.font_badge = CreateFontW(-S(12), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
            g_toast_state.font_title = CreateFontW(-S(14), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
            g_toast_state.font_small = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
            g_toast_state.font_btn = CreateFontW(-S(13), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");

            /* 4 saniyelik otomatik kaybolma zamanlayıcısı */
            SetTimer(hWnd, 1, 4000, NULL);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                KillTimer(hWnd, 1);
                DestroyWindow(hWnd);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);

            /* Game Boy kartı */
            gb_fill(hdc, &rcClient, GB_LIGHTEST);
            gb_frame(hdc, &rcClient, GB_DARKEST, S(2));

            /* Üst şerit */
            {
                RECT bar = { S(2), S(2), rcClient.right - S(2), S(24) };
                gb_fill(hdc, &bar, GB_DARKEST);
            }

            /* 1. Platform rozeti */
            {
                RECT rcBadge = { S(8), S(4), S(86), S(22) };
                wchar_t badge[40];
                wcsncpy(badge, g_toast_state.meta.platform.badge_label, 39);
                badge[39] = L'\0';
                if (badge[0] == L'\0') wcscpy(badge, L"WEB");
                gb_fill(hdc, &rcBadge, GB_DARK);
                gb_text(hdc, g_toast_state.font_badge, GB_LIGHTEST, &rcBadge, badge,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            /* Süre bilgisi */
            if (g_toast_state.meta.duration_str[0] != L'\0') {
                RECT rcDuration = { S(96), S(4), S(176), S(22) };
                gb_text(hdc, g_toast_state.font_small, GB_LIGHTEST, &rcDuration,
                        g_toast_state.meta.duration_str,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            /* Kapat Butonu */
            {
                RECT rcClose = { rcClient.right - S(26), S(2), rcClient.right - S(2), S(24) };
                g_toast_state.rc_close = rcClose;
                gb_text(hdc, g_toast_state.font_badge,
                        g_toast_state.close_hover ? GB_LIGHT : GB_LIGHTEST,
                        &rcClose, L"X",
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            /* 2. Video Başlığı */
            {
                RECT rcTitle = { S(10), S(30), rcClient.right - S(108), S(66) };
                gb_text(hdc, g_toast_state.font_title, GB_DARKEST, &rcTitle,
                        g_toast_state.meta.title,
                        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
            }

            /* 3. Alt Açıklama */
            {
                RECT rcSub = { S(10), S(70), rcClient.right - S(108), S(88) };
                gb_text(hdc, g_toast_state.font_small, GB_DARK, &rcSub,
                        L"İNDİRMEYE HAZIR - 4 SN SONRA KAPANIR",
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            /* 4. İndir Butonu */
            {
                RECT rcBtn = { rcClient.right - S(98), S(30), rcClient.right - S(10), S(66) };
                g_toast_state.rc_btn = rcBtn;
                gb_fill(hdc, &rcBtn, g_toast_state.btn_hover ? GB_DARK : GB_DARKEST);
                gb_frame(hdc, &rcBtn, GB_DARKEST, S(2));
                gb_text(hdc, g_toast_state.font_btn, GB_LIGHTEST, &rcBtn, L"İNDİR",
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int btn_h = PtInRect(&g_toast_state.rc_btn, pt);
            int close_h = PtInRect(&g_toast_state.rc_close, pt);

            if (btn_h != g_toast_state.btn_hover) {
                g_toast_state.btn_hover = btn_h;
                InvalidateRect(hWnd, &g_toast_state.rc_btn, FALSE);
            }
            if (close_h != g_toast_state.close_hover) {
                g_toast_state.close_hover = close_h;
                InvalidateRect(hWnd, &g_toast_state.rc_close, FALSE);
            }

            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);

            /* Fare üstündeyken kapanmayı geciktir: her hareket sayacı sıfırlar,
               fare durunca 4 sn sonra kapanır (hiç kapanmama riski yok). */
            SetTimer(hWnd, 1, 4000, NULL);
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_toast_state.btn_hover) {
                g_toast_state.btn_hover = 0;
                InvalidateRect(hWnd, &g_toast_state.rc_btn, FALSE);
            }
            if (g_toast_state.close_hover) {
                g_toast_state.close_hover = 0;
                InvalidateRect(hWnd, &g_toast_state.rc_close, FALSE);
            }
            /* Fare ayrıldı: kısa bir süre sonra yine kapan. */
            SetTimer(hWnd, 1, 2500, NULL);
            return 0;
        }

        case WM_SETCURSOR: {
            SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
            return TRUE;
        }

        case WM_LBUTTONUP: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&g_toast_state.rc_close, pt)) {
                DestroyWindow(hWnd);
            } else if (PtInRect(&g_toast_state.rc_btn, pt)) {
                apply_toast_to_main(&g_toast_state, 1);
                DestroyWindow(hWnd);
            } else {
                apply_toast_to_main(&g_toast_state, 0);
                DestroyWindow(hWnd);
            }
            return 0;
        }

        case WM_DESTROY: {
            if (g_toast_state.font_badge) { DeleteObject(g_toast_state.font_badge); g_toast_state.font_badge = NULL; }
            if (g_toast_state.font_title) { DeleteObject(g_toast_state.font_title); g_toast_state.font_title = NULL; }
            if (g_toast_state.font_small) { DeleteObject(g_toast_state.font_small); g_toast_state.font_small = NULL; }
            if (g_toast_state.font_btn) { DeleteObject(g_toast_state.font_btn); g_toast_state.font_btn = NULL; }
            g_hToastWnd = NULL;
            memset(&g_toast_state, 0, sizeof(g_toast_state));
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void show_toast_popup(HWND hMainWnd, const MediaMetadata *meta, const wchar_t *url)
{
    (void)hMainWnd;
    if (g_hToastWnd && IsWindow(g_hToastWnd)) {
        DestroyWindow(g_hToastWnd);
        g_hToastWnd = NULL;
    }

    if (!meta || !url) return;

    memset(&g_toast_state, 0, sizeof(g_toast_state));
    g_toast_state.meta = *meta;
    wcsncpy(g_toast_state.url, url, 1023);
    g_toast_state.url[1023] = L'\0';

    HINSTANCE hInst = GetModuleHandleW(NULL);
    const wchar_t *toastClass = L"FfDlpToastWindow";

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = ToastWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_HAND);
    wc.hbrBackground = NULL;
    wc.lpszClassName = toastClass;
    wc.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);

    /* Boyut ve kenar boşlukları DPI ile ölçeklenir (ana pencere gibi). */
    int toast_w = S(370);
    int toast_h = S(96);
    int toast_x = rcWork.right - toast_w - S(20);
    int toast_y = rcWork.bottom - toast_h - S(20);

    g_hToastWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        toastClass,
        L"ff-dlp-viewer Bildirim",
        WS_POPUP,
        toast_x, toast_y, toast_w, toast_h,
        NULL, NULL, hInst, NULL
    );

    if (g_hToastWnd) {
        ShowWindow(g_hToastWnd, SW_SHOWNOACTIVATE);
        UpdateWindow(g_hToastWnd);
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
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_CHECK_UPDATE, L"Güncellemeleri Denetle (yt-dlp)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Çıkış");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
    /* Menünün dışarı tıklamayla da güvenle kapanması için gerekli. */
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

static void draw_panel(HDC hdc, const RECT *prc, const wchar_t *title)
{
    RECT tab;
    SIZE sz;
    HGDIOBJ old;
    int text_w = 0;
    int tab_h = S(20);
    int tab_w;

    gb_fill(hdc, prc, GB_LIGHTEST);
    gb_frame(hdc, prc, GB_DARKEST, S(2));

    /* Panel başlığı: üst kenara yapışık koyu şerit */
    old = SelectObject(hdc, g_fontBold);
    if (GetTextExtentPoint32W(hdc, title, (int)wcslen(title), &sz)) text_w = sz.cx;
    SelectObject(hdc, old);

    tab_w = text_w + S(20);
    tab.left = prc->left + S(2);
    tab.top = prc->top + S(2);
    tab.right = tab.left + tab_w;
    tab.bottom = tab.top + tab_h;
    gb_fill(hdc, &tab, GB_DARKEST);

    {
        RECT tr = tab;
        tr.left += S(8);
        tr.right -= S(8);
        gb_text(hdc, g_fontBold, GB_LIGHTEST, &tr, title,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

static void draw_progress(HDC hdc, const RECT *prc, int percent)
{
    RECT inner;
    int inner_w, fill_w, x;
    int block, gap;

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    gb_fill(hdc, prc, GB_LIGHT);
    gb_frame(hdc, prc, GB_DARKEST, S(2));

    inner = *prc;
    InflateRect(&inner, -S(3), -S(3));
    inner_w = inner.right - inner.left;
    if (inner_w <= 0) return;

    fill_w = MulDiv(inner_w, percent, 100);
    block = S(7);
    gap = S(2);
    if (block < 2) block = 2;
    if (gap < 1) gap = 1;

    for (x = 0; x < fill_w; x += block + gap) {
        RECT b;
        b.left = inner.left + x;
        b.top = inner.top;
        b.right = b.left + block;
        if (b.right > inner.left + fill_w) b.right = inner.left + fill_w;
        b.bottom = inner.bottom;
        if (b.right > b.left) gb_fill(hdc, &b, GB_DARKEST);
    }
}

static void paint_main(HWND hWnd, HDC hdc)
{
    RECT rc;
    RECT inner;
    RECT title_rc;
    RECT badge_rc;
    RECT edit_frame;
    wchar_t badge[32];
    int icon_scale;

    GetClientRect(hWnd, &rc);

    /* Sayfa zemini + konsol çerçevesi */
    gb_fill(hdc, &rc, GB_LIGHT);
    gb_frame(hdc, &rc, GB_DARK, S(6));
    inner = rc;
    InflateRect(&inner, -S(6), -S(6));
    gb_frame(hdc, &inner, GB_DARKEST, S(1));

    /* Başlık */
    icon_scale = S(2);
    if (icon_scale < 2) icon_scale = 2;
    gb_icon(hdc, S(22), S(14), icon_scale, 0);

    title_rc.left = S(22) + 12 * icon_scale + S(10);
    title_rc.top = S(10);
    title_rc.right = rc.right - S(200);
    title_rc.bottom = S(38);
    gb_text(hdc, g_fontTitle, GB_DARKEST, &title_rc, L"FF-DLP-VIEWER",
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    /* Platform rozeti */
    if (g_has_meta) {
        wcsncpy(badge, g_current_meta.platform.badge_label, 31);
        badge[31] = L'\0';
    } else {
        wcscpy(badge, L"WEB");
    }
    badge_rc.right = rc.right - S(14) - S(26) * 2 - S(6) - S(10);
    badge_rc.left = badge_rc.right - S(82);
    badge_rc.top = S(14);
    badge_rc.bottom = S(36);
    gb_fill(hdc, &badge_rc, GB_DARKEST);
    gb_frame(hdc, &badge_rc, GB_DARK, S(2));
    {
        RECT tr = badge_rc;
        tr.left += S(4);
        tr.right -= S(4);
        gb_text(hdc, g_fontBold, GB_LIGHTEST, &tr, badge,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    /* Başlık çubuğu ayırıcısı */
    {
        RECT divider = { S(12), S(43), rc.right - S(12), S(44) };
        gb_fill(hdc, &divider, GB_DARK);
    }

    /* Bölüm panelleri */
    draw_panel(hdc, &g_rc_panel_url, L"1  BAĞLANTI");
    draw_panel(hdc, &g_rc_panel_media, L"2  MEDYA VE FORMAT");
    draw_panel(hdc, &g_rc_panel_download, L"3  İNDİRME DURUMU");

    /* URL kutusunun kendi çerçevesi */
    edit_frame = g_rc_edit;
    InflateRect(&edit_frame, S(2), S(2));
    gb_fill(hdc, &edit_frame, GB_DARKEST);

    /* İlerleme çubuğu */
    draw_progress(hdc, &g_rc_progress, g_progress);
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
            AddClipboardFormatListener(hWnd);
            g_dpi = query_window_dpi(hWnd);
            update_fixed_metrics();
            gb_set_dpi(g_dpi);
            create_fonts();
            load_settings();

            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

            /* =========================================================
               GROUP 1: URL & BAĞLANTI
               ========================================================= */
            g_hGrpUrl = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | BS_GROUPBOX,
                20, 45, 590, 65, hWnd, (HMENU)IDC_GRP_URL, hInst, NULL);
            SendMessageW(g_hGrpUrl, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hEditUrl = CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                32, 68, 390, 25, hWnd, (HMENU)IDC_EDIT_URL, hInst, NULL);
            SendMessageW(g_hEditUrl, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            SendMessageW(g_hEditUrl, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                         MAKELPARAM(S(8), S(8)));

            /* Subclass edit box for auto-fetch on paste and Enter */
            g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_hEditUrl, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            g_hBtnPaste = CreateWindowExW(0, L"BUTTON", L"Yapıştır",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                430, 67, 80, 27, hWnd, (HMENU)IDC_BTN_PASTE, hInst, NULL);
            SendMessageW(g_hBtnPaste, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnFetch = CreateWindowExW(0, L"BUTTON", L"Bilgi Getir",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                516, 67, 82, 27, hWnd, (HMENU)IDC_BTN_FETCH, hInst, NULL);
            SendMessageW(g_hBtnFetch, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            /* =========================================================
               GROUP 2: MEDYA VE FORMAT AYARLARI
               ========================================================= */
            g_hGrpMedia = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | BS_GROUPBOX,
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

            g_hRadioVideo = CreateWindowExW(0, L"BUTTON", L"VİDEO",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | WS_GROUP,
                32, 192, 150, 22, hWnd, (HMENU)IDC_RADIO_VIDEO, hInst, NULL);
            SendMessageW(g_hRadioVideo, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hRadioAudio = CreateWindowExW(0, L"BUTTON", L"SADECE SES",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                195, 192, 160, 22, hWnd, (HMENU)IDC_RADIO_AUDIO, hInst, NULL);
            SendMessageW(g_hRadioAudio, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hComboRes = CreateWindowExW(0, L"BUTTON", L"EN YÜKSEK KALİTE",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                32, 222, 220, 26, hWnd, (HMENU)IDC_COMBO_RES, hInst, NULL);
            SendMessageW(g_hComboRes, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hChkForceMp4 = CreateWindowExW(0, L"BUTTON", L"MP4 OLARAK ZORLA",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                265, 223, 180, 24, hWnd, (HMENU)IDC_CHK_FORCE_MP4, hInst, NULL);
            SendMessageW(g_hChkForceMp4, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hComboAudio = CreateWindowExW(0, L"BUTTON", L"OPUS  EN YÜKSEK KALİTE",
                WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                32, 222, 220, 26, hWnd, (HMENU)IDC_COMBO_AUDIO, hInst, NULL);
            SendMessageW(g_hComboAudio, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            /* Paketlenen ffmpeg'de MP3 kodlayıcısı yoksa seçeneği hiç gösterme:
               seçilip de indirme sonunda hata vermesi kullanıcıyı yanıltıyordu. */
            g_mp3_available = engine_ffmpeg_can_encode_mp3();
            fill_aud_items();

            g_hStaticReason = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                32, 248, 565, 34, hWnd, (HMENU)IDC_STATIC_REASON, hInst, NULL);
            SendMessageW(g_hStaticReason, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            g_hBtnChooseFolder = CreateWindowExW(0, L"BUTTON", L"Konum Seç",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                32, 288, 110, 26, hWnd, (HMENU)IDC_BTN_CHOOSE_FOLDER, hInst, NULL);
            SendMessageW(g_hBtnChooseFolder, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hStaticFolder = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS | SS_NOPREFIX | SS_NOTIFY,
                150, 292, 440, 20, hWnd, (HMENU)IDC_STATIC_FOLDER, hInst, NULL);
            SendMessageW(g_hStaticFolder, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            /* =========================================================
               GROUP 3: İNDİRME DURUMU & EYLEMLER
               ========================================================= */
            g_hGrpDownload = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | BS_GROUPBOX,
                20, 338, 590, 130, hWnd, (HMENU)IDC_GRP_DOWNLOAD, hInst, NULL);
            SendMessageW(g_hGrpDownload, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hStaticStatus = CreateWindowExW(0, L"STATIC", L"Hazır",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                32, 386, 565, 20, hWnd, (HMENU)IDC_STATIC_STATUS, hInst, NULL);
            SendMessageW(g_hStaticStatus, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnDownload = CreateWindowExW(0, L"BUTTON", L"İNDİR",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                32, 416, 120, 36, hWnd, (HMENU)IDC_BTN_DOWNLOAD, hInst, NULL);
            SendMessageW(g_hBtnDownload, WM_SETFONT, (WPARAM)g_fontBold, TRUE);

            g_hBtnCancel = CreateWindowExW(0, L"BUTTON", L"İptal",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | WS_DISABLED,
                160, 416, 90, 36, hWnd, (HMENU)IDC_BTN_CANCEL, hInst, NULL);
            SendMessageW(g_hBtnCancel, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnOpenLocation = CreateWindowExW(0, L"BUTTON", L"Dosya Konumunu Aç",
                WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                438, 416, 170, 36, hWnd, (HMENU)IDC_BTN_OPEN_LOCATION, hInst, NULL);
            SendMessageW(g_hBtnOpenLocation, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            ShowWindow(g_hBtnOpenLocation, SW_HIDE);

            /* Kendi başlık çubuğumuzun düğmeleri */
            g_hBtnMin = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                0, 0, 26, 24, hWnd, (HMENU)IDC_BTN_MIN, hInst, NULL);
            g_hBtnClose = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                0, 0, 26, 24, hWnd, (HMENU)IDC_BTN_CLOSE, hInst, NULL);

            gb_set_dpi(g_dpi);
            track_button(g_hBtnPaste);
            track_button(g_hBtnFetch);
            track_button(g_hRadioVideo);
            track_button(g_hRadioAudio);
            track_button(g_hComboRes);
            track_button(g_hChkForceMp4);
            track_button(g_hComboAudio);
            track_button(g_hBtnChooseFolder);
            track_button(g_hBtnDownload);
            track_button(g_hBtnCancel);
            track_button(g_hBtnOpenLocation);
            track_button(g_hBtnMin);
            track_button(g_hBtnClose);

            g_mode_audio = 0;
            g_force_mp4 = 0;
            g_res_sel = 0;
            g_aud_sel = 0;
            g_progress = 0;
            fill_res_items();
            fill_aud_items();
            refresh_dropdown_labels();

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
                    wchar_t first_line[1024] = L"";
                    wchar_t found_url[1024] = L"";
                    int line_no = 0;
                    /* .url kısayolu "[InternetShortcut]" ile başlar; gerçek adres
                       "URL=" satırındadır. İlk 64 satırı tara, URL= bul. */
                    while (fgetws(line, 1023, f) && line_no < 64) {
                        line_no++;
                        wchar_t *nl = wcschr(line, L'\n');
                        if (nl) *nl = L'\0';
                        nl = wcschr(line, L'\r');
                        if (nl) *nl = L'\0';
                        if (line_no == 1) wcsncpy(first_line, line, 1023);
                        if (_wcsnicmp(line, L"URL=", 4) == 0) {
                            wcsncpy(found_url, line + 4, 1023);
                            found_url[1023] = L'\0';
                            break;
                        }
                    }
                    fclose(f);

                    const wchar_t *candidate = found_url[0] ? found_url : first_line;
                    if (candidate[0]) {
                        SetWindowTextW(g_hEditUrl, candidate);
                        if (is_valid_url(candidate)) trigger_fetch();
                    }
                } else {
                    SetWindowTextW(g_hEditUrl, dropped_file);
                    if (is_valid_url(dropped_file)) trigger_fetch();
                }
            }
            DragFinish(hDrop);
            return 0;
        }

        case WM_NCHITTEST: {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            ScreenToClient(hWnd, &pt);
            if (pt.y < S(44)) return HTCAPTION;
            break;
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
                SetTextColor(hdc, GB_DARKEST);
                return (LRESULT)panel_brush();
            }
            SetTextColor(hdc, GB_DARKEST);
            return (LRESULT)panel_brush();
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, GB_LIGHTEST);
            SetTextColor(hdc, GB_DARKEST);
            return (LRESULT)panel_brush();
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc;
            HDC mem;
            HBITMAP bmp;
            HGDIOBJ old;
            RECT rc;

            hdc = BeginPaint(hWnd, &ps);
            GetClientRect(hWnd, &rc);

            mem = CreateCompatibleDC(hdc);
            bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            old = SelectObject(mem, bmp);

            paint_main(hWnd, mem);

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);

            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT *dis = (const DRAWITEMSTRUCT*)lParam;
            int role = -1;
            int checked = 0;
            int bold = 0;

            if (!dis) break;
            switch (dis->CtlID) {
                case IDC_BTN_PASTE:
                case IDC_BTN_FETCH:
                case IDC_BTN_CHOOSE_FOLDER:
                case IDC_BTN_OPEN_LOCATION:
                    role = GB_ROLE_PUSH;
                    break;
                case IDC_BTN_DOWNLOAD:
                    role = GB_ROLE_PRIMARY;
                    bold = 1;
                    break;
                case IDC_BTN_CANCEL:
                    role = GB_ROLE_DANGER;
                    break;
                case IDC_RADIO_VIDEO:
                    role = GB_ROLE_CHIP;
                    checked = !g_mode_audio;
                    bold = 1;
                    break;
                case IDC_RADIO_AUDIO:
                    role = GB_ROLE_CHIP;
                    checked = g_mode_audio;
                    bold = 1;
                    break;
                case IDC_CHK_FORCE_MP4:
                    role = GB_ROLE_CHECK;
                    checked = g_force_mp4;
                    break;
                case IDC_COMBO_RES:
                case IDC_COMBO_AUDIO:
                    role = GB_ROLE_DROP;
                    break;
                case IDC_BTN_MIN:
                    role = GB_ROLE_TITLE;
                    break;
                case IDC_BTN_CLOSE:
                    role = GB_ROLE_TITLE;
                    checked = 1;
                    break;
                default:
                    break;
            }

            if (role >= 0) {
                HFONT font = g_fontNormal;
                if (dis->CtlID == IDC_BTN_PASTE || dis->CtlID == IDC_BTN_FETCH ||
                    dis->CtlID == IDC_BTN_CHOOSE_FOLDER || dis->CtlID == IDC_BTN_OPEN_LOCATION ||
                    dis->CtlID == IDC_BTN_CANCEL) {
                    font = g_fontSmall;
                } else if (bold) {
                    font = g_fontBold;
                }
                gb_draw_control(dis, font, role, checked, tracked_hover(dis->hwndItem));
                return TRUE;
            }
            break;
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
                            /* GlobalUnlock'tan SONRA bu belleğe dokunmak
                               use-after-unlock olur; kontrolü önce yap. */
                            int valid = is_valid_url(pText);
                            SetWindowTextW(g_hEditUrl, pText);
                            GlobalUnlock(hData);
                            if (valid) {
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
            } else if (wmId == IDC_RADIO_VIDEO && wmEvent == BN_CLICKED) {
                g_mode_audio = 0;
                update_format_selection();
                InvalidateRect(hWnd, NULL, TRUE);
            } else if (wmId == IDC_RADIO_AUDIO && wmEvent == BN_CLICKED) {
                g_mode_audio = 1;
                update_format_selection();
                InvalidateRect(hWnd, NULL, TRUE);
            } else if (wmId == IDC_CHK_FORCE_MP4 && wmEvent == BN_CLICKED) {
                g_force_mp4 = !g_force_mp4;
                update_format_selection();
                InvalidateRect(hWnd, NULL, TRUE);
            } else if (wmId == IDC_COMBO_RES && wmEvent == BN_CLICKED) {
                open_res_dropdown();
            } else if (wmId == IDC_COMBO_AUDIO && wmEvent == BN_CLICKED) {
                open_aud_dropdown();
            } else if (wmId == IDC_BTN_MIN) {
                ShowWindow(hWnd, SW_MINIMIZE);
            } else if (wmId == IDC_BTN_CLOSE) {
                PostMessageW(hWnd, WM_CLOSE, 0, 0);
            } else if (wmId == ID_TRAY_RESTORE) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            } else if (wmId == ID_TRAY_CHECK_UPDATE) {
                bootstrapper_update_ytdlp(hWnd);
            } else if (wmId == ID_TRAY_EXIT) {
                int can_exit = 1;
                if (engine_is_busy()) {
                    int res = gb_message_box(hWnd, L"ÇIKIŞ",
                        L"İndirme işlemi devam ediyor.\nYine de çıkılsın mı?",
                        MB_YESNO | MB_ICONQUESTION);
                    can_exit = (res == IDYES);
                }
                if (can_exit) {
                    engine_cancel();
                    DestroyWindow(hWnd);
                }
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

                /* Çözünürlük listesini medya bilgisinden yeniden doldur */
                g_res_sel = 0;
                fill_res_items();
                refresh_dropdown_labels();

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
                if (wcscmp(msg->error_msg, ENGINE_MSG_CANCELLED) == 0) {
                    /* İptal bir hata değil: kullanıcı zaten biliyor, dialog ile ürkütme. */
                    SetWindowTextW(g_hStaticStatus, L"Bilgi çekme iptal edildi.");
                } else {
                    SetWindowTextW(g_hStaticStatus, L"Hata oluştu.");
                    gb_message_box(hWnd, L"HATA",
                        msg->error_msg[0] ? msg->error_msg : L"Video bilgisi alınamadı.",
                        MB_OK | MB_ICONERROR);
                }
            }

            free(msg);
            return 0;
        }

        case WM_USER_DOWNLOAD_TICK: {
            DownloadTickMsg *msg = (DownloadTickMsg*)lParam;
            if (!msg) break;

            if (wcscmp(msg->status, L"downloading") == 0) {
                g_progress = (int)msg->percent;
                InvalidateRect(hWnd, &g_rc_progress, FALSE);

                wchar_t speed_str[64];
                format_bytes_str((long long)msg->speed, speed_str, 64);

                wchar_t dl_str[64];
                format_bytes_str(msg->downloaded, dl_str, 64);

                /* yt-dlp toplam boyutu bilmiyorsa "NA" yazar; 0 bayt değil,
                 "bilinmiyor" anlamına gelir. */
                wchar_t tot_str[64];
                if (msg->total > 0) format_bytes_str(msg->total, tot_str, 64);
                else wcscpy(tot_str, L"??");

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
                g_progress = 100;
                InvalidateRect(hWnd, &g_rc_progress, FALSE);
                SetWindowTextW(g_hStaticStatus, L"Akışlar birleştiriliyor ve konteyner tamamlanıyor...");
                tray_set_tip(L"ff-dlp-viewer - Birleştiriliyor...");
            } else if (wcscmp(msg->status, L"finished") == 0) {
                g_is_downloading = 0;
                g_last_tray_percent = -1;
                g_progress = 100;
                InvalidateRect(hWnd, &g_rc_progress, FALSE);
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
                gb_message_box(hWnd, L"İNDİRME HATASI",
                    msg->error_msg[0] ? msg->error_msg : L"İndirme sırasında bir hata oluştu.",
                    MB_OK | MB_ICONERROR);
            }

            free(msg);
            return 0;
        }

        case WM_USER_TRAY_ICON: {
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                show_tray_menu(hWnd);
            } else if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
                /* Tek sol tık da pencereyi geri getirsin (Python sürümdeki gibi). */
                ShowWindow(hWnd, SW_RESTORE);
                ShowWindow(hWnd, SW_SHOW);
                SetForegroundWindow(hWnd);
            }
            return 0;
        }

        case WM_CLIPBOARDUPDATE: {
            if (g_is_downloading || g_is_fetching || engine_is_busy()) break;

            wchar_t clip_text[1024] = L"";
            int opened = 0;
            for (int r = 0; r < 5; r++) {
                if (OpenClipboard(hWnd)) { opened = 1; break; }
                Sleep(20);
            }
            if (!opened) break;

            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                const wchar_t *pText = (const wchar_t*)GlobalLock(hData);
                if (pText) {
                    wcsncpy(clip_text, pText, 1023);
                    clip_text[1023] = L'\0';
                    GlobalUnlock(hData);
                }
            } else {
                HANDLE hText = GetClipboardData(CF_TEXT);
                if (hText) {
                    const char *pStr = (const char*)GlobalLock(hText);
                    if (pStr) {
                        MultiByteToWideChar(CP_ACP, 0, pStr, -1, clip_text, 1024);
                        GlobalUnlock(hText);
                    }
                }
            }
            CloseClipboard();

            wchar_t *start = clip_text;
            while (*start == L' ' || *start == L'\t' || *start == L'\r' || *start == L'\n' || *start == L'"' || *start == L'\'') start++;
            wchar_t *end = start + wcslen(start);
            while (end > start && (end[-1] == L' ' || end[-1] == L'\t' || end[-1] == L'\r' || end[-1] == L'\n' || end[-1] == L'"' || end[-1] == L'\'')) {
                end--;
                *end = L'\0';
            }

            PlatformInfo plat = detect_platform(start);
            int is_yt = (plat.type == PLATFORM_YOUTUBE);

            if (start[0] == L'\0' || !is_valid_url(start)) break;
            if (!is_yt) break;

            if (wcscmp(start, g_last_clipboard_url) == 0) break;
            wcsncpy(g_last_clipboard_url, start, 1023);
            g_last_clipboard_url[1023] = L'\0';

            /* Ana pencere zaten odakta ve bu link yüklü ise toast açma */
            if (GetForegroundWindow() == hWnd && wcscmp(start, g_last_fetched_url) == 0) {
                break;
            }

            trigger_autofetch(start);
            return 0;
        }

        case WM_USER_AUTOFETCH_DONE: {
            AutoFetchResultMsg *msg = (AutoFetchResultMsg*)lParam;
            if (!msg) break;

            if (msg->success) {
                show_toast_popup(hWnd, &msg->meta, msg->url);
            }
            free(msg);
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
            gb_set_dpi(g_dpi);
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
            /* Motor meşgulse başlığı "bilgiler alınıyor" diye bırakma:
               fetch hiç başlamayacağı için öyle asılı kalıyordu. */
            if (g_is_fetching || engine_is_busy()) {
                g_auto_download_after_fetch = 0;
                SetWindowTextW(g_hStaticStatus,
                    L"Motor meşgul; işlem bitince 'Bilgi Getir' ile bu bağlantıyı çekebilirsiniz.");
                return TRUE;
            }
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
            /* Steam/WhatsApp davranışı: kapatma = tepsiye gizle.
               Gerçek çıkış yalnızca tepsi menüsünden yapılır. */
            hide_to_tray(hWnd);
            return 0;
        }

        case WM_DESTROY: {
            g_hWnd = NULL;
            RemoveClipboardFormatListener(hWnd);
            if (g_hToastWnd && IsWindow(g_hToastWnd)) {
                DestroyWindow(g_hToastWnd);
                g_hToastWnd = NULL;
            }
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
    icex.dwICC = ICC_STANDARD_CLASSES;
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
    wc.hbrBackground = NULL;
    wc.lpszClassName = UI_WINDOW_CLASS;
    wc.style = CS_DROPSHADOW;

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

    {
        RECT work;
        int x, y;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        x = work.left + (work.right - work.left - init_w) / 2;
        y = work.top + (work.bottom - work.top - init_h) / 2;
        SetWindowPos(hWnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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
