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
#define IDC_BTN_OPEN_FOLDER   1018
#define IDC_BTN_CHOOSE_FOLDER 1019

#define ID_TRAY_RESTORE       2001
#define ID_TRAY_EXIT          2002

static HWND g_hWnd = NULL;
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
static HWND g_hBtnDownload = NULL;
static HWND g_hBtnCancel = NULL;
static HWND g_hBtnOpenFolder = NULL;
static HWND g_hBtnChooseFolder = NULL;

static HFONT g_fontTitle = NULL;
static HFONT g_fontBold = NULL;
static HFONT g_fontNormal = NULL;
static HFONT g_fontSmall = NULL;

static NOTIFYICONDATAW g_nid;
static int g_in_tray = 0;
static wchar_t g_download_dir[MAX_PATH];
static MediaMetadata g_current_meta;
static int g_has_meta = 0;
static SelectionResult g_current_selection;

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
    wchar_t filename[128];
    wchar_t error_msg[256];
} DownloadTickMsg;

static void update_format_selection(void);

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
                }
                psi->lpVtbl->Release(psi);
            }
        }
        pfd->lpVtbl->Release(pfd);
    }
}

static void on_fetch_callback(int success, const MediaMetadata *meta, const wchar_t *err, void *user_data)
{
    FetchResultMsg *msg = (FetchResultMsg*)malloc(sizeof(FetchResultMsg));
    msg->success = success;
    if (meta) msg->meta = *meta;
    else memset(&msg->meta, 0, sizeof(MediaMetadata));
    if (err) wcsncpy(msg->error_msg, err, 255);
    else msg->error_msg[0] = L'\0';

    PostMessageW(g_hWnd, WM_USER_FETCH_DONE, 0, (LPARAM)msg);
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
    DownloadTickMsg *msg = (DownloadTickMsg*)malloc(sizeof(DownloadTickMsg));
    wcsncpy(msg->status, status ? status : L"", 31);
    msg->percent = percent;
    msg->speed = speed;
    msg->eta = eta;
    msg->downloaded = downloaded;
    msg->total = total;
    wcsncpy(msg->filename, filename ? filename : L"", 127);
    if (err) wcsncpy(msg->error_msg, err, 255);
    else msg->error_msg[0] = L'\0';

    PostMessageW(g_hWnd, WM_USER_DOWNLOAD_TICK, 0, (LPARAM)msg);
}

static void trigger_fetch(void)
{
    wchar_t url[1024];
    GetWindowTextW(g_hEditUrl, url, 1023);
    if (!is_valid_url(url)) {
        MessageBoxW(g_hWnd, L"Lütfen geçerli bir video veya ses bağlantısı girin.", L"Geçersiz Bağlantı", MB_OK | MB_ICONWARNING);
        return;
    }

    SetWindowTextW(g_hBtnFetch, L"Çekiliyor...");
    EnableWindow(g_hBtnFetch, FALSE);
    SetWindowTextW(g_hStaticTitle, L"Medya bilgileri sorgulanıyor, lütfen bekleyin...");
    SetWindowTextW(g_hStaticMeta, L"");
    SetWindowTextW(g_hStaticReason, L"");

    engine_fetch_info_async(url, on_fetch_callback, NULL);
}

