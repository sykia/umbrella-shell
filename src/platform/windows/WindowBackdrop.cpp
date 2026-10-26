#include "WindowBackdrop.h"
#include <QWindow>
#include <Windows.h>
#include <dwmapi.h>

namespace umbrella::platform {
namespace {
// Existing Windows 11 compatibility path: system Acrylic adds an opaque tint
// on these frameless Qt surfaces. Keep the private ABI isolated and fail closed.
struct AccentPolicy { int state; unsigned flags; unsigned color; int animation; };
struct CompositionData { int attribute; void* data; SIZE_T size; };
using SetComposition = BOOL(WINAPI*)(HWND, CompositionData*);
bool accentBlur(HWND hwnd, bool enabled) {
    const auto user32 = GetModuleHandleW(L"user32.dll");
    const auto set = user32 ? reinterpret_cast<SetComposition>(GetProcAddress(user32, "SetWindowCompositionAttribute")) : nullptr;
    if (!set) return false;
    AccentPolicy policy{enabled ? 3 : 0, 0, 0, 0};
    CompositionData data{19, &policy, sizeof(policy)};
    return set(hwnd, &data) != FALSE;
}
}

bool applyWindowBackdrop(QWindow& window, bool enabled, bool dark, bool acrylic) {
    const auto hwnd = reinterpret_cast<HWND>(window.winId());
    if ((GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) != 0) return false;
    const BOOL darkMode = dark;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    if (acrylic) {
        // A clear compositor blur, shared by editor and the glass material.
        // Do not combine it with the system Acrylic's opaque base layer.
        const auto none = DWMSBT_NONE;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &none, sizeof(none));
        const MARGINS reset{};
        DwmExtendFrameIntoClientArea(hwnd, &reset);
        if (enabled && accentBlur(hwnd, true)) return true;
        accentBlur(hwnd, false);
        if (!enabled) return false;
    } else {
        accentBlur(hwnd, false);
    }
    // Documented system backdrop fallback.
    auto type = enabled ? (acrylic ? DWMSBT_TRANSIENTWINDOW : DWMSBT_MAINWINDOW) : DWMSBT_NONE;
    auto result = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
    if (enabled && FAILED(result)) {
        type = DWMSBT_TRANSIENTWINDOW;
        result = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
    }
    const MARGINS margins = enabled && SUCCEEDED(result) ? MARGINS{-1,-1,-1,-1} : MARGINS{};
    const auto frame = DwmExtendFrameIntoClientArea(hwnd, &margins);
    if (!enabled || FAILED(result) || FAILED(frame)) {
        type = DWMSBT_NONE;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
        const MARGINS reset{};
        DwmExtendFrameIntoClientArea(hwnd, &reset);
        return false;
    }
    return true;
}
}
