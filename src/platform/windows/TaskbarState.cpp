#include "TaskbarState.h"
#include <Windows.h>
#include <shellapi.h>

namespace umbrella::platform {
std::optional<bool> taskbarAutoHide() {
    if (!FindWindowW(L"Shell_TrayWnd", nullptr)) return std::nullopt;
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    return (SHAppBarMessage(ABM_GETSTATE, &data) & ABS_AUTOHIDE) != 0;
}
bool requestTaskbarAutoHide(bool enabled) {
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!data.hWnd) return false;
    const auto current = SHAppBarMessage(ABM_GETSTATE, &data);
    data.lParam = static_cast<LPARAM>(enabled ? current | ABS_AUTOHIDE : current & ~ULONG_PTR(ABS_AUTOHIDE));
    SHAppBarMessage(ABM_SETSTATE, &data);
    return true;
}
}
