#include "WindowPlatformAttributes.h"
#include <QWidget>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace Resources {
void applyWindowPlatformAttributes(QWidget* window)
{
#ifdef Q_OS_WIN
    if (!window) return;
    HWND hwnd = (HWND)window->winId();
    if (hwnd) {
        // 33: DWMWA_WINDOW_CORNER_PREFERENCE, 2: DWMWCP_ROUND
        DWORD cornerPreference = 2;
        DwmSetWindowAttribute(hwnd, 33, &cornerPreference, sizeof(cornerPreference));
        
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
#endif
    Q_UNUSED(window);
}
}