static void update_format_selection(void)
{
    if (!g_has_meta) return;

    int is_video = (SendMessageW(g_hRadioVideo, BM_GETCHECK, 0, 0) == BST_CHECKED);
    int target_res = 0;

    if (is_video) {
        ShowWindow(g_hComboRes, SW_SHOW);
        ShowWindow(g_hChkForceMp4, SW_SHOW);
        ShowWindow(g_hComboAudio, SW_HIDE);

        int cur_sel = (int)SendMessageW(g_hComboRes, CB_GETCURSEL, 0, 0);
        if (cur_sel > 0 && cur_sel - 1 < g_current_meta.num_resolutions) {
            target_res = g_current_meta.available_resolutions[cur_sel - 1];
        }

        int force_mp4 = (SendMessageW(g_hChkForceMp4, BM_GETCHECK, 0, 0) == BST_CHECKED);
        select_best_format(g_current_meta.formats, g_current_meta.format_count, L"video", target_res, force_mp4, L"opus", &g_current_selection);
    } else {
        ShowWindow(g_hComboRes, SW_HIDE);
        ShowWindow(g_hChkForceMp4, SW_HIDE);
        ShowWindow(g_hComboAudio, SW_SHOW);

        int aud_sel = (int)SendMessageW(g_hComboAudio, CB_GETCURSEL, 0, 0);
        const wchar_t *pref = (aud_sel == 1) ? L"aac" : ((aud_sel == 2) ? L"mp3" : L"opus");
        select_best_format(g_current_meta.formats, g_current_meta.format_count, L"audio", 0, 0, pref, &g_current_selection);
    }

    wchar_t reason_text[512];
    if (g_current_selection.warning[0] != L'\0') {
        _snwprintf(reason_text, 512, L"[!] %ls\n%ls", g_current_selection.warning, g_current_selection.reason);
    } else {
        _snwprintf(reason_text, 512, L">> Hedef: .%ls | %ls", g_current_selection.expected_container, g_current_selection.reason);
    }
    SetWindowTextW(g_hStaticReason, reason_text);
}

