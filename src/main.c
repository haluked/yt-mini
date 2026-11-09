#include "ui.h"
#include "engine.h"
#include "detector.h"
#include "codec_selector.h"
#include "sanitization.h"
#include "bootstrapper.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <shlobj.h>

#define APP_VERSION_STR L"2.4.0"
#define APP_MUTEX_NAME  L"Local\\ff-dlp-viewer"
#define WM_COPYDATA_URL 1

/* If the first instance is still in the first-run bootstrapper, it has no
   window yet and WM_COPYDATA cannot be delivered. Store the link for the
   instance that is starting up instead of silently dropping it. */
static int pending_url_path(wchar_t *path, size_t max_len)
{
    wchar_t appdata[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata) != S_OK) return 0;
    _snwprintf(path, max_len, L"%ls\\ff-dlp-viewer\\pending_url.txt", appdata);
    path[max_len - 1] = L'\0';
    return 1;
}

static int save_pending_url(const wchar_t *url)
{
    wchar_t path[MAX_PATH];
    wchar_t dir[MAX_PATH];
    if (!url || !url[0] || !pending_url_path(path, MAX_PATH)) return 0;

    wcsncpy(dir, path, MAX_PATH - 1);
    dir[MAX_PATH - 1] = L'\0';
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) *slash = L'\0';
    SHCreateDirectoryExW(NULL, dir, NULL);

    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD written = 0;
    DWORD bytes = (DWORD)((wcslen(url) + 1) * sizeof(wchar_t));
    int ok = WriteFile(h, url, bytes, &written, NULL) && written == bytes;
    CloseHandle(h);
    return ok;
}

static int take_pending_url(wchar_t *out, size_t max_len)
{
    wchar_t path[MAX_PATH];
    if (!out || max_len < 2 || !pending_url_path(path, MAX_PATH)) return 0;

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;

    DWORD bytes = 0;
    DWORD capacity = (DWORD)((max_len - 1) * sizeof(wchar_t));
    int ok = ReadFile(h, out, capacity, &bytes, NULL) && bytes >= sizeof(wchar_t);
    CloseHandle(h);
    DeleteFileW(path);

    size_t chars = bytes / sizeof(wchar_t);
    if (chars >= max_len) chars = max_len - 1;
    out[chars] = L'\0';
    if (!ok || !is_valid_url(out)) {
        out[0] = L'\0';
        return 0;
    }
    return 1;
}

static void show_cli_help(void)
{
    MessageBoxW(NULL,
        L"ff-dlp-viewer (Native Win32 UI, < 1 MB)\n\n"
        L"Usage:\n"
        L"  ff-dlp-viewer.exe                   -> Opens the graphical interface (GUI).\n"
        L"  ff-dlp-viewer.exe --help            -> Shows this help window.\n"
        L"  ff-dlp-viewer.exe --version         -> Shows version information.\n"
        L"  ff-dlp-viewer.exe --url <URL>       -> Starts the app with the given URL.\n"
        L"  ff-dlp-viewer.exe --url=<URL>       -> Same, as a single argument.\n\n"
        L"Close (X): hides the window to the system tray.\n"
        L"Full exit: right-click the tray icon -> Quit.\n"
        L"If the app is already running, no new window opens; the URL is sent to the existing window.\n",
        L"ff-dlp-viewer Help",
        MB_OK | MB_ICONINFORMATION
    );
}

