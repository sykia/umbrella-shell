#include "WindowBackdrop.h"
#include <QWindow>
#include <Windows.h>
#include <dwmapi.h>

namespace umbrella::platform {
namespace {
// Undocumented Windows composition ABI: keep it isolated to this adapter.
struct AccentPolicy { int state; unsigned flags; unsigned color; int animation; };
struct CompositionData { int attribute; void* data; SIZE_T size; };
using SetComposition = BOOL(WINAPI*)(HWND, CompositionData*);
}
bool applyWindowBackdrop(QWindow& window) {
    const auto hwnd = reinterpret_cast<HWND>(window.winId());
    const auto user32 = GetModuleHandleW(L"user32.dll");
    const auto setComposition = user32 ? reinterpret_cast<SetComposition>(
        GetProcAddress(user32, "SetWindowCompositionAttribute")) : nullptr;
    if (!setComposition) return false;

    constexpr auto backdropAttribute = static_cast<DWMWINDOWATTRIBUTE>(38);
    constexpr auto borderAttribute = static_cast<DWMWINDOWATTRIBUTE>(34);
    constexpr auto cornerAttribute = static_cast<DWMWINDOWATTRIBUTE>(33);
    constexpr int none = 1; // Disable system Acrylic's opaque tint.
    constexpr int round = 2;
    constexpr COLORREF noBorder = 0xfffffffe;
    // These optional appearance attributes are unavailable on older Windows builds.
    DwmSetWindowAttribute(hwnd, backdropAttribute, &none, sizeof(none));
    DwmSetWindowAttribute(hwnd, borderAttribute, &noBorder, sizeof(noBorder));
    DwmSetWindowAttribute(hwnd, cornerAttribute, &round, sizeof(round));

    AccentPolicy accent{3, 0, 0, 0}; // ACCENT_ENABLE_BLURBEHIND, no tint or border.
    CompositionData data{19, &accent, sizeof(accent)}; // WCA_ACCENT_POLICY.
    if (!setComposition(hwnd, &data)) {
        accent.state = 0;
        setComposition(hwnd, &data);
        return false;
    }
    return true;
}
}