static void trigger_download(void)
{
    if (!g_has_meta) {
        MessageBoxW(g_hWnd, L"Lütfen önce bir video bağlantısı getirip seçin.", L"Uyarı", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t url[1024];
    GetWindowTextW(g_hEditUrl, url, 1023);

    EnableWindow(g_hBtnDownload, FALSE);
    EnableWindow(g_hBtnCancel, TRUE);
    SendMessageW(g_hProgressBar, PBM_SETPOS, 0, 0);
    SetWindowTextW(g_hStaticStatus, L"İndirme başlatılıyor...");

    engine_download_async(url, &g_current_selection, g_download_dir, on_download_tick_callback, NULL);
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
    if (!g_nid.hIcon) g_nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcsncpy(g_nid.szTip, L"ff-dlp-viewer", sizeof(g_nid.szTip)/sizeof(wchar_t) - 1);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
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
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;
            DragAcceptFiles(hWnd, TRUE);

            /* Default Download Directory */
            if (SHGetFolderPathW(NULL, CSIDL_MYVIDEO, NULL, 0, g_download_dir) != S_OK) {
                SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, g_download_dir);
                wcsncat(g_download_dir, L"\\Downloads", MAX_PATH - 1);
            }

            /* Clean native Segoe UI fonts */
            g_fontTitle = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_fontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_fontNormal = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_fontSmall = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

            /* =========================================================
               GROUP 1: URL & BAĞLANTI (x=20, y=45, w=590, h=65)
               ========================================================= */
            HWND hGrpUrl = CreateWindowExW(0, L"BUTTON", L" Bağlantı ",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                20, 45, 590, 65, hWnd, (HMENU)IDC_GRP_URL, hInst, NULL);
            SendMessageW(hGrpUrl, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hEditUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                32, 68, 390, 25, hWnd, (HMENU)IDC_EDIT_URL, hInst, NULL);
            SendMessageW(g_hEditUrl, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnPaste = CreateWindowExW(0, L"BUTTON", L"Yapıştır",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                430, 67, 80, 27, hWnd, (HMENU)IDC_BTN_PASTE, hInst, NULL);
            SendMessageW(g_hBtnPaste, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnFetch = CreateWindowExW(0, L"BUTTON", L"Bilgi Getir",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                516, 67, 82, 27, hWnd, (HMENU)IDC_BTN_FETCH, hInst, NULL);
            SendMessageW(g_hBtnFetch, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            /* =========================================================
               GROUP 2: MEDYA VE FORMAT AYARLARI (x=20, y=118, w=590, h=195)
               ========================================================= */
            HWND hGrpMedia = CreateWindowExW(0, L"BUTTON", L" Medya ve Format Seçimi ",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                20, 118, 590, 195, hWnd, (HMENU)IDC_GRP_MEDIA, hInst, NULL);
            SendMessageW(hGrpMedia, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hStaticTitle = CreateWindowExW(0, L"STATIC", L"Bağlantıyı girin ve 'Bilgi Getir' butonuna basın.",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                32, 142, 565, 20, hWnd, (HMENU)IDC_STATIC_TITLE, hInst, NULL);
            SendMessageW(g_hStaticTitle, WM_SETFONT, (WPARAM)g_fontBold, TRUE);

            g_hStaticMeta = CreateWindowExW(0, L"STATIC", L"Desteklenenler: YouTube, TikTok, Instagram, X (Twitter), Web",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                32, 164, 565, 18, hWnd, (HMENU)IDC_STATIC_META, hInst, NULL);
            SendMessageW(g_hStaticMeta, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            g_hRadioVideo = CreateWindowExW(0, L"BUTTON", L"Video Olarak İndir",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                32, 192, 150, 22, hWnd, (HMENU)IDC_RADIO_VIDEO, hInst, NULL);
            SendMessageW(g_hRadioVideo, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            SendMessageW(g_hRadioVideo, BM_SETCHECK, BST_CHECKED, 0);

            g_hRadioAudio = CreateWindowExW(0, L"BUTTON", L"Yalnızca Ses (Müzik)",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                195, 192, 160, 22, hWnd, (HMENU)IDC_RADIO_AUDIO, hInst, NULL);
            SendMessageW(g_hRadioAudio, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hComboRes = CreateWindowExW(0, L"COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                32, 222, 200, 200, hWnd, (HMENU)IDC_COMBO_RES, hInst, NULL);
            SendMessageW(g_hComboRes, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            SendMessageW(g_hComboRes, CB_ADDSTRING, 0, (LPARAM)L"En Yüksek Kalite (Varsayılan)");
            SendMessageW(g_hComboRes, CB_SETCURSEL, 0, 0);

            g_hChkForceMp4 = CreateWindowExW(0, L"BUTTON", L"MP4 Olarak Zorla",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                245, 223, 160, 22, hWnd, (HMENU)IDC_CHK_FORCE_MP4, hInst, NULL);
            SendMessageW(g_hChkForceMp4, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hComboAudio = CreateWindowExW(0, L"COMBOBOX", NULL,
                WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                32, 222, 200, 200, hWnd, (HMENU)IDC_COMBO_AUDIO, hInst, NULL);
            SendMessageW(g_hComboAudio, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            SendMessageW(g_hComboAudio, CB_ADDSTRING, 0, (LPARAM)L"Opus (Ham En Yüksek Kalite)");
            SendMessageW(g_hComboAudio, CB_ADDSTRING, 0, (LPARAM)L"AAC (M4A - Apple Uyumlu)");
            SendMessageW(g_hComboAudio, CB_ADDSTRING, 0, (LPARAM)L"MP3 (Evrensel Format)");
            SendMessageW(g_hComboAudio, CB_SETCURSEL, 0, 0);

            g_hStaticReason = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                32, 256, 565, 45, hWnd, (HMENU)IDC_STATIC_REASON, hInst, NULL);
            SendMessageW(g_hStaticReason, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

            /* =========================================================
               GROUP 3: İNDİRME DURUMU & EYLEMLER (x=20, y=320, w=590, h=135)
               ========================================================= */
            HWND hGrpDownload = CreateWindowExW(0, L"BUTTON", L" İndirme Durumu ",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                20, 320, 590, 135, hWnd, (HMENU)IDC_GRP_DOWNLOAD, hInst, NULL);
            SendMessageW(hGrpDownload, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                32, 342, 565, 20, hWnd, (HMENU)IDC_PROGRESS, hInst, NULL);

            g_hStaticStatus = CreateWindowExW(0, L"STATIC", L"Hazır",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                32, 368, 565, 20, hWnd, (HMENU)IDC_STATIC_STATUS, hInst, NULL);
            SendMessageW(g_hStaticStatus, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnDownload = CreateWindowExW(0, L"BUTTON", L"İNDİR",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                32, 400, 120, 36, hWnd, (HMENU)IDC_BTN_DOWNLOAD, hInst, NULL);
            SendMessageW(g_hBtnDownload, WM_SETFONT, (WPARAM)g_fontBold, TRUE);

            g_hBtnCancel = CreateWindowExW(0, L"BUTTON", L"İptal",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                160, 400, 90, 36, hWnd, (HMENU)IDC_BTN_CANCEL, hInst, NULL);
            SendMessageW(g_hBtnCancel, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnOpenFolder = CreateWindowExW(0, L"BUTTON", L"Klasörü Aç",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                400, 400, 95, 36, hWnd, (HMENU)IDC_BTN_OPEN_FOLDER, hInst, NULL);
            SendMessageW(g_hBtnOpenFolder, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            g_hBtnChooseFolder = CreateWindowExW(0, L"BUTTON", L"Klasör Seç",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                502, 400, 95, 36, hWnd, (HMENU)IDC_BTN_CHOOSE_FOLDER, hInst, NULL);
            SendMessageW(g_hBtnChooseFolder, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

            init_tray(hWnd, hInst);
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
                        trigger_fetch();
                    }
                    fclose(f);
                } else {
                    SetWindowTextW(g_hEditUrl, dropped_file);
                    trigger_fetch();
                }
            }
            DragFinish(hDrop);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
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

            /* Clean native dialog background */
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));

            /* Clean plain title header */
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            SelectObject(hdc, g_fontTitle);
            TextOutW(hdc, 22, 14, L"ff-dlp-viewer", 13);

            /* Subtitle */
            SelectObject(hdc, g_fontSmall);
            SetTextColor(hdc, RGB(110, 110, 110));
            TextOutW(hdc, 150, 18, L"Hafif Medya İndirici (Native < 1 MB)", 36);

            /* Platform Badge */
            SelectObject(hdc, g_fontBold);
            const wchar_t *badge = g_has_meta ? g_current_meta.platform.badge_label : L"WEB";
            RECT badge_rc = { 530, 14, 610, 36 };
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
                            trigger_fetch();
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
                SetWindowTextW(g_hStaticStatus, L"İptal ediliyor...");
            } else if (wmId == IDC_BTN_OPEN_FOLDER) {
                ShellExecuteW(NULL, L"open", g_download_dir, NULL, NULL, SW_SHOWNORMAL);
            } else if (wmId == IDC_BTN_CHOOSE_FOLDER) {
                choose_folder_com(hWnd);
            } else if (wmId == IDC_RADIO_VIDEO || wmId == IDC_RADIO_AUDIO ||
                       wmId == IDC_CHK_FORCE_MP4 ||
                       (wmId == IDC_COMBO_RES && wmEvent == CBN_SELCHANGE) ||
                       (wmId == IDC_COMBO_AUDIO && wmEvent == CBN_SELCHANGE)) {
                update_format_selection();
            } else if (wmId == ID_TRAY_RESTORE) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
                g_in_tray = 0;
            } else if (wmId == ID_TRAY_EXIT) {
                engine_cancel();
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_USER_FETCH_DONE: {
            FetchResultMsg *msg = (FetchResultMsg*)lParam;
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
            } else {
                g_has_meta = 0;
                SetWindowTextW(g_hStaticTitle, L"Bilgi alınamadı.");
                SetWindowTextW(g_hStaticMeta, msg->error_msg);
                MessageBoxW(hWnd, msg->error_msg, L"Hata", MB_OK | MB_ICONERROR);
            }

            free(msg);
            return 0;
        }

        case WM_USER_DOWNLOAD_TICK: {
            DownloadTickMsg *msg = (DownloadTickMsg*)lParam;

            if (wcscmp(msg->status, L"downloading") == 0) {
                SendMessageW(g_hProgressBar, PBM_SETPOS, (WPARAM)(int)msg->percent, 0);

                wchar_t speed_str[64];
                format_bytes_str((long long)msg->speed, speed_str, 64);

                wchar_t dl_str[64];
                format_bytes_str(msg->downloaded, dl_str, 64);

                wchar_t tot_str[64];
                format_bytes_str(msg->total, tot_str, 64);

                wchar_t status_line[256];
                _snwprintf(status_line, 256, L"İndiriliyor: %%.1f | Hız: %ls/s | Kalan: %02d:%02d | %ls / %ls",
                    msg->percent, speed_str, msg->eta / 60, msg->eta % 60, dl_str, tot_str);
                SetWindowTextW(g_hStaticStatus, status_line);
            } else if (wcscmp(msg->status, L"processing") == 0) {
                SendMessageW(g_hProgressBar, PBM_SETPOS, 100, 0);
                SetWindowTextW(g_hStaticStatus, L"Akışlar birleştiriliyor ve konteyner tamamlanıyor...");
            } else if (wcscmp(msg->status, L"finished") == 0) {
                SendMessageW(g_hProgressBar, PBM_SETPOS, 100, 0);
                SetWindowTextW(g_hStaticStatus, L"[Tamamlandı] Dosya başarıyla kaydedildi.");
                EnableWindow(g_hBtnDownload, TRUE);
                EnableWindow(g_hBtnCancel, FALSE);
            } else if (wcscmp(msg->status, L"cancelled") == 0) {
                SetWindowTextW(g_hStaticStatus, L"İndirme iptal edildi.");
                EnableWindow(g_hBtnDownload, TRUE);
                EnableWindow(g_hBtnCancel, FALSE);
            } else if (wcscmp(msg->status, L"error") == 0) {
                SetWindowTextW(g_hStaticStatus, msg->error_msg);
                EnableWindow(g_hBtnDownload, TRUE);
                EnableWindow(g_hBtnCancel, FALSE);
                MessageBoxW(hWnd, msg->error_msg, L"İndirme Hatası", MB_OK | MB_ICONERROR);
            }

            free(msg);
            return 0;
        }

        case WM_USER_TRAY_ICON: {
            if (lParam == WM_RBUTTONUP) {
                show_tray_menu(hWnd);
            } else if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
                g_in_tray = 0;
            }
            return 0;
        }

        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) {
                if (engine_is_busy()) {
                    ShowWindow(hWnd, SW_HIDE);
                    g_in_tray = 1;
                }
            }
            break;
        }

        case WM_CLOSE: {
            if (engine_is_busy()) {
                int res = MessageBoxW(hWnd,
                    L"İndirme işlemi devam ediyor.\nArka planda devam etmesi için sistem tepsisine küçültülsün mü?",
                    L"İndirme Sürüyor", MB_YESNOCANCEL | MB_ICONQUESTION);
                if (res == IDYES) {
                    ShowWindow(hWnd, SW_HIDE);
                    g_in_tray = 1;
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
            remove_tray();
            if (g_fontTitle) DeleteObject(g_fontTitle);
            if (g_fontBold) DeleteObject(g_fontBold);
            if (g_fontNormal) DeleteObject(g_fontNormal);
            if (g_fontSmall) DeleteObject(g_fontSmall);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int ui_run(HINSTANCE hInstance, int nCmdShow)
{
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(101));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"FfDlpViewerLightWindow";

    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"FfDlpViewerLightWindow",
        L"ff-dlp-viewer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 646, 505,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        CoUninitialize();
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
