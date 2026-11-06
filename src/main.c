#include "ui.h"
#include "engine.h"
#include "detector.h"
#include "codec_selector.h"
#include "sanitization.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>

#define APP_VERSION_STR L"2.0.0-light"
#define APP_MUTEX_NAME  L"Local\\ff-dlp-viewer-light"
#define WM_COPYDATA_URL 1

static void show_cli_help(void)
{
    MessageBoxW(NULL,
        L"ff-dlp-viewer (Native Ultra-Lightweight < 1 MB)\n\n"
        L"Kullanım:\n"
        L"  ff-dlp-viewer.exe                   -> Grafik arayüzü (GUI) açar.\n"
        L"  ff-dlp-viewer.exe --help            -> Bu yardım penceresini gösterir.\n"
        L"  ff-dlp-viewer.exe --version         -> Sürüm bilgisini gösterir.\n"
        L"  ff-dlp-viewer.exe --url <URL>       -> Verilen URL ile uygulamayı başlatır.\n"
        L"  ff-dlp-viewer.exe --url=<URL>       -> Aynı işi tek argümanla yapar.\n\n"
        L"Uygulama zaten açıksa yeni pencere açılmaz; URL mevcut pencereye aktarılır.\n",
        L"ff-dlp-viewer Yardım",
        MB_OK | MB_ICONINFORMATION
    );
}

/* Aynı anda tek pencere çalışsın: yeni çağrı, açık pencereye URL gönderir. */
static int forward_to_existing_instance(const wchar_t *url)
{
    HWND existing = FindWindowW(UI_WINDOW_CLASS, NULL);
    if (!existing) return 0;

    ShowWindow(existing, SW_RESTORE);
    SetForegroundWindow(existing);

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
                _snwprintf(ver_buf, 128, L"ff-dlp-viewer v%ls (Win32 Native Ultra-Lightweight)", APP_VERSION_STR);
                MessageBoxW(NULL, ver_buf, L"Sürüm", MB_OK | MB_ICONINFORMATION);
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
            L"--url parametresi geçerli bir http/https bağlantısı olmalı.\n"
            L"Örnek: ff-dlp-viewer.exe --url \"https://...\"",
            L"Geçersiz Bağlantı",
            MB_OK | MB_ICONWARNING);
        start_url[0] = L'\0';
    }

    HANDLE instance_mutex = CreateMutexW(NULL, TRUE, APP_MUTEX_NAME);
    if (instance_mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        int forwarded = forward_to_existing_instance(start_url[0] ? start_url : NULL);
        if (instance_mutex) {
            ReleaseMutex(instance_mutex);
            CloseHandle(instance_mutex);
        }
        if (forwarded) return 0;
        /* Pencere henüz oluşmadıysa normal başlatmaya devam et. */
    }

    engine_init();

    int ret = ui_run(hInstance, nCmdShow, start_url[0] ? start_url : NULL);

    engine_cleanup();

    if (instance_mutex) {
        ReleaseMutex(instance_mutex);
        CloseHandle(instance_mutex);
    }
    return ret;
}
