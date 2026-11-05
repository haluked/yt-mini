#ifndef UI_H
#define UI_H

#include <windows.h>
#include "engine.h"

#define WM_USER_FETCH_DONE    (WM_USER + 101)
#define WM_USER_DOWNLOAD_TICK (WM_USER + 102)
#define WM_USER_TRAY_ICON     (WM_USER + 103)

int ui_run(HINSTANCE hInstance, int nCmdShow);

#endif /* UI_H */
