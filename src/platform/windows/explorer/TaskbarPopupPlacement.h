#pragma once
#include <Windows.h>
#include <algorithm>

namespace umbrella::explorer::popup_placement {
inline bool overflow(HWND window) {
    wchar_t name[96]{};
    return GetClassNameW(window, name, 96) &&
        (!wcscmp(name, L"TopLevelWindowForOverflowXamlIsland") || !wcscmp(name, L"NotifyIconOverflowWindow"));
}
inline POINT position(const RECT& panel, const RECT& popup, const RECT& screen, unsigned edge, int gap) {
    const LONG width = popup.right - popup.left, height = popup.bottom - popup.top;
    LONG x = popup.left, y = popup.top;
    if (edge == 1) y = panel.bottom + gap;
    else if (edge == 3) y = panel.top - height - gap;
    else if (edge == 0) x = panel.right + gap;
    else if (edge == 2) x = panel.left - width - gap;
    x = (std::clamp)(x, screen.left, (std::max)(screen.left, screen.right - width));
    y = (std::clamp)(y, screen.top, (std::max)(screen.top, screen.bottom - height));
    return {x, y};
}
inline constexpr wchar_t ownerProperty[] = L"Umbrella.Taskbar.OverflowOwner.1";
}
