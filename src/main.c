#include "ui.h"
#include "engine.h"
#include "detector.h"
#include "codec_selector.h"
#include "sanitization.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>

#define APP_VERSION_STR L"2.0.0-light"

static void show_cli_help(void)
{
    MessageBoxW(NULL,
        L"ff-dlp-viewer (Native Ultra-Lightweight < 1 MB)\n\n"
        L"Kullanım:\n"
        L"  ff-dlp-viewer.exe                   -> Grafik arayüzü (GUI) açar.\n"
        L"  ff-dlp-viewer.exe --help            -> Bu yardım penceresini gösterir.\n"
        L"  ff-dlp-viewer.exe --version         -> Sürüm bilgisini gösterir.\n"
        L"  ff-dlp-viewer.exe --url <URL>       -> Verilen URL ile uygulamayı başlatır.\n",
        L"ff-dlp-viewer Yardım",
        MB_OK | MB_ICONINFORMATION
    );
}

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow
)
{
    (void)hPrevInstance;

    engine_init();

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc > 1 && argv) {
        for (int i = 1; i < argc; i++) {
            if (_wcsicmp(argv[i], L"--help") == 0 || _wcsicmp(argv[i], L"-h") == 0 || _wcsicmp(argv[i], L"/?") == 0) {
                show_cli_help();
                LocalFree(argv);
                engine_cleanup();
                return 0;
            } else if (_wcsicmp(argv[i], L"--version") == 0 || _wcsicmp(argv[i], L"-v") == 0) {
                wchar_t ver_buf[128];
                _snwprintf(ver_buf, 128, L"ff-dlp-viewer v%ls (Win32 Native Ultra-Lightweight)", APP_VERSION_STR);
                MessageBoxW(NULL, ver_buf, L"Sürüm", MB_OK | MB_ICONINFORMATION);
                LocalFree(argv);
                engine_cleanup();
                return 0;
            }
        }
    }

    if (argv) LocalFree(argv);

    int ret = ui_run(hInstance, nCmdShow);

    engine_cleanup();
    return ret;
}
