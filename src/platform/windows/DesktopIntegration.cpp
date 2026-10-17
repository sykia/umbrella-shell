#include "DesktopIntegration.h"
#include <windows.h>
#include <array>
namespace umbrella::platform {
std::wstring desktopWallpaper() {
    std::array<wchar_t, 32768> path{};
    return SystemParametersInfoW(SPI_GETDESKWALLPAPER, static_cast<UINT>(path.size()), path.data(), 0) ? path.data() : L"";
}
bool applyDesktopWallpaper(const std::wstring& path) {
    return SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, const_cast<wchar_t*>(path.c_str()), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE) != FALSE;
}
bool interactiveDesktopAvailable() {
    HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desktop) return false;
    wchar_t name[64]{}; DWORD required = 0;
    const bool interactive = GetUserObjectInformationW(desktop, UOI_NAME, name, sizeof(name), &required) && !_wcsicmp(name, L"Default");
    CloseDesktop(desktop);
    return interactive;
}
std::wstring activeApplication() {
    DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (!pid) return {};
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    std::array<wchar_t, 32768> path{}; DWORD length = static_cast<DWORD>(path.size());
    const bool ok = QueryFullProcessImageNameW(process, 0, path.data(), &length) != FALSE;
    CloseHandle(process);
    if (!ok) return {};
    const std::wstring full(path.data(), length);
    const auto slash = full.find_last_of(L"\\/");
    return full.substr(slash == std::wstring::npos ? 0 : slash + 1);
}
}