/* Keep a single window: a new call brings the open window forward and sends the URL if present. */
static int forward_to_existing_instance(const wchar_t *url)
{
    HWND existing = NULL;
    /* A few seconds covers the window-creation gap; a first-run bootstrapper
       still has no window, so main stores the link in pending_url.txt. */
    for (int retry = 0; retry < 50; retry++) {
        existing = FindWindowW(UI_WINDOW_CLASS, NULL);
        if (existing) break;
        Sleep(100);
    }
    if (!existing) return 0;

    /* It may be hidden in the tray (SW_HIDE) or minimized; make it visible and bring it forward. */
    ShowWindow(existing, SW_SHOW);
    ShowWindow(existing, SW_RESTORE);

    DWORD curThread = GetCurrentThreadId();
    DWORD foreThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    if (curThread != foreThread) {
        AttachThreadInput(curThread, foreThread, TRUE);
        BringWindowToTop(existing);
        SetForegroundWindow(existing);
        AttachThreadInput(curThread, foreThread, FALSE);
    } else {
        BringWindowToTop(existing);
        SetForegroundWindow(existing);
    }

    if (url && url[0] != L'\0') {
        COPYDATASTRUCT cds;
        cds.dwData = WM_COPYDATA_URL;
        cds.cbData = (DWORD)((wcslen(url) + 1) * sizeof(wchar_t));
        cds.lpData = (void*)url;
        SendMessageW(existing, WM_COPYDATA, 0, (LPARAM)&cds);
    }
    return 1;
}

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow
)
{
    (void)hPrevInstance;
    (void)pCmdLine;

    wchar_t start_url[1024] = L"";

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc > 1 && argv) {
        for (int i = 1; i < argc; i++) {
            if (_wcsicmp(argv[i], L"--help") == 0 || _wcsicmp(argv[i], L"-h") == 0 || _wcsicmp(argv[i], L"/?") == 0) {
                show_cli_help();
                LocalFree(argv);
                return 0;
            } else if (_wcsicmp(argv[i], L"--version") == 0 || _wcsicmp(argv[i], L"-v") == 0) {
                wchar_t ver_buf[128];
                _snwprintf(ver_buf, 128, L"ff-dlp-viewer v%ls (Win32 Native)", APP_VERSION_STR);
                MessageBoxW(NULL, ver_buf, L"Version", MB_OK | MB_ICONINFORMATION);
                LocalFree(argv);
                return 0;
            } else if (_wcsicmp(argv[i], L"--url") == 0) {
                if (i + 1 < argc) {
                    wcsncpy(start_url, argv[i + 1], 1023);
                    start_url[1023] = L'\0';
                    i++;
                }
            } else if (_wcsnicmp(argv[i], L"--url=", 6) == 0) {
                wcsncpy(start_url, argv[i] + 6, 1023);
                start_url[1023] = L'\0';
            }
        }
    }

    if (argv) LocalFree(argv);

    if (start_url[0] != L'\0' && !is_valid_url(start_url)) {
        MessageBoxW(NULL,
            L"--url must be a valid http/https link.\n"
            L"Example: ff-dlp-viewer.exe --url \"https://...\"",
            L"Invalid Link",
            MB_OK | MB_ICONWARNING);
        start_url[0] = L'\0';
    }

    HANDLE instance_mutex = CreateMutexW(NULL, TRUE, APP_MUTEX_NAME);
    if (instance_mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!forward_to_existing_instance(start_url[0] ? start_url : NULL) &&
            start_url[0] != L'\0') {
            /* The running copy may still be in the first-run setup dialog. */
            save_pending_url(start_url);
        }
        CloseHandle(instance_mutex);
        return 0; /* Firmly prevent a second copy from opening a window */
    }

    engine_init();

    /* First-run check: download yt-dlp or ffmpeg if missing */
    if (!bootstrapper_ensure_tools(NULL)) {
        engine_cleanup();
        if (instance_mutex) {
            ReleaseMutex(instance_mutex);
            CloseHandle(instance_mutex);
        }
        return 0;
    }

    if (start_url[0] == L'\0') {
        take_pending_url(start_url, 1024);
    }

    int ret = ui_run(hInstance, nCmdShow, start_url[0] ? start_url : NULL);

    engine_cleanup();

    if (instance_mutex) {
        ReleaseMutex(instance_mutex);
        CloseHandle(instance_mutex);
    }
    return ret;
}
